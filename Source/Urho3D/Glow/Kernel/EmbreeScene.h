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

#pragma once

#include "Embree.h"

#include "../../Core/Object.h"

namespace AtomicGlow
{

using namespace Urho3D;

class EmbreeScenePrivate;
class BakeMesh;

class EmbreeScene : public Object
{
    URHO3D_OBJECT(EmbreeScene, Object)

    public:

    EmbreeScene(Context* context);
    virtual ~EmbreeScene();

    void Commit();

    RTCScene GetRTCScene() const { return rtcScene_; }

    void RegisterBakeMesh(unsigned geomID, BakeMesh* mesh)
    {
        meshMapLookup_[geomID] = mesh;
    }

    BakeMesh* GetBakeMesh(unsigned geomID) const
    {
        auto iter = meshMapLookup_.find(geomID);
        return iter != meshMapLookup_.end() ? iter->second : nullptr;
    }


private:

    // embree geomID -> MeshMap
    ea::unordered_map<unsigned, WeakPtr<BakeMesh>> meshMapLookup_;

    RTCDevice rtcDevice_;
    RTCScene rtcScene_;


};


}
