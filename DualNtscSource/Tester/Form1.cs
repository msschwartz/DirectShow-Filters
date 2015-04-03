using DirectShowLib;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace Tester
{
    public partial class Form1 : Form, IAlphaSetter
    {
        private IFilterGraph2 filterGraph;
        private IVideoWindow videoWindow;
        private IMediaControl mediaControl;

        private IntPtr video1Ptr;
        private IntPtr video2Ptr;
        private uint bufferSize;

        private bool increaseAlpha;
        private int video2Alpha;

#if DEBUG
        DsROTEntry m_DsRot;
#endif
        
        
        public Form1()
        {
            InitializeComponent();

            increaseAlpha = true;
            video2Alpha = 0;

            bufferSize = (720 * 480 * 4);
            video1Ptr = Marshal.AllocHGlobal((int)bufferSize);
            video2Ptr = Marshal.AllocHGlobal((int)bufferSize);

            UInt32 red = 0xFFFF0000;
            UInt32 blue = 0xFF0000FF;

            int index = 0;
            for (uint y = 0; y < 480; y++)
            {
                for (uint x = 0; x < 720; x++)
                {
                    Marshal.WriteInt32(video1Ptr, index * 4, (Int32)red);
                    index++;
                }
            }

            index = 0;
            for (uint y = 0; y < 480; y++)
            {
                for (uint x = 0; x < 720; x++)
                {
                    Marshal.WriteInt32(video2Ptr, index * 4, (Int32)blue);
                    index++;
                }
            }

            SetupGraph();
            mediaControl.Run();
        }

        private void SetupGraph()
        {
            int hr;

            // Get the graphbuilder object
            filterGraph = new FilterGraph() as IFilterGraph2;

            // Get a ICaptureGraphBuilder2 to help build the graph
            ICaptureGraphBuilder2 icgb2 = (ICaptureGraphBuilder2)new CaptureGraphBuilder2();

            try
            {
                // Link the ICaptureGraphBuilder2 to the IFilterGraph2
                hr = icgb2.SetFiltergraph(filterGraph);
                DsError.ThrowExceptionForHR(hr);

#if DEBUG
                // Allows you to view the graph with GraphEdit File/Connect
                m_DsRot = new DsROTEntry(filterGraph);
#endif

                // source filter
                IBaseFilter sourceFilter = (IBaseFilter)new DualNtscSourceFilter();
                hr = filterGraph.AddFilter(sourceFilter, "DualNtscSourceFilter");
                DsError.ThrowExceptionForHR(hr);

                // sample config
                IDualNtscSourceFilterConfig sourceConfig = sourceFilter as IDualNtscSourceFilterConfig;
                sourceConfig.SetAlphaSetter(this);
                sourceConfig.SetVideoPointers(video1Ptr, video2Ptr);

                // render filters
                IBaseFilter previewRenderFilter = (IBaseFilter)new VideoRendererDefault();
                hr = filterGraph.AddFilter(previewRenderFilter, "previewRenderFilter");
                DsError.ThrowExceptionForHR(hr);

                // Connect the filters together, use the default renderer
                hr = icgb2.RenderStream(null, null, sourceFilter, null, previewRenderFilter);
                DsError.ThrowExceptionForHR(hr);

                // Configure the Video Window
                videoWindow = filterGraph as IVideoWindow;
                ConfigureVideoWindow();

                // Grab some other interfaces
                mediaControl = filterGraph as IMediaControl;
            }
            finally
            {
                Marshal.ReleaseComObject(icgb2);
            }
        }

        private void ConfigureVideoWindow()
        {
            int hr;

            // Set the output window
            hr = videoWindow.put_Owner(Handle);
            if (hr >= 0) // If there is video
            {
                // Set the window style
                hr = videoWindow.put_WindowStyle((WindowStyle.Child | WindowStyle.ClipChildren | WindowStyle.ClipSiblings));
                DsError.ThrowExceptionForHR(hr);

                // Make the window visible
                hr = videoWindow.put_Visible(OABool.True);
                DsError.ThrowExceptionForHR(hr);

                // Position the playing location
                hr = videoWindow.SetWindowPosition(0, 0, ClientRectangle.Right, ClientRectangle.Bottom);
                DsError.ThrowExceptionForHR(hr);
            }
        }

        public int GetVideo2Alpha(out int alpha)
        {
            if (increaseAlpha)
                video2Alpha += 1;
            else
                video2Alpha -= 1;

            if (video2Alpha == 100)
                increaseAlpha = false;
            if (video2Alpha == 0)
                increaseAlpha = true;

            alpha = video2Alpha;

            return 0;
        }
    }
}
