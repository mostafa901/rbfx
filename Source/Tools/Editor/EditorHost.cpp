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
#include <Urho3D/Engine/EngineEvents.h>
#if URHO3D_CSHARP
#   include <Urho3D/Script/Script.h>
#endif
#include "Editor.h"
#if URHO3D_SAMPLES && URHO3D_STATIC
#   include "../Samples/103_GamePlugin/GamePlugin.h"
#endif

namespace Urho3D
{

/// A simple editor loader.
class EditorHost : public Editor
{
    URHO3D_OBJECT(EditorHost, Editor);
public:
    /// Construct.
    explicit EditorHost(Context* context) : Editor(context) { }
    /// Extend initialization of editor application.
    void Start() override
    {
#if URHO3D_SAMPLES && URHO3D_STATIC
        // Static plugins must be initialized manually.
        SubscribeToEvent(E_REGISTERSTATICPLUGINS, [this](StringHash, VariantMap&) {
            RegisterPlugin(new GamePlugin(context_));
            UnsubscribeFromEvent(E_REGISTERSTATICPLUGINS);
        });
#endif
        BaseClassName::Start();
    }
};

}

#if URHO3D_CSHARP
URHO3D_DEFINE_APPLICATION_MAIN_CSHARP(Urho3D::EditorHost);
#else
URHO3D_DEFINE_APPLICATION_MAIN(Urho3D::EditorHost);
#endif
