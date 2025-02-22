//
// Copyright (c) 2008-2019 the Urho3D project.
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

#include "../Precompiled.h"

#include "../Core/Context.h"
#include "../Core/Profiler.h"
#include "../Graphics/Batch.h"
#include "../Graphics/Camera.h"
#include "../Graphics/CustomGeometry.h"
#include "../Graphics/Geometry.h"
#include "../Graphics/Material.h"
#include "../Graphics/OcclusionBuffer.h"
#include "../Graphics/OctreeQuery.h"
#include "../Graphics/VertexBuffer.h"
#include "../IO/Log.h"
#include "../IO/MemoryBuffer.h"
#include "../Resource/ResourceCache.h"
#include "../Scene/Node.h"

#include "../DebugNew.h"

namespace Urho3D
{

extern const char* GEOMETRY_CATEGORY;

CustomGeometry::CustomGeometry(Context* context) :
    Drawable(context, DRAWABLE_GEOMETRY),
    vertexBuffer_(context->CreateObject<VertexBuffer>()),
    elementMask_(MASK_POSITION),
    geometryIndex_(0),
    materialsAttr_(Material::GetTypeStatic()),
    dynamic_(false)
{
    vertexBuffer_->SetShadowed(true);
    SetNumGeometries(1);
}

CustomGeometry::~CustomGeometry() = default;

void CustomGeometry::RegisterObject(Context* context)
{
    context->RegisterFactory<CustomGeometry>(GEOMETRY_CATEGORY);

    URHO3D_ACCESSOR_ATTRIBUTE("Is Enabled", IsEnabled, SetEnabled, bool, true, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Dynamic Vertex Buffer", bool, dynamic_, false, AM_DEFAULT);
    URHO3D_MIXED_ACCESSOR_ATTRIBUTE("Geometry Data", GetGeometryDataAttr, SetGeometryDataAttr, ea::vector<unsigned char>,
        Variant::emptyBuffer, AM_FILE | AM_NOEDIT);
    URHO3D_ACCESSOR_ATTRIBUTE("Materials", GetMaterialsAttr, SetMaterialsAttr, ResourceRefList, ResourceRefList(Material::GetTypeStatic()),
        AM_DEFAULT);
    URHO3D_ATTRIBUTE("Is Occluder", bool, occluder_, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Can Be Occluded", IsOccludee, SetOccludee, bool, true, AM_DEFAULT);
    URHO3D_ATTRIBUTE("Cast Shadows", bool, castShadows_, false, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Draw Distance", GetDrawDistance, SetDrawDistance, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("Shadow Distance", GetShadowDistance, SetShadowDistance, float, 0.0f, AM_DEFAULT);
    URHO3D_ACCESSOR_ATTRIBUTE("LOD Bias", GetLodBias, SetLodBias, float, 1.0f, AM_DEFAULT);
    URHO3D_COPY_BASE_ATTRIBUTES(Drawable);
}

void CustomGeometry::ProcessRayQuery(const RayOctreeQuery& query, ea::vector<RayQueryResult>& results)
{
    RayQueryLevel level = query.level_;

    switch (level)
    {
    case RAY_AABB:
        Drawable::ProcessRayQuery(query, results);
        break;

    case RAY_OBB:
    case RAY_TRIANGLE:
        {
            Matrix3x4 inverse(node_->GetWorldTransform().Inverse());
            Ray localRay = query.ray_.Transformed(inverse);
            float distance = localRay.HitDistance(boundingBox_);
            Vector3 normal = -query.ray_.direction_;

            if (level == RAY_TRIANGLE && distance < query.maxDistance_)
            {
                distance = M_INFINITY;

                for (unsigned i = 0; i < batches_.size(); ++i)
                {
                    Geometry* geometry = batches_[i].geometry_;
                    if (geometry)
                    {
                        Vector3 geometryNormal;
                        float geometryDistance = geometry->GetHitDistance(localRay, &geometryNormal);
                        if (geometryDistance < query.maxDistance_ && geometryDistance < distance)
                        {
                            distance = geometryDistance;
                            normal = (node_->GetWorldTransform() * Vector4(geometryNormal, 0.0f)).Normalized();
                        }
                    }
                }
            }

            if (distance < query.maxDistance_)
            {
                RayQueryResult result;
                result.position_ = query.ray_.origin_ + distance * query.ray_.direction_;
                result.normal_ = normal;
                result.distance_ = distance;
                result.drawable_ = this;
                result.node_ = node_;
                result.subObject_ = M_MAX_UNSIGNED;
                results.push_back(result);
            }
        }
        break;

    case RAY_TRIANGLE_UV:
        URHO3D_LOGWARNING("RAY_TRIANGLE_UV query level is not supported for CustomGeometry component");
        break;
    }
}

Geometry* CustomGeometry::GetLodGeometry(unsigned batchIndex, unsigned level)
{
    return batchIndex < geometries_.size() ? geometries_[batchIndex] : nullptr;
}

unsigned CustomGeometry::GetNumOccluderTriangles()
{
    unsigned triangles = 0;

    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, 0);
        if (!geometry)
            continue;

        // Check that the material is suitable for occlusion (default material always is)
        Material* mat = batches_[i].material_;
        if (mat && !mat->GetOcclusion())
            continue;

        triangles += geometry->GetVertexCount() / 3;
    }

    return triangles;
}

bool CustomGeometry::DrawOcclusion(OcclusionBuffer* buffer)
{
    bool success = true;

    for (unsigned i = 0; i < batches_.size(); ++i)
    {
        Geometry* geometry = GetLodGeometry(i, 0);
        if (!geometry)
            continue;

        // Check that the material is suitable for occlusion (default material always is) and set culling mode
        Material* material = batches_[i].material_;
        if (material)
        {
            if (!material->GetOcclusion())
                continue;
            buffer->SetCullMode(material->GetCullMode());
        }
        else
            buffer->SetCullMode(CULL_CCW);

        const unsigned char* vertexData;
        unsigned vertexSize;
        const unsigned char* indexData;
        unsigned indexSize;
        const ea::vector<VertexElement>* elements;

        geometry->GetRawData(vertexData, vertexSize, indexData, indexSize, elements);
        // Check for valid geometry data
        if (!vertexData || !elements || VertexBuffer::GetElementOffset(*elements, TYPE_VECTOR3, SEM_POSITION) != 0)
            continue;

        // Draw and check for running out of triangles
        success = buffer->AddTriangles(node_->GetWorldTransform(), vertexData, vertexSize, geometry->GetVertexStart(),
            geometry->GetVertexCount());

        if (!success)
            break;
    }

    return success;
}

ea::vector<Vector3> CustomGeometry::GetCircleShape(float radius, size_t iterations, float startTheta, float endTheta)
{
    float stepSize = (endTheta - startTheta) / (float)iterations;
    ea::vector<Vector3> shapeList;
    for (int i = 0; i < iterations; i++) {
        float curTheta1 = startTheta + ((float)i * stepSize);
        float curTheta2 = startTheta + ((float)(i+1) * stepSize);
        float curX1 = radius * cos(curTheta1);
        float curY1 = radius * sin(curTheta1);
        float curX2 = radius * cos(curTheta2);
        float curY2 = radius * sin(curTheta2);

        shapeList.push_back(Urho3D::Vector3(curX1, 0, curY1));
        shapeList.push_back(Urho3D::Vector3(curX2, 0, curY2));

        if (i >= iterations - 1) {
            float curTheta =0;
            if (Abs(endTheta - startTheta) < (2*M_PI)) {
                curTheta = endTheta;
            }
            float curX = radius * cos(curTheta);
            float curY = radius * sin(curTheta);
            shapeList.push_back(Urho3D::Vector3(curX, 0, curY));
        }
    }
    return shapeList;
}

ea::vector<Vector3> CustomGeometry::GetSquareShape(float size)
{
    ea::vector<Vector3> mSquareList = { Urho3D::Vector3(-(size / 2.0f),0,(size / 2.0f)),Urho3D::Vector3(-(size / 2.0f),0,-(size / 2.0f)),
        Urho3D::Vector3((size / 2.0f), 0, -(size / 2.0f)),Urho3D::Vector3((size / 2.0f), 0, (size / 2.0f)) };
    return mSquareList;
}

void CustomGeometry::MakeCircle(float radius, size_t iterations, float startTheta,
    float endTheta, bool clear, int geomNum)
{
    ea::vector<Vector3> mCircleShape = GetCircleShape(radius, iterations, startTheta, endTheta);
    FillShape(mCircleShape,false,clear,geomNum);
}

void CustomGeometry::MakeCircleGraph(const ea::vector<ea::pair<float, Urho3D::SharedPtr<Urho3D::Material> > >& parts,
    int radius, int iterations)
{
    if (parts.size() > 0) {
        float totalWeight = 0;
        SetNumGeometries(parts.size());
        auto it = parts.begin();
        while (it != parts.end()) {
            const auto current = (*it);
            totalWeight += current.first;
            it++;
        }

        it = parts.begin();
        float currentStartTheta = 0;
        float currentEndTheta = 0;
        int count = 0;
        while (it != parts.end()) {
            const auto current = (*it);
            currentEndTheta = ((current.first / totalWeight)*(2 * M_PI)) + currentStartTheta;
            MakeCircle(radius, (iterations / parts.size()), currentStartTheta, currentEndTheta,false,count);
            if (current.second.NotNull()) {
                SetMaterial(count, current.second);
            }
            it++;
            count++;
            currentStartTheta = currentEndTheta;
        }
    }
}

void CustomGeometry::MakeShape(const ea::vector<Vector3>& pointList , bool connectTail)
{
    Clear();
    SetNumGeometries(1);
    BeginGeometry(0, Urho3D::PrimitiveType::LINE_STRIP);
    Vector3 current;
    Vector3 next;
    for (size_t i = 0; i < pointList.size(); i++) {
        if ((connectTail && i >= pointList.size() - 1) || i < pointList.size() - 1) {
            current = pointList[i];
            next = pointList[0];
            if (i < pointList.size() - 1) {
                next = pointList[i + 1];
            }
            DefineVertex(current);
            DefineVertex(next);
        }

    }
    Commit();
}

void CustomGeometry::FillShape(const ea::vector<Vector3>& shapeList, bool connectTail, bool clear, int geomNum)
{
    if (shapeList.size() > 0) {
        int usedGeomNum = geomNum;
        if (clear) {
            Clear();
            SetNumGeometries(1);
            usedGeomNum = 0;
        }
        BeginGeometry(usedGeomNum, PrimitiveType::TRIANGLE_STRIP);
        auto centerPoint = Vector3(0, 0, 0);
        if (connectTail) {
            auto centerPoint = Average(shapeList.begin(), shapeList.end());
        }
        ea::vector<Vector3> vertices(3);
        Vector3 current;
        Vector3 next;
        Vector3 normal;
        auto it = shapeList.begin();
        auto nextIt = it;
        while (it != shapeList.end()) {
            nextIt = it + 1;
            if (connectTail && nextIt == shapeList.end() || nextIt != shapeList.end())
            {
                current = (*it);

                if (nextIt != shapeList.end()) {
                    next = (*nextIt);
                }
                else {
                    next = (*shapeList.begin());
                }

                vertices = { centerPoint, current, next };

                normal = Average(vertices.begin(), vertices.end());
                normal.Normalize();
                normal.Orthogonalize(normal);
                DefineVertex(vertices.at(0));
                DefineNormal(normal);

                DefineVertex(vertices.at(1));
                DefineNormal(normal);

                DefineVertex(vertices.at(2));
                DefineNormal(normal);
            }
            it++;
        }
        Commit();
    }
}

void CustomGeometry::MakeSphere(float radius, size_t iterations)
{
    //Create the geometry buffer
    float angleStepSize = (2.0f * M_PI) / (float)iterations;
    ea::vector<Vector3> m_xyPoints;
    for (int i = 0; i < iterations; i++) {
        float curTheta = i * angleStepSize;
        for (int j = 0; j < iterations; j++) {
            float curPhi = j * angleStepSize;
            float curX = radius * cos(curTheta) * sin(curPhi);
            float curY = radius * sin(curTheta) * sin(curPhi);
            float curZ = radius * cos(curPhi);
            m_xyPoints.push_back(Vector3(curX,curY,curZ));
        }
    }

    CreateQuadsFromBuffer(m_xyPoints, iterations, iterations, true);
}

void CustomGeometry::ProtrudeShape(const ea::vector<Vector3>& mShapeList,
    const ea::vector<Vector3>& mPointList, bool connectTail)
{
    Vector3 centerPoint = Average(mShapeList.begin(), mShapeList.end());
    Vector3 pointCurrent;
    Vector3 shapeCurrent;
    Vector3 shapePointVec;
    Vector3 shapePointDir;
    ea::vector<Vector3> mPointBuffer(mShapeList.size() * mPointList.size() + mShapeList.size());

    ea::vector<Vector3> mLastShapePos = mShapeList;
    auto pointIter = mPointList.begin();
    auto shapeIter = mLastShapePos.begin();

    int bufferCount = 0;
    while (shapeIter != mLastShapePos.end()) {
        mPointBuffer.at(bufferCount) = (*shapeIter);
        shapeIter++;
        bufferCount++;
    }


    int count = 0;
    while (pointIter != mPointList.end()) {
        shapeIter = mLastShapePos.begin();
        pointCurrent = (*pointIter);
        count = 0;
        while (shapeIter != mLastShapePos.end()) {
            shapeCurrent = (*shapeIter);
            if (shapeIter == mLastShapePos.begin()) { //protrude from first point of the shape and create dir Vector to point
                shapePointVec = pointCurrent - centerPoint;
                centerPoint = pointCurrent;
            }
            // protrude from the rest of the points on the shape to the next point given a dir and length vector
            shapePointDir = shapePointVec;
            shapePointDir.Normalize();
            mLastShapePos[count] = mLastShapePos[count] + shapePointDir * shapePointVec.Length();
            mPointBuffer.at(bufferCount) = mLastShapePos[count];

            bufferCount++;
            shapeIter++;
            count++;
        }
        pointIter++;
    }
    CreateQuadsFromBuffer(mPointBuffer, mPointList.size() + 1, mShapeList.size(), connectTail);
}

void CustomGeometry::CreateQuadsFromBuffer(const ea::vector<Vector3>& pointList, size_t zIterations,
    size_t thetaIterations, bool connectTail)
{
    if (!connectTail) {
        SetNumGeometries(3);
    }
    else {
        SetNumGeometries(1);
    }

    //Create the quads from the buffer
    BeginGeometry(0, Urho3D::PrimitiveType::TRIANGLE_STRIP);
    for (size_t i = 0; i < zIterations; i++) {
        if ((i >= zIterations - 1 && connectTail) || i < zIterations - 1) {
            for (size_t j = 0; j < thetaIterations; j++) {
                //if at the end connect to the beginning to complete pass
                size_t iplus = i + 1;
                size_t jplus = j + 1;
                if (i >= zIterations - 1) {
                    iplus = 0;
                }
                if (j >= thetaIterations - 1) {
                    jplus = 0;
                }
                ea::vector<Vector3> avList;
                avList = { pointList.at((i*thetaIterations) + j) ,pointList.at((iplus*thetaIterations)+
                    j) ,pointList.at((i*thetaIterations) + jplus) };
                Vector3 normal = Average(avList.begin(), avList.end());
                normal.Normalize();
                DefineVertex(avList.at(0));
                DefineVertex(avList.at(1));
                DefineVertex(avList.at(2));
                DefineNormal(normal);
                avList.clear();

                avList = { pointList.at((i*thetaIterations) + j) ,pointList.at((iplus*thetaIterations)+
                    j) ,pointList.at((iplus*thetaIterations) + jplus) };
                normal = Average(avList.begin(), avList.end());
                normal.Normalize();
                DefineVertex(avList.at(0));
                DefineVertex(avList.at(1));
                DefineVertex(avList.at(2));
                DefineNormal(normal);
                avList.clear();
            }
        }
    }
    Commit();

    if (!connectTail) {
        //fill in the head and tail
        auto tailBegin = pointList.begin();
        auto tailEnd = pointList.begin() + thetaIterations;
        ea::vector<Vector3> tail(tailBegin, tailEnd);
        auto headBegin = pointList.begin() + pointList.size() - thetaIterations;
        auto headEnd = pointList.begin() + pointList.size();
        ea::vector<Vector3> head(headBegin, headEnd);

        FillShape(tail, true, false, 1);
        FillShape(head, true, false, 2);
    }
}

void CustomGeometry::MakeSquare(float size)
{
    ea::vector<Vector3> mSquareList = { Urho3D::Vector3(-(size / 2.0f),0,(size / 2.0f)),Urho3D::Vector3(-(size / 2.0f),0,-(size / 2.0f)),
        Urho3D::Vector3((size / 2.0f), 0, -(size / 2.0f)),Urho3D::Vector3((size / 2.0f), 0, (size / 2.0f)) };
    FillShape(mSquareList,true);
}

void CustomGeometry::Clear()
{
    elementMask_ = MASK_POSITION;
    batches_.clear();
    geometries_.clear();
    primitiveTypes_.clear();
    vertices_.clear();
}

void CustomGeometry::SetNumGeometries(unsigned num)
{
    batches_.resize(num);
    geometries_.resize(num);
    primitiveTypes_.resize(num);
    vertices_.resize(num);

    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        if (!geometries_[i])
            geometries_[i] = context_->CreateObject<Geometry>();

        batches_[i].geometry_ = geometries_[i];
    }
}

void CustomGeometry::SetDynamic(bool enable)
{
    dynamic_ = enable;

    MarkNetworkUpdate();
}

void CustomGeometry::BeginGeometry(unsigned index, PrimitiveType type)
{
    if (index > geometries_.size())
    {
        URHO3D_LOGERROR("Geometry index out of bounds");
        return;
    }

    geometryIndex_ = index;
    primitiveTypes_[index] = type;
    vertices_[index].clear();

    // If beginning the first geometry, reset the element mask
    if (!index)
        elementMask_ = MASK_POSITION;
}

void CustomGeometry::DefineVertex(const Vector3& position)
{
    if (vertices_.size() < geometryIndex_)
        return;

    vertices_[geometryIndex_].resize(vertices_[geometryIndex_].size() + 1);
    vertices_[geometryIndex_].back().position_ = position;
}

void CustomGeometry::DefineNormal(const Vector3& normal)
{
    if (vertices_.size() < geometryIndex_ || vertices_[geometryIndex_].empty())
        return;

    vertices_[geometryIndex_].back().normal_ = normal;
    elementMask_ |= MASK_NORMAL;
}

void CustomGeometry::DefineColor(const Color& color)
{
    if (vertices_.size() < geometryIndex_ || vertices_[geometryIndex_].empty())
        return;

    vertices_[geometryIndex_].back().color_ = color.ToUInt();
    elementMask_ |= MASK_COLOR;
}

void CustomGeometry::DefineTexCoord(const Vector2& texCoord)
{
    if (vertices_.size() < geometryIndex_ || vertices_[geometryIndex_].empty())
        return;

    vertices_[geometryIndex_].back().texCoord_ = texCoord;
    elementMask_ |= MASK_TEXCOORD1;
}

void CustomGeometry::DefineTangent(const Vector4& tangent)
{
    if (vertices_.size() < geometryIndex_ || vertices_[geometryIndex_].empty())
        return;

    vertices_[geometryIndex_].back().tangent_ = tangent;
    elementMask_ |= MASK_TANGENT;
}

void CustomGeometry::DefineGeometry(unsigned index, PrimitiveType type, unsigned numVertices, bool hasNormals, bool hasColors,
    bool hasTexCoords, bool hasTangents)
{
    if (index > geometries_.size())
    {
        URHO3D_LOGERROR("Geometry index out of bounds");
        return;
    }

    geometryIndex_ = index;
    primitiveTypes_[index] = type;
    vertices_[index].resize(numVertices);

    // If defining the first geometry, reset the element mask
    if (!index)
        elementMask_ = MASK_POSITION;
    if (hasNormals)
        elementMask_ |= MASK_NORMAL;
    if (hasColors)
        elementMask_ |= MASK_COLOR;
    if (hasTexCoords)
        elementMask_ |= MASK_TEXCOORD1;
    if (hasTangents)
        elementMask_ |= MASK_TANGENT;
}

void CustomGeometry::Commit()
{
    URHO3D_PROFILE("CommitCustomGeometry");

    unsigned totalVertices = 0;
    boundingBox_.Clear();

    for (unsigned i = 0; i < vertices_.size(); ++i)
    {
        totalVertices += vertices_[i].size();

        for (unsigned j = 0; j < vertices_[i].size(); ++j)
            boundingBox_.Merge(vertices_[i][j].position_);
    }

    // Make sure world-space bounding box will be updated
    OnMarkedDirty(node_);

    // Resize (recreate) the vertex buffer only if necessary
    if (vertexBuffer_->GetVertexCount() != totalVertices || vertexBuffer_->GetElementMask() != elementMask_ ||
        vertexBuffer_->IsDynamic() != dynamic_)
        vertexBuffer_->SetSize(totalVertices, elementMask_, dynamic_);

    if (totalVertices)
    {
        auto* dest = (unsigned char*)vertexBuffer_->Lock(0, totalVertices, true);
        if (dest)
        {
            unsigned vertexStart = 0;

            for (unsigned i = 0; i < vertices_.size(); ++i)
            {
                unsigned vertexCount = 0;

                for (unsigned j = 0; j < vertices_[i].size(); ++j)
                {
                    *((Vector3*)dest) = vertices_[i][j].position_;
                    dest += sizeof(Vector3);

                    if (elementMask_ & MASK_NORMAL)
                    {
                        *((Vector3*)dest) = vertices_[i][j].normal_;
                        dest += sizeof(Vector3);
                    }
                    if (elementMask_ & MASK_COLOR)
                    {
                        *((unsigned*)dest) = vertices_[i][j].color_;
                        dest += sizeof(unsigned);
                    }
                    if (elementMask_ & MASK_TEXCOORD1)
                    {
                        *((Vector2*)dest) = vertices_[i][j].texCoord_;
                        dest += sizeof(Vector2);
                    }
                    if (elementMask_ & MASK_TANGENT)
                    {
                        *((Vector4*)dest) = vertices_[i][j].tangent_;
                        dest += sizeof(Vector4);
                    }

                    ++vertexCount;
                }

                geometries_[i]->SetVertexBuffer(0, vertexBuffer_);
                geometries_[i]->SetDrawRange(primitiveTypes_[i], 0, 0, vertexStart, vertexCount);
                vertexStart += vertexCount;
            }

            vertexBuffer_->Unlock();
        }
        else
            URHO3D_LOGERROR("Failed to lock custom geometry vertex buffer");
    }
    else
    {
        for (unsigned i = 0; i < geometries_.size(); ++i)
        {
            geometries_[i]->SetVertexBuffer(0, vertexBuffer_);
            geometries_[i]->SetDrawRange(primitiveTypes_[i], 0, 0, 0, 0);
        }
    }

    vertexBuffer_->ClearDataLost();
}

void CustomGeometry::SetMaterial(Material* material)
{
    for (unsigned i = 0; i < batches_.size(); ++i)
        batches_[i].material_ = material;

    MarkNetworkUpdate();
}

bool CustomGeometry::SetMaterial(unsigned index, Material* material)
{
    if (index >= batches_.size())
    {
        URHO3D_LOGERROR("Material index out of bounds");
        return false;
    }

    batches_[index].material_ = material;
    MarkNetworkUpdate();
    return true;
}

unsigned CustomGeometry::GetNumVertices(unsigned index) const
{
    return index < vertices_.size() ? vertices_[index].size() : 0;
}

Material* CustomGeometry::GetMaterial(unsigned index) const
{
    return index < batches_.size() ? batches_[index].material_ : nullptr;
}

CustomGeometryVertex* CustomGeometry::GetVertex(unsigned geometryIndex, unsigned vertexNum)
{
    return (geometryIndex < vertices_.size() && vertexNum < vertices_[geometryIndex].size()) ?
           &vertices_[geometryIndex][vertexNum] : nullptr;
}

void CustomGeometry::SetGeometryDataAttr(const ea::vector<unsigned char>& value)
{
    if (value.empty())
        return;

    MemoryBuffer buffer(value);

    SetNumGeometries(buffer.ReadVLE());
    elementMask_ = VertexMaskFlags(buffer.ReadUInt());

    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        unsigned numVertices = buffer.ReadVLE();
        vertices_[i].resize(numVertices);
        primitiveTypes_[i] = (PrimitiveType)buffer.ReadUByte();

        for (unsigned j = 0; j < numVertices; ++j)
        {
            if (elementMask_ & MASK_POSITION)
                vertices_[i][j].position_ = buffer.ReadVector3();
            if (elementMask_ & MASK_NORMAL)
                vertices_[i][j].normal_ = buffer.ReadVector3();
            if (elementMask_ & MASK_COLOR)
                vertices_[i][j].color_ = buffer.ReadUInt();
            if (elementMask_ & MASK_TEXCOORD1)
                vertices_[i][j].texCoord_ = buffer.ReadVector2();
            if (elementMask_ & MASK_TANGENT)
                vertices_[i][j].tangent_ = buffer.ReadVector4();
        }
    }

    Commit();
}

void CustomGeometry::SetMaterialsAttr(const ResourceRefList& value)
{
    auto* cache = GetSubsystem<ResourceCache>();
    for (unsigned i = 0; i < value.names_.size(); ++i)
        SetMaterial(i, cache->GetResource<Material>(value.names_[i]));
}

ea::vector<unsigned char> CustomGeometry::GetGeometryDataAttr() const
{
    VectorBuffer ret;

    ret.WriteVLE(geometries_.size());
    ret.WriteUInt(elementMask_);

    for (unsigned i = 0; i < geometries_.size(); ++i)
    {
        unsigned numVertices = vertices_[i].size();
        ret.WriteVLE(numVertices);
        ret.WriteUByte(primitiveTypes_[i]);

        for (unsigned j = 0; j < numVertices; ++j)
        {
            if (elementMask_ & MASK_POSITION)
                ret.WriteVector3(vertices_[i][j].position_);
            if (elementMask_ & MASK_NORMAL)
                ret.WriteVector3(vertices_[i][j].normal_);
            if (elementMask_ & MASK_COLOR)
                ret.WriteUInt(vertices_[i][j].color_);
            if (elementMask_ & MASK_TEXCOORD1)
                ret.WriteVector2(vertices_[i][j].texCoord_);
            if (elementMask_ & MASK_TANGENT)
                ret.WriteVector4(vertices_[i][j].tangent_);
        }
    }

    return ret.GetBuffer();
}

const ResourceRefList& CustomGeometry::GetMaterialsAttr() const
{
    materialsAttr_.names_.resize(batches_.size());
    for (unsigned i = 0; i < batches_.size(); ++i)
        materialsAttr_.names_[i] = GetResourceName(batches_[i].material_);

    return materialsAttr_;
}

void CustomGeometry::OnWorldBoundingBoxUpdate()
{
    worldBoundingBox_ = boundingBox_.Transformed(node_->GetWorldTransform());
}

}
