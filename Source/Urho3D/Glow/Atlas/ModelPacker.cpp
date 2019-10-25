
#include "../../Graphics/IndexBuffer.h"
#include "../../Graphics/VertexBuffer.h"

#include "ModelPacker.h"

namespace AtomicGlow
{

ModelPacker::ModelPacker(Context* context) : Object(context)
{

}

ModelPacker::~ModelPacker()
{

}

/// Get the total vertex and index counts of all LOD geometry
void MPLODLevel::GetTotalCounts(unsigned& totalVertices, unsigned& totalIndices) const
{
    totalVertices = 0;
    totalIndices = 0;

    for (unsigned i = 0; i < mpGeometry_.size(); i++)
    {
        MPGeometry* mpGeo = mpGeometry_[i];

        totalVertices += mpGeo->vertices_.size();
        totalIndices += mpGeo->numIndices_;
    }

}

/// Returns true if all LOD geometry contains element
bool MPLODLevel::HasElement(VertexElementType type, VertexElementSemantic semantic, unsigned char index) const
{
    if (!mpGeometry_.size())
        return false;

    for (unsigned i = 0; i < mpGeometry_.size(); i++)
    {
        MPGeometry* mpGeo = mpGeometry_[i];

        if (!VertexBuffer::HasElement(mpGeo->elements_, type, semantic, index))
            return false;

    }

    return true;

}


bool ModelPacker::Pack()
{

    if (model_.Null())
    {
        SetError("No model to pack");
        return false;
    }

    for (unsigned i = 0; i < lodLevels_.size(); i++)
    {
        MPLODLevel* lodLevel = lodLevels_[i];

        if (!lodLevel->mpGeometry_.size())
            continue;

        unsigned totalVertices = 0;
        unsigned totalIndices = 0;

        for (unsigned j = 0; j < lodLevel->mpGeometry_.size(); j++)
        {
            totalVertices += lodLevel->mpGeometry_[j]->vertices_.size();
            totalIndices += lodLevel->mpGeometry_[j]->numIndices_;
        }

        bool combineBuffers = true;

        // Check if buffers can be combined (same vertex element mask, under 65535 vertices)
        const ea::vector<VertexElement>& elements = lodLevel->mpGeometry_[0]->elements_;

        for (unsigned j = 1; j < lodLevel->mpGeometry_.size(); j++)
        {
            if (elements != lodLevel->mpGeometry_[j]->elements_)
            {
                combineBuffers = false;
                break;
            }

        }

        // Check if keeping separate buffers allows to avoid 32-bit indices
        if (combineBuffers && totalVertices > 65535)
        {
            bool allUnder65k = true;

            for (unsigned j = 0; j < lodLevel->mpGeometry_.size(); j++)
            {
                if (lodLevel->mpGeometry_[j]->vertices_.size() > 65535)
                {
                    allUnder65k = false;
                    break;
                }
            }

            if (allUnder65k == true)
                combineBuffers = false;
        }

        SharedPtr<IndexBuffer> ib;
        SharedPtr<VertexBuffer> vb;
        ea::vector<SharedPtr<VertexBuffer> > vbVector;
        ea::vector<SharedPtr<IndexBuffer> > ibVector;
        unsigned startVertexOffset = 0;
        unsigned startIndexOffset = 0;

        for (unsigned j = 0; j < lodLevel->mpGeometry_.size(); j++)
        {

            MPGeometry* mpGeo = lodLevel->mpGeometry_[j];

            bool largeIndices;

            if (combineBuffers)
                largeIndices = totalIndices > 65535;
            else
                largeIndices = mpGeo->vertices_.size() > 65535;

            // Create new buffers if necessary
            if (!combineBuffers || vbVector.empty())
            {
                vb = new VertexBuffer(context_);
                ib = new IndexBuffer(context_);

                vb->SetShadowed(true);
                ib->SetShadowed(true);

                if (combineBuffers)
                {
                    ib->SetSize(totalIndices, largeIndices);
                    vb->SetSize(totalVertices, mpGeo->elements_);
                }
                else
                {
                    ib->SetSize(mpGeo->numIndices_, largeIndices);
                    vb->SetSize(mpGeo->vertices_.size(), mpGeo->elements_);
                }

                vbVector.push_back(vb);
                ibVector.push_back(ib);
                startVertexOffset = 0;
                startIndexOffset = 0;
            }

            unsigned char* vertexData = (unsigned char*)vb->Lock(0, totalVertices);
            unsigned char* indexData = (unsigned char*)ib->Lock(0, totalIndices);

            assert(vertexData);
            assert(indexData);

            // Build the index data
            if (!largeIndices)
            {
                unsigned short* dest = (unsigned short*)indexData + startIndexOffset;

                for (unsigned k = 0; k < mpGeo->numIndices_; k++)
                {
                    *dest++ = (unsigned short) (mpGeo->indices_[k] + startVertexOffset);
                }
            }
            else
            {
                unsigned* dest = (unsigned*)indexData + startIndexOffset;

                for (unsigned k = 0; k < mpGeo->numIndices_; k++)
                {
                    *dest++ = mpGeo->indices_[k] + startVertexOffset;
                }

            }

            // Build vertex data
            float* vertexDest = (float*)((unsigned char*)vertexData + startVertexOffset * vb->GetVertexSize());

            for (unsigned k = 0; k < mpGeo->vertices_.size(); k++)
            {
                const MPVertex& vertex = mpGeo->vertices_[k];

                unsigned texCoordCount = 0;

                const ea::vector<VertexElement>& elements = vb->GetElements();

                for (unsigned x = 0; x < elements.size(); x++)
                {
                    VertexElement element = elements[x];

                    if (element.type_ == TYPE_VECTOR3 && element.semantic_ == SEM_POSITION)
                    {
                        *vertexDest++ = vertex.position_.x_;
                        *vertexDest++ = vertex.position_.y_;
                        *vertexDest++ = vertex.position_.z_;
                    }
                    if (element.type_ == TYPE_VECTOR3 && element.semantic_ == SEM_NORMAL)
                    {
                        *vertexDest++ = vertex.normal_.x_;
                        *vertexDest++ = vertex.normal_.y_;
                        *vertexDest++ = vertex.normal_.z_;
                    }
                    if ((element.type_ == TYPE_UBYTE4 || element.type_ == TYPE_UBYTE4_NORM) && element.semantic_ == SEM_COLOR)
                    {
                        *((unsigned*)vertexDest) = vertex.color_;
                        vertexDest++;
                    }
                    if (element.type_ == TYPE_VECTOR2 && element.semantic_ == SEM_TEXCOORD)
                    {
                        if (texCoordCount == 0)
                        {
                            *vertexDest++ = vertex.uv0_.x_;
                            *vertexDest++ = vertex.uv0_.y_;
                        }
                        else
                        {

                            *vertexDest++ = vertex.uv1_.x_;
                            *vertexDest++ = vertex.uv1_.y_;
                        }

                        texCoordCount++;
                    }
                    if (element.type_ == TYPE_VECTOR4 && element.semantic_ == SEM_TANGENT)
                    {
                        *vertexDest++ = vertex.tangent_.x_;
                        *vertexDest++ = vertex.tangent_.y_;
                        *vertexDest++ = vertex.tangent_.z_;
                        *vertexDest++ = vertex.tangent_.w_;
                    }

                }

            }

            // Update geometry, note that this assumes geometry center, etc are consistent with original
            Geometry* geom = mpGeo->geometry_;

            ib->Unlock();
            vb->Unlock();
            geom->SetIndexBuffer(ib);
            geom->SetVertexBuffer(0, vb);
            geom->SetDrawRange(TRIANGLE_LIST, startIndexOffset, mpGeo->numIndices_, true);

            startVertexOffset += mpGeo->vertices_.size();
            startIndexOffset += mpGeo->numIndices_;
        }

        // update model
        ea::vector<unsigned> emptyMorphRange;
        model_->SetVertexBuffers(vbVector, emptyMorphRange, emptyMorphRange);
        model_->SetIndexBuffers(ibVector);

    }

    return true;
}

bool ModelPacker::Unpack(Model* model)
{

    model_ = model;

    unsigned maxLOD = 0;

    for (unsigned i = 0; i < model_->GetNumGeometries(); i++)
    {
        unsigned numLOD = model_->GetNumGeometryLodLevels(i);

        if (numLOD > maxLOD)
            maxLOD = numLOD;
    }

    if (!maxLOD)
    {
        SetError("No LOD in model");
        return false;
    }

    for (unsigned i = 0; i < maxLOD; i++)
    {
        if (!UnpackLODLevel(i))
            return false;
    }

    return true;
}

bool ModelPacker::UnpackLODLevel(unsigned level)
{
    ea::vector<Geometry*> lodGeo;

    for (unsigned i = 0; i < model_->GetNumGeometries(); i++)
    {
        Geometry * geo = model_->GetGeometry(i, level);

        if (geo)
            lodGeo.push_back(geo);
    }

    if (!level && !lodGeo.size())
    {
        SetError("No geometry in LOD 0 for model");
        return false;
    }

    if (!lodGeo.size())
        return true;

    SharedPtr<MPLODLevel> lodLevel (new MPLODLevel());

    for (unsigned i = 0; i < lodGeo.size(); i++)
    {
        if (!UnpackGeometry(lodLevel, lodGeo[i]))
            return false;
    }

    lodLevel->level_ = level;

    lodLevels_.push_back(lodLevel);

    return true;

}

bool ModelPacker::UnpackGeometry(MPLODLevel *level, Geometry* geometry)
{
    SharedPtr<MPGeometry> mpGeo(new MPGeometry());

    mpGeo->geometry_ = geometry;

    // We only support vertex buffer operations on vertex buffer 0
    // TODO: should it be an error if > 1 vertex buffer in geo since we might be destructive to index buffers?

    const unsigned char* indexData = 0;
    unsigned indexSize = 0;

    unsigned vertexSize = 0;
    const unsigned char* vertexData = 0;

    const ea::vector<VertexElement>* elements = 0;

    geometry->GetRawData(vertexData, vertexSize, indexData, indexSize, elements);

    if (!indexData || !indexSize || !vertexData || !vertexSize || !elements)
    {
        SetError("ModelPacker::UnpackGeometry - Failed to get raw data for geometry");
        return false;
    }

    // VERTEX DATA

    mpGeo->elements_ = *elements;

    const unsigned char* positionData = 0;
    const unsigned char* normalData = 0;
    const unsigned char* tangentData = 0;
    const unsigned char* colorData = 0;
    const unsigned char* uv0Data = 0;
    const unsigned char* uv1Data = 0;

    unsigned vertexStart = geometry->GetVertexStart();
    unsigned vertexCount = geometry->GetVertexCount();

    vertexData += vertexStart * vertexSize;

    for (unsigned i = 0; i < elements->size(); i++)
    {
        VertexElement element = elements->at(i);

        if (element.type_ == TYPE_VECTOR3 && element.semantic_ == SEM_POSITION)
        {
            positionData = vertexData + element.offset_;
        }
        else if (element.type_ == TYPE_VECTOR3 && element.semantic_ == SEM_NORMAL)
        {
            normalData = vertexData + element.offset_;
        }
        else if ((element.type_ == TYPE_UBYTE4 || element.type_ == TYPE_UBYTE4_NORM) && element.semantic_ == SEM_COLOR)
        {
            colorData = vertexData + element.offset_;
        }
        else if (element.type_ == TYPE_VECTOR4 && element.semantic_ == SEM_TANGENT)
        {
            tangentData = vertexData + element.offset_;
        }
        else if (element.type_ == TYPE_VECTOR2 && element.semantic_ == SEM_TEXCOORD)
        {
            if (!uv0Data)
            {
                uv0Data = vertexData + element.offset_;
            }
            else
            {
                uv1Data = vertexData + element.offset_;
            }
        }
    }

    if (!positionData)
    {
        SetError("Geometry has no position data");
        return false;
    }

    mpGeo->vertices_.resize(vertexCount);

    MPVertex* v = &mpGeo->vertices_[0];

    v->clear();

    for (unsigned i = 0; i < vertexCount; i++, v++)
    {
        float* fp = (float *) positionData;

        v->position_.x_ = fp[0];
        v->position_.y_ = fp[1];
        v->position_.z_ = fp[2];

        positionData += vertexSize;

        if (normalData)
        {
            fp = (float *) normalData;

            v->normal_.x_ = fp[0];
            v->normal_.y_ = fp[1];
            v->normal_.z_ = fp[2];

            normalData += vertexSize;
        }

        if (tangentData)
        {
            fp = (float *) tangentData;

            v->tangent_.x_ = fp[0];
            v->tangent_.y_ = fp[1];
            v->tangent_.z_ = fp[2];
            v->tangent_.w_ = fp[3];

            tangentData += vertexSize;
        }

        if (uv0Data)
        {
            fp = (float *) uv0Data;

            v->uv0_.x_ = fp[0];
            v->uv0_.y_ = fp[1];

            uv0Data += vertexSize;
        }

        if (uv1Data)
        {
            fp = (float *) uv1Data;

            v->uv1_.x_ = fp[0];
            v->uv1_.y_ = fp[1];

            uv1Data += vertexSize;
        }

        if (colorData)
        {
            v->color_ = *((unsigned *)colorData);
            colorData += vertexSize;
        }

    }

    // INDICES

    unsigned indexStart = geometry->GetIndexStart();
    unsigned indexCount = geometry->GetIndexCount();

    // source indices converted to unsigned value
    ea::vector<unsigned> geoIndices;

    if (indexSize == sizeof(unsigned short))
    {
        // 16-bit indices
        const unsigned short* indices = ((const unsigned short*)indexData) + indexStart;
        const unsigned short* indicesEnd = indices + indexCount;

        while (indices < indicesEnd)
        {
            unsigned idx = (unsigned) *indices++;

            if (idx >= vertexStart && idx < (vertexStart + vertexCount))
            {
                geoIndices.push_back(idx - vertexStart);
            }
        }
    }
    else
    {
        // 32-bit indices
        const unsigned* indices = ((const unsigned*)indexData) + indexStart;
        const unsigned* indicesEnd = indices + indexCount;

        while (indices < indicesEnd)
        {
            unsigned idx = *indices++;

            if (idx >= vertexStart && idx < (vertexStart + vertexCount))
            {
                geoIndices.push_back(idx - vertexStart);
            }
        }

    }

    mpGeo->indices_.resize(geoIndices.size());
    mpGeo->numIndices_ = geoIndices.size();
    memcpy(&mpGeo->indices_[0], &geoIndices[0], sizeof(unsigned) * geoIndices.size());

    level->mpGeometry_.push_back(mpGeo);

    return true;
}

}
