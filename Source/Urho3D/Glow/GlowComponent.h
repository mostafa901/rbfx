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

#pragma once


#include "Common/GlowSettings.h"
#include "GlowService/GlowServiceEvents.h"

#include "../Scene/Component.h"

using namespace AtomicGlow;

namespace Urho3D
{

// These glow events are here pretty much for editor access
// due to Glow not currently having a scripting module
URHO3D_EVENT(E_ATOMICGLOWBAKECANCEL, AtomicGlowBakeCancel)
{
}

URHO3D_EVENT(E_ATOMICGLOWBAKERESULT, AtomicGlowBakeResult)
{
    URHO3D_PARAM(P_SUCCESS, Success);    // bool
    URHO3D_PARAM(P_RESULT, Result);    // ea::string
}

URHO3D_EVENT(E_ATOMICGLOWLOGEVENT, AtomicGlowLogEvent)
{
    URHO3D_PARAM(P_LEVEL, Level);    // bool
    URHO3D_PARAM(P_MESSAGE, Message);    // ea::string
}

class GlowComponent : public Component
{
    URHO3D_OBJECT(GlowComponent, Component)

public:
    /// Construct.
    GlowComponent(Context* context);

    /// Destruct.
    virtual ~GlowComponent();

    bool Bake();

    /// Register object factory.
    static void RegisterObject(Context* context);

protected:

private:

    void HandleAtomicGlowBakeCancel(StringHash eventType, VariantMap& eventData);

    void HandleAtomicGlowServiceBakeResult(StringHash eventType, VariantMap& eventData);
    void HandleAtomicGlowServiceLogEvent(StringHash eventType, VariantMap& eventData);

    void SetFromGlowSettings(const GlowSettings& settings);

    void CopyToGlowSettings(GlowSettings& settings) const;

    // global scalar
    float lexelDensity_;

    // global illumination
    bool giEnabled_;
    int giGranularity_;
    int giMaxBounces_;

    // ambient occlusion
    bool aoEnabled_;
    float aoDepth_;
    unsigned nsamples_;
    float aoMin_;
    float aoMultiply_;

    // TODO Remove this hack
    static bool stub_;
};


}
