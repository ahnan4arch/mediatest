// using Media Foundation API for Video capture
#pragma once


#include "AutoLock.h"
#include "ComPtrDefs.h"
#include "WinRTCSDKTypes.h"
#include "Looper.h"
#include "D3D9Renderer.h"

#include "CamControl.h"

using namespace std;

namespace WinRTCSDK {

struct VideoCaptureFormat
{
	string name; // readable name of the type
	GUID type; // retrived by MF_MT_SUBTYPE 
	UINT32 fsNumerator;
	UINT32 fsDenominator;
	UINT32 width;
	UINT32 height;
};

class IVideoCaptureCallback
{
public:
	virtual void OnVideoCaptureData(VideoFrameInfo & pSample) = 0;
};

class MFVidCapture : IMFSourceReaderCallback
{
public:
	MFVidCapture(IVideoCaptureCallback *callback);
	~MFVidCapture(void);

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv){
		static const QITAB qit[] = { QITABENT(MFVidCapture, IMFSourceReaderCallback), { 0 } };
		return ::QISearch(this, qit, iid, ppv);
	}
	STDMETHODIMP_(ULONG) AddRef(){
	   return InterlockedIncrement(&nRefCount_);
	}
    STDMETHODIMP_(ULONG) Release(){
		ULONG uCount = InterlockedDecrement(&nRefCount_);
		//if (uCount == 0); // this object is managed by its owner, do not need to delete this; 
		return uCount;
	}
    // IMFSourceReaderCallback methods
    STDMETHODIMP OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
		LONGLONG llTimestamp, IMFSample *pSample )
	{
		LooperTask t = [=](){ _HandleCameraSample(hrStatus, dwStreamIndex, dwStreamFlags, llTimestamp, pSample); };
		dispatcher_.runAsyncTask(t); // don't block the MF internal thread
		//if ( pSample ) pSample->Release();
		return S_OK;
	}

    STDMETHODIMP OnEvent(DWORD, IMFMediaEvent *) {
        return S_OK;
    }
    STDMETHODIMP OnFlush(DWORD){
        return S_OK;
    }

public:
	HRESULT        CheckDeviceLost(std::wstring removedId, BOOL *pbDeviceLost);
	std::vector<MediaDevInfo> GetDevList ();
	HRESULT        OpenCamera(std::wstring symbolicLink);
	HRESULT        CloseCamera();
	VOID           SetD3DDevManager( _com_ptr_IDirect3DDeviceManager9 pD3D9Mgr);

private:
	static std::string _GetFmtName( GUID fmt);
	HRESULT        _GetFormatFromMediaType( VideoCaptureFormat& fmt, _com_ptr_IMFMediaType type);
	HRESULT        _SetFormat ();
	HRESULT        _UpdateDevList ();
	HRESULT        _CreateVideoCaptureDevice(IMFMediaSource **ppSource);
	HRESULT        _CreateSourceReader ();
	HRESULT        _DestroySourceReader();
	HRESULT        _GetDefaultStride(_com_ptr_IMFMediaType pType, LONG *plStride);
	HRESULT        _HandleCameraSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
						LONGLONG llTimestamp, IMFSample * pSample);

private:
	// internal thread for synchronous mode
    HRESULT        _CameraRunner(); 

private:
	_com_ptr_IDirect3DDeviceManager9    pD3DManager_;
	_com_ptr_IMFMediaSource             pSource_;
	_com_ptr_IMFPresentationDescriptor  ppresentationDescriptor_;
	std::vector<MediaDevInfo>           devList_;
	IVideoCaptureCallback              *callback_;
    long                                nRefCount_;        // Reference count.
    _com_ptr_IMFSourceReader            pReader_;
	// symboliclink is used to check the device lost 
	std::wstring                        symbolicLink_ ; // for current device
	Mutex                               lock_;
	int                                 frameWidth_;
	int                                 frameHeight_;
	long                                defaultStride_;
	GUID                                format_;
	UINT32                              maxFSNumerator_;
	UINT32                              maxFSDenominator_;
	Looper                              dispatcher_;
	AtomicVar<bool>                     cameraRunningFlag_;
	bool                                asyncMode_; // async read mode.
	bool                                isWindows7_;
	CameraControl                       cameraControl_;
};

////////////// This is helper class to access locked media buffer
class MediaBufferLock
{   
public:
    MediaBufferLock(_com_ptr_IMFMediaBuffer pBuffer) : m_p2DBuffer(NULL), m_bLocked(FALSE)
    {
        m_pBuffer = pBuffer;
        // Query for the 2-D buffer interface. OK if this fails.
        (void)m_pBuffer->QueryInterface(IID_PPV_ARGS(&m_p2DBuffer));

		m_pD3DSurf_ = NULL;
		HRESULT hr = ::MFGetService(pBuffer, MR_BUFFER_SERVICE, __uuidof(m_pD3DSurf_), (LPVOID*)&m_pD3DSurf_);
    }

    ~MediaBufferLock()
    {
        UnlockBuffer();
    }
	
	_com_ptr_IDirect3DSurface9 GetD3DSurface()
	{
		return m_pD3DSurf_;
	}
    
	HRESULT LockBuffer (LONG  lDefaultStride,  BYTE  **ppbScanLine0, LONG  *plStride)
    {
        HRESULT hr = S_OK;
        // Use the 2-D version if available.
        if (m_p2DBuffer)
        {
            hr = m_p2DBuffer->Lock2D(ppbScanLine0, plStride);
        }
        else// Use non-2D version.
        {        
            BYTE *pData = NULL;
            hr = m_pBuffer->Lock(&pData, NULL, NULL);
            if (SUCCEEDED(hr))
            {
                *plStride = lDefaultStride;
                *ppbScanLine0 = pData;
            }
        }
        m_bLocked = (SUCCEEDED(hr));
        return hr;
    }

    void UnlockBuffer()
    {
        if (m_bLocked)
        {
            if (m_p2DBuffer)
            {
                (void)m_p2DBuffer->Unlock2D();
            }
            else
            {
                (void)m_pBuffer->Unlock();
            }
            m_bLocked = FALSE;
        }

		m_pD3DSurf_ = NULL;
    }

private:
    _com_ptr_IDirect3DSurface9 m_pD3DSurf_;
    _com_ptr_IMFMediaBuffer    m_pBuffer;
    _com_ptr_IMF2DBuffer       m_p2DBuffer;
    BOOL   m_bLocked;
};

}// namespace WinRTCSDK