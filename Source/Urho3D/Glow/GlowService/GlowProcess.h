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

//#include "../../IPC/IPCServer.h"

using namespace Urho3D;

namespace AtomicGlow
{

class GlowProcessResultHandler;

class GlowProcess : public Object
{
    friend class GlowProcessResultHandler;

    URHO3D_OBJECT(GlowProcess, Object)

public:

    /// Construct.
    GlowProcess(Context* context);
    /// Destruct.
    virtual ~GlowProcess();

    bool Start() { return false; }

    bool Start(const ea::string& glowBinaryPath, const StringVector& baseArgs);

    void Exit();

    bool GetExitCalled() const { return exitCalled_; }

    unsigned QueueCommand(const VariantMap& cmdMap);

private:

    void OnIPCWorkerLog(int level, const ea::string& message);
    void OnIPCWorkerExited();

    void HandleResult(unsigned cmdID, const VariantMap& cmdResult);

    void HandleAtomicGlowResult(StringHash eventType, VariantMap& eventData);

    bool exitCalled_;

    SharedPtr<GlowProcessResultHandler> resultHandler_;

};

class GlowProcessResultHandler : public Object
{
    URHO3D_OBJECT(GlowProcessResultHandler, Object)

public:
    /// Construct.
    GlowProcessResultHandler(Context* context, GlowProcess* process);
    /// Destruct.
    virtual ~GlowProcessResultHandler();

    void HandleResult(unsigned cmdID, const VariantMap& cmdResult);

private:

    WeakPtr<GlowProcess> process_;

};

}
