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

namespace DemoApplication
{
    class VmapfromXML: Application
    {
        private Scene _scene;
        private Viewport _viewport;
        private Node _camera;
        private Node _cube;
        private Node _light;

        public VmapfromXML(Context context) : base(context)
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

            // Cube
            _cube = _scene.CreateChild("Cube");
            var model = _cube.CreateComponent<StaticModel>();
            model.SetModel(Cache.GetResource<Model>("Models/Box.mdl"));
            model.SetMaterial(0, Cache.GetResource<Material>("Materials/Stone.xml"));

            // RotateObject component is implemented in Data/Scripts/RotateObject.cs
            _cube.CreateComponent("ScriptRotateObject");
            var cusnode = _cube.CreateComponent<CustomNodeComponent>();
            cusnode.Vmap.Add("test", "Note: My notes is mentioned here");
            cusnode.Vmap.Add("test1", "testing01");
            cusnode.Vmap.Add("test2", "testing02");
            cusnode.Vmap.Add("test3", "testing03");
            cusnode.Vmap.Add("test4", new Vector3(0, 1, 2));
            cusnode.Note = "testing General Note";
            // Light
            _light = _scene.CreateChild("Light");
            _light.CreateComponent<Light>();
            _light.Position = new Vector3(0, 2, -1);
            _light.LookAt(Vector3.Zero);
            string path = Path.GetTempPath() + "\\cube.xml";
            SubscribeToEvent(E.Update, args =>
            {
                var timestep = args[E.Update.TimeStep].Float;
                Debug.Assert(this != null);

                if (ImGui.Begin("Urho3D.NET"))
                {
                    ImGui.TextColored(Color.Red, $"Hello world from C#.\nFrame time: {timestep}");
                }
                ImGui.End();

                if (ImGui.Button("Save"))
                {
                    var xmlFile = new XMLFile(Context);
                    var xmlRoot = xmlFile.GetOrCreateRoot("root");
                    if (_scene.SaveXML(xmlRoot))
                    {
                        xmlFile.SaveFile(path);
                        new MessageBox(Context, "File is saved", "Save Result");

                        //open externally xml file in editor
                        if (path.EndsWith("xml") || path.EndsWith("json"))
                            Process.Start("notepad++.exe", path);
                    }
                    else
                    {
                        new MessageBox(Context, "File is not saved", "Save Result");
                    }
                }

                if (ImGui.Button("Load"))
                {
                    _scene.RemoveAllChildren();
                    _scene.LoadFile(path);

                    var cube = _scene.GetChild("Cube");
                    Debug.Assert(!(cube is null), "Error loading Cube from File");

                    var customnodeloaded = cube.GetComponent<CustomNodeComponent>();
                    Debug.Assert(!(customnodeloaded is null), "Error loading component from File");

                    Debug.Assert(customnodeloaded.Vmap.Count > 0, "Error CustomNodeComponent.Vmap not fully loaded from File");
                    Debug.Assert(!string.IsNullOrEmpty(customnodeloaded.Note), "Error CustomNodeComponent.Note not fully loaded from File");
                }
            });
        }
    }
}
