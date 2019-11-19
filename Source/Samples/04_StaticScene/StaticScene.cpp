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

#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Engine/Engine.h>
#include <Urho3D/Glow/LightmapUVGenerator.h>
#include <Urho3D/Glow/LightmapBaker.h>
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

SharedPtr<Image> ConvertToImage(Context* context, unsigned width, unsigned height, const ea::vector<Color>& data)
{
    auto image = MakeShared<Image>(context);
    image->SetSize(width, height, 4);
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            Color color = data[y * width + x];
            color.r_ = Pow(color.r_, 1 / 2.2f);
            color.g_ = Pow(color.g_, 1 / 2.2f);
            color.b_ = Pow(color.b_, 1 / 2.2f);
            color.a_ = 1.0f;
            image->SetPixel(x, y, color);
        }
    }
    return image;
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
    scene_->LoadFile("Scenes/LightmapperScene1.xml");

    Node* staticNodesContainer = scene_->GetChild("StaticObjects");
    ea::vector<Node*> staticNodes;
    ea::vector<Node*> staticLights;
    for (Node* child : staticNodesContainer->GetChildren())
    {
        if (child->HasComponent<Light>())
        {
            //child->SetDirection(Vector3::DOWN);
            staticLights.push_back(child);
            child->SetEnabled(false);
        }
        else if (child->HasComponent<StaticModel>())
        {
            staticNodes.push_back(child);
        }
    }

    LightmapBakingSettings lightmapSettings;
    lightmapSettings.texelDensity_ = 32;
    LightmapBaker baker(context_);
    baker.Initialize(lightmapSettings, scene_, staticNodes, staticNodes, staticLights);
    baker.CookRaytracingScene();
    baker.BuildPhotonMap();

    LightmapBakedData LightmapBakedData;
    ea::vector<ea::string> lightmapNames;
    for (unsigned i = 0; i < baker.GetNumLightmaps(); ++i)
    {
        baker.RenderLightmapGBuffer(i);
        baker.BakeLightmap(LightmapBakedData);

        const ea::string lightmapName = "Textures/Lightmap-" + ea::to_string(i) + ".png";
        lightmapNames.push_back(lightmapName);

        auto image = ConvertToImage(context_, LightmapBakedData.lightmapSize_.x_, LightmapBakedData.lightmapSize_.y_,
            LightmapBakedData.backedLighting_);
        image->SetName(lightmapName);
        image->SaveFile(cache->GetResourceDir(1) + image->GetName());
    }

    const unsigned baseLightmapIndex = scene_->GetNumLightmaps();
    for (const ea::string& lightmapName : lightmapNames)
        scene_->AddLightmap(lightmapName);
    baker.ApplyLightmapsToScene(baseLightmapIndex);

    // Create a scene node for the camera, which we will move around
    // The camera will use default settings (1000 far clip distance, 45 degrees FOV, set aspect ratio automatically)
    cameraNode_ = scene_->CreateChild("Camera");
    cameraNode_->CreateComponent<Camera>();

    // Set an initial position for the camera scene node above the plane
    cameraNode_->SetPosition(Vector3(0.0f, 2.0f, -3.0f));
    cameraNode_->SetDirection(Vector3(0.0f, 0.0f, 5.0f));
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
    const float MOVE_SPEED = 7.0f;
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
