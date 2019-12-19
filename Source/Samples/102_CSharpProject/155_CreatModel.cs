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
    internal class CreateModelApplication: Application
    {
        private Scene _scene;
        private Viewport _viewport;
        private Node _camera;
        private Node _cube;
        private Node _light;

        public CreateModelApplication(Context context) : base(context)
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
            _viewport.Camera.FarClip = float.MaxValue - 1;
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

                if (Context.Input.GetMouseButtonClick(MouseButton.MousebLeft))
                {
                    var ray = _viewport.Camera.GetScreenRay(Context.Input.MousePosition.X, Context.Input.MousePosition.Y);
                }
            });
        }


        unsafe private void CreateModelfromScratch()
        {
            const int numVertices = 18;
            float[] vertexData =
            {
					// Position             Normal				Texture     Tangent
					0.0f, 0.5f, 0.0f,       0.5f, 0.5f, 0.5f,
                    -0.5f, -0.5f, 0.5f,     0.5f, 0.5f, 0.5f,
                    0.5f, -0.5f, 0.5f,      0.5f, 0.0f, 0.5f,

                    0.0f, 0.5f, 0.0f,      -0.5f, 0.5f, 0.0f,
                    -0.5f, -0.5f, -0.5f,   -0.5f, 0.5f, 0.0f,
                    -0.5f, -0.5f, 0.5f,    -0.5f, 0.5f, 0.0f,

                    0.0f, 0.5f, 0.0f,      -0.5f, -0.5f, 0.5f,
                    0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, 0.5f,
                    -0.5f, -0.5f, -0.5f,   -0.5f, -0.5f, 0.5f,

                    0.0f, 0.5f, 0.0f,      -0.5f, -0.5f, 0.5f,
                    0.5f, -0.5f, 0.5f,     -0.5f, -0.5f, 0.5f,
                    0.5f, -0.5f, -0.5f,    -0.5f, -0.5f, 0.5f,

                    0.5f, -0.5f, -0.5f,     0.0f, 0.0f, 0.0f,
                    0.5f, -0.5f, 0.5f,      0.0f, 0.0f, 0.0f,
                    -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 0.0f,

                    0.5f, -0.5f, -0.5f,     0.0f, 0.0f, 0.0f,
                    -0.5f, -0.5f, 0.5f,     0.0f, 0.0f, 0.0f,
                    -0.5f, -0.5f, -0.5f,    0.0f, 0.0f, 0.0f,
                };

            short[] indexData =
            {
                    0, 1, 2,
                    3, 4, 5,
                    6, 7, 8,
                    9, 10, 11,
                    12, 13, 14,
                    15, 16, 17
            };

            Urho3DNet.Model fromScratchModel = new Urho3DNet.Model(Context);
            VertexBuffer vb = new VertexBuffer(Context, false);
            IndexBuffer ib = new IndexBuffer(Context, false);
            Geometry geom = new Geometry(Context);

            // Shadowed buffer needed for raycasts to work, and so that data can be automatically restored on device loss
            vb.SetShadowed(true);
            vb.SetSize(numVertices, VertexMask.MaskPosition | VertexMask.MaskNormal, false);
            fixed (float* vertexDataIntPtr = vertexData)
            {
                vb.SetData((IntPtr)vertexDataIntPtr);
            }

            ib.SetShadowed(true);
            ib.SetSize(numVertices, false, false);
            fixed (short* indexDataIntPtr = indexData)
            {
                ib.SetData((IntPtr)indexDataIntPtr);
            }

            // Urho3D.GenerateTangents(vertexData, vb.GetVertexSize(), indexData, 0, indexData.Length);

            geom.SetVertexBuffer(0, vb);
            geom.IndexBuffer = ib;
            geom.SetDrawRange(PrimitiveType.TriangleList, 0, numVertices, true);

            fromScratchModel.NumGeometries = 1;
            fromScratchModel.SetGeometry(0, 0, geom);

            var v1 = new Vector3(-0.5f, -0.5f, -0.5f);
            var v2 = new Vector3(0.5f, 0.5f, 0.5f);
            Vector3[] vcs = new Vector3[2] { v1, v2 };

            fromScratchModel.BoundingBox = new BoundingBox(vcs);


            Node node = _scene.CreateChild("FromScratchObject");
            node.Position = (new Vector3(0.0f, 0.0f, 0.0f));

            StaticModel staticModel = node.CreateComponent<StaticModel>();
            staticModel.SetModel(fromScratchModel);

            // var mat = new Material(Context);
            // Material Texture notWorking
            //  var mat = _scene.Cache.GetResource<Material>("Materials/Stone.xml");
            var mat = new Material(Context);
            mat.SetShaderParameter("MatDiffColor", Color.Green);
            staticModel.SetMaterial(mat);

            var st = Context.Cache.GetResource<Model>("Models/Box.mdl");
            var vers = st.VertexBuffers[0].GetUnpackedData();
            List<string> vecs = new List<string>();
            for (int i = 0; i < vers.Count; i++)
            {
                vecs.Add(vers[i].ToString().Replace(" ", "f,"));
            }
            Debug.WriteLine(string.Join(",", vecs));
            node.CreateComponent("ScriptRotateObject");
        }


    }
}
