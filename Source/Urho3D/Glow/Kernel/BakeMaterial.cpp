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

#include "../../IO/Log.h"
#include "../../Resource/ResourceCache.h"
#include "../../Resource/XMLFile.h"
#include "../../Graphics/Material.h"

#include "BakeMaterial.h"

namespace AtomicGlow
{


BakeMaterialCache::BakeMaterialCache(Context* context) : Object(context)
{
}

BakeMaterialCache::~BakeMaterialCache()
{

}

BakeMaterial* BakeMaterialCache::GetBakeMaterial(Material *material)
{
    if (!material)
        return 0;

    SharedPtr<BakeMaterial> bakeMaterial = bakeCache_[material->GetName()];

    //if (bakeCache_.TryGetValue(material->GetName(), bakeMaterial))
    if (bakeMaterial)
    {
        return bakeMaterial;
    }

    bakeMaterial = new BakeMaterial(context_);

    if (!bakeMaterial->LoadMaterial(material))
    {
        bakeCache_.erase(material->GetName());
        return 0;
    }

    bakeCache_[material->GetName()] = bakeMaterial;

    return bakeMaterial;

}

BakeMaterial::BakeMaterial(Context* context) : Object(context),
    occlusionMasked_(false)
{
    uoffset_ = Vector4(1.0f, 0.0f, 0.0f, 0.0f);
    voffset_ = Vector4(0.0f, 1.0f, 0.0f, 0.0f);
}

BakeMaterial::~BakeMaterial()
{

}

Variant BakeMaterial::ParseShaderParameterValue(const ea::string& value)
{
    ea::string valueTrimmed = value.trimmed();
    if (valueTrimmed.length() && IsAlpha((unsigned)valueTrimmed[0]))
        return Variant(ToBool(valueTrimmed));
    else
        return ToVectorVariant(valueTrimmed);
}

bool BakeMaterial::LoadMaterial(Material *material)
{
    material_ = material;

    URHO3D_LOGINFOF("Material: %s", material->GetName().c_str());

    ResourceCache* cache = GetSubsystem<ResourceCache>();

    XMLFile* xmlFile = cache->GetResource<XMLFile>(material->GetName());

    if (!xmlFile)
        return false;

    XMLElement rootElem = xmlFile->GetRoot();

    XMLElement techniqueElem = rootElem.GetChild("technique");

    if (techniqueElem)
    {
        ea::string name = techniqueElem.GetAttribute("name").to_lower();

        // TODO: better way of setting/detecting occlusion masked materials
        if (name.contains("diffalpha") || name.contains("difflightmapalpha"))
        {
            occlusionMasked_ = true;
        }
    }

    // techniques

    // textures
    XMLElement textureElem = rootElem.GetChild("texture");
    while (textureElem)
    {
        ea::string unit = textureElem.GetAttribute("unit").to_lower();

        if (unit == "diffuse")
        {
            ea::string name = textureElem.GetAttribute("name");

            diffuseTexture_ = cache->GetResource<Image>(name);

            if (diffuseTexture_.Null())
                return false;

            URHO3D_LOGINFOF("diffuse: %s %ux%u", name.c_str(), diffuseTexture_->GetWidth(), diffuseTexture_->GetHeight());
        }

        textureElem = textureElem.GetNext("texture");
    }

    // Shader parameters

    XMLElement parameterElem = rootElem.GetChild("parameter");
    while (parameterElem)
    {
        ea::string name = parameterElem.GetAttribute("name").to_lower();

        Variant value;

        if (!parameterElem.HasAttribute("type"))
            value = ParseShaderParameterValue(parameterElem.GetAttribute("value"));
        else
            value = Variant(parameterElem.GetAttribute("type"), parameterElem.GetAttribute("value"));

        if (name == "uoffset")
            uoffset_ = value.GetVector4();

        else if (name == "voffset")
            voffset_ = value.GetVector4();

        parameterElem = parameterElem.GetNext("parameter");
    }


    return true;
}


}
