#pragma once

#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>

#include <queue>
#include <vector>
#include <memory>

#include "autolock.h"
#include "win32event.h"
#include "codec_defs.h"

#include "codec_utils.h"
#include "base_allocator.h"
#include "d3d_allocator.h"

#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

using namespace WinRTCSDK;

struct IOFrameSurface
{
	IOFrameSurface() 
	{
		::memset(this, 0, sizeof (IOFrameSurface));
	}
    mfxFrameSurface1  frame;
    msdk_tick submit;  // the time when submitting to codec
	mfxSyncPoint syncPoint;
};

struct sInputParams
{
    mfxU32 videoType;
    bool    bUseHWLib; // true if application wants to use HW mfx library
    mfxU32  nMaxFPS; //rendering limited by certain fps
    mfxU16  nAsyncDepth; // asyncronous queue
    mfxU16  gpuCopy; // GPU Copy mode (three-state option)
    msdk_char     strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char     strDstFile[MSDK_MAX_FILENAME_LEN];

    sInputParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
};

struct CPipelineStatistics
{
    CPipelineStatistics():
        m_input_count(0),
        m_output_count(0),
        m_synced_count(0),
        m_tick_overall(0),
        m_tick_fread(0),
        m_tick_fwrite(0),
        m_timer_overall(m_tick_overall)
    {
    }
    virtual ~CPipelineStatistics(){}

    mfxU32 m_input_count;     // number of received incoming packets (frames or bitstreams)
    mfxU32 m_output_count;    // number of delivered outgoing packets (frames or bitstreams)
    mfxU32 m_synced_count;

    msdk_tick m_tick_overall; // overall time passed during processing
    msdk_tick m_tick_fread;   // part of tick_overall: time spent to receive incoming data
    msdk_tick m_tick_fwrite;  // part of tick_overall: time spent to deliver outgoing data

    CAutoTimer m_timer_overall; // timer which corresponds to m_tick_overall

private:
    CPipelineStatistics(const CPipelineStatistics&);
    void operator=(const CPipelineStatistics&);
};

class CDecodingPipeline:
    public CPipelineStatistics
{
public:
    CDecodingPipeline();
    virtual ~CDecodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams, IDirect3DDeviceManager9* pD3DMan);
    virtual mfxStatus RunDecoding();
    virtual void Close();
    virtual mfxStatus ResetDecoder(sInputParams *pParams);

    virtual void PrintInfo();

protected: // functions
    virtual mfxStatus InitMfxParams(sInputParams *pParams);

    virtual mfxStatus CreateAllocator();
    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();
    virtual void DeleteAllocator();

    virtual mfxStatus DeliverOutput(mfxFrameSurface1* frame);
    virtual void PrintPerFrameStat(bool force = false);
    virtual mfxStatus DeliverLoop(void);
    static DWORD MFX_STDCALL DeliverThreadFunc(void* ctx);

	// buffering operation
	IOFrameSurface * dequeOutSurface()
	{
		WinRTCSDK::AutoLock l (m_lock);
        IOFrameSurface *  pSurf = NULL;
		if ( m_OutQueue.empty() == false)
		{
			pSurf = m_OutQueue.front();
			m_OutQueue.pop();
		}
		return pSurf;
	}
	void enqueueOutSurface(IOFrameSurface * pSurf)
	{
		WinRTCSDK::AutoLock l (m_lock);
		m_OutQueue.push(pSurf);
	}

	void returnOutSurface(IOFrameSurface * pSurf)
	{
		WinRTCSDK::AutoLock l (m_lock);
		if ( pSurf->frame.Data.Locked ==0 ){// locked by decoder
			m_FreeQueue.push(pSurf);
		}else{
			m_UsedQueue.push_back(pSurf);
		}
	}

	IOFrameSurface * dequeFreeSurface()
	{
		WinRTCSDK::AutoLock l (m_lock);
        IOFrameSurface *  pSurf = NULL;
		if ( m_FreeQueue.empty() == false)
		{
			pSurf = m_FreeQueue.front();
			m_FreeQueue.pop();
		}
		return pSurf;
	}

	void recycleUsedSurface()
	{
		WinRTCSDK::AutoLock l (m_lock);
		std::vector<IOFrameSurface*>::iterator i = m_UsedQueue.begin();
		while (i != m_UsedQueue.end()){
			if ( (*i)->frame.Data.Locked ==0 ){ // freed by decoder
				i = m_UsedQueue.erase(i);
				continue;
			}else{
				i++;
			}
		}
	}

	mfxStatus  syncOutSurface ( int waitTime)
	{
		IOFrameSurface* pSurf = NULL;
		if ( m_SyncQueue.empty()) return MFX_ERR_MORE_DATA;

		pSurf = m_SyncQueue.front();

		mfxStatus sts = m_mfxSession.SyncOperation(pSurf->syncPoint, waitTime);

		if (MFX_WRN_IN_EXECUTION == sts) {
			return sts;
		}

		if (MFX_ERR_NONE == sts)
		{
			// we got completely decoded frame - pushing it to the delivering thread...
			++m_synced_count;
			if (m_bPrintLatency) {
				m_vLatency.push_back(m_timer_overall.Sync() - pSurf->submit);
			}
			else {
				PrintPerFrameStat();
			}

			if(m_nMaxFps)
			{
				//calculation of a time to sleep in order not to exceed a given fps
				mfxF64 currentTime = (m_output_count) ? CTimer::ConvertToSeconds(m_tick_overall) : 0.0;
				int time_to_sleep = (int)(1000 * ((double)m_output_count / m_nMaxFps - currentTime));
				if (time_to_sleep > 0)
				{
					MSDK_SLEEP(time_to_sleep);
				}
			}

			m_SyncQueue.pop();
			enqueueOutSurface(pSurf);

			m_pDeliveredEvent->Reset();
			m_pDeliverOutputSemaphore->Post();
		}

		if (MFX_ERR_NONE != sts) {
			sts = MFX_ERR_UNKNOWN;
		}

		return sts;
	}
	

protected: // variables
    CSmplYUVWriter          m_FileWriter;
    std::auto_ptr<CSmplBitstreamReader>  m_FileReader;
    mfxBitstream            m_mfxBS; // contains encoded data

    MFXVideoSession         m_mfxSession;
    mfxIMPL                 m_impl;
    MFXVideoDECODE*         m_pmfxDEC;
    mfxVideoParam           m_mfxVideoParams;

    BaseFrameAllocator*     m_pFrameAllocator;
	D3DFrameAllocator*      m_pD3DAllocator;

    mfxFrameAllocResponse   m_mfxResponse; // memory allocation response for decoder

	std::queue<IOFrameSurface*> m_OutQueue;
	std::queue<IOFrameSurface*> m_FreeQueue;
	std::queue<IOFrameSurface*> m_SyncQueue;
	std::vector<IOFrameSurface*> m_UsedQueue; // locked by decoder

    IOFrameSurface*         m_IOSurfacePool; // surface detached from free surfaces array
	mfxU32                  m_IOSurfaceNum;

	Win32Semaphore*         m_pDeliverOutputSemaphore; // to access to DeliverOutput method
	Win32Event*             m_pDeliveredEvent; // to signal when output surfaces will be processed
    mfxStatus               m_error; // error returned by DeliverOutput method
    bool                    m_bStopDeliverLoop;

    bool                    m_bIsCompleteFrame;
    mfxU32                  m_fourcc; // color format of vpp out, i420 by default
    bool                    m_bPrintLatency;

    mfxU32                  m_nTimeout; // enables timeout for video playback, measured in seconds
    mfxU32                  m_nMaxFps; // limit of fps, if isn't specified equal 0.
    mfxU32                  m_nFrames; //limit number of output frames

    std::vector<msdk_tick>  m_vLatency;

	IDirect3DDeviceManager9* m_pD3DManager;
	WinRTCSDK::Mutex         m_lock;
private:
    CDecodingPipeline(const CDecodingPipeline&);
    void operator=(const CDecodingPipeline&);
};
