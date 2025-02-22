//
// Copyright (c) 2017-2019 the rbfx project.
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
#include <Urho3D/Engine/EngineDefs.h>
#include <Urho3D/Core/CoreEvents.h>
#include <Urho3D/Core/CommandLine.h>
#include <Urho3D/Core/StringUtils.h>
#include <Urho3D/Input/Input.h>
#include <Urho3D/Input/InputEvents.h>
#include <Urho3D/IO/FileSystem.h>
#include <Urho3D/Resource/ResourceCache.h>
#include <Urho3D/UI/Font.h>
#include <Urho3D/UI/Button.h>
#include <Urho3D/UI/ListView.h>
#include <Urho3D/UI/Text.h>
#include <Urho3D/UI/UI.h>
#include <Urho3D/UI/UIEvents.h>

#include "01_HelloWorld/HelloWorld.h"
#include "02_HelloGUI/HelloGUI.h"
#include "03_Sprites/Sprites.h"
#include "04_StaticScene/StaticScene.h"
#include "05_AnimatingScene/AnimatingScene.h"
#include "06_SkeletalAnimation/SkeletalAnimation.h"
#include "07_Billboards/Billboards.h"
#include "08_Decals/Decals.h"
#include "09_MultipleViewports/MultipleViewports.h"
#include "10_RenderToTexture/RenderToTexture.h"
#if URHO3D_PHYSICS
#include "11_Physics/Physics.h"
#include "12_PhysicsStressTest/PhysicsStressTest.h"
#include "13_Ragdolls/Ragdolls.h"
#endif
#include "14_SoundEffects/SoundEffects.h"
#if URHO3D_NAVIGATION
#include "15_Navigation/Navigation.h"
#endif
#if URHO3D_NETWORK
#include "16_Chat/Chat.h"
#include "17_SceneReplication/SceneReplication.h"
#endif
#if URHO3D_PHYSICS
#include "18_CharacterDemo/CharacterDemo.h"
#include "19_VehicleDemo/VehicleDemo.h"
#endif
#include "20_HugeObjectCount/HugeObjectCount.h"
#include "23_Water/Water.h"
#if URHO3D_URHO2D
#include "24_Urho2DSprite/Urho2DSprite.h"
#include "25_Urho2DParticle/Urho2DParticle.h"
#include "26_ConsoleInput/ConsoleInput.h"
#include "27_Urho2DPhysics/Urho2DPhysics.h"
#include "28_Urho2DPhysicsRope/Urho2DPhysicsRope.h"
#endif
#include "29_SoundSynthesis/SoundSynthesis.h"
#include "30_LightAnimation/LightAnimation.h"
#include "31_MaterialAnimation/MaterialAnimation.h"
#if URHO3D_URHO2D
#include "32_Urho2DConstraints/Urho2DConstraints.h"
#include "33_Urho2DSpriterAnimation/Urho2DSpriterAnimation.h"
#endif
#include "34_DynamicGeometry/DynamicGeometry.h"
#include "35_SignedDistanceFieldText/SignedDistanceFieldText.h"
#if URHO3D_URHO2D
#include "36_Urho2DTileMap/Urho2DTileMap.h"
#endif
#include "37_UIDrag/UIDrag.h"
#include "38_SceneAndUILoad/SceneAndUILoad.h"
#if URHO3D_NAVIGATION
#include "39_CrowdNavigation/CrowdNavigation.h"
#endif
#include "40_Localization/L10n.h"
#if !__EMSCRIPTEN__
#include "42_PBRMaterials/PBRMaterials.h"
#endif
#if URHO3D_NETWORK
#include "43_HttpRequestDemo/HttpRequestDemo.h"
#endif
#include "44_RibbonTrailDemo/RibbonTrailDemo.h"
#if URHO3D_PHYSICS
#if URHO3D_IK
#include "45_InverseKinematics/InverseKinematics.h"
#endif
#include "46_RaycastVehicle/RaycastVehicleDemo.h"
#endif
#include "47_Typography/Typography.h"
#include "48_Hello3DUI/Hello3DUI.h"
#if URHO3D_URHO2D
#include "49_Urho2DIsometricDemo/Urho2DIsometricDemo.h"
#include "50_Urho2DPlatformer/Urho2DPlatformer.h"
#endif
#if URHO3D_NETWORK
#include "52_NATPunchtrough/NATPunchtrough.h"
#include "53_LANDiscovery/LANDiscovery.h"
#endif
#if URHO3D_SYSTEMUI
#include "100_HelloSystemUI/HelloSystemUI.h"
#endif
#include "105_Serialization/Serialization.h"
#include "Rotator.h"

#include "SamplesManager.h"

// Expands to this example's entry-point
URHO3D_DEFINE_APPLICATION_MAIN(Urho3D::SamplesManager);

namespace Urho3D
{

SamplesManager::SamplesManager(Context* context) :
    Application(context)
{
}

void SamplesManager::Setup()
{
    // Modify engine startup parameters
    engineParameters_[EP_WINDOW_TITLE] = "rbfx samples";
    engineParameters_[EP_LOG_NAME]     = GetSubsystem<FileSystem>()->GetAppPreferencesDir("rbfx", "samples") + GetTypeName() + ".log";
    engineParameters_[EP_FULL_SCREEN]  = false;
    engineParameters_[EP_HEADLESS]     = false;
    engineParameters_[EP_SOUND]        = true;
#if MOBILE
    engineParameters_[EP_ORIENTATIONS] = "Portrait";
#else
    engineParameters_[EP_WINDOW_WIDTH] = 1440;
    engineParameters_[EP_WINDOW_HEIGHT] = 900;
#endif
    if (!engineParameters_.contains(EP_RESOURCE_PREFIX_PATHS))
        engineParameters_[EP_RESOURCE_PREFIX_PATHS] = ";..;../..";

    GetCommandLineParser().add_option("--sample", startSample_);
}

void SamplesManager::Start()
{
    Input* input = context_->GetInput();
    UI* ui = context_->GetUI();

    // Register an object factory for our custom Rotator component so that we can create them to scene nodes
    context_->RegisterFactory<Rotator>();

    input->SetMouseMode(MM_FREE);
    input->SetMouseVisible(true);

#if URHO3D_SYSTEMUI
    context_->GetEngine()->CreateDebugHud()->ToggleAll();
#endif

    SubscribeToEvent(E_RELEASED, [this](StringHash, VariantMap& args) { OnClickSample(args); });
    SubscribeToEvent(E_KEYUP, [this](StringHash, VariantMap& args) { OnKeyPress(args); });
    SubscribeToEvent(E_BEGINFRAME, [this](StringHash, VariantMap& args) { OnFrameStart(); });

    ui->GetRoot()->SetDefaultStyle(context_->GetCache()->GetResource<XMLFile>("UI/DefaultStyle.xml"));

    auto* layout = ui->GetRoot()->CreateChild<UIElement>();
    listViewHolder_ = layout;
    layout->SetLayoutMode(LM_VERTICAL);
    layout->SetAlignment(HA_CENTER, VA_CENTER);
    layout->SetSize(300, 600);
    layout->SetStyleAuto();

    auto* list = layout->CreateChild<ListView>();
    list->SetMinSize(300, 600);
    list->SetSelectOnClickEnd(true);
    list->SetHighlightMode(HM_ALWAYS);
    list->SetStyleAuto();
    list->SetName("SampleList");

    // Get logo texture
    ResourceCache* cache = GetSubsystem<ResourceCache>();
    Texture2D* logoTexture = cache->GetResource<Texture2D>("Textures/rbfx-logo.png");
    if (!logoTexture)
        return;

    logoSprite_ = ui->GetRoot()->CreateChild<Sprite>();
    logoSprite_->SetTexture(logoTexture);

    int textureWidth = logoTexture->GetWidth();
    int textureHeight = logoTexture->GetHeight();
    logoSprite_->SetScale(256.0f / textureWidth);
    logoSprite_->SetSize(textureWidth, textureHeight);
    logoSprite_->SetHotSpot(textureWidth, textureHeight);
    logoSprite_->SetAlignment(HA_RIGHT, VA_BOTTOM);
    logoSprite_->SetOpacity(0.9f);
    logoSprite_->SetPriority(-100);

    RegisterSample<HelloWorld>();
    RegisterSample<HelloGUI>();
    RegisterSample<Sprites>();
    RegisterSample<StaticScene>();
    RegisterSample<AnimatingScene>();
    RegisterSample<SkeletalAnimation>();
    RegisterSample<Billboards>();
    RegisterSample<Decals>();
    RegisterSample<MultipleViewports>();
    RegisterSample<RenderToTexture>();
#if URHO3D_PHYSICS
    RegisterSample<Physics>();
    RegisterSample<PhysicsStressTest>();
    RegisterSample<Ragdolls>();
#endif
    RegisterSample<SoundEffects>();
#if URHO3D_NAVIGATION
    RegisterSample<Navigation>();
#endif
#if URHO3D_NETWORK
    RegisterSample<Chat>();
    RegisterSample<SceneReplication>();
#endif
#if URHO3D_PHYSICS
    RegisterSample<CharacterDemo>();
    RegisterSample<VehicleDemo>();
#endif
    RegisterSample<HugeObjectCount>();
    RegisterSample<Water>();
#if URHO3D_URHO2D
    RegisterSample<Urho2DSprite>();
    RegisterSample<Urho2DParticle>();
#endif
#if URHO3D_SYSTEMUI
    RegisterSample<ConsoleInput>();
#endif
#if URHO3D_URHO2D
    RegisterSample<Urho2DPhysics>();
    RegisterSample<Urho2DPhysicsRope>();
#endif
    RegisterSample<SoundSynthesis>();
    RegisterSample<LightAnimation>();
    RegisterSample<MaterialAnimation>();
#if URHO3D_URHO2D
    RegisterSample<Urho2DConstraints>();
    RegisterSample<Urho2DSpriterAnimation>();
#endif
    RegisterSample<DynamicGeometry>();
    RegisterSample<SignedDistanceFieldText>();
#if URHO3D_URHO2D
    RegisterSample<Urho2DTileMap>();
#endif
    RegisterSample<UIDrag>();
    RegisterSample<SceneAndUILoad>();
#if URHO3D_NAVIGATION
    RegisterSample<CrowdNavigation>();
#endif
    RegisterSample<L10n>();
#if !__EMSCRIPTEN__
    RegisterSample<PBRMaterials>();
#endif
#if URHO3D_NETWORK
    RegisterSample<HttpRequestDemo>();
#endif
    RegisterSample<RibbonTrailDemo>();
#if URHO3D_PHYSICS
#if URHO3D_IK
    RegisterSample<InverseKinematics>();
#endif
    RegisterSample<RaycastVehicleDemo>();
#endif
    RegisterSample<Typography>();
    RegisterSample<Hello3DUI>();
#if URHO3D_URHO2D
    RegisterSample<Urho2DIsometricDemo>();
    RegisterSample<Urho2DPlatformer>();
#endif
#if URHO3D_NETWORK
    RegisterSample<NATPunchtrough>();
    RegisterSample<LANDiscovery>();
#endif
#if URHO3D_SYSTEMUI
    RegisterSample<HelloSystemUi>();
#endif
    RegisterSample<Serialization>();

    if (!startSample_.empty())
        StartSample(startSample_);
}

void SamplesManager::Stop()
{
    engine_->DumpResources(true);
}

void SamplesManager::OnClickSample(VariantMap& args)
{
    using namespace Released;
    StringHash sampleType = static_cast<UIElement*>(args[P_ELEMENT].GetPtr())->GetVar("SampleType").GetStringHash();
    if (!sampleType)
        return;

    StartSample(sampleType);
}

void SamplesManager::StartSample(StringHash sampleType)
{
    UI* ui = context_->GetUI();
    ui->GetRoot()->RemoveAllChildren();
    ui->SetFocusElement(nullptr);

#if MOBILE
    Graphics* graphics = context_->GetGraphics();
    graphics->SetOrientations("LandscapeLeft LandscapeRight");
    IntVector2 screenSize = graphics->GetSize();
    graphics->SetMode(Max(screenSize.x_, screenSize.y_), Min(screenSize.x_, screenSize.y_));
#endif
    runningSample_.StaticCast<Object>(context_->CreateObject(sampleType));
    if (runningSample_.NotNull())
        runningSample_->Start();
    else
        ErrorExit("Specified sample does not exist.");
}

void SamplesManager::OnKeyPress(VariantMap& args)
{
    using namespace KeyUp;

    int key = args[P_KEY].GetInt();

    // Close console (if open) or exit when ESC is pressed
    if (key == KEY_ESCAPE)
        isClosing_ = true;
}

void SamplesManager::OnFrameStart()
{
    if (isClosing_)
    {
        isClosing_ = false;
        if (runningSample_.NotNull())
        {
            Input* input = context_->GetInput();
            UI* ui = context_->GetUI();
            runningSample_->Stop();
            runningSample_ = nullptr;
            input->SetMouseMode(MM_FREE);
            input->SetMouseVisible(true);
            ui->SetCursor(nullptr);
            ui->GetRoot()->RemoveAllChildren();
            ui->GetRoot()->AddChild(listViewHolder_);
            ui->GetRoot()->AddChild(logoSprite_);
#if MOBILE
            Graphics* graphics = context_->GetGraphics();
            graphics->SetOrientations("Portrait");
            IntVector2 screenSize = graphics->GetSize();
            graphics->SetMode(Min(screenSize.x_, screenSize.y_), Max(screenSize.x_, screenSize.y_));
#endif
        }
        else
        {
#if URHO3D_SYSTEMUI
            if (auto* console = GetSubsystem<Console>())
            {
                if (console->IsVisible())
                {
                    console->SetVisible(false);
                    return;
                }
            }
#endif
            context_->GetEngine()->Exit();
        }
    }
}

template<typename T>
void SamplesManager::RegisterSample()
{
    context_->RegisterFactory<T>();

    auto* button = context_->CreateObject<Button>().Detach();
    button->SetMinHeight(30);
    button->SetStyleAuto();
    button->SetVar("SampleType", T::GetTypeStatic());

    auto* title = button->CreateChild<Text>();
    title->SetAlignment(HA_CENTER, VA_CENTER);
    title->SetText(T::GetTypeNameStatic());
    title->SetFont(context_->GetCache()->GetResource<Font>("Fonts/Anonymous Pro.ttf"), 30);
    title->SetStyleAuto();

    context_->GetUI()->GetRoot()->GetChildStaticCast<ListView>("SampleList", true)->AddItem(button);
}

}
