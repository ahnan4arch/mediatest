#pragma once

#pragma warning(disable : 4201)

#include <algorithm>
#include <queue>
#include <vector>
#include <map>
#include <memory>

#include "autolock.h"
#include "win32event.h"

// interface for MP
#include "va_interface.h"
#include "bitstream_if.h"

#include "codec_defs.h"
#include "codec_utils.h"
#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

using namespace WinRTCSDK;


struct DecInitParams
{
    mfxU32 videoType;
    bool    bUseHWLib; // true if application wants to use HW mfx library
    mfxU32  nMaxFPS; //rendering limited by certain fps
    mfxU16  nAsyncDepth; // asyncronous queue
    msdk_char     strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char     strDstFile[MSDK_MAX_FILENAME_LEN];
	
	bool   bLoopback; // loopback test mode

    DecInitParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
};

struct IOFrameSurface
{
	IOFrameSurface() 
	{
		::memset(this, 0, sizeof (IOFrameSurface));
	}
    mfxFrameSurface1  frame;
    msdk_tick         submit;  // the time when submitting to codec
	mfxU16            renderLock;  // locked for render
	mfxSyncPoint      syncPoint;
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
    ~CDecodingPipeline();
	mfxStatus Run();
    mfxStatus Init(DecInitParams *pParams, MP::IDXVAVideoRender* pRender, IBitstreamSource* pBSSoure);
    mfxStatus RunDecoding();
    void Close();
    mfxStatus ResetDecoder(DecInitParams *pParams);
	void      Stop () { m_StopFlag = true;}
    void PrintInfo();

protected: // functions
    mfxStatus InitMfxParams(DecInitParams *pParams);
    mfxStatus AllocFrames();
    void DeleteFrames();

    mfxStatus DeliverOutput(mfxFrameSurface1* frame);
    void PrintPerFrameStat(bool force = false);
    mfxStatus DeliverLoop(void);
    static DWORD MFX_STDCALL DeliverThreadFunc(void* ctx);

	// buffering operation
	IOFrameSurface * dequeOutSurface();
	void enqueueOutSurface(IOFrameSurface * pSurf);
	void returnOutSurface(IOFrameSurface * pSurf);
	IOFrameSurface * dequeFreeSurface();
	void recycleUsedSurface();
	mfxStatus  syncOutSurface ( int waitTime);

protected: // variables
	IBitstreamSource      * m_pBitStreamSource; 
    CSmplYUVWriter          m_FileWriter;
    CH264FrameReader        m_FileReader;
    mfxBitstream            m_mfxBS; // contains encoded data

    MFXVideoSession         m_mfxSession;
    mfxIMPL                 m_impl;
    MFXVideoDECODE*         m_pmfxDEC;
    mfxVideoParam           m_mfxVideoParams;

    mfxFrameAllocResponse   m_mfxResponse; // memory allocation response for decoder

	std::queue<IOFrameSurface*> m_OutQueue;
	std::queue<IOFrameSurface*> m_FreeQueue;
	std::queue<IOFrameSurface*> m_SyncQueue;
	std::vector<IOFrameSurface*> m_UsedQueue;
	std::map<mfxFrameSurface1 *, IOFrameSurface*> m_SurfaceMap; // map mfxFrameSurface1 * to IOFrameSurface*
	WinRTCSDK::Mutex         m_lock;

    IOFrameSurface*         m_IOSurfacePool; // surface detached from free surfaces array
	mfxU32                  m_IOSurfaceNum;

	Win32Semaphore*         m_pDeliverOutputSemaphore; // to access to DeliverOutput method
	Win32Event*             m_pDeliveredEvent; // to signal when output surfaces will be processed
    mfxStatus               m_render_error; // error returned by DeliverOutput method
    bool                    m_bStopDeliverLoop;

    bool                    m_bPrintLatency;

    mfxU32                  m_nMaxFps; // limit of fps, if isn't specified equal 0.
    mfxU32                  m_nFrames; //limit number of output frames

    std::vector<msdk_tick>  m_vLatency;

	MP::IDXVAVideoRender*   m_pRender;
	DecInitParams            m_Params;
	bool                    m_StopFlag;
private:
    CDecodingPipeline(const CDecodingPipeline&);
    void operator=(const CDecodingPipeline&);
};
