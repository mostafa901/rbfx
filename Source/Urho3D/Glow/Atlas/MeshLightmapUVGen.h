
#pragma once

#include "ModelPacker.h"

#include <EASTL/vector.h>

using namespace Urho3D;

namespace Thekla
{

struct Atlas_Output_Mesh;
struct Atlas_Input_Mesh;

}

namespace AtomicGlow
{

class ModelPacker;

class MeshLightmapUVGen : public Object
{
    URHO3D_OBJECT(MeshLightmapUVGen, Object)

public:

    struct Settings
    {
        bool genChartID_;

        Settings()
        {
            genChartID_ = false;
        }

    };

    MeshLightmapUVGen(Context* context, Model* model, const Settings& settings);
    virtual ~MeshLightmapUVGen();

    bool Generate();

private:

    inline void EmitVertex(ea::vector<MPVertex>& vertices, unsigned& index, const MPVertex& vertex);

    void WriteLightmapUVCoords();

    struct LMVertex
    {
        MPGeometry* geometry_;
        unsigned geometryIdx_;
        unsigned originalVertex_;
    };

    SharedPtr<Model> model_;
    SharedPtr<ModelPacker> modelPacker_;

    SharedPtr<MPLODLevel> curLOD_;

    ea::vector<LMVertex> lmVertices_;

    Settings settings_;

    Thekla::Atlas_Output_Mesh* tOutputMesh_;
    Thekla::Atlas_Input_Mesh* tInputMesh_;

};

}
