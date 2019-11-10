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

#include <Urho3D/Glow/Embree.h>

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Glow/LightmapUVGenerator.h>
#include <Urho3D/Graphics/Camera.h>
#include <Urho3D/Graphics/Graphics.h>
#include <Urho3D/Graphics/Material.h>
#include <Urho3D/Graphics/Model.h>
#include <Urho3D/Graphics/Octree.h>
#include <Urho3D/Graphics/Renderer.h>
#include <Urho3D/Graphics/StaticModel.h>
#include <Urho3D/Graphics/Zone.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/Scene/Scene.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>

#include <Urho3D/Graphics/View.h>
#include <Urho3D/Graphics/RenderPath.h>
#include <Urho3D/Graphics/GraphicsDefs.h>

#include "StaticScene.h"

#include <Urho3D/DebugNew.h>

bool GenerateModelLightmapUV(Model* model)
{
    NativeModelView nativeModelView(model->GetContext());
    nativeModelView.ImportModel(model);

    ModelView modelView(model->GetContext());
    modelView.ImportModel(nativeModelView);

    if (!GenerateLightmapUV(modelView, {}))
    {
        assert(0);
        return false;
    }

    modelView.ExportModel(nativeModelView);

    nativeModelView.ExportModel(model);
    return true;
}

void ReadTextureRGBA32Float(Texture* texture, ea::vector<Vector4>& dest)
{
    auto texture2D = dynamic_cast<Texture2D*>(texture);
    const unsigned numElements = texture->GetDataSize(texture->GetWidth(), texture->GetHeight()) / sizeof(Vector4);
    dest.resize(numElements);
    texture2D->GetData(0, dest.data());
}

SharedPtr<Image> ConvertToImage(Context* context,
    unsigned width, unsigned height, const ea::vector<Vector4>& data, const Vector4& scale, const Vector4& pad)
{
    auto image = MakeShared<Image>(context);
    image->SetSize(width, height, 4);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const Vector4& texel = data[y * width + x] * scale + pad;
            if (texel.w_ > 0.5)
                image->SetPixel(x, y, Color(texel.x_, texel.y_, texel.z_ , 1.0));
            else
                image->SetPixel(x, y, Color::BLACK);
        }
    }
    return image;
}

SharedPtr<Image> ConvertToImage(Context* context, unsigned width, unsigned height, const ea::vector<Color>& data)
{
    auto image = MakeShared<Image>(context);
    image->SetSize(width, height, 4);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Color color = data[y * width + x];
            color.a_ = 1.0f;
            image->SetPixel(x, y, color);
        }
    }
    return image;
}

bool GenerateLightMap(Context* context, Model* model, const Vector3& lightDirection)
{
    RTCDevice embreeDevice = rtcNewDevice("");
    RTCScene embreeScene = rtcNewScene(embreeDevice);

    {
        NativeModelView nativeModelView(model->GetContext());
        nativeModelView.ImportModel(model);

        ModelView modelView(model->GetContext());
        modelView.ImportModel(nativeModelView);

        const GeometryLODView& geometry = modelView.GetGeometries()[0].lods_[0];

        RTCGeometry embreeGeometry = rtcNewGeometry(embreeDevice, RTC_GEOMETRY_TYPE_TRIANGLE);

        float* vertices = reinterpret_cast<float*>(rtcSetNewGeometryBuffer(embreeGeometry, RTC_BUFFER_TYPE_VERTEX,
            0, RTC_FORMAT_FLOAT3, sizeof(Vector3), geometry.vertices_.size()));

        for (unsigned i = 0; i < geometry.vertices_.size(); ++i)
        {
            vertices[i * 3 + 0] = geometry.vertices_[i].position_.x_;
            vertices[i * 3 + 1] = geometry.vertices_[i].position_.y_;
            vertices[i * 3 + 2] = geometry.vertices_[i].position_.z_;
        }

        unsigned* indices = reinterpret_cast<unsigned*>(rtcSetNewGeometryBuffer(embreeGeometry, RTC_BUFFER_TYPE_INDEX,
            0, RTC_FORMAT_UINT3, sizeof(unsigned) * 3, geometry.faces_.size()));

        for (unsigned i = 0; i < geometry.faces_.size(); ++i)
        {
            indices[i * 3 + 0] = geometry.faces_[i].indices_[0];
            indices[i * 3 + 1] = geometry.faces_[i].indices_[1];
            indices[i * 3 + 2] = geometry.faces_[i].indices_[2];
        }

        rtcCommitGeometry(embreeGeometry);
        rtcAttachGeometry(embreeScene, embreeGeometry);
        rtcReleaseGeometry(embreeGeometry);
    }

    rtcCommitScene(embreeScene);

    ResourceCache* cache = context->GetCache();
    Graphics* graphics = context->GetGraphics();

    unsigned width = 1024;
    unsigned height = 1024;

    // Get render path
    auto renderPathXml = cache->GetResource<XMLFile>("RenderPaths/LightmapGBuffer.xml");
    auto renderPath = MakeShared<RenderPath>();
    renderPath->Load(renderPathXml);

    // Get material
    auto material = cache->GetResource<Material>("Materials/LightmapBaker.xml");
    material->SetShaderParameter("LMOffset", Vector4(1, 1, 0, 0));

    // Create scene
    auto scene = MakeShared<Scene>(context);
    scene->CreateComponent<Octree>();
    scene->CreateComponent<Camera>();

    Node* node = scene->CreateChild();
    auto staticModel = node->CreateComponent<StaticModel>();
    staticModel->SetModel(model);
    staticModel->SetMaterial(material);

    // Allocate destination buffers
    SharedPtr<Texture2D> texture = MakeShared<Texture2D>(context);
    texture->SetSize(width, height, Graphics::GetRGBAFormat(), TEXTURE_RENDERTARGET);
    SharedPtr<RenderSurface> renderSurface(texture->GetRenderSurface());

    // Prepare views
    if (!graphics->BeginFrame())
        return false;

    // Get camera
    Camera* camera = scene->GetComponent<Camera>();

    // Setup viewport
    Viewport viewport(context);
    viewport.SetCamera(camera);
    viewport.SetRect(IntRect::ZERO);
    viewport.SetRenderPath(renderPath);
    viewport.SetScene(scene);

    // Render scene
    View view(context);
    view.Define(renderSurface, &viewport);
    view.Update(FrameInfo());
    view.Render();

    graphics->EndFrame();

    // Read buffers
    ea::vector<Vector4> positionBuffer;
    ea::vector<Vector4> smoothPositionBuffer;
    ea::vector<Vector4> faceNormalBuffer;
    ea::vector<Vector4> smoothNormalBuffer;

    ReadTextureRGBA32Float(view.GetExtraRenderTarget("position"), positionBuffer);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("smoothposition"), smoothPositionBuffer);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("facenormal"), faceNormalBuffer);
    ReadTextureRGBA32Float(view.GetExtraRenderTarget("smoothnormal"), smoothNormalBuffer);

    ea::vector<Color> lightmapBuffer;
    lightmapBuffer.resize(width * height);

    const Vector3 rayDirection = -lightDirection.Normalized();
    for (unsigned i = 0; i < lightmapBuffer.size(); ++i)
    {
        const unsigned geometryId = static_cast<unsigned>(positionBuffer[i].w_);
        if (!geometryId)
            continue;

        const Vector3 position = static_cast<Vector3>(positionBuffer[i]);
        const Vector3 smoothNormal = static_cast<Vector3>(smoothNormalBuffer[i]);
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
        rtcIntersect1(embreeScene, &rayContext, &rayHit);

        const float shadow = rayHit.hit.geomID == RTC_INVALID_GEOMETRY_ID ? 1.0f : 0.0f;

        lightmapBuffer[i] = Color::WHITE * diffuseLighting * shadow;
    }

    // Save images
    const Vector4 testNormalScale{ 0.5f, 0.5f, 0.5f, 1.0f };
    const Vector4 testNormalPad{ 0.5f, 0.5f, 0.5f, 0.0f };

    auto smoothNormalImage = ConvertToImage(context, width, height, smoothNormalBuffer, testNormalScale, testNormalPad);
    smoothNormalImage->SaveFile("D:/temp/lm_smoothnormal.png");

    auto faceNormalImage = ConvertToImage(context, width, height, faceNormalBuffer, testNormalScale, testNormalPad);
    faceNormalImage->SaveFile("D:/temp/lm_facenormal.png");

    auto lightmapImage = ConvertToImage(context, width, height, lightmapBuffer);
    lightmapImage->SetName("Textures/Lightmap.png");
    lightmapImage->SaveFile(cache->GetResourceDir(1) + lightmapImage->GetName());

    rtcReleaseScene(embreeScene);
    rtcReleaseDevice(embreeDevice);
    return true;
}


StaticScene::StaticScene(Context* context) :
    Sample(context)
{
}

void StaticScene::Start()
{
    // Execute base class startup
    Sample::Start();

    // Create the scene content
    CreateScene();

    // Create the UI content
    CreateInstructions();

    // Setup the viewport for displaying the scene
    SetupViewport();

    // Hook up to the frame update events
    SubscribeToEvents();

    // Set the mouse mode to use in the sample
    Sample::InitMouseMode(MM_RELATIVE);
}

void StaticScene::CreateScene()
{
    auto* renderer = GetSubsystem<Renderer>();
    renderer->SetDynamicInstancing(false);

    auto* cache = GetSubsystem<ResourceCache>();

    scene_ = new Scene(context_);
    scene_->AddLightmap("Textures/Lightmap.png");

    //auto planeModel = cache->GetResource<Model>("Models/Plane.mdl");
    auto mushroomModel = cache->GetResource<Model>("Models/Mushroom.mdl"); //Mushroom
    //planeModel->SetNumGeometryLodLevels(0, 1);
    mushroomModel->SetNumGeometryLodLevels(0, 1);
    //GenerateModelLightmapUV(planeModel);
    GenerateModelLightmapUV(mushroomModel);
    //auto planeMaterial = cache->GetResource<Material>("Materials/DefaultGrey.xml");
    auto mushroomMaterial = cache->GetResource<Material>("Materials/DefaultGrey.xml"); //Mushroom
    mushroomMaterial->SetVertexShaderDefines(mushroomMaterial->GetVertexShaderDefines() + " LIGHTMAP");
    mushroomMaterial->SetPixelShaderDefines(mushroomMaterial->GetPixelShaderDefines() + " LIGHTMAP");
    //planeMaterial->SetVertexShaderDefines(planeMaterial->GetVertexShaderDefines() + " LIGHTMAP");
    //planeMaterial->SetPixelShaderDefines(planeMaterial->GetPixelShaderDefines() + " LIGHTMAP");

    //const Vector3 lightDirection{ 0.6f, -0.5f, 0.8f };
    const Vector3 lightDirection{ 0.0f, -1.0f, 0.0f };
    GenerateLightMap(context_, mushroomModel, lightDirection);

    // Create the Octree component to the scene. This is required before adding any drawable components, or else nothing will
    // show up. The default octree volume will be from (-1000, -1000, -1000) to (1000, 1000, 1000) in world coordinates; it
    // is also legal to place objects outside the volume but their visibility can then not be checked in a hierarchically
    // optimizing manner
    scene_->CreateComponent<Octree>();

    Node* zoneNode = scene_->CreateChild("Zone");
    Zone* zone = zoneNode->CreateComponent<Zone>();
    zone->SetBoundingBox(BoundingBox(-1000, 1000));
    zone->SetAmbientColor(Color::WHITE * 0.5f);

    {
        Node* mushroomNode = scene_->CreateChild("Mushroom");
        //mushroomNode->SetScale(Vector3(2.0f, 2.0f, 2.0f));
        //mushroomNode->SetPosition(Vector3(0.0f, 3.0f, 0.0f));
        auto* mushroomObject = mushroomNode->CreateComponent<StaticModel>();
        mushroomObject->SetModel(mushroomModel);
        mushroomObject->SetMaterial(mushroomMaterial);
        mushroomObject->SetLightmap(true);
        mushroomObject->SetCastShadows(true);
        mushroomObject->SetLightmapIndex(0);
        mushroomObject->SetLightmapTilingOffset(Vector4(1, 1, 0, 0));
    }

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(-5.0f, 5.0f, -5.0f));
    cameraNode_->SetDirection(Vector3(5.0f, -5.0f, 5.0f));
    yaw_ = cameraNode_->GetRotation().YawAngle();
    pitch_ = cameraNode_->GetRotation().PitchAngle();
}

void StaticScene::CreateInstructions()
{
    auto* cache = GetSubsystem<ResourceCache>();
    auto* ui = GetSubsystem<UI>();

    // Construct new Text object, set string to display and font to use
    auto* instructionText = ui->GetRoot()->CreateChild<Text>();
    instructionText->SetText("Use WASD keys and mouse/touch to move");
    instructionText->SetFont(cache->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 15);

    // Position the text relative to the screen center
    instructionText->SetHorizontalAlignment(HA_CENTER);
    instructionText->SetVerticalAlignment(VA_CENTER);
    instructionText->SetPosition(0, ui->GetRoot()->GetHeight() / 4);
}

void StaticScene::SetupViewport()
{
    auto* renderer = GetSubsystem<Renderer>();

    // Set up a viewport to the Renderer subsystem so that the 3D scene can be seen. We need to define the scene and the camera
    // at minimum. Additionally we could configure the viewport screen size and the rendering path (eg. forward / deferred) to
    // use, but now we just use full screen and default render path configured in the engine command line options
    SharedPtr<Viewport> viewport(new Viewport(context_, scene_, cameraNode_->GetComponent<Camera>()));
    renderer->SetViewport(0, viewport);
}

void StaticScene::MoveCamera(float timeStep)
{
    // Do not move if the UI has a focused element (the console)
    if (GetSubsystem<UI>()->GetFocusElement())
        return;

    auto* input = GetSubsystem<Input>();

    // Movement speed as world units per second
    const float MOVE_SPEED = 20.0f;
    // Mouse sensitivity as degrees per pixel
    const float MOUSE_SENSITIVITY = 0.1f;

    // Use this frame's mouse motion to adjust camera node yaw and pitch. Clamp the pitch between -90 and 90 degrees
    IntVector2 mouseMove = input->GetMouseMove();
    yaw_ += MOUSE_SENSITIVITY * mouseMove.x_;
    pitch_ += MOUSE_SENSITIVITY * mouseMove.y_;
    pitch_ = Clamp(pitch_, -90.0f, 90.0f);

    // Construct new orientation for the camera scene node from yaw and pitch. Roll is fixed to zero
    cameraNode_->SetRotation(Quaternion(pitch_, yaw_, 0.0f));

    // Read WASD keys and move the camera scene node to the corresponding direction if they are pressed
    // Use the Translate() function (default local space) to move relative to the node's orientation.
    if (input->GetKeyDown(KEY_W))
        cameraNode_->Translate(Vector3::FORWARD * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_S))
        cameraNode_->Translate(Vector3::BACK * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_A))
        cameraNode_->Translate(Vector3::LEFT * MOVE_SPEED * timeStep);
    if (input->GetKeyDown(KEY_D))
        cameraNode_->Translate(Vector3::RIGHT * MOVE_SPEED * timeStep);
}

void StaticScene::SubscribeToEvents()
{
    // Subscribe HandleUpdate() function for processing update events
    SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(StaticScene, HandleUpdate));
}

void StaticScene::HandleUpdate(StringHash eventType, VariantMap& eventData)
{
    using namespace Update;

    // Take the frame time step, which is stored as a float
    float timeStep = eventData[P_TIMESTEP].GetFloat();

    // Move the camera, scale movement with time step
    MoveCamera(timeStep);
}
