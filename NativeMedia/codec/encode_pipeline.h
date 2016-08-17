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

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"


#define TIME_STATS

struct EncodeParams
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

};

struct sTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;
    CSmplBitstreamWriter *pWriter;

    sTask();
    mfxStatus WriteBitstream();
    mfxStatus Reset();
    mfxStatus Init(mfxU32 nBufferSize, CSmplBitstreamWriter *pWriter = NULL);
    mfxStatus Close();
};

class CEncTaskPool
{
public:
    CEncTaskPool();
    ~CEncTaskPool();

    mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize);
    mfxStatus GetFreeTask(sTask **ppTask);
    mfxStatus SynchronizeFirstTask();

    CTimeStatistics& GetOverallStatistics() { return m_statOverall;}
    CTimeStatistics& GetFileStatistics() { return m_statFile;}
    void Close();
protected:
    sTask* m_pTasks;
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

    mfxStatus Init(EncodeParams *pParams, MP::IDXVAVideoProcessor* pDXVAProcessor );
    mfxStatus Run();
    void Close();
    mfxStatus ResetMFXComponents(EncodeParams* pParams);

    void  PrintInfo();

private:

	mfxStatus RunEncoding();

    mfxStatus InitMfxEncParams(EncodeParams *pParams);
    mfxStatus InitFileWriter( const msdk_char *filename);
    mfxStatus AllocFrames();
    void DeleteFrames();
    mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);
    mfxStatus GetFreeTask(sTask **ppTask);
	bool  Input (void * pSurface/*d3d9surface*/);
private:
    CSmplBitstreamWriter *    m_FileWriter;
    CSmplYUVReader            m_FileReader;
    CEncTaskPool              m_TaskPool;

    MFXVideoSession               m_mfxSession;
    MFXVideoENCODE*               m_pmfxENC;
    mfxVideoParam                 m_mfxEncParams;
    mfxFrameSurface1*             m_pEncSurfaces; // frames array for encoder input
    mfxFrameAllocResponse         m_EncResponse;  // memory allocation response for encoder
    mfxFrameAllocator*            m_pMFXAllocator;

	std::queue<mfxFrameSurface1*> m_ReadyQueue;
	WinRTCSDK::Mutex              m_lock;

    mfxExtCodingOption m_CodingOption;
    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;

    CTimeStatistics m_statOverall;
    CTimeStatistics m_statFile;
	EncodeParams    m_Params;
	// The DXVA interface
	MP::IDXVAVideoProcessor* m_pDXVAProcessor;
};
