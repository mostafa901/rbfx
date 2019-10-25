
#include <thekla/thekla_atlas.h>

#include "../../IO/Log.h"

#include "ModelPacker.h"
#include "MeshLightmapUVGen.h"


namespace AtomicGlow
{


MeshLightmapUVGen::MeshLightmapUVGen(Context* context, Model* model, const Settings& settings) : Object(context),
    model_(model),
    modelPacker_(new ModelPacker(context)),
    settings_(settings),
    tOutputMesh_(0),
    tInputMesh_(0)
{


}

MeshLightmapUVGen::~MeshLightmapUVGen()
{

}

inline void MeshLightmapUVGen::EmitVertex(ea::vector<MPVertex>& vertices, unsigned& index, const MPVertex& vertex)
{
    index = 0;

    for (unsigned i = 0; i < vertices.size(); i++)
    {
        if (vertices[i] == vertex)
        {
            index = i;
            return;
        }
    }

    index = vertices.size();
    vertices.push_back(vertex);
}

void MeshLightmapUVGen::WriteLightmapUVCoords()
{

    //Thekla::atlas_write_debug_textures(tOutputMesh_, tInputMesh_, ToString("/Users/jenge/Desktop/%s_lmWorldSpaceTexture.png", modelName.c_str()).c_str() ,
    //                                                              ToString("/Users/jenge/Desktop/%s_lmNormalTexture.png", modelName.c_str()).c_str() );

    ea::vector<ea::vector<MPVertex>> geoVerts;
    ea::vector<ea::vector<unsigned>> geoIndices;

    geoVerts.resize(curLOD_->mpGeometry_.size());
    geoIndices.resize(curLOD_->mpGeometry_.size());

    float uscale = 1.f / tOutputMesh_->atlas_width;
    float vscale = 1.f / tOutputMesh_->atlas_height;

    for (unsigned i = 0; i < tOutputMesh_->index_count/3; i++)
    {
        unsigned v0 = (unsigned) tOutputMesh_->index_array[i * 3];
        unsigned v1 = (unsigned) tOutputMesh_->index_array[i * 3 + 1];
        unsigned v2 = (unsigned) tOutputMesh_->index_array[i * 3 + 2];

        Thekla::Atlas_Output_Vertex& tv0 = tOutputMesh_->vertex_array[v0];
        Thekla::Atlas_Output_Vertex& tv1 = tOutputMesh_->vertex_array[v1];
        Thekla::Atlas_Output_Vertex& tv2 = tOutputMesh_->vertex_array[v2];

        LMVertex& lv0 = lmVertices_[tv0.xref];
        LMVertex& lv1 = lmVertices_[tv1.xref];
        LMVertex& lv2 = lmVertices_[tv2.xref];

        unsigned geometryIdx = lv0.geometryIdx_;

        // check for mixed geometry in triangle
        if (geometryIdx != lv1.geometryIdx_ || geometryIdx != lv2.geometryIdx_)
        {
            assert(0);
        }

        MPGeometry* mpGeo = curLOD_->mpGeometry_[geometryIdx];

        ea::vector<MPVertex>& verts = geoVerts[geometryIdx];
        ea::vector<unsigned>& indices = geoIndices[geometryIdx];

        unsigned ovindices[3];
        Vector2 uvs[3];

        uvs[0] = Vector2(tv0.uv[0], tv0.uv[1]);
        uvs[1] = Vector2(tv1.uv[0], tv1.uv[1]);
        uvs[2] = Vector2(tv2.uv[0], tv2.uv[1]);

        ovindices[0] = lv0.originalVertex_;
        ovindices[1] = lv1.originalVertex_;
        ovindices[2] = lv2.originalVertex_;

        Vector2 center(uvs[0]);
        center += uvs[1];
        center += uvs[2];
        center /= 3.0f;

        unsigned index;
        for (unsigned j = 0; j < 3; j++)
        {
            Vector2 uv = uvs[j];

            /*
            uv -= center;
            uv *= 0.98f;
            uv += center;

            uv.x_ = Clamp<float>(uv.x_, 2, tOutputMesh_->atlas_width - 2);
            uv.y_ = Clamp<float>(uv.y_, 2, tOutputMesh_->atlas_height - 2);
            */

            uv.x_ *= uscale;
            uv.y_ *= vscale;

            MPVertex mpv = mpGeo->vertices_[ovindices[j]];
            mpv.uv1_ = uv;
            EmitVertex(verts, index, mpv);
            indices.push_back(index);
        }
    }

    for (unsigned i = 0; i < curLOD_->mpGeometry_.size(); i++)
    {
        MPGeometry* mpGeo = curLOD_->mpGeometry_[i];

        mpGeo->vertices_ = geoVerts[i];
        mpGeo->indices_.resize(geoIndices[i].size());
        memcpy(&mpGeo->indices_[0], &geoIndices[i][0], sizeof(unsigned) * geoIndices[i].size());
        mpGeo->numIndices_ = geoIndices[i].size();

        // Check whether we need to add UV1 semantic

        ea::vector<VertexElement> nElements;

        unsigned texCoordCount = 0;
        for (unsigned j = 0; j < mpGeo->elements_.size(); j++)
        {
            VertexElement element = mpGeo->elements_[j];

            if (element.type_ == TYPE_VECTOR2 && element.semantic_ == SEM_TEXCOORD)
                texCoordCount++;
        }

        if (texCoordCount == 0)
        {
            // We don't have a valid UV set in UV0
            // FIX ME: This doesn't currently work
            mpGeo->elements_.push_back(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD, 0));
            mpGeo->elements_.push_back(VertexElement(TYPE_VECTOR2, SEM_TEXCOORD, 1));
        }
        else if (texCoordCount == 1)
        {
            bool added = false;
            for (unsigned j = 0; j < mpGeo->elements_.size(); j++)
            {
                VertexElement element = mpGeo->elements_[j];

                nElements.push_back(element);

                if ( (element.type_ == TYPE_VECTOR2 && element.semantic_ == SEM_TEXCOORD) || (!added && j == (mpGeo->elements_.size() - 1) ) )
                {
                    added = true;
                    VertexElement element(TYPE_VECTOR2, SEM_TEXCOORD, 1);
                    nElements.push_back(element);
                }

            }

            mpGeo->elements_ = nElements;
        }

    }

}

bool MeshLightmapUVGen::Generate()
{
    if (model_.Null())
        return false;

    if (!modelPacker_->Unpack(model_))
    {
        return false;
    }

    for (unsigned i = 0; i < modelPacker_->lodLevels_.size(); i++)
    {

        curLOD_ = modelPacker_->lodLevels_[i];

        // combine all LOD vertices/indices

        unsigned totalVertices = 0;
        unsigned totalIndices = 0;
        for (unsigned j = 0; j < curLOD_->mpGeometry_.size(); j++)
        {
            MPGeometry* geo = curLOD_->mpGeometry_[j];
            totalVertices += geo->vertices_.size();
            totalIndices += geo->numIndices_;
        }

        // Setup thekla input mesh
        tInputMesh_ = new Thekla::Atlas_Input_Mesh();

        // Allocate vertex arrays
        lmVertices_.resize(totalVertices);

        tInputMesh_->vertex_count = totalVertices;
        tInputMesh_->vertex_array = new Thekla::Atlas_Input_Vertex[tInputMesh_->vertex_count];

        tInputMesh_->face_count = totalIndices / 3;
        tInputMesh_->face_array = new Thekla::Atlas_Input_Face[tInputMesh_->face_count];

        unsigned vCount = 0;
        unsigned fCount = 0;

        for (unsigned j = 0; j < curLOD_->mpGeometry_.size(); j++)
        {
            MPGeometry* geo = curLOD_->mpGeometry_[j];

            unsigned vertexStart = vCount;

            for (unsigned k = 0; k < geo->vertices_.size(); k++, vCount++)
            {
                const MPVertex& mpv = geo->vertices_[k];

                LMVertex& lmv = lmVertices_[vCount];
                Thekla::Atlas_Input_Vertex& tv = tInputMesh_->vertex_array[vCount];

                lmv.geometry_ = geo;
                lmv.geometryIdx_ = j;
                lmv.originalVertex_ = k;

                tv.position[0] = mpv.position_.x_;
                tv.position[1] = mpv.position_.y_;
                tv.position[2] = mpv.position_.z_;

                tv.normal[0] = mpv.normal_.x_;
                tv.normal[1] = mpv.normal_.y_;
                tv.normal[2] = mpv.normal_.z_;

                tv.uv[0] = mpv.uv0_.x_;
                tv.uv[1] = mpv.uv0_.y_;

                // this appears unused in thekla atlas?
                tv.first_colocal = vCount;

            }

            for (unsigned k = 0; k < geo->numIndices_/3; k++, fCount++)
            {
                Thekla::Atlas_Input_Face& tface = tInputMesh_->face_array[fCount];

                tface.vertex_index[0] = (int) (geo->indices_[k * 3] + vertexStart);
                tface.vertex_index[1] = (int) (geo->indices_[k * 3 + 1] + vertexStart);
                tface.vertex_index[2] = (int) (geo->indices_[k * 3 + 2] + vertexStart);

                if (tface.vertex_index[0] > totalVertices || tface.vertex_index[1] > totalVertices || tface.vertex_index[2] > totalVertices)
                {
                    URHO3D_LOGERROR("Vertex overflow");
                    return false;
                }

                tface.material_index = j;

            }

        }

        Thekla::Atlas_Options atlasOptions;
        atlas_set_default_options(&atlasOptions);

        // disable brute force packing quality, as it has a number of notes about performance
        // and it is turned off in Thekla example in repo as well.  I am also seeing some meshes
        // having problems packing with it and hanging on import
        atlasOptions.packer_options.witness.packing_quality = 1;

        atlasOptions.packer_options.witness.texels_per_unit = 8;
        atlasOptions.packer_options.witness.conservative = true;

        Thekla::Atlas_Error error = Thekla::Atlas_Error_Success;

        tOutputMesh_ = atlas_generate(tInputMesh_, &atlasOptions, &error);

        if (tOutputMesh_)
        {
            WriteLightmapUVCoords();
        }

        delete [] tInputMesh_->vertex_array;
        delete [] tInputMesh_->face_array;
        delete tInputMesh_;
        tInputMesh_ = 0;

        atlas_free(tOutputMesh_);
        tOutputMesh_ = 0;

    }

    // update model
    modelPacker_->Pack();

    return true;

}

}
