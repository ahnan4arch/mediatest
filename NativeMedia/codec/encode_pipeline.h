#pragma once

#include "codec_defs.h"

#pragma warning(disable : 4201)


#include "codec_utils.h"
#include "base_allocator.h"
#include "time_statistics.h"

#include "mfxmvc.h"
#include "mfxvideo.h"
#include "mfxvp8.h"
#include "mfxvideo++.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"

#include <vector>
#include <memory>

msdk_tick time_get_tick(void);
msdk_tick time_get_frequency(void);

struct EncodeParams
{
    mfxU16 nTargetUsage;
    mfxU32 CodecId;
    mfxU16 nDstWidth; // source picture width
    mfxU16 nDstHeight; // source picture height
    mfxF64 dFrameRate;
    mfxU16 nBitRate;
    mfxU16 nGopPicSize;
    mfxU16 nGopRefDist;
    mfxU16 nNumRefFrame;
    mfxU16 nBRefType;
    mfxU16 nIdrInterval;
    mfxU16 reserved[4];

    bool bUseHWLib; // true if application wants to use HW MSDK library

    msdk_char strSrcFile[MSDK_MAX_FILENAME_LEN];

    msdk_char* srcFileBuff;
    msdk_char* dstFileBuff;

    mfxU16 nAsyncDepth; // depth of asynchronous pipeline, this number can be tuned to achieve better performance

    mfxU16 nRateControlMethod;
    mfxU16 nLADepth; // depth of the look ahead bitrate control  algorithm
    mfxU16 nMaxSliceSize; //maximum size of slice
    mfxU16 nQPI;
    mfxU16 nQPP;
    mfxU16 nQPB;

    mfxU16 nNumSlice;
};

struct sTask
{
    mfxBitstream mfxBS;
    mfxSyncPoint EncSyncP;
    std::list<mfxSyncPoint> DependentVppTasks;
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

    mfxStatus Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter, mfxU32 nPoolSize, mfxU32 nBufferSize, CSmplBitstreamWriter *pOtherWriter = NULL);
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

    mfxStatus Init(EncodeParams *pParams);
    mfxStatus Run();
    void Close();
    mfxStatus ResetMFXComponents(EncodeParams* pParams);

    void  PrintInfo();

protected:
    std::pair<CSmplBitstreamWriter *,
              CSmplBitstreamWriter *> m_FileWriters;
    CSmplYUVReader m_FileReader;
    CEncTaskPool m_TaskPool;

    MFXVideoSession m_mfxSession;
    MFXVideoENCODE* m_pmfxENC;

    mfxVideoParam m_mfxEncParams;

    MFXFrameAllocator* m_pMFXAllocator;

    mfxFrameSurface1* m_pEncSurfaces; // frames array for encoder input (vpp output)
    mfxFrameAllocResponse m_EncResponse;  // memory allocation response for encoder

    mfxExtCodingOption m_CodingOption;
    // for look ahead BRC configuration
    mfxExtCodingOption2 m_CodingOption2;

    // external parameters for each component are stored in a vector
    std::vector<mfxExtBuffer*> m_EncExtParams;

    CTimeStatistics m_statOverall;
    CTimeStatistics m_statFile;
    mfxStatus InitMfxEncParams(EncodeParams *pParams);

    mfxStatus InitFileWriters(EncodeParams *pParams);
    void FreeFileWriters();
    mfxStatus InitFileWriter(CSmplBitstreamWriter **ppWriter, const msdk_char *filename);

    mfxStatus AllocFrames();
    void DeleteFrames();

    mfxStatus AllocateSufficientBuffer(mfxBitstream* pBS);

    mfxStatus GetFreeTask(sTask **ppTask);
    MFXVideoSession& GetFirstSession(){return m_mfxSession;}
};
