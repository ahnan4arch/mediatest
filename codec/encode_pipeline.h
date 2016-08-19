#pragma once

#include <queue>
#include <vector>
#include <memory>

#include "codec_defs.h"
#include "codec_utils.h"
#include "base_allocator.h"
#include "time_statistics.h"

#include "autolock.h"
#include "va_interface.h"
#include "bitstream_if.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"


#define TIME_STATS

struct EncInitParams
{
    mfxU16 nTargetUsage;
    mfxU32 CodecId;
    mfxU16 nDstWidth; // source picture width
    mfxU16 nDstHeight; // source picture height
	mfxU32 nFrameRateExtN;
	mfxU32 nFrameRateExtD;

	mfxU16 nBitRate;
    mfxU16 nGopRefDist;

    bool bUseHWLib; // true if application wants to use HW MSDK library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char strDstFile[MSDK_MAX_FILENAME_LEN];

    mfxU16 nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance
	bool bLoopBack;
};

struct EncTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;

	EncTask(): EncSyncP(0)
	{
		MSDK_ZERO_MEMORY(mfxBS);
	}

	mfxStatus Init(mfxU32 nBufferSize)
	{
		Close();
		Reset();
		mfxStatus sts = InitMfxBitstream(&mfxBS, nBufferSize);
		MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(&mfxBS));
		return sts;
	}

	mfxStatus Close()
	{
		WipeMfxBitstream(&mfxBS);
		EncSyncP = 0;
		return MFX_ERR_NONE;
	}

	void Reset()
	{
		// mark sync point as free
		EncSyncP = NULL;
		// prepare bit stream
		mfxBS.DataOffset = 0;
		mfxBS.DataLength = 0;
	}
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    ~CEncTaskPool();

    mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, IBitstreamSink *pSink,
		mfxU32 nPoolSize, mfxU32 nBufferSize);
    mfxStatus GetFreeTask(EncTask **ppTask);
    mfxStatus SynchronizeFirstTask();

    CTimeStatistics& GetOverallStatistics() { return m_statOverall;}
    CTimeStatistics& GetFileStatistics() { return m_statFile;}
    void Close();
protected:
    CSmplBitstreamWriter *m_pWriter;
	IBitstreamSink *m_pBitstreamSink;
    EncTask* m_pTasks;
    mfxU32 m_nPoolSize;
    mfxU32 m_nTaskBufferStart;

    MFXVideoSession* m_pmfxSession;

    CTimeStatistics m_statOverall;
    CTimeStatistics m_statFile;
    mfxU32 GetFreeTaskIndex();
};

/* This class implements a pipeline with 2 mfx components: vpp (video preprocessing) and encode */
class CEncodingPipeline
{
public:
    CEncodingPipeline();
    ~CEncodingPipeline();

    mfxStatus Init(EncInitParams *pParams, MP::IDXVAVideoProcessor* pDXVAProcessor,IBitstreamSink *pSink );
    mfxStatus Run();
    void      Close();
    mfxStatus ResetMFXComponents(EncInitParams* pParams);
    void      PrintInfo();
	bool      Input (void * pSurface/*d3d9surface*/); 
	void      Stop () { m_StopFlag = true;}
private:

	mfxStatus RunEncoding();
    mfxStatus InitMfxEncParams(EncInitParams *pParams);
	mfxStatus AllocFrames();
    void      DeleteFrames();
    mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
    mfxStatus GetFreeTask(EncTask **ppTask);
	mfxU16    GetNextSurface ();

private:

	IBitstreamSink *          m_pBitstreamSink;
    CSmplBitstreamWriter *    m_FileWriter;
    CSmplYUVReader            m_FileReader;
    CEncTaskPool              m_TaskPool;

	mfxVersion                    m_Version;     // real API version with which library is initialized
    MFXVideoSession               m_mfxSession;
    MFXVideoENCODE*               m_pmfxENC;
    mfxVideoParam                 m_mfxEncParams;
    mfxFrameSurface1*             m_pEncSurfaces; // frames array for encoder input
    mfxFrameAllocResponse         m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocator*            m_pMFXAllocator;

	std::queue<mfxU16>            m_ReadyQueue; //a queue of indices of mfxFrameSurface1* in m_pEncSurfaces, 
	WinRTCSDK::Mutex              m_lock;

	/////////////////////////////
	// ext params
	mfxExtAvcTemporalLayers      m_TempoLayers;
    mfxExtCodingOption           m_CodingOption;
    // for look ahead BRC configuration
    mfxExtCodingOption2          m_CodingOption2;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*>   m_EncExtParams;

	bool                         m_StopFlag;

    CTimeStatistics m_statOverall;
    CTimeStatistics m_statFile;

	EncInitParams    m_Params;
	// The DXVA interface
	MP::IDXVAVideoProcessor* m_pDXVAProcessor;
};
