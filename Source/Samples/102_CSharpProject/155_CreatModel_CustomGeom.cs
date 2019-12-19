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
using System.Collections.Generic;
using System.Linq;

namespace DemoApplication
{
    internal class CreateCustomApplication: Application
    {
        private Scene _scene;
        private Viewport _viewport;
        private Node _camera;
        private Node _cube;
        private Node _light;

        public CreateCustomApplication(Context context) : base(context)
        {
        }

        protected override void Dispose(bool disposing)
        {
            Context.Renderer.SetViewport(0, null);    // Enable disposal of viewport by making it unreferenced by engine.
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
            Context.Input.SetMouseVisible(true);

            // Viewport
            _scene = new Scene(Context);
            _scene.CreateComponent<Octree>();

            _camera = _scene.CreateChild("Camera");
            _viewport = new Viewport(Context);
            _viewport.Scene = _scene;
            _viewport.Camera = _camera.CreateComponent<Camera>();
            Context.Renderer.SetViewport(0, _viewport);

            // Background
            //Renderer.DefaultZone.FogColor = new Color(0.5f, 0.5f, 0.7f);
            Context.Renderer.DefaultZone.FogColor = new Color(0.5f, 0.5f, 0.7f, .2f);

            // Scene
            _camera.Position = new Vector3(0, 2, -2);
            _camera.LookAt(Vector3.Zero);

            // Light
            _light = _scene.CreateChild("Light");
            _light.CreateComponent<Light>();
            _light.Position = new Vector3(0, 2, -1);
            _light.LookAt(Vector3.Zero);

            //Create Model from Scratch
            CreateModelfromScratch();

            string path = Path.GetTempPath() + "\\test_CustomNode.xml";

            _scene.SubscribeToEvent(E.Update, a =>
            {
                if (ImGui.Button("Save"))
                {
                    XMLFile file = new XMLFile(Context);
                    var fileroot = file.CreateRoot("RootNode");
                    _scene.SaveXML(fileroot);
                    if (file.SaveFile(path))
                    {
                        new MessageBox(Context, "File is saved", "Save Result");

                        //open externally xml file in editor
                        Process.Start(path);
                    }
                    else
                    {
                        new MessageBox(Context, "File is not saved", "Save Result");
                    }

                    file.Dispose();
                }
            });
        }


        unsafe private void CreateModelfromScratch()
        {
            var cv1 = new CustomGeometryVertex();
            cv1.Position = new Vector3(1, 0, 0);
            cv1.TexCoord = new Vector2(0, 0);
            cv1.Normal = new Vector3(0, 1, 0);

            var cv2 = new CustomGeometryVertex();
            cv2.Position = new Vector3(10, 0, -10);
            cv2.TexCoord = new Vector2(1, 0);
            cv2.Normal = new Vector3(0, 1, 0);

            var cv3 = new CustomGeometryVertex();
            cv3.Position = new Vector3(-10, 0, -10);
            cv3.TexCoord = new Vector2(1, 1);
            cv3.Normal = new Vector3(0, 1, 0);

            var cv4 = new CustomGeometryVertex();
            cv4.Position = new Vector3(-10, 0, 10);
            cv4.TexCoord = new Vector2(0, 1);
            cv4.Normal = new Vector3(0, 1, 0);

            var IndexData = new short[] { 0, 1, 2, 0, 2, 3 };
            var verlist = new CustomGeometryVerticesList(4);
            verlist.Add(cv1);
            verlist.Add(cv2);
            verlist.Add(cv3);
            verlist.Add(cv4);



            var cusGeo = _scene.CreateComponent<CustomGeometry>();


            var mat = new Material(Context);
            mat.SetShaderParameter("MatDiffColor", Color.Red);
            mat.CullMode = CullMode.CullNone;
            cusGeo.SetMaterial(mat);
            cusGeo.NumGeometries = 1;
            cusGeo.BeginGeometry(0, PrimitiveType.TriangleList);

            cusGeo.DefineVertex(cv1.Position);
            //cusGeo.DefineNormal(cv1.Normal);
            //cusGeo.DefineTexCoord(cv1.TexCoord);

            //cusGeo.DefineVertex(cv2.Position);
            //cusGeo.DefineNormal(cv2.Normal);
            //cusGeo.DefineTexCoord(cv2.TexCoord);

            //cusGeo.DefineVertex(cv3.Position);
            //cusGeo.DefineNormal(cv3.Normal);
            //cusGeo.DefineTexCoord(cv3.TexCoord);

            //cusGeo.DefineVertex(cv4.Position);
            //cusGeo.DefineNormal(cv4.Normal);
            //cusGeo.DefineTexCoord(cv4.TexCoord);

            cusGeo.Commit();



            cusGeo.Commit();

        }


    }
}
