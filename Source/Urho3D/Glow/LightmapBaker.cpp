//
// Copyright (c) 2008-2019 the Urho3D project.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Embree includes must be first
#include <embree3/rtcore.h>
#include <embree3/rtcore_ray.h>
#define _SSIZE_T_DEFINED

#include "../Glow/LightmapBaker.h"
#include "../Glow/LightmapUVGenerator.h"
#include "../Graphics/Camera.h"
#include "../Graphics/Graphics.h"
#include "../Graphics/Material.h"
#include "../Graphics/Model.h"
#include "../Graphics/Octree.h"
#include "../Graphics/StaticModel.h"
#include "../Graphics/RenderPath.h"
#include "../Graphics/RenderSurface.h"
#include "../Graphics/Texture2D.h"
#include "../Graphics/View.h"
#include "../Graphics/Viewport.h"
#include "../Math/AreaAllocator.h"
#include "../Resource/ResourceCache.h"
#include "../Resource/XMLFile.h"

// TODO: Use thread pool?
#include <future>

namespace Urho3D
{

/// Description of lightmap region.
struct LightmapRegion
{
    /// Construct default.
    LightmapRegion() = default;
    /// Construct actual region.
    LightmapRegion(unsigned index, const IntVector2& position, const IntVector2& size, unsigned maxSize)
        : lightmapIndex_(index)
        , lightmapTexelRect_(position, position + size)
    {
        lightmapUVRect_.min_ = static_cast<Vector2>(lightmapTexelRect_.Min()) / static_cast<float>(maxSize);
        lightmapUVRect_.max_ = static_cast<Vector2>(lightmapTexelRect_.Max()) / static_cast<float>(maxSize);
    }
    /// Get lightmap offset vector.
    Vector4 GetScaleOffset() const
    {
        const Vector2 offset = lightmapUVRect_.Min();
        const Vector2 size = lightmapUVRect_.Size();
        return { size.x_, size.y_, offset.x_, offset.y_ };
    }

    /// Lightmap index.
    unsigned lightmapIndex_;
    /// Lightmap rectangle (in texels).
    IntRect lightmapTexelRect_;
    /// Lightmap rectangle (UV).
    Rect lightmapUVRect_;
};

/// Description of lightmap receiver.
struct LightmapReceiver
{
    /// Node.
    Node* node_;
    /// Static model.
    StaticModel* staticModel_{};
    /// Lightmap region.
    LightmapRegion region_;
};

/// Lightmap description.
struct LightmapDesc
{
    /// Area allocator for lightmap texture.
    AreaAllocator allocator_;
    /// Baking bakingScene.
    SharedPtr<Scene> bakingScene_;
    /// Baking camera.
    Camera* bakingCamera_{};
};

struct LightmapBakerImpl
{
    /// Destruct.
    ~LightmapBakerImpl()
    {
        if (embreeScene_)
            rtcReleaseScene(embreeScene_);
        if (embreeDevice_)
            rtcReleaseDevice(embreeDevice_);
    }
    /// Context.
    Context* context_{};
    /// Settings.
    LightmapBakingSettings settings_;
    /// Scene.
    Scene* scene_{};
    /// Lightmaps.
    ea::vector<LightmapDesc> lightmaps_;
    /// Lightmap receivers.
    ea::vector<LightmapReceiver> lightmapReceivers_;
    /// Baking render path.
    SharedPtr<RenderPath> bakingRenderPath_;
    /// Embree device.
    RTCDevice embreeDevice_{};
    /// Embree scene.
    RTCScene embreeScene_{};
    /// Render texture placeholder.
    SharedPtr<Texture2D> renderTexturePlaceholder_;
    /// Render surface placeholder.
    SharedPtr<RenderSurface> renderSurfacePlaceholder_;

    /// Calculation: texel positions
    ea::vector<Vector4> positionBuffer_;
    /// Calculation: texel smooth positions
    ea::vector<Vector4> smoothPositionBuffer_;
    /// Calculation: texel face normals
    ea::vector<Vector4> faceNormalBuffer_;
    /// Calculation: texel smooth normals
    ea::vector<Vector4> smoothNormalBuffer_;
};

/// Calculate model lightmap size.
static IntVector2 CalculateModelLightmapSize(const LightmapBakerImpl& impl, Model* model, const Vector3& scale)
{
    const Variant& lightmapSizeVar = model->GetMetadata(LightmapUVGenerationSettings::LightmapSizeKey);
    const Variant& lightmapDensityVar = model->GetMetadata(LightmapUVGenerationSettings::LightmapDensityKey);

    const auto modelLightmapSize = static_cast<Vector2>(lightmapSizeVar.GetIntVector2());
    const unsigned modelLightmapDensity = lightmapDensityVar.GetUInt();

    const float nodeScale = scale.DotProduct(DOT_SCALE);
    const float rescaleFactor = nodeScale * static_cast<float>(impl.settings_.texelDensity_) / modelLightmapDensity;
    const float clampedRescaleFactor = ea::max(impl.settings_.minLightmapScale_, rescaleFactor);

    return VectorCeilToInt(modelLightmapSize * clampedRescaleFactor);
}

/// Allocate lightmap region.
static LightmapRegion AllocateLightmapRegion(LightmapBakerImpl& impl, const IntVector2& size)
{
    const int padding = static_cast<int>(impl.settings_.lightmapPadding_);
    const IntVector2 paddedSize = size + 2 * padding * IntVector2::ONE;

    // Try existing maps
    unsigned lightmapIndex = 0;
    for (LightmapDesc& lightmapDesc : impl.lightmaps_)
    {
        IntVector2 paddedPosition;
        if (lightmapDesc.allocator_.Allocate(paddedSize.x_, paddedSize.y_, paddedPosition.x_, paddedPosition.y_))
        {
            const IntVector2 position = paddedPosition + padding * IntVector2::ONE;
            return { lightmapIndex, position, size, impl.settings_.lightmapSize_ };
        }
        ++lightmapIndex;
    }

    // Create new map
    const int lightmapSize = static_cast<int>(impl.settings_.lightmapSize_);
    LightmapDesc& lightmapDesc = impl.lightmaps_.push_back();

    // Allocate dedicated map for this specific region
    if (size.x_ > lightmapSize || size.y_ > lightmapSize)
    {
        lightmapDesc.allocator_.Reset(size.x_, size.y_, 0, 0, false);
        return { lightmapIndex, IntVector2::ZERO, size, impl.settings_.lightmapSize_ };
    }

    // Allocate chunk from new map
    lightmapDesc.allocator_.Reset(lightmapSize, lightmapSize, 0, 0, false);

    IntVector2 paddedPosition;
    const bool success = lightmapDesc.allocator_.Allocate(paddedSize.x_, paddedSize.y_, paddedPosition.x_, paddedPosition.y_);

    assert(success);
    assert(paddedPosition == IntVector2::ZERO);

    const IntVector2 position = paddedPosition + padding * IntVector2::ONE;
    return { lightmapIndex, position, size, impl.settings_.lightmapSize_ };
}

/// Initialize camera from bounding box.
void InitializeCameraBoundingBox(Camera* camera, const BoundingBox& boundingBox)
{
    Node* node = camera->GetNode();

    const float zNear = 1.0f;
    const float zFar = boundingBox.Size().z_ + zNear;
    Vector3 position = boundingBox.Center();
    position.z_ = boundingBox.min_.z_ - zNear;

    node->SetPosition(position);
    node->SetDirection(Vector3::FORWARD);

    camera->SetOrthographic(true);
    camera->SetOrthoSize(Vector2(boundingBox.Size().x_, boundingBox.Size().y_));
    camera->SetNearClip(zNear);
    camera->SetFarClip(zFar);
}

/// Parsed model key and value.
struct ParsedModelKeyValue
{
    Model* model_{};
    SharedPtr<ModelView> parsedModel_;
};

/// Parse model data.
ParsedModelKeyValue ParseModelForEmbree(Model* model)
{
    NativeModelView nativeModelView(model->GetContext());
    nativeModelView.ImportModel(model);

    auto modelView = MakeShared<ModelView>(model->GetContext());
    modelView->ImportModel(nativeModelView);

    return { model, modelView };
}

/// Embree geometry desc.
struct EmbreeGeometry
{
    /// Node.
    Node* node_{};
    /// Geometry index.
    unsigned geometryIndex_{};
    /// Geometry LOD.
    unsigned geometryLOD_{};
    /// Embree geometry.
    RTCGeometry embreeGeometry_;
};

/// Create Embree geometry from geometry view.
RTCGeometry CreateEmbreeGeometry(RTCDevice embreeDevice, const GeometryLODView& geometryLODView, Node* node)
{
    const Matrix3x4 worldTransform = node->GetWorldTransform();
    RTCGeometry embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);

    float* vertices = reinterpret_cast<float*>(rtcSetNewGeometryBuffer(embreeGeometry, RTC_BUFFER_TYPE_VERTEX,
        0, RTC_FORMAT_FLOAT3, sizeof(Vector3), geometryLODView.vertices_.size()));

    for (unsigned i = 0; i < geometryLODView.vertices_.size(); ++i)
    {
        const Vector3 localPosition = static_cast<Vector3>(geometryLODView.vertices_[i].position_);
        const Vector3 worldPosition = worldTransform * localPosition;
        vertices[i * 3 + 0] = worldPosition.x_;
        vertices[i * 3 + 1] = worldPosition.y_;
        vertices[i * 3 + 2] = worldPosition.z_;
    }

    unsigned* indices = reinterpret_cast<unsigned*>(rtcSetNewGeometryBuffer(embreeGeometry, RTC_BUFFER_TYPE_INDEX,
        0, RTC_FORMAT_UINT3, sizeof(unsigned) * 3, geometryLODView.faces_.size()));

    for (unsigned i = 0; i < geometryLODView.faces_.size(); ++i)
    {
        indices[i * 3 + 0] = geometryLODView.faces_[i].indices_[0];
        indices[i * 3 + 1] = geometryLODView.faces_[i].indices_[1];
        indices[i * 3 + 2] = geometryLODView.faces_[i].indices_[2];
    }

    rtcCommitGeometry(embreeGeometry);
    return embreeGeometry;
}

/// Create Embree geometry from parsed model.
ea::vector<EmbreeGeometry> CreateEmbreeGeometryArray(RTCDevice embreeDevice, ModelView* modelView, Node* node)
{
    ea::vector<EmbreeGeometry> result;

    unsigned geometryIndex = 0;
    for (const GeometryView& geometryView : modelView->GetGeometries())
    {
        unsigned geometryLod = 0;
        for (const GeometryLODView& geometryLODView : geometryView.lods_)
        {
            const RTCGeometry embreeGeometry = CreateEmbreeGeometry(embreeDevice, geometryLODView, node);
            result.push_back(EmbreeGeometry{ node, geometryIndex, geometryLod, embreeGeometry });
            ++geometryLod;
        }
        ++geometryIndex;
    }
    return result;
}

/// Read RGBA32 float texture to vector.
void ReadTextureRGBA32Float(Texture* texture, ea::vector<Vector4>& dest)
{
    auto texture2D = dynamic_cast<Texture2D*>(texture);
    const unsigned numElements = texture->GetDataSize(texture->GetWidth(), texture->GetHeight()) / sizeof(Vector4);
    dest.resize(numElements);
    texture2D->GetData(0, dest.data());
}

LightmapBaker::LightmapBaker(Context* context)
    : Object(context)
{
}

LightmapBaker::~LightmapBaker()
{
}

void LightmapBaker::Initialize(const LightmapBakingSettings& settings, Scene* scene,
        const ea::vector<Node*>& receivers, const ea::vector<Node*>& obstacles, const ea::vector<Light*>& lights)
{
    ResourceCache* cache = context_->GetCache();
    Material* bakingMaterial = cache->GetResource<Material>(settings.bakingMaterial_);

    impl_ = ea::make_unique<LightmapBakerImpl>();
    impl_->context_ = context_;
    impl_->settings_ = settings;
    impl_->scene_ = scene;

    // Load render path
    auto renderPath = MakeShared<RenderPath>();
    auto renderPathXml = cache->GetResource<XMLFile>("RenderPaths/LightmapGBuffer.xml");
    renderPath->Load(renderPathXml);
    impl_->bakingRenderPath_ = renderPath;

    // Initialize lightmap receivers
    BoundingBox receiversBoundingBox;
    for (Node* node : receivers)
    {
        auto staticModel = node->GetComponent<StaticModel>();
        const IntVector2 nodeLightmapSize = CalculateModelLightmapSize(*impl_, staticModel->GetModel(), node->GetWorldScale());
        const LightmapRegion lightmapRegion = AllocateLightmapRegion(*impl_, nodeLightmapSize);

        impl_->lightmapReceivers_.push_back(LightmapReceiver{ node, staticModel, lightmapRegion });

        receiversBoundingBox.Merge(node->GetWorldPosition());
    }

    // Allocate lightmap baking scenes
    for (LightmapDesc& lightmapDesc : impl_->lightmaps_)
    {
        auto bakingScene = MakeShared<Scene>(context_);
        bakingScene->CreateComponent<Octree>();

        auto camera = bakingScene->CreateComponent<Camera>();
        InitializeCameraBoundingBox(camera, receiversBoundingBox);

        lightmapDesc.bakingCamera_ = camera;
        lightmapDesc.bakingScene_ = bakingScene;
    }

    // Prepare baking scenes
    for (const LightmapReceiver& receiver : impl_->lightmapReceivers_)
    {
        LightmapDesc& lightmapDesc = impl_->lightmaps_[receiver.region_.lightmapIndex_];
        Scene* bakingScene = lightmapDesc.bakingScene_;

        if (receiver.staticModel_)
        {
            auto material = bakingMaterial->Clone();
            material->SetShaderParameter("LMOffset", receiver.region_.GetScaleOffset());

            Node* node = bakingScene->CreateChild();
            node->SetPosition(receiver.node_->GetWorldPosition());
            node->SetRotation(receiver.node_->GetWorldRotation());
            node->SetScale(receiver.node_->GetWorldScale());

            StaticModel* staticModel = node->CreateComponent<StaticModel>();
            staticModel->SetModel(receiver.staticModel_->GetModel());
            staticModel->SetMaterial(material);
        }
    }

    // Load models
    ea::vector<std::future<ParsedModelKeyValue>> asyncParsedModels;
    for (Node* node : obstacles)
    {
        if (auto staticModel = node->GetComponent<StaticModel>())
            asyncParsedModels.push_back(std::async(ParseModelForEmbree, staticModel->GetModel()));
    }

    // Prepare model cache
    ea::unordered_map<Model*, SharedPtr<ModelView>> parsedModelCache;
    for (auto& asyncModel : asyncParsedModels)
    {
        const ParsedModelKeyValue& parsedModel = asyncModel.get();
        parsedModelCache.emplace(parsedModel.model_, parsedModel.parsedModel_);
    }

    // Prepare Embree scene
    impl_->embreeDevice_ = rtcNewDevice("");
    impl_->embreeScene_ = rtcNewScene(impl_->embreeDevice_);

    ea::vector<std::future<ea::vector<EmbreeGeometry>>> asyncEmbreeGeometries;
    for (Node* node : obstacles)
    {
        if (auto staticModel = node->GetComponent<StaticModel>())
        {
            ModelView* parsedModel = parsedModelCache[staticModel->GetModel()];
            asyncEmbreeGeometries.push_back(std::async(CreateEmbreeGeometryArray, impl_->embreeDevice_, parsedModel, node));
        }
    }

    for (auto& asyncGeometry : asyncEmbreeGeometries)
    {
        const ea::vector<EmbreeGeometry> embreeGeometriesArray = asyncGeometry.get();
        for (const EmbreeGeometry& embreeGeometry : embreeGeometriesArray)
        {
            rtcAttachGeometry(impl_->embreeScene_, embreeGeometry.embreeGeometry_);
            rtcReleaseGeometry(embreeGeometry.embreeGeometry_);
        }
    }

    rtcCommitScene(impl_->embreeScene_);

    // Create render surface
    impl_->renderTexturePlaceholder_ = MakeShared<Texture2D>(context_);
    impl_->renderTexturePlaceholder_->SetSize(
        impl_->settings_.lightmapSize_, impl_->settings_.lightmapSize_, Graphics::GetRGBAFormat(), TEXTURE_RENDERTARGET);
    impl_->renderSurfacePlaceholder_ = impl_->renderTexturePlaceholder_->GetRenderSurface();
}

unsigned LightmapBaker::GetNumLightmaps() const
{
    return impl_->lightmaps_.size();
}

bool LightmapBaker::RenderLightmap(unsigned index, LightmapBakedData& data)
{
    if (index >= GetNumLightmaps())
        return false;

    Graphics* graphics = GetGraphics();
    ResourceCache* cache = GetCache();
    const unsigned lightmapSize = impl_->settings_.lightmapSize_;
    const LightmapDesc& lightmapDesc = impl_->lightmaps_[index];

    if (!graphics->BeginFrame())
        return false;

    // Setup viewport
    Viewport viewport(context_);
    viewport.SetCamera(lightmapDesc.bakingCamera_);
    viewport.SetRect(IntRect::ZERO);
    viewport.SetRenderPath(impl_->bakingRenderPath_);
    viewport.SetScene(lightmapDesc.bakingScene_);

    // Render bakingScene
    View view(context_);
    view.Define(impl_->renderSurfacePlaceholder_, &viewport);
    view.Update(FrameInfo());
    view.Render();

    graphics->EndFrame();

    // Read buffers
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("position"), impl_->positionBuffer_);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("smoothposition"), impl_->smoothPositionBuffer_);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("facenormal"), impl_->faceNormalBuffer_);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("smoothnormal"), impl_->smoothNormalBuffer_);

    // Prepare output buffers
    data.lightmapSize_ = lightmapSize;
    data.backedLighting_.resize(lightmapSize * lightmapSize);
    ea::fill(data.backedLighting_.begin(), data.backedLighting_.end(), Color::WHITE);

    // TODO: Unhack me
    // @{
    const Vector3 lightDirection{ 2.0f, -1.0f, 2.0f };
    const Vector3 rayDirection = -lightDirection.Normalized();
    for (unsigned i = 0; i < data.backedLighting_.size(); ++i)
    {
        const unsigned geometryId = static_cast<unsigned>(impl_->positionBuffer_[i].w_);
        if (!geometryId)
            continue;

        const Vector3 position = static_cast<Vector3>(impl_->positionBuffer_[i]);
        const Vector3 smoothNormal = static_cast<Vector3>(impl_->smoothNormalBuffer_[i]);
        const float diffuseLighting = ea::max(0.0f, smoothNormal.DotProduct(rayDirection));
        const Vector3 rayOrigin = position + rayDirection * 0.001f;

        RTCRayHit rayHit;
        rayHit.ray.org_x = rayOrigin.x_;
        rayHit.ray.org_y = rayOrigin.y_;
        rayHit.ray.org_z = rayOrigin.z_;
        rayHit.ray.dir_x = rayDirection.x_;
        rayHit.ray.dir_y = rayDirection.y_;
        rayHit.ray.dir_z = rayDirection.z_;
        rayHit.ray.tnear = 0.0f;
        rayHit.ray.tfar = 100.0f;
        rayHit.ray.time = 0.0f;
        rayHit.ray.id = 0;
        rayHit.ray.mask = 0xffffffff;
        rayHit.ray.flags = 0xffffffff;
        rayHit.hit.geomID = RTC_INVALID_GEOMETRY_ID;

        RTCIntersectContext rayContext;
        rtcInitIntersectContext(&rayContext);
        rtcIntersect1(impl_->embreeScene_, &rayContext, &rayHit);

        const float shadow = rayHit.hit.geomID == RTC_INVALID_GEOMETRY_ID ? 1.0f : 0.0f;

        data.backedLighting_[i] = Color::WHITE * diffuseLighting * shadow;
    }
    // @}

    return true;
}

void LightmapBaker::ApplyLightmapsToScene(unsigned baseLightmapIndex)
{
    for (const LightmapReceiver& receiver : impl_->lightmapReceivers_)
    {
        if (receiver.staticModel_)
        {
            receiver.staticModel_->SetLightmap(true);
            receiver.staticModel_->SetLightmapIndex(baseLightmapIndex + receiver.region_.lightmapIndex_);
            receiver.staticModel_->SetLightmapScaleOffset(receiver.region_.GetScaleOffset());
        }
    }
}

}
