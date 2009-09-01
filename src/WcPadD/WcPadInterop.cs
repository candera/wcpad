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
        [DllImport("wcpadlib.dll", CallingConvention=CallingConvention.StdCall)]
        internal static extern void Initialize();

        [DllImport("wcpadlib.dll", CallingConvention = CallingConvention.StdCall)]
        internal static extern int Update();

        [DllImport("wcpadlib.dll", CallingConvention = CallingConvention.StdCall)]
        internal static extern FingertipInfo GetFingertipInfo(int n); 

        [DllImport("wcpadlib.dll", CallingConvention=CallingConvention.StdCall)]
        internal static extern void Cleanup(); 
        
    }
}
