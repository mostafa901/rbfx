#if 0
//
// Copyright (c) 2008-2014 the Urho3D project.
// Copyright (c) 2014-2015, THUNDERBEAST GAMES LLC All rights reserved
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

#include "../../Core/CoreEvents.h"
#include "../../Core/WorkQueue.h"
#include "../../Engine/EngineDefs.h"
#include "../../Engine/Engine.h"
#include "../../IO/FileSystem.h"
#include "../../IO/Log.h"
#include "../../IO/IOEvents.h"

// #include "../../Resource/ResourceMapRouter.h"
// #include "../../IPC/IPCEvents.h"

// #include <ToolCore/ToolSystem.h>
// #include <ToolCore/ToolEnvironment.h>

#include <AtomicGlow/Common/GlowSettings.h>
#include <AtomicGlow/Common/GlowEvents.h>
#include <AtomicGlow/Kernel/BakeModel.h>
#include <AtomicGlow/Kernel/BakeMaterial.h>
#include <AtomicGlow/Kernel/SceneBaker.h>

#include "AtomicGlowApp.h"

using namespace ToolCore;

#ifdef ATOMIC_PLATFORM_OSX
#include <unistd.h>
#endif

#ifdef ATOMIC_PLATFORM_WINDOWS
#include <stdio.h>
#endif


ATOMIC_DEFINE_APPLICATION_MAIN(AtomicGlow::AtomicGlowApp);

namespace AtomicGlow
{

    AtomicGlowApp::AtomicGlowApp(Context* context) :
        IPCClientApp(context)
    {

    }

    AtomicGlowApp::~AtomicGlowApp()
    {

    }

    void AtomicGlowApp::HandleIPCCmd(StringHash eventType, VariantMap& eventData)
    {

        using namespace IPCCmd;

        IPC* ipc = GetSubsystem<IPC>();

        ea::string cmd = eventData[P_COMMAND].GetString().to_lower();
        unsigned id = eventData[P_ID].GetUInt();

        VariantMap result;
        result[P_COMMAND] = cmd;
        result[P_ID] = id;

        if (cmd == "bake")
        {
            timer_.Reset();

            ea::string sceneName = eventData["scenename"].GetString();

            // settings
            VectorBuffer settingsBuffer = eventData["settings"].GetVectorBuffer();
            GlobalGlowSettings.Unpack(settingsBuffer);

            URHO3D_LOGINFOF("AtomicGlow baking scene: %s", sceneName.c_str());

            sceneBaker_->SetStandaloneMode(false);
            sceneBaker_->LoadScene(sceneName);

            using namespace IPCCmdResult;
            result["result"] = "success";

            ipc->SendEventToBroker(E_IPCCMDRESULT, result);

        }

        if (cmd == "quit")
        {
            URHO3D_LOGINFO("AtomicGlow quit received, exiting");
            GetSubsystem<WorkQueue>()->TerminateThreads();
            exitCode_ = EXIT_SUCCESS;
            engine_->Exit();
        }

    }

    void AtomicGlowApp::HandleLogMessage(StringHash eventType, VariantMap& eventData)
    {
        using namespace LogMessage;

        if (GetBrokerActive())
        {

            if (!GetIPC())
                return;

            VariantMap logEvent;
            logEvent[IPCWorkerLog::P_LEVEL] = eventData[P_LEVEL].GetInt();
            logEvent[IPCWorkerLog::P_MESSAGE] = eventData[P_MESSAGE].GetString();
            GetIPC()->SendEventToBroker(E_IPCWORKERLOG, logEvent);
        }

    }


    void AtomicGlowApp::HandleUpdate(StringHash eventType, VariantMap& eventData)
    {
        // if no scene has been loaded, return
        if (!sceneBaker_->GetScene())
        {
            return;
        }

        if (!GetSubsystem<WorkQueue>()->IsCompleted(M_MAX_UNSIGNED))
        {
            return;
        }

        // if we're done, exit in standalone mode, or send IPC event if
        // running off Glow service
        if (sceneBaker_->GetCurrentLightMode() == GLOW_LIGHTMODE_COMPLETE)
        {
            ea::string resultString = ToString("Scene lit in %i seconds", (int) (timer_.GetMSec(false) / 1000.0f));

            URHO3D_LOGINFO(resultString);

            UnsubscribeFromEvent(E_UPDATE);

            sceneBaker_->GenerateLightmaps();

            if (sceneBaker_->GetStandaloneMode())
            {
                // TODO: write scene file/lightmaps in standalone mode

                exitCode_ = EXIT_SUCCESS;
                engine_->Exit();
                return;
            }
            else
            {
                using namespace AtomicGlowResult;
                VariantMap eventData;
                eventData[P_SUCCESS] = true;
                eventData[P_RESULT] = resultString;
                eventData[P_BAKEDATA] = sceneBaker_->GetBakeData();
                GetIPC()->SendEventToBroker(E_ATOMICGLOWRESULT, eventData);
            }

            return;
        }

        if (sceneBaker_->GetCurrentLightMode() == GLOW_LIGHTMODE_UNDEFINED)
        {

            // light mode will either move to direct or complete, depending on work to do
            sceneBaker_->Light(GLOW_LIGHTMODE_DIRECT);
            return;
        }

        if (sceneBaker_->GetCurrentLightMode() == GLOW_LIGHTMODE_DIRECT)
        {
            sceneBaker_->LightFinishCycle();

            // light mode will either move to indirect or complete, depending on work to do
            sceneBaker_->Light(GLOW_LIGHTMODE_INDIRECT);
            return;
        }

        if (sceneBaker_->GetCurrentLightMode() == GLOW_LIGHTMODE_INDIRECT)
        {
            sceneBaker_->LightFinishCycle();

            // light mode will either move to indirect or complete, depending on work to do
            sceneBaker_->Light(GLOW_LIGHTMODE_INDIRECT);
            return;
        }

    }

    void AtomicGlowApp::Start()
    {
        // IMPORTANT!!!
        // This needs to be refactored, see // ATOMIC GLOW HACK in AssetDatabase.cpp
        // SharedPtr<ResourceMapRouter> router(new ResourceMapRouter(context_, "__atomic_ResourceCacheMap.json"));
        // IMPORTANT!!!

        if (exitCode_)
            return;

        if (!engine_->Initialize(engineParameters_))
        {
            ErrorExit();
            return;
        }

        context_->RegisterSubsystem(new BakeMaterialCache(context_));
        context_->RegisterSubsystem(new BakeModelCache(context_));

        SubscribeToEvent(E_UPDATE, URHO3D_HANDLER(AtomicGlowApp, HandleUpdate));
        SubscribeToEvent(E_LOGMESSAGE, URHO3D_HANDLER(AtomicGlowApp, HandleLogMessage));
        SubscribeToEvent(E_IPCMESSAGE, URHO3D_HANDLER(AtomicGlowApp, HandleUpdate));
        SubscribeToEvent(E_IPCCMD, URHO3D_HANDLER(AtomicGlowApp, HandleIPCCmd));

        bool ipc = IPCClientApp::Initialize(arguments_);

        ToolSystem* tsystem = GetSubsystem<ToolSystem>();

        // Load project, in read only mode
        if (!tsystem->LoadProject(projectPath_, true))
        {
            ErrorExit(ToString("Unable to load project %s", projectPath_.c_str()));
            return;
        }

        sceneBaker_ = new SceneBaker(context_, projectPath_);

        if (!ipc)
        {
            if (!sceneBaker_->LoadScene(scenePath_))
            {
                ea::string message = scenePath_.length() ? "AtomicGlowApp::Start() - standalone mode unable to load scene: " + scenePath_:
                                                       "AtomicGlowApp::Start() - standalone mode scene not specified";

                ErrorExit(message);
            }
        }

    }

    void AtomicGlowApp::Setup()
    {
        // AtomicGlow is always headless
        engineParameters_["Headless"] = true;

        FileSystem* fileSystem = GetSubsystem<FileSystem>();

        ToolSystem* tsystem = new ToolSystem(context_);
        context_->RegisterSubsystem(tsystem);

        ToolEnvironment* env = new ToolEnvironment(context_);
        context_->RegisterSubsystem(env);

        // Initialize the ToolEnvironment
        if (!env->Initialize(true))
        {
            ErrorExit("Unable to initialize tool environment");
            return;
        }

        for (unsigned i = 0; i < arguments_.size(); ++i)
        {
            if (arguments_[i].length() > 1)
            {
                ea::string argument = arguments_[i].to_lower();
                ea::string value = i + 1 < arguments_.size() ? arguments_[i + 1] : ea::string::EMPTY;

                if (argument == "--project" && value.length())
                {
                    if (GetExtension(value) == ".atomic")
                    {
                        value = GetPath(value);
                    }

                    if (!fileSystem->DirExists(value))
                    {
                        ErrorExit(ToString("%s project path does not exist", value.c_str()));
                        return;
                    }

                    projectPath_ = AddTrailingSlash(value);

                }
                else if (argument == "--scene" && value.length())
                {
                    scenePath_ = value;

                }

            }
        }

        if (!projectPath_.length())
        {
            ErrorExit(ToString("%s project path not specified"));
            return;
        }

        engineParameters_.InsertNew("LogName", fileSystem->GetAppPreferencesDir("AtomicEditor", "Logs") + "AtomicGlow.log");

    #ifdef ATOMIC_DEV_BUILD
        engineParameters_["ResourcePrefixPaths"] = env->GetRootSourceDir() + "/Resources/";
        engineParameters_["ResourcePaths"] = ToString("CoreData;EditorData;%sResources;%sCache", projectPath_.c_str(), projectPath_.c_str());
    #endif

        // TODO: change to using new workerthreadcount command line arg
        // which exposed in editor GlowComponent UI
        // also, check how the number of threads affects light times
        engineParameters_[EP_WORKER_THREADS_COUNT] = 4;

        IPCClientApp::Setup();

    }


}


#endif // 0
