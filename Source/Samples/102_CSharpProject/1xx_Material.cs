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
using System;
using System.Diagnostics;
using System.IO;
using Urho3DNet;
using ImGuiNet;
using System.Runtime.InteropServices;

namespace DemoApplication
{
    class MaterialCheck : Application
    {
        private Scene _scene;
        private Viewport _viewport;
        private Node _camera;
        private Node _cube;
        private Node _light;

        public MaterialCheck(Context context) : base(context)
        {
        }

        protected override void Dispose(bool disposing)
        {
            Renderer.SetViewport(0, null);    // Enable disposal of viewport by making it unreferenced by engine.
            _viewport.Dispose();
            _scene.Dispose();
            _camera.Dispose();
            _cube.Dispose();
            _light.Dispose();
            base.Dispose(disposing);
        }

        public override void Setup()
        {
            var currentDir = Directory.GetCurrentDirectory();
            engineParameters_[Urho3D.EpFullScreen] = false;
            engineParameters_[Urho3D.EpWindowWidth] = 800;
            engineParameters_[Urho3D.EpWindowHeight] = 600;
            engineParameters_[Urho3D.EpWindowTitle] = "Hello C#";
            engineParameters_[Urho3D.EpResourcePrefixPaths] = $"{currentDir};{currentDir}/..";
        }

        public override void Start()
        {
            Input.SetMouseVisible(true);

            // Viewport
            _scene = new Scene(Context);
            _scene.CreateComponent<Octree>();

            _camera = _scene.CreateChild("Camera");
            _viewport = new Viewport(Context);
            _viewport.Scene = _scene;
            _viewport.Camera = _camera.CreateComponent<Camera>();
            Renderer.SetViewport(0, _viewport);

            // Background
            Renderer.DefaultZone.FogColor = new Color(0.5f, 0.5f, 0.7f);

            // Scene
            _camera.Position = new Vector3(0, 2, -2);
            _camera.LookAt(Vector3.Zero);

            // Light
            _light = _scene.CreateChild("Light");
            _light.CreateComponent<Light>();
            _light.Position = new Vector3(0, 2, -1);
            _light.LookAt(Vector3.Zero);

            Cache.AddResourceDir(@"D:\Revit_API\Downloaded_Library\Source\rbfx\cmake-build\bin\Debug\Data\Textures");

            // Cube
            _cube = _scene.CreateChild("Cube");
            var model = _cube.CreateComponent<StaticModel>();
            model.SetModel(Cache.GetResource<Model>("Models/Sphere.mdl"));
            //  model.SetMaterial(0, Cache.GetResource<Material>("Materials/Stone.xml"));
            var mat = new Material(Context);
            mat.SetTechnique(0, Cache.GetResource<Technique>("Techniques/Diff.xml"));
            mat.SetTexture(TextureUnit.TuDiffuse, Cache.GetResource<Texture2D>("Textures/StoneDiffuse.dds"));
            
            mat.SetShaderParameter("MatSpecColor", new Vector4(.3f, .3f, .3f,16));
            model.SetMaterial(mat);

            var mat2 = mat.Clone("alpha");
            mat2.SetTechnique(0, Cache.GetResource<Technique>("Techniques/DiffAlpha.xml"));
            mat2.SetTexture(TextureUnit.TuDiffuse, Cache.GetResource<Texture2D>(@"D:\Revit_API\GRAPHICS\ArchLogo_White.png"));
           
            model.SetMaterial(mat2);

            // RotateObject component is implemented in Data/Scripts/RotateObject.cs
            _cube.CreateComponent("ScriptRotateObject");

            SubscribeToEvent(E.Update, args =>
            {
                SetupMenu();

            });
        }
        float[] specvals = new float[1];
        float[] Diffvals = new float[1];

        void SetupMenu()
        {
            if (ImGui.SliderFloat("Blend", Diffvals, 0, 1))
            {
                var gfx = _cube.GetComponent<StaticModel>().GetMaterial().Graphics;
                gfx.SetBlendMode(BlendMode.BlendMultiply);
                
            }
            if (ImGui.SliderFloat("Roughness Color", Diffvals, 0, 1))
            {
                _cube.GetComponent<StaticModel>().GetMaterial().SetShaderParameter("Roughness", Diffvals[0]);
            }
            if (ImGui.SliderFloat("Specular Radius", specvals, 0, 64))
            {
                _cube.GetComponent<StaticModel>().GetMaterial().SetShaderParameter("MatSpecColor", new Vector4(.5f, .5f, .5f, (specvals[0])));
            }
            if (ImGui.SliderFloat("Diffuse Color", Diffvals, 0, 64))
            {
                _cube.GetComponent<StaticModel>().GetMaterial().SetShaderParameter("MatDiffColor", new Vector3(Diffvals[0], Diffvals[0], Diffvals[0]));
            }
            if (ImGui.SliderFloat("Ambient Color", Diffvals, 0, 64))
            {
                _cube.GetComponent<StaticModel>().GetMaterial().SetShaderParameter("AmbientColor", new Vector3(Diffvals[0], Diffvals[0], Diffvals[0]));
            }
            if (ImGui.SliderFloat("Pos", Diffvals, 0, 64))
            {
                var mat = _cube.GetComponent<StaticModel>().GetMaterial();
                var p = mat.GetShaderParameter("LightPosPS");
                _light.Position = new Vector3(Diffvals[0], Diffvals[0], Diffvals[0]);
                mat.SetShaderParameter("LightPosPS", new Vector3(Diffvals[0], Diffvals[0], Diffvals[0]));
            }

            ImGui.ProgressBar(.5f, new Vector2(200, 20), "progval");
        }
    }
}
