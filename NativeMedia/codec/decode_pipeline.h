#pragma once


#pragma warning(disable : 4201)
#include <d3d9.h>
#include <dxva2api.h>

#include <vector>
#include <memory>

#include "win32event.h"
#include "codec_defs.h"
#include "mfx_buffering.h"

#include "codec_utils.h"
#include "base_allocator.h"
#include "d3d_allocator.h"
#include "sysmem_allocator.h"

#include "mfxmvc.h"
#include "mfxjpeg.h"
#include "mfxplugin.h"
#include "mfxplugin++.h"
#include "mfxvideo.h"
#include "mfxvideo++.h"

using namespace WinRTCSDK;

enum MemType {
    SYSTEM_MEMORY = 0x00,
    D3D9_MEMORY   = 0x01,
    D3D11_MEMORY  = 0x02,
};

enum eWorkMode {
  MODE_PERFORMANCE,
  MODE_RENDERING,
  MODE_FILE_DUMP
};

struct sInputParams
{
    mfxU32 videoType;
    eWorkMode mode;
    MemType memType;
    bool    bUseHWLib; // true if application wants to use HW mfx library
    mfxU32  nMaxFPS; //rendering limited by certain fps
    mfxU32  nRotation; // rotation for Motion JPEG Codec
    mfxU16  nAsyncDepth; // asyncronous queue
    mfxU16  gpuCopy; // GPU Copy mode (three-state option)

    mfxU16  scrWidth;
    mfxU16  scrHeight;

    mfxU16  Width;
    mfxU16  Height;

    mfxU32  fourcc;
    mfxU32  nFrames;
    mfxU16  eDeinterlace;

    msdk_char     strSrcFile[MSDK_MAX_FILENAME_LEN];
    msdk_char     strDstFile[MSDK_MAX_FILENAME_LEN];

    sInputParams()
    {
        MSDK_ZERO_MEMORY(*this);
    }
};

template<>struct mfx_ext_buffer_id<mfxExtMVCSeqDesc>{
    enum {id = MFX_EXTBUFF_MVC_SEQ_DESC};
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
    public CBuffering,
    public CPipelineStatistics
{
public:
    CDecodingPipeline();
    virtual ~CDecodingPipeline();

    virtual mfxStatus Init(sInputParams *pParams, IDirect3DDeviceManager9* pD3DMan);
    virtual mfxStatus RunDecoding();
    virtual void Close();
    virtual mfxStatus ResetDecoder(sInputParams *pParams);

    void SetExtBuffersFlag()       { m_bIsExtBuffers = true; }
    virtual void PrintInfo();

protected: // functions
    virtual mfxStatus InitMfxParams(sInputParams *pParams);

    // function for allocating a specific external buffer
    template <typename Buffer>
    mfxStatus AllocateExtBuffer();
    virtual void DeleteExtBuffers();
    virtual void AttachExtParam();

    virtual mfxStatus InitVppParams();
    virtual mfxStatus AllocAndInitVppFilters();
    virtual bool IsVppRequired(sInputParams *pParams);

    virtual mfxStatus CreateAllocator();
    virtual mfxStatus AllocFrames();
    virtual void DeleteFrames();
    virtual void DeleteAllocator();

    /** \brief Performs SyncOperation on the current output surface with the specified timeout.
     *
     * @return MFX_ERR_NONE Output surface was successfully synced and delivered.
     * @return MFX_ERR_MORE_DATA Array of output surfaces is empty, need to feed decoder.
     * @return MFX_WRN_IN_EXECUTION Specified timeout have elapsed.
     * @return MFX_ERR_UNKNOWN An error has occurred.
     */
    virtual mfxStatus SyncOutputSurface(mfxU32 wait);
    virtual mfxStatus DeliverOutput(mfxFrameSurface1* frame);
    virtual void PrintPerFrameStat(bool force = false);

    virtual mfxStatus DeliverLoop(void);

    static DWORD MFX_STDCALL DeliverThreadFunc(void* ctx);

protected: // variables
    CSmplYUVWriter          m_FileWriter;
    std::auto_ptr<CSmplBitstreamReader>  m_FileReader;
    mfxBitstream            m_mfxBS; // contains encoded data

    MFXVideoSession         m_mfxSession;
    mfxIMPL                 m_impl;
    MFXVideoDECODE*         m_pmfxDEC;
    MFXVideoVPP*            m_pmfxVPP;
    mfxVideoParam           m_mfxVideoParams;
    mfxVideoParam           m_mfxVppVideoParams;

    std::vector<mfxExtBuffer *> m_ExtBuffers;

    BaseFrameAllocator*     m_pFrameAllocator;
	SysMemFrameAllocator *  m_pSysMemAllocator;
	D3DFrameAllocator*      m_pD3DAllocator;

	MemType                 m_memType;      // memory type of surfaces to use
    bool                    m_bExternalAlloc; // use memory allocator as external for Media SDK
    bool                    m_bDecOutSysmem; // use system memory between Decoder and VPP, if false - video memory
    mfxFrameAllocResponse   m_mfxResponse; // memory allocation response for decoder
    mfxFrameAllocResponse   m_mfxVppResponse;   // memory allocation response for vpp

    msdkFrameSurface*       m_pCurrentFreeSurface; // surface detached from free surfaces array
    msdkFrameSurface*       m_pCurrentFreeVppSurface; // VPP surface detached from free VPP surfaces array
    msdkOutputSurface*      m_pCurrentFreeOutputSurface; // surface detached from free output surfaces array
    msdkOutputSurface*      m_pCurrentOutputSurface; // surface detached from output surfaces array

	Win32Semaphore*         m_pDeliverOutputSemaphore; // to access to DeliverOutput method
	Win32Event*             m_pDeliveredEvent; // to signal when output surfaces will be processed
    mfxStatus               m_error; // error returned by DeliverOutput method
    bool                    m_bStopDeliverLoop;

    eWorkMode               m_eWorkMode; // work mode for the pipeline

	bool                    m_bIsExtBuffers; // indicates if external buffers were allocated
    bool                    m_bIsCompleteFrame;
    mfxU32                  m_fourcc; // color format of vpp out, i420 by default
    bool                    m_bPrintLatency;

    mfxU16                  m_vppOutWidth;
    mfxU16                  m_vppOutHeight;

    mfxU32                  m_nTimeout; // enables timeout for video playback, measured in seconds
    mfxU32                  m_nMaxFps; // limit of fps, if isn't specified equal 0.
    mfxU32                  m_nFrames; //limit number of output frames

    mfxU16                  m_diMode;
    bool                    m_bVppIsUsed;
    std::vector<msdk_tick>  m_vLatency;

    mfxExtVPPDoNotUse       m_VppDoNotUse;      // for disabling VPP algorithms
    mfxExtVPPDeinterlacing  m_VppDeinterlacing;
    std::vector<mfxExtBuffer*> m_VppExtParams;

	IDirect3DDeviceManager9* m_pD3DManager;

private:
    CDecodingPipeline(const CDecodingPipeline&);
    void operator=(const CDecodingPipeline&);
};
