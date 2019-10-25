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
#include "../../Core/Context.h"
// #include "../../IPC/IPCBroker.h"
// #include "../../IPC/IPCEvents.h"

#include "../Common/GlowEvents.h"

#include "GlowService.h"
#include "GlowServiceEvents.h"
#include "GlowProcess.h"

namespace AtomicGlow
{


// Atomic Glow Process
GlowProcess::GlowProcess(Context* context) : Object(context),
    exitCalled_(false),
    resultHandler_(new GlowProcessResultHandler(context, this))
{

}

GlowProcess::~GlowProcess()
{

}

void GlowProcess::OnIPCWorkerExited()
{
    GetSubsystem<GlowService>()->OnGlowProcessExited();
}

void GlowProcess::OnIPCWorkerLog(int level, const ea::string& message)
{
    using namespace AtomicGlowServiceLogEvent;

    VariantMap eventData;
    eventData[P_LEVEL] = level;
    eventData[P_MESSAGE] = message;

    SendEvent(E_ATOMICGLOWSERVICELOGEVENT, eventData);
}

bool GlowProcess::Start(const ea::string &glowBinaryPath, const StringVector &baseArgs)
{
    if (!IPCServer::StartInternal(glowBinaryPath, baseArgs))
    {
        return false;
    }

    SubscribeToEvent(GetServerBroker(), E_ATOMICGLOWRESULT, URHO3D_HANDLER(GlowProcess, HandleAtomicGlowResult));

    return true;
}

void GlowProcess::HandleAtomicGlowResult(StringHash eventType, VariantMap& eventData)
{
    GlowService* glowService = GetSubsystem<GlowService>();

    using namespace AtomicGlowResult;

    const ea::string& result = eventData[P_RESULT].GetString();

    if (!eventData[P_SUCCESS].GetBool())
    {
        glowService->OnBakeError(result);
        return;
    }

    Variant variant = eventData[P_BAKEDATA];

    /*if (!eventData.TryGetValue(P_BAKEDATA, variant) || variant.GetType() != VAR_BUFFER)
    {
        glowService->OnBakeError("GlowProcess::HandleResult() - Unable to get bake data");
        return;
    }*/

    VectorBuffer bakeData = variant.GetVectorBuffer();

    glowService->ProcessBakeData(bakeData);

    glowService->OnBakeSuccess();

}

unsigned GlowProcess::QueueCommand(const VariantMap& cmdMap)
{
    return IPCServer::QueueCommand(resultHandler_, cmdMap);
}

void GlowProcess::Exit()
{
    if (exitCalled_)
        return;

    exitCalled_ = true;

    using namespace IPCCmd;
    VariantMap cmdMap;
    cmdMap[P_COMMAND] = "quit";
    QueueCommand(cmdMap);
}

void GlowProcess::HandleResult(unsigned cmdID, const VariantMap& cmdResult)
{
    using namespace IPCCmdResult;

    GlowService* glowService = GetSubsystem<GlowService>();

    Variant variant = cmdResult[P_COMMAND];
    ea::string cmd;
    ea::string result;

    /*if (!cmdResult.TryGetValue(P_COMMAND, variant) || variant.GetType() != VAR_STRING)
    {
        URHO3D_LOGERROR("GlowProcess::HandleResult() - Unable to process result, command key missing");
        return;
    }*/

    cmd = variant.GetString();
    variant = cmdResult["result"];

    /*if (!cmdResult.TryGetValue("result", variant) || variant.GetType() != VAR_STRING)
    {
        URHO3D_LOGERROR("GlowProcess::HandleResult() - Unable to process result, command key missing");
        return;
    }*/

    result = variant.GetString();

    if (cmd == "bake")
    {
        if (result != "success")
        {
            glowService->OnBakeError(result);
            return;
        }
    }

}

// Result Handler

GlowProcessResultHandler::GlowProcessResultHandler(Context* context, GlowProcess* process) :
    Object(context),
    process_(process)
{

}

GlowProcessResultHandler::~GlowProcessResultHandler()
{

}

void GlowProcessResultHandler::HandleResult(unsigned cmdID, const VariantMap& cmdResult)
{
    if (!process_)
    {
        URHO3D_LOGWARNING("lowProcessResultHandler::HandleResult() - called without current Glow process");
        return;
    }

    process_->HandleResult(cmdID, cmdResult);

}


}

#endif // 0
