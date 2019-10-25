// Copyright (c) 2014-2017, THUNDERBEAST GAMES LLC All rights reserved
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

#include <xmmintrin.h>
#include <pmmintrin.h>
#include <cmath>
#include <cfloat>

#include <STB/stb_rect_pack.h>

#include "../../Glow/Kernel/Embree.h"

#include "../../Core/WorkQueue.h"
#include "../../IO/Log.h"
#include "../../IO/FileSystem.h"
#include "../../Resource/ResourceCache.h"
#include "../../Graphics/Zone.h"
#include "../../Graphics/Light.h"
#include "../../Graphics/StaticModel.h"

#include "LightRay.h"
#include "BakeModel.h"
#include "BakeMesh.h"
#include "BakeLight.h"
#include "EmbreeScene.h"
#include "LightMapPacker.h"
#include "SceneBaker.h"
#include "Photons.h"

// #define STRINGIFY(x) #x
// #define TOSTRING(x) STRINGIFY(x)
// #define PRINT(x) static_assert(0, TOSTRING(x))
// PRINT(RTCORE_API);

using namespace Urho3D;

namespace AtomicGlow
{

SceneBaker::SceneBaker(Context* context, const ea::string &projectPath) : Object(context),
    currentLightMode_(GLOW_LIGHTMODE_UNDEFINED),
    currentGIPass_(0),
    projectPath_(AddTrailingSlash(projectPath)),
    standaloneMode_(true)
{
    embreeScene_ = new EmbreeScene(context_);
}

SceneBaker::~SceneBaker()
{

}

bool SceneBaker::SaveLitScene()
{
    assert(0);
    return false;

//     if (!standaloneMode_)
//     {
//         URHO3D_LOGERROR("SceneBaker::SaveLitScene() - only supported in standalone mode");
//         return false;
//     }
//
//     ea::string sceneFilename = AddTrailingSlash(projectPath_) + "Resources/" + scene_->GetFileName();
//
//     File saveFile(context_, sceneFilename, FILE_WRITE);
//     return scene_->SaveXML(saveFile);
}

bool SceneBaker::WriteBakeData(VectorBuffer& buffer)
{
    buffer.Clear();

    // protocol is very simple right now, can easily be expanded

    buffer.WriteUInt(bakeMeshes_.size());

    for (unsigned i = 0; i < bakeMeshes_.size(); i++)
    {
        BakeMesh* bakeMesh = bakeMeshes_[i];
        Node* node = bakeMesh->GetNode();
        StaticModel* staticModel = bakeMesh->GetStaticModel();

        buffer.WriteUInt(node->GetID());
        buffer.WriteUInt(staticModel->GetID());
        buffer.WriteUInt(staticModel->GetLightMask());
        buffer.WriteUInt(staticModel->GetLightmapIndex());
        buffer.WriteVector4(staticModel->GetLightmapTilingOffset());
    }

    return true;
}

bool SceneBaker::GenerateLightmaps(ea::vector<ea::string>& lightmapNames)
{
    URHO3D_LOGINFO("Generating Lightmaps");

    for (unsigned i = 0; i < bakeMeshes_.size(); i++)
    {
        BakeMesh* mesh = bakeMeshes_[i];
        mesh->GenerateRadianceMap();
    }

    SharedPtr<LightMapPacker> packer(new LightMapPacker(context_));

    for (unsigned i = 0; i < bakeMeshes_.size(); i++)
    {
        BakeMesh* mesh = bakeMeshes_[i];

        SharedPtr<RadianceMap> radianceMap = mesh->GetRadianceMap();

        if (radianceMap.NotNull())
        {
            packer->AddRadianceMap(radianceMap);
        }

    }

    packer->Pack();

    if (!packer->SaveLightmaps(projectPath_, scene_->GetFileName(), lightmapNames))
    {
        return false;
    }

    if (standaloneMode_)
    {
        if (!SaveLitScene())
            return false;
    }

    return WriteBakeData(bakeData_);

}

void SceneBaker::TraceRay(LightRay* lightRay, const ea::vector<BakeLight*>& bakeLights)
{
    if (currentLightMode_ == GLOW_LIGHTMODE_DIRECT)
    {
        DirectLight(lightRay, bakeLights);
    }
    else
    {
        IndirectLight(lightRay);
    }

}

bool SceneBaker::LightDirect()
{
    // Direct Lighting
    currentLightMode_ = GLOW_LIGHTMODE_DIRECT;

    if (!bakeMeshes_.size())
    {
        URHO3D_LOGINFO("SceneBaker::LightDirect() - No bake meshes found");
        bakeLights_.clear();
        return false;
    }

    for (unsigned i = 0; i < bakeMeshes_.size(); i++)
    {
        BakeMesh* mesh = bakeMeshes_[i];
        mesh->Light(GLOW_LIGHTMODE_DIRECT);
    }

    return true;

}

void SceneBaker::LightDirectFinish()
{

}

bool SceneBaker::EmitPhotons()
{
    Photons photons(this, GlobalGlowSettings.photonPassCount_, GlobalGlowSettings.photonBounceCount_,
                          GlobalGlowSettings.photonEnergyThreshold_, GlobalGlowSettings.photonMaxDistance_);

    int numPhotons = photons.Emit(bakeLights_);

    URHO3D_LOGINFOF("SceneBaker::EmitPhotons() - %i photons emitted", numPhotons);

    if (!numPhotons)
    {
        return false;
    }


    for( unsigned i = 0; i < bakeMeshes_.size(); i++ )
    {
        if( PhotonMap* photons = bakeMeshes_[i]->GetPhotonMap() )
        {
            photons->Gather( GlobalGlowSettings.finalGatherRadius_ );
        }
    }

    return true;
}

bool SceneBaker::LightGI()
{
    // Indirect Lighting
    currentLightMode_ = GLOW_LIGHTMODE_INDIRECT;

    // We currently only need one GI pass
    if (!GlobalGlowSettings.giEnabled_ || currentGIPass_ >= 1)
    {
        return false;
    }

    URHO3D_LOGINFOF("GI Pass #%i of %i", currentGIPass_ + 1, 1);

    bool photons = EmitPhotons();

    if (!photons)
        return false;

    for (unsigned i = 0; i < bakeMeshes_.size(); i++)
    {
        BakeMesh* mesh = bakeMeshes_[i];
        mesh->Light(GLOW_LIGHTMODE_INDIRECT);
    }

    return true;

}

void SceneBaker::LightGIFinish()
{
    currentGIPass_++;
}


bool SceneBaker::Light(const GlowLightMode lightMode)
{
    if (lightMode == GLOW_LIGHTMODE_DIRECT)
    {
        if (!LightDirect())
        {
            currentLightMode_ = GLOW_LIGHTMODE_COMPLETE;
            URHO3D_LOGINFO("Cycle: Direct Lighting - no work to be done");
            return false;
        }

        URHO3D_LOGINFO("Cycle: Direct Lighting");

        return true;
    }

    if (lightMode == GLOW_LIGHTMODE_INDIRECT)
    {
        if (!LightGI())
        {
            currentLightMode_ = GLOW_LIGHTMODE_COMPLETE;

            // We currently only need one GI pass
            if (GlobalGlowSettings.giEnabled_ && currentGIPass_ == 0)
            {
                URHO3D_LOGINFO("Cycle: GI - no work to be done");
            }

            return false;
        }

        return true;
    }


    return true;
}

void SceneBaker::LightFinishCycle()
{
    if (currentLightMode_ == GLOW_LIGHTMODE_DIRECT)
    {
        LightDirectFinish();
    }

    if (currentLightMode_ == GLOW_LIGHTMODE_INDIRECT)
    {
        LightGIFinish();
    }

}

void SceneBaker::QueryLights(const BoundingBox& bbox, ea::vector<BakeLight*>& lights)
{
    lights.clear();

    for (unsigned i = 0; i < bakeLights_.size(); i++)
    {

        // TODO: filter on zone, range, groups
        lights.push_back(bakeLights_[i]);

    }

}

bool SceneBaker::Preprocess()
{
    auto itr = bakeMeshes_.begin();

    while (itr != bakeMeshes_.end())
    {
        (*itr)->Preprocess();
        itr++;
    }

    embreeScene_->Commit();

    return true;
}

bool SceneBaker::LoadScene(const XMLElement& sceneXML)
{
    scene_ = new Scene(context_);

    if (!scene_->LoadXML(sceneXML))
    {
        scene_ = 0;
        return false;
    }

    // IMPORTANT!, if scene updates are enabled
    // the Octree component will add work queue items
    // and will call WorkQueue->Complete(), see Octree::HandleRenderUpdate
    scene_->SetUpdateEnabled(false);

    // Zones
    ea::vector<Node*> zoneNodes;
    ea::vector<Zone*> zones;
    scene_->GetChildrenWithComponent<Zone>(zoneNodes, true);

    for (unsigned i = 0; i < zoneNodes.size(); i++)
    {
        Zone* zone = zoneNodes[i]->GetComponent<Zone>();

        if (!zone->GetNode()->IsEnabled() || !zone->IsEnabled())
            continue;

        zones.push_back(zone);;

        BakeLight* bakeLight = BakeLight::CreateZoneLight(this, zone->GetAmbientColor());
        bakeLights_.push_back(SharedPtr<BakeLight>(bakeLight));
    }

    // Lights
    ea::vector<Node*> lightNodes;
    scene_->GetChildrenWithComponent<Urho3D::Light>(lightNodes, true);

    for (unsigned i = 0; i < lightNodes.size(); i++)
    {
        BakeLight* bakeLight = 0;
        Node* lightNode = lightNodes[i];
        Urho3D::Light* light = lightNode->GetComponent<Urho3D::Light>();

        if (!lightNode->IsEnabled() || !light->IsEnabled())
            continue;

        if (light->GetLightType() == LIGHT_DIRECTIONAL)
        {
            bakeLight = BakeLight::CreateDirectionalLight(this, lightNode->GetDirection(), light->GetColor(), 1.0f, light->GetCastShadows());
        }
        else if (light->GetLightType() == LIGHT_POINT)
        {
            bakeLight = BakeLight::CreatePointLight(this, lightNode->GetWorldPosition(), light->GetRange(), light->GetColor(), 1.0f, light->GetCastShadows());
        }

        if (bakeLight)
        {
            bakeLights_.push_back(SharedPtr<BakeLight>(bakeLight));
        }

    }

    // Static Models
    ea::vector<StaticModel*> staticModels;
    scene_->GetComponents<StaticModel>(staticModels, true);

    for (unsigned i = 0; i < staticModels.size(); i++)
    {
        StaticModel* staticModel = staticModels[i];

        if (!staticModel->GetNode()->IsEnabled() || !staticModel->IsEnabled())
            continue;

        const BoundingBox& modelWBox = staticModel->GetWorldBoundingBox();

        sceneBounds_.Merge(modelWBox);

        Vector3 center = modelWBox.Center();
        int bestPriority = M_MIN_INT;
        Zone* newZone = 0;

        for (auto i = zones.begin(); i != zones.end(); ++i)
        {
            Zone* zone = *i;
            int priority = zone->GetPriority();
            if (priority > bestPriority && (staticModel->GetZoneMask() & zone->GetZoneMask()) && zone->IsInside(center))
            {
                newZone = zone;
                bestPriority = priority;
            }
        }

        staticModel->SetZone(newZone, false);

        if (staticModel->GetModel() && (staticModel->GetLightmap() || staticModel->GetCastShadows()))
        {
            Model* model = staticModel->GetModel();

            for (unsigned i = 0; i < model->GetNumGeometries(); i++)
            {
                Geometry* geo = model->GetGeometry(i, 0);

                if (!geo)
                {
                    URHO3D_LOGERRORF("SceneBaker::LoadScene - model without geometry: %s", model->GetName().c_str());
                    return false;
                }

                const unsigned char* indexData = 0;
                unsigned indexSize = 0;
                unsigned vertexSize = 0;
                const unsigned char* vertexData = 0;
                const ea::vector<VertexElement>* elements = 0;

                geo->GetRawData(vertexData, vertexSize, indexData, indexSize, elements);

                if (!indexData || !indexSize || !vertexData || !vertexSize || !elements)
                {
                    URHO3D_LOGERRORF("SceneBaker::LoadScene - Unable to inspect geometry elements: %s",  model->GetName().c_str());
                    return false;
                }

                int texcoords = 0;
                for (unsigned i = 0; i < elements->size(); i++)
                {
                    const VertexElement& element = elements->at(i);

                    if (element.type_ == TYPE_VECTOR2 && element.semantic_ == SEM_TEXCOORD)
                    {
                        texcoords++;
                    }
                }

                if (texcoords < 2)
                {
                    URHO3D_LOGERRORF("SceneBaker::LoadScene - Model without lightmap UV set, skipping: %s",  model->GetName().c_str());
                    continue;
                }

            }

            SharedPtr<BakeMesh> meshMap (new BakeMesh(context_, this));
            meshMap->SetStaticModel(staticModel);
            bakeMeshes_.push_back(meshMap);

            if (staticModel->GetNode()->GetName() == "Plane")
            {
                // NOTE: photo emitter should probably be using the vertex generator not staticModel world position
                BakeLight* bakeLight = BakeLight::CreateAreaLight(this, meshMap, staticModel->GetNode()->GetWorldPosition(), Color::WHITE);
                if (bakeLight)
                {
                    bakeLights_.push_back(SharedPtr<BakeLight>(bakeLight));
                }

            }
        }

    }

    return Preprocess();
}

// DIRECT LIGHT

void SceneBaker::DirectLight( LightRay* lightRay, const ea::vector<BakeLight*>& bakeLights )
{
    for (unsigned i = 0; i < bakeLights.size(); i++)
    {
        BakeLight* bakeLight = bakeLights[i];

        Color influence;

        if (bakeLight->GetVertexGenerator())
        {
            influence = DirectLightFromPointSet( lightRay, bakeLight );
        }
        else
        {
            influence = DirectLightFromPoint( lightRay, bakeLight );
        }

        if (influence.r_ || influence.g_ || influence.b_ )
        {
            lightRay->samplePoint_.bakeMesh->ContributeRadiance(lightRay, influence.ToVector3());
        }

    }
}

Color SceneBaker::DirectLightFromPoint( LightRay* lightRay, const BakeLight* light ) const
{
    float influence = DirectLightInfluenceFromPoint( lightRay, light->GetPosition(), light );

    if( influence > 0.0f )
    {
        return light->GetColor() * light->GetIntensity() * influence;
    }

    return Color::BLACK;
}

Color SceneBaker::DirectLightFromPointSet( LightRay* lightRay, const BakeLight* light ) const
{
    LightVertexGenerator* vertexGenerator = light->GetVertexGenerator();

    if (!vertexGenerator)
    {
        // this should not happen
        URHO3D_LOGERROR("SceneBaker::DirectLightFromPointSet - called without vertex generator");
        return Color::BLACK;
    }

    // No light vertices generated - just exit
    if( !vertexGenerator->GetVertexCount())
    {
        return Color::BLACK;
    }

    const LightVertexVector& vertices = vertexGenerator->GetVertices();
    Color color = Color::BLACK;

    for( unsigned i = 0, n = vertexGenerator->GetVertexCount(); i < n; i++ )
    {
        const LightVertex&  vertex = vertices[i];
        float influence = DirectLightInfluenceFromPoint( lightRay, vertex.position_ /*+ light->GetPosition()*/, light );

        // ** We have a non-zero light influence - add a light color to final result
        if( influence > 0.0f )
        {
            color += light->GetColor() * light->GetIntensity() * influence;
        }
    }

    color = color * ( 1.0f / static_cast<float>( vertexGenerator->GetVertexCount()));

    return color;
}

// ** DirectLight::influenceFromPoint
float SceneBaker::DirectLightInfluenceFromPoint(LightRay* lightRay, const Vector3 &point, const BakeLight* light ) const
{
    float inf       = 1.0f;
    float att       = 1.0f;
    float cut       = 1.0f;
    float distance  = 0.0f;

    // Calculate light influence.
    if( const LightInfluence* influence = light->GetInfluenceModel() )
    {
        inf = influence->Calculate( lightRay, point, distance );
    }

    // Calculate light cutoff.
    if( const LightCutoff* cutoff = light->GetCutoffModel() )
    {
        cut = cutoff->Calculate( lightRay->samplePoint_.position);
    }

    // Calculate light attenuation
    if( const LightAttenuation* attenuation = light->GetAttenuationModel() )
    {
        att = attenuation->Calculate( distance );
    }

    // Return final influence
    return inf * att * cut;
}

// INDIRECT LIGHT

void SceneBaker::IndirectLight( LightRay* lightRay)
{
    BakeMesh* bakeMesh = 0;
    LightRay::SamplePoint& source = lightRay->samplePoint_;

    Vector3 gathered;

    int nsamples = GlobalGlowSettings.finalGatherSamples_;
    float maxDistance = GlobalGlowSettings.finalGatherDistance_; // , settings.m_finalGatherRadius, settings.m_skyColor, settings.m_ambientColor

    int hits = 0;
    for( int k = 0; k <nsamples; k++ )
    {
        Vector3 dir;
        RandomHemisphereDirection(dir, source.normal);

        float influence = Max<float>( source.normal.DotProduct(dir), 0.0f );

        if (influence > 1.0f)
        {
            // URHO3D_LOGINFO("This shouldn't happen");
        }

        RTCRay& ray = lightRay->rtcRay_;
        lightRay->SetupRay(source.position, dir, .001f, maxDistance);

        rtcIntersect(GetEmbreeScene()->GetRTCScene(), ray);

        bool skyEnabled = true;

        if (ray.geomID == RTC_INVALID_GEOMETRY_ID)
        {
            if (skyEnabled)
            {
                Color skyColor(130.0f/255.0f, 209.0f/255.0f, 207.0f/255.0f);
                skyColor = Color::WHITE * 0.15f;
                gathered += (skyColor * influence).ToVector3();// + ambientColor;
                hits++;
            }

            continue;
        }

        bakeMesh = GetEmbreeScene()->GetBakeMesh(ray.geomID);

        if (!bakeMesh)
        {
            continue;
        }

        if (bakeMesh == source.bakeMesh && ray.primID == source.triangle)
        {
            // do not self light
            continue;
        }

        const BakeMesh::MMTriangle* tri = bakeMesh->GetTriangle(ray.primID);

        // TODO: interpolate normal, if artifacting
        if( dir.DotProduct(tri->normal_) >= 0.0f )
        {
            continue;
        }

        PhotonMap* photonMap = bakeMesh->GetPhotonMap();

        if (!photonMap)
        {
            continue;
        }

        Vector3 bary(ray.u, ray.v, 1.0f-ray.u-ray.v);
        Vector2 st;
        bakeMesh->GetST(ray.primID, 1, bary, st);

        int photons;
        Color pcolor;
        Color gcolor;

        if (!photonMap->GetPhoton( (int) ray.primID, st, pcolor, photons, gcolor, true))
        {
            continue;
        }

        hits++;
        gathered += gcolor.ToVector3() * influence;// + ambientColor;

    }

    if (!hits)
        return;

    gathered /= static_cast<float>( nsamples );

    if (gathered.x_ >= 0.01f || gathered.y_ >= 0.01f || gathered.z_ >= 0.01f )
    {
        source.bakeMesh->ContributeRadiance(lightRay, gathered, GLOW_LIGHTMODE_INDIRECT);
    }

}

}
