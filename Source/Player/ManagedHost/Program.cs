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
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using File = System.IO.File;
using Urho3DNet;

namespace Player
{
    internal class Program
    {
        private Context _context;

        private Program()
        {
        }

        private void Run(string[] args)
        {
            var argc = args.Length + 1;                 // args + executable path
            var argv = new string[args.Length + 2];     // args + executable path + null
            argv[0] = new Uri(Assembly.GetExecutingAssembly().CodeBase).LocalPath;
            args.CopyTo(argv, 1);
            Urho3D.ParseArguments(argc, argv);

            using (_context = new Context())
            {
                using (Application editor = Application.wrap(CreateApplication(Context.getCPtr(_context).Handle), true))
                {
                    Environment.ExitCode = editor.Run();
                }
            }
        }

        [STAThread]
        public static void Main(string[] args)
        {
            new Program().Run(args);
        }

        [DllImport("libPlayer")]
        private static extern IntPtr CreateApplication(IntPtr context);
    }
}
