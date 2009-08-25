using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Runtime.InteropServices;

namespace Wangdera.WcPad.Driver
{
    [StructLayout(LayoutKind.Sequential)]
    internal struct FingertipInfo
    {
        internal float x;
        internal float y;
    }

    internal static class WcPadInterop
    {
        [DllImport("wcpad.dll")]
        internal static extern void Initialize();

        [DllImport("wcpad.dll")]
        internal static extern int Update();

        [DllImport("wcpad.dll")]
        internal static extern FingertipInfo GetFingertipInfo(int n); 

        [DllImport("wcpad.dll")]
        internal static extern void Cleanup(); 
        
    }
}
