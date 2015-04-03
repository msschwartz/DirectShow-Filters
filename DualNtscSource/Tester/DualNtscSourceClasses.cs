using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;

namespace Tester
{
    [ComImport,
    Guid("46454D0D-22A9-46C2-BA41-4F39B80EFE02")]
    public class DualNtscSourceFilter
    {
    }

    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
    Guid("034C40D9-57AA-458E-B9D2-87EE04530B5A")]
    public interface IAlphaSetter
    {
        [PreserveSig]
        int GetVideo2Alpha(out int video2Alpha);
    }

    [InterfaceType(ComInterfaceType.InterfaceIsIUnknown),
    Guid("19F5248C-2CA2-4B21-8287-7B10BC4A64CB")]
    public interface IDualNtscSourceFilterConfig
    {
        [PreserveSig]
        int SetVideoPointers(IntPtr vid1Ptr, IntPtr vid2Ptr);

        [PreserveSig]
        int SetAlphaSetter(IAlphaSetter setter);
    }
}
