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
using ImGuiNet;
using System.Diagnostics;
using System.IO;
using Urho3DNet;

namespace DemoApplication
{
    internal class CameraFarClipApplication: Application
    {
        private Scene _scene;
        private Viewport _viewport;
        private Node _camera;
        private Node _cube;
        private Node _light;

        public CameraFarClipApplication(Context context) : base(context)
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
            _viewport.Camera.FarClip = 100f;
            Renderer.SetViewport(0, _viewport);

            // Background
            //Renderer.DefaultZone.FogColor = new Color(0.5f, 0.5f, 0.7f);
            Renderer.DefaultZone.FogColor = new Color(0.5f, 0.5f, 0.7f, .2f);

            // Scene
            _camera.Position = new Vector3(-10, -2, 0);
            _camera.LookAt(Vector3.Zero);

            // Light
            _light = _scene.CreateChild("Light");
            _light.CreateComponent<Light>();
            _light.Position = new Vector3(0, 2, -1);
            _light.LookAt(Vector3.Zero);

            // var cubeNode = _scene.CreateChild("cube");
            // var cubeStModel = cubeNode.CreateComponent<StaticModel>();
            // cubeStModel.SetModel(Cache.GetResource<Model>("Models/Box.mdl"));
            //  cubeStModel.SetMaterial(0, Cache.GetResource<Material>("Materials/Stone.xml"));

            var cusnode = CreateCustomGeometry();

            float[] cameraFarClip = new float[1] { _viewport.Camera.FarClip };
            float[] cubeDepth = new float[1];
            string path = Path.GetTempPath() + "\\test_CustomNode.xml";
            _scene.SubscribeToEvent(E.Update, a =>
            {
                if (ImGui.SliderFloat("Camera FarClip", cameraFarClip, 0, 2000))
                {
                    _viewport.Camera.FarClip = cameraFarClip[0];
                }
                if (ImGui.SliderFloat("Cube depth", cubeDepth, 0, 100))
                {
                    cusnode.Position = new Vector3(cusnode.Position.X, cusnode.Position.Y, cubeDepth[0]);
                    _camera.LookAt(cusnode.Position);
                }

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

        private Node CreateCustomGeometry()
        {
            var vColTech = Cache.GetResource<Technique>("Techniques/NoTexture.xml");
            var mat = new Material(Context);
            mat.SetTechnique(0, vColTech);
            mat.SetShaderParameter("MatDiffColor", Color.Red);
            mat.CullMode = CullMode.CullNone;

            var geoChild = _scene.CreateChild("cusGeo");
            geoChild.Rotate(new Quaternion(90, 0, 0));
            var cusgeo = geoChild.CreateComponent<CustomGeometry>();
            cusgeo.NumGeometries = 1;
            cusgeo.SetMaterial(mat);

            Vector3 normal = new Vector3(1, 0, 0);
            Color color = new Color(1, 0, 0, 1);
            cusgeo.BeginGeometry(0, PrimitiveType.TriangleList);

            cusgeo.DefineVertex(new Vector3(10, 0, 10));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.DefineVertex(new Vector3(10, 0, -10));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.DefineVertex(new Vector3(-10, 10, -10));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.DefineVertex(new Vector3(-10, 10, -10));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.DefineVertex(new Vector3(-10, 10, 00));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.DefineVertex(new Vector3(10, 10, 00));
            cusgeo.DefineNormal(normal);
            cusgeo.DefineColor(Color.Red);

            cusgeo.Commit();

            return geoChild;
        }
    }
}
