
#include <streams.h>

#include "DualNtscSource.h"
#include "Guids.h"

#define FRAME_WIDTH 720
#define FRAME_HEIGHT 480

const AMOVIESETUP_MEDIATYPE sudOpPinTypes =
{
    &MEDIATYPE_Video,       // Major type
    &MEDIASUBTYPE_NULL      // Minor type
};




/**********************************************
*
*  CDualNtscSourceStream Class
*
**********************************************/

CDualNtscSourceStream::CDualNtscSourceStream(HRESULT *phr, CSource *pFilter)
      : CSourceStream(NAME("Dual NTSC Source"), phr, pFilter, L"Out"),
        m_FramesWritten(0),
        m_bZeroMemory(0),
        m_pBmi(0),
        m_cbBitmapInfo(0),
        video1Ptr(NULL),
        video2Ptr(NULL),
		alphaSetter(NULL),
		m_iFrameNumber(0),
        m_rtFrameLength(FPS_NTSC)
{
}



CDualNtscSourceStream::~CDualNtscSourceStream()
{
}



// Single media type - RGB32, NTSC, 720x480
HRESULT CDualNtscSourceStream::GetMediaType(CMediaType *pmt)
{
    CAutoLock cAutoLock(m_pFilter->pStateLock());

	CheckPointer(pmt, E_POINTER);

	VIDEOINFO *pvi = (VIDEOINFO *)pmt->AllocFormatBuffer(sizeof(VIDEOINFO));
	if (NULL == pvi)
		return(E_OUTOFMEMORY);

	ZeroMemory(pvi, sizeof(VIDEOINFO));

	// Return our highest quality 32bit format
	pvi->bmiHeader.biCompression = BI_RGB;
	pvi->bmiHeader.biBitCount = 32;
	pvi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	pvi->bmiHeader.biWidth = FRAME_WIDTH;
	pvi->bmiHeader.biHeight = FRAME_HEIGHT;
	pvi->bmiHeader.biPlanes = 1;
	pvi->bmiHeader.biSizeImage = GetBitmapSize(&pvi->bmiHeader);
	pvi->bmiHeader.biClrImportant = 0;

	SetRectEmpty(&(pvi->rcSource)); // we want the whole image area rendered.
	SetRectEmpty(&(pvi->rcTarget)); // no particular destination rectangle

	pmt->SetType(&MEDIATYPE_Video);
	pmt->SetFormatType(&FORMAT_VideoInfo);
	pmt->SetTemporalCompression(FALSE);

	// Work out the GUID for the subtype from the header info.
	const GUID SubTypeGUID = GetBitmapSubtype(&pvi->bmiHeader);
	pmt->SetSubtype(&SubTypeGUID);
	pmt->SetSampleSize(pvi->bmiHeader.biSizeImage);

	return S_OK;
}


HRESULT CDualNtscSourceStream::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *pRequest)
{
    HRESULT hr;
    CAutoLock cAutoLock(m_pFilter->pStateLock());

    CheckPointer(pAlloc, E_POINTER);
    CheckPointer(pRequest, E_POINTER);

    VIDEOINFOHEADER *pvi = (VIDEOINFOHEADER*) m_mt.Format();
    
    // Ensure a minimum number of buffers
    if (pRequest->cBuffers == 0)
    {
        pRequest->cBuffers = 2;
    }
    pRequest->cbBuffer = pvi->bmiHeader.biSizeImage;

    ALLOCATOR_PROPERTIES Actual;
    hr = pAlloc->SetProperties(pRequest, &Actual);
    if (FAILED(hr)) 
    {
        return hr;
    }

    // Is this allocator unsuitable?
    if (Actual.cbBuffer < pRequest->cbBuffer) 
    {
        return E_FAIL;
    }

    return S_OK;
}


// This is where we insert the DIB bits into the video stream.
// FillBuffer is called once for every sample in the stream.
HRESULT CDualNtscSourceStream::FillBuffer(IMediaSample *pSample)
{
    BYTE *pData;
    long cbData;

    CheckPointer(pSample, E_POINTER);

    CAutoLock cAutoLockShared(&m_cSharedState);

	// grab video2 alpha from our setter
	int video2Alpha = 0;
	if (alphaSetter != NULL) {
		alphaSetter->GetVideo2Alpha(&video2Alpha);
	}

    // Access the sample's data buffer
    pSample->GetPointer(&pData);
    cbData = pSample->GetSize();

    // Check that we're still using video
    ASSERT(m_mt.formattype == FORMAT_VideoInfo);

    VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER*)m_mt.pbFormat;

	// Get the image properties from the BITMAPINFOHEADER
	int cxImage = pVih->bmiHeader.biWidth;
	int cyImage = pVih->bmiHeader.biHeight;
	int numPixels = cxImage * cyImage;

	// normalize video 2 alpha
	video2Alpha = min(100, max(0, video2Alpha));
	double video2AlphaMultiplier = video2Alpha / 100.0;

	// setting stream pixel data
	if (video1Ptr != NULL && video2Ptr != NULL) {
		RGBQUAD *prgb = (RGBQUAD*)pData;
		RGBQUAD *pVid1rgb = (RGBQUAD*)video1Ptr;
		RGBQUAD *pVid2rgb = (RGBQUAD*)video2Ptr;
		for (int iPixel = 0; iPixel < numPixels; iPixel++, prgb++, pVid1rgb++, pVid2rgb++) {
			prgb->rgbRed = (BYTE)((pVid1rgb->rgbRed * (1 - video2AlphaMultiplier)) + (pVid2rgb->rgbRed * video2AlphaMultiplier));
			prgb->rgbGreen = (BYTE)((pVid1rgb->rgbGreen * (1 - video2AlphaMultiplier)) + (pVid2rgb->rgbGreen * video2AlphaMultiplier));
			prgb->rgbBlue = (BYTE)((pVid1rgb->rgbBlue * (1 - video2AlphaMultiplier)) + (pVid2rgb->rgbBlue * video2AlphaMultiplier));
		}
	}

    // Set the timestamps that will govern playback frame rate.
    // If this file is getting written out as an AVI,
    // then you'll also need to configure the AVI Mux filter to 
    // set the Average Time Per Frame for the AVI Header.
    // The current time is the sample's start
    REFERENCE_TIME rtStart = m_iFrameNumber * m_rtFrameLength;
    REFERENCE_TIME rtStop  = rtStart + m_rtFrameLength;

    pSample->SetTime(&rtStart, &rtStop);
    m_iFrameNumber++;

    // Set TRUE on every sample for uncompressed frames
    pSample->SetSyncPoint(TRUE);

    return S_OK;
}





/**********************************************
 *
 *  CDualNtscSource Class
 *
 **********************************************/

CDualNtscSource::CDualNtscSource(IUnknown *pUnk, HRESULT *phr)
	: CSource(NAME("Dual NTSC Source"), pUnk, CLSID_DualNtscSourceFilter)
{
    // The pin magically adds itself to our pin array.
    m_pPin = new CDualNtscSourceStream(phr, this);

    if (phr)
    {
        if (m_pPin == NULL)
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }  
}


CDualNtscSource::~CDualNtscSource()
{
    delete m_pPin;
}


CUnknown * WINAPI CDualNtscSource::CreateInstance(IUnknown *pUnk, HRESULT *phr)
{
	CDualNtscSource *pNewFilter = new CDualNtscSource(pUnk, phr);

    if (phr)
    {
        if (pNewFilter == NULL) 
            *phr = E_OUTOFMEMORY;
        else
            *phr = S_OK;
    }

    return pNewFilter;
}


// Standard COM stuff done with a baseclasses flavor

STDMETHODIMP CDualNtscSource::NonDelegatingQueryInterface(REFIID riid, void **ppv)
{
	if (riid == CLSID_IDualNtscSourceFilterConfig)
	{
		return GetInterface((IDualNtscSourceFilterConfig *) this, ppv);
	}
	return CSource::NonDelegatingQueryInterface(riid, ppv);
}





// IDualNtscSourceFilterConfig: SetCallback 

STDMETHODIMP CDualNtscSource::SetAlphaSetter(IAlphaSetter *setter)
{
	CheckPointer(setter, E_POINTER);

	CAutoLock cAutoLock(pStateLock());

	m_pPin->alphaSetter = setter;
	m_pPin->alphaSetter->AddRef();

	return S_OK;
}


// IDualNtscSourceFilterConfig: SetVideoPointers 

STDMETHODIMP CDualNtscSource::SetVideoPointers(BYTE *vid1Ptr, BYTE *vid2Ptr)
{
	CheckPointer(vid1Ptr, E_POINTER);
	CheckPointer(vid2Ptr, E_POINTER);

	CAutoLock cAutoLock(pStateLock());

	m_pPin->video1Ptr = vid1Ptr;
	m_pPin->video2Ptr = vid2Ptr;

	return S_OK;
}
