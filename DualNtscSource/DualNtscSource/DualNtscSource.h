
#include <strsafe.h>

// UNITS = 10 ^ 7
const REFERENCE_TIME FPS_NTSC = (long)((float)UNITS / (30000.0 / 1001.0));

// Filter name
#define FILTER_NAME     L"Dual NTSC Source Filter"


// alpha setter interface - called before filling the buffer
DECLARE_INTERFACE_(IAlphaSetter, IUnknown) {
	STDMETHOD(GetVideo2Alpha)(THIS_
		int *alpha
	) PURE;
};


// filter config interface
DECLARE_INTERFACE_(IDualNtscSourceFilterConfig, IUnknown) {

	STDMETHOD(SetVideoPointers) (THIS_
		BYTE *vid1Ptr,
		BYTE *vid2Ptr
	) PURE;

	STDMETHOD(SetAlphaSetter) (THIS_
		IAlphaSetter *setter
	) PURE;
};


class CDualNtscSourceStream : public CSourceStream
{
protected:

    int m_FramesWritten;				// To track where we are in the file
    BOOL m_bZeroMemory;                 // Do we need to clear the buffer?
    CRefTime m_rtSampleTime;	        // The time stamp for each sample

    BITMAPINFO *m_pBmi;                 // Pointer to the bitmap header
    DWORD       m_cbBitmapInfo;         // Size of the bitmap header

	int video2Alpha; // We only need one alpha for blending

    int m_iFrameNumber;
    const REFERENCE_TIME m_rtFrameLength;

    CCritSec m_cSharedState;            // Protects our internal state
    CImageDisplay m_Display;            // Figures out our media type for us

public:

	CDualNtscSourceStream(HRESULT *phr, CSource *pFilter);
	~CDualNtscSourceStream();

    // Override the version that offers exactly one media type
    HRESULT GetMediaType(CMediaType *pMediaType);
    HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest);
    HRESULT FillBuffer(IMediaSample *pSample);
	
	BYTE *video1Ptr; // Points to pixel bits
	BYTE *video2Ptr; // Points to pixel bits  
	IAlphaSetter *alphaSetter;

    // Quality control
	// Not implemented because we aren't going in real time.
	// If the file-writing filter slows the graph down, we just do nothing, which means
	// wait until we're unblocked. No frames are ever dropped.
    STDMETHODIMP Notify(IBaseFilter *pSelf, Quality q)
    {
        return E_FAIL;
    }

};



class CDualNtscSource : public CSource, public IDualNtscSourceFilterConfig
{

private:
    // Constructor is private because you have to use CreateInstance
	CDualNtscSource(IUnknown *pUnk, HRESULT *phr);
	~CDualNtscSource();

	CDualNtscSourceStream *m_pPin;

public:
    static CUnknown * WINAPI CreateInstance(IUnknown *pUnk, HRESULT *phr);

	STDMETHODIMP NonDelegatingQueryInterface(REFIID riid, void ** ppv);
	DECLARE_IUNKNOWN;

	STDMETHODIMP SetVideoPointers(BYTE *vid1Ptr, BYTE *vid2Ptr);
	STDMETHODIMP SetAlphaSetter(IAlphaSetter *setter);

};
