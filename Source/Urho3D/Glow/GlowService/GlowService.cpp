#if 0
//
// Copyright (c) 2014-2017 THUNDERBEAST GAMES LLC
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

#include "../../IO/Log.h"
#include "../../IO/FileSystem.h"
// #include "../../IPC/IPCEvents.h"
#include "../../Graphics/StaticModel.h"

#include "GlowProcess.h"
#include "GlowServiceEvents.h"
#include "GlowService.h"

namespace AtomicGlow
{

GlowService::GlowService(Context* context) : Object(context),
    bakeCanceled_(false)
{

}

GlowService::~GlowService()
{

}

void GlowService::OnGlowProcessExited()
{
    if (glowProcess_.NotNull())
    {
        if (!glowProcess_->GetExitCalled() || bakeCanceled_)
        {
            using namespace AtomicGlowServiceBakeResult;

            VariantMap eventData;
            eventData[P_RESULT] = bakeCanceled_ ? "Bake canceled" : "GlowService::OnGlowProcessExited() - Glow process exited unexpectedly";
            eventData[P_SUCCESS] = false;
            SendEvent(E_ATOMICGLOWSERVICEBAKERESULT, eventData);
        }
    }

    if (scene_)
    {
        scene_->LoadLightmaps(true);
    }

    projectPath_ = ea::string::EMPTY;
    glowProcess_ = 0;
    scene_ = 0;

}

void GlowService::OnBakeError(const ea::string& result)
{
    URHO3D_LOGERRORF("%s", result.c_str());
    glowProcess_->Exit();

    using namespace AtomicGlowServiceBakeResult;

    VariantMap eventData;
    eventData[P_RESULT] = result;
    eventData[P_SUCCESS] = false;

    SendEvent(E_ATOMICGLOWSERVICEBAKERESULT, eventData);
}

void GlowService::OnBakeSuccess()
{
    glowProcess_->Exit();

    using namespace AtomicGlowServiceBakeResult;

    VariantMap eventData;
    eventData[P_RESULT] = "success";
    eventData[P_SUCCESS] = true;

    SendEvent(E_ATOMICGLOWSERVICEBAKERESULT, eventData);

}

void GlowService::ProcessBakeData(VectorBuffer& bakeData)
{
    if (!scene_)
    {
        URHO3D_LOGERROR("GlowService::ProcessBakeData() - called with null scene");
        return;
    }

    unsigned count = bakeData.ReadUInt();

    ea::vector<Node*> children;

    scene_->GetChildrenWithComponent<StaticModel>(children, true);

    for (unsigned i = 0; i < count; i++)
    {
        Node* node = 0;
        StaticModel* staticModel = 0;

        unsigned nodeID = bakeData.ReadUInt();
        unsigned staticModelID = bakeData.ReadUInt();
        unsigned lightMask = bakeData.ReadUInt();
        unsigned lightmapIndex = bakeData.ReadUInt();
        Vector4 tilingOffset = bakeData.ReadVector4();

        for (unsigned j = 0; j < children.size(); j++)
        {
            if (children[j]->GetID() == nodeID)
            {
                node = children[j];
                staticModel = node->GetComponent<StaticModel>();

                if (!staticModel || staticModel->GetID() != staticModelID)
                {
                    URHO3D_LOGERROR("GlowService::ProcessBakeData() - mismatched node <-> static model ID");
                    return;
                }

                break;
            }

        }

        if (!node)
        {
            URHO3D_LOGERROR("GlowService::ProcessBakeData() - unable to find node ID");
            return;
        }

        staticModel->SetLightMask(lightMask);
        staticModel->SetLightmapIndex(lightmapIndex);
        staticModel->SetLightmapTilingOffset(tilingOffset);

    }

}

bool GlowService::Bake(const ea::string& projectPath, Scene* scene, const GlowSettings& settings)
{
    if (!scene)
    {
        URHO3D_LOGERROR("GlowService::Bake() - Called with null scene");
        return false;
    }

    ea::string sceneName = scene->GetFileName();

    if (!sceneName.length())
    {
        URHO3D_LOGERROR("GlowService::Bake() - Called with unnamed scene");
        return false;
    }

    if (!projectPath.length())
    {
        URHO3D_LOGERROR("GlowService::Bake() - zero length projectPath");
        return false;
    }


    if (glowProcess_.NotNull())
    {
        URHO3D_LOGERROR("GlowService::Bake() - Called with existing glow process");
        return false;
    }

    if (!glowBinaryPath_.length())
    {
        URHO3D_LOGERROR("GlowService::Bake() - Called with empty glowBinaryPath_");
        return false;
    }


    bakeCanceled_ = false;
    result_.clear();

    glowProcess_ = new GlowProcess(context_);

    projectPath_ = projectPath;

    StringVector args;
    args.push_back("--project");
    args.push_back(projectPath_);
    args += glowBaseArgs_;

    if (!glowProcess_->Start(glowBinaryPath_, args))
    {
        URHO3D_LOGERRORF("GlowService::Bake() - Glow process failed to start: %s", glowBinaryPath_.c_str());
        return false;
    }

    scene_ = scene;
    projectPath_ = projectPath;

    using namespace IPCCmd;
    VariantMap cmdMap;
    cmdMap[P_COMMAND] = "bake";
    cmdMap["scenename"] = sceneName;

    // settings
    VectorBuffer settingsBuffer;
    settings.Pack(settingsBuffer);
    cmdMap["settings"] = settingsBuffer;

    glowProcess_->QueueCommand(cmdMap);

    return true;

}

void GlowService::CancelBake()
{

    if (glowProcess_.Null())
    {
        URHO3D_LOGERROR("GlowService::CancelBake() - Called without existing glow process");
        return;
    }

    bakeCanceled_ = true;
    glowProcess_->Exit();

}

bool GlowService::LocateServiceExecutable()
{

    glowBinaryPath_ = ea::string::EMPTY;
    glowBaseArgs_.clear();

    FileSystem* fileSystem = GetSubsystem<FileSystem>();

#ifdef ATOMIC_DEV_BUILD

    ea::string rootSourceDir = ATOMIC_ROOT_SOURCE_DIR;
    rootSourceDir = AddTrailingSlash(rootSourceDir);

    glowBinaryPath_ = rootSourceDir + "Artifacts/Build/AtomicGlow/AtomicGlow";

#else

#ifdef ATOMIC_PLATFORM_OSX
    ea::string resourcesDir = GetPath(RemoveTrailingSlash(fileSystem->GetProgramDir())) + "Resources/";
#else
    ea::string resourcesDir = fileSystem->GetProgramDir() + "Resources/";
#endif

    glowBinaryPath_ = resourcesDir + "ToolData/AtomicGlow/AtomicGlow";

#endif

#ifdef ATOMIC_PLATFORM_WINDOWS

    glowBinaryPath_ += ".exe";

#endif

    if (!fileSystem->FileExists(glowBinaryPath_))
    {
        URHO3D_LOGERRORF("AtomicGlow binary not found: %s", glowBinaryPath_.c_str());

        glowBinaryPath_.clear();
        glowBaseArgs_.clear();

        return false;
    }

    return true;
}

bool GlowService::GetServiceExecutable(ea::string& execPath, ea::vector<ea::string>& baseArgs) const
{
    execPath.clear();
    baseArgs.clear();

    if (!glowBinaryPath_.length())
    {
        return false;
    }

    execPath = glowBinaryPath_;
    baseArgs = glowBaseArgs_;

    return true;
}

bool GlowService::Start()
{
    if (!LocateServiceExecutable())
    {
        URHO3D_LOGERROR("GlowService::Start() - Unable to start AtomicGlow service");
        return false;
    }

    return true;

}



}
#endif // 0
