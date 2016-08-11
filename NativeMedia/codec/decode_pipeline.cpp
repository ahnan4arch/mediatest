 
#include "stdafx.h"

#include "codec_defs.h"

#if defined(_WIN32) || defined(_WIN64)
#include <tchar.h>
#include <windows.h>
#endif

#include <ctime>
#include <algorithm>
#include "decode_pipeline.h"
#include "sys_primitives.h"
#include "d3d_allocator.h"

#pragma warning(disable : 4100)

CDecodingPipeline::CDecodingPipeline()
{
    MSDK_ZERO_MEMORY(m_mfxBS);

    m_pmfxDEC = NULL;
    m_impl = 0;

    MSDK_ZERO_MEMORY(m_mfxVideoParams);

	m_pFrameAllocator = NULL;
	m_pD3DAllocator = NULL; 

    MSDK_ZERO_MEMORY(m_mfxResponse);

    m_pDeliverOutputSemaphore = NULL;
    m_pDeliveredEvent = NULL;
    m_error = MFX_ERR_NONE;
    m_bStopDeliverLoop = false;

    m_bIsCompleteFrame = false;
    m_bPrintLatency = false;
    m_fourcc = 0;

    m_nTimeout = 0;
    m_nMaxFps = 0;


    m_vLatency.reserve(1000); // reserve some space to reduce dynamic reallocation impact on pipeline execution

}

CDecodingPipeline::~CDecodingPipeline()
{
    Close();
}

mfxStatus CDecodingPipeline::Init(sInputParams *pParams, IDirect3DDeviceManager9* pD3DMan)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);

	// set the d3d manager
	m_pD3DManager = pD3DMan;

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input stream file reader
    switch (pParams->videoType)
    {
    case MFX_CODEC_HEVC:
    case MFX_CODEC_AVC:
        m_FileReader.reset(new CH264FrameReader());
        m_bIsCompleteFrame = true;
        m_bPrintLatency = true;
        break;
    default:
        return MFX_ERR_UNSUPPORTED; // latency mode is supported only for H.264 and JPEG codecs
	}


    m_nMaxFps = pParams->nMaxFPS;
    m_nFrames =  MFX_INFINITE;

    sts = m_FileReader->Init(pParams->strSrcFile);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxInitParam initPar;
    mfxVersion version;     // real API version with which library is initialized

    MSDK_ZERO_MEMORY(initPar);

    // we set version to 1.0 and later we will query actual version of the library which will got leaded
    initPar.Version.Major = 1;
    initPar.Version.Minor = 0;

    initPar.GPUCopy = pParams->gpuCopy;

    // Init session
    if (pParams->bUseHWLib) {
        // try searching on all display adapters
        initPar.Implementation = MFX_IMPL_HARDWARE_ANY;
        initPar.Implementation |= MFX_IMPL_VIA_D3D9;
        sts = m_mfxSession.InitEx(initPar);
        // MSDK API version may not support multiple adapters - then try initialize on the default
        if (MFX_ERR_NONE != sts) {
            initPar.Implementation = (initPar.Implementation & !MFX_IMPL_HARDWARE_ANY) | MFX_IMPL_HARDWARE;
            sts = m_mfxSession.InitEx(initPar);
        }
    } else {
        initPar.Implementation = MFX_IMPL_SOFTWARE;
        sts = m_mfxSession.InitEx(initPar);
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_mfxSession.QueryVersion(&version); // get real API version of the loaded library
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_mfxSession.QueryIMPL(&m_impl); // get actual library implementation
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if ((pParams->videoType == MFX_CODEC_JPEG) && !CheckVersion(&version, MSDK_FEATURE_JPEG_DECODE)) {
        msdk_printf(MSDK_STRING("error: Jpeg is not supported in the %d.%d API version\n"),
            version.Major, version.Minor);
        return MFX_ERR_UNSUPPORTED;
    }

    // create decoder
    m_pmfxDEC = new MFXVideoDECODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_MEMORY_ALLOC);

    // set video type in parameters
    m_mfxVideoParams.mfx.CodecId = pParams->videoType;

    // prepare bit stream
    if (MFX_CODEC_CAPTURE != pParams->videoType)
    {
        sts = InitMfxBitstream(&m_mfxBS, 1024 * 1024);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    if (CheckVersion(&version, MSDK_FEATURE_PLUGIN_API)) {
	}

    // Populate parameters. Involves DecodeHeader call
    sts = InitMfxParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
       
    // prepare YUV file writer
    sts = m_FileWriter.Init(pParams->strDstFile, 1);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = CreateAllocator();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // in case of HW accelerated decode frames must be allocated prior to decoder initialization
    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxDEC->Init(&m_mfxVideoParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxDEC->GetVideoParam(&m_mfxVideoParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return sts;
}

void CDecodingPipeline::Close()
{

    WipeMfxBitstream(&m_mfxBS);
    MSDK_SAFE_DELETE(m_pmfxDEC);

    DeleteFrames();

    m_mfxSession.Close();
    m_FileWriter.Close();
    if (m_FileReader.get()){
        m_FileReader->Close();
	}

    // allocator if used as external for MediaSDK must be deleted after decoder
    DeleteAllocator();

    return;
}

mfxStatus CDecodingPipeline::InitMfxParams(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    while(true)
    {
        // trying to find PicStruct information in AVI headers
        if ( m_mfxVideoParams.mfx.CodecId == MFX_CODEC_JPEG )
            MJPEG_AVI_ParsePicStruct(&m_mfxBS);

        // parse bit stream and fill mfx params
        sts = m_pmfxDEC->DecodeHeader(&m_mfxBS, &m_mfxVideoParams);

        if (MFX_ERR_MORE_DATA == sts)
        {
            if (m_mfxBS.MaxLength == m_mfxBS.DataLength)
            {
                sts = ExtendMfxBitstream(&m_mfxBS, m_mfxBS.MaxLength * 2);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            // read a portion of data
            sts = m_FileReader->ReadNextFrame(&m_mfxBS);
            if (MFX_ERR_MORE_DATA == sts &&
                !(m_mfxBS.DataFlag & MFX_BITSTREAM_EOS))
            {
                m_mfxBS.DataFlag |= MFX_BITSTREAM_EOS;
                sts = MFX_ERR_NONE;
            }
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            continue;
        }

		if ( sts == MFX_ERR_NONE) break;
    }

    // check DecodeHeader status
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (!m_mfxVideoParams.mfx.FrameInfo.FrameRateExtN || !m_mfxVideoParams.mfx.FrameInfo.FrameRateExtD) {
        msdk_printf(MSDK_STRING("pretending that stream is 30fps one\n"));
        m_mfxVideoParams.mfx.FrameInfo.FrameRateExtN = 30;
        m_mfxVideoParams.mfx.FrameInfo.FrameRateExtD = 1;
    }
    if (!m_mfxVideoParams.mfx.FrameInfo.AspectRatioW || !m_mfxVideoParams.mfx.FrameInfo.AspectRatioH) {
        msdk_printf(MSDK_STRING("pretending that aspect ratio is 1:1\n"));
        m_mfxVideoParams.mfx.FrameInfo.AspectRatioW = 1;
        m_mfxVideoParams.mfx.FrameInfo.AspectRatioH = 1;
    }
      
	m_mfxVideoParams.IOPattern = (mfxU16)(pParams->bUseHWLib ? MFX_IOPATTERN_OUT_VIDEO_MEMORY : MFX_IOPATTERN_OUT_SYSTEM_MEMORY);

    m_mfxVideoParams.AsyncDepth = pParams->nAsyncDepth;

    return MFX_ERR_NONE;
}


mfxStatus CDecodingPipeline::AllocFrames()
{
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_NULL_PTR);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest Request;

    MSDK_ZERO_MEMORY(Request);

	sts = m_pmfxDEC->Query(&m_mfxVideoParams, &m_mfxVideoParams);
    MSDK_IGNORE_MFX_STS(sts, MFX_WRN_INCOMPATIBLE_VIDEO_PARAM);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // calculate number of surfaces required for decoder
    sts = m_pmfxDEC->QueryIOSurf(&m_mfxVideoParams, &Request);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        //MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
		
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if ((Request.NumFrameSuggested < m_mfxVideoParams.AsyncDepth) &&
        (m_impl & MFX_IMPL_HARDWARE_ANY))
        return MFX_ERR_MEMORY_ALLOC;

    Request.Type |= MFX_MEMTYPE_VIDEO_MEMORY_DECODER_TARGET;


    // alloc frames for decoder
    sts = m_pFrameAllocator->Alloc(m_pFrameAllocator->pthis, &Request, &m_mfxResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare mfxFrameSurface1 array for decoder
    m_IOSurfaceNum = m_mfxResponse.NumFrameActual;
	m_IOSurfacePool = new IOFrameSurface[m_IOSurfaceNum];

    for (int i = 0; i < m_IOSurfaceNum; i++)
    {
        // initating each frame:
        MSDK_MEMCPY_VAR(m_IOSurfacePool[i].frame.Info, &(Request.Info), sizeof(mfxFrameInfo));
        m_IOSurfacePool[i].frame.Data.MemId = m_mfxResponse.mids[i];

		// add to free queue
		m_FreeQueue.push(&m_IOSurfacePool[i]);
    }

    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::CreateAllocator()
{
    mfxStatus sts = MFX_ERR_NONE;

	// set device handle
	mfxHDL hdl = (mfxHDL)m_pD3DManager;
	mfxHandleType hdl_t = MFX_HANDLE_D3D9_DEVICE_MANAGER;		
	sts = m_mfxSession.SetHandle(hdl_t, hdl);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	m_pD3DAllocator = new D3DFrameAllocator();
	m_pD3DAllocator->Init(m_pD3DManager);
	m_pFrameAllocator = m_pD3DAllocator;
	/* In case of video memory we must provide MediaSDK with external allocator
    thus we demonstrate "external allocator" usage model.
    Call SetAllocator to pass allocator to mediasdk */
    sts = m_mfxSession.SetFrameAllocator(m_pFrameAllocator);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

void CDecodingPipeline::DeleteFrames()
{

	// delete IO pool
	delete m_IOSurfacePool;

    // delete frames
    if (m_pFrameAllocator)
    {
        m_pFrameAllocator->Free(m_pFrameAllocator->pthis, &m_mfxResponse);
    }

    return;
}

void CDecodingPipeline::DeleteAllocator()
{
	m_pFrameAllocator = NULL;
    // delete allocator
    MSDK_SAFE_DELETE(m_pD3DAllocator);
}

mfxStatus CDecodingPipeline::ResetDecoder(sInputParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;

    // close decoder
    sts = m_pmfxDEC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();

    // initialize parameters with values from parsed header
    sts = InitMfxParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // in case of HW accelerated decode frames must be allocated prior to decoder initialization
    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // init decoder
    sts = m_pmfxDEC->Init(&m_mfxVideoParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);


    return MFX_ERR_NONE;
}

mfxStatus CDecodingPipeline::DeliverOutput(mfxFrameSurface1* frame)
{
    CAutoTimer timer_fwrite(m_tick_fwrite);

    mfxStatus res = MFX_ERR_NONE, sts = MFX_ERR_NONE;

    if (!frame) {
        return MFX_ERR_NULL_PTR;
    }

    res = m_pFrameAllocator->Lock(m_pFrameAllocator->pthis, frame->Data.MemId, &(frame->Data));
    if (MFX_ERR_NONE == res) {
        res = m_FileWriter.WriteNextFrame(frame);
        sts = m_pFrameAllocator->Unlock(m_pFrameAllocator->pthis, frame->Data.MemId, &(frame->Data));
    }
    if ((MFX_ERR_NONE == res) && (MFX_ERR_NONE != sts)) {
        res = sts;
    }
    //res = m_hwdev->RenderFrame(frame, m_pFrameAllocator);

    return res;
}

mfxStatus CDecodingPipeline::DeliverLoop(void)
{
    mfxStatus res = MFX_ERR_NONE;

    while (!m_bStopDeliverLoop) 
	{
        m_pDeliverOutputSemaphore->Wait();
        if (m_bStopDeliverLoop) {
            continue;
        }
        if (MFX_ERR_NONE != m_error) {
            continue;
        }

		IOFrameSurface *  pSurf = dequeOutSurface();

		if (!pSurf) {
            m_error = MFX_ERR_NULL_PTR;
            continue;
        }

        mfxFrameSurface1* frame = &(pSurf->frame);

        m_error = DeliverOutput(frame);

		returnOutSurface(pSurf);

		++m_output_count;
		m_pDeliveredEvent->SetEvent();
    }
    return res;
}

DWORD MFX_STDCALL CDecodingPipeline::DeliverThreadFunc(void* ctx)
{
    CDecodingPipeline* pipeline = (CDecodingPipeline*)ctx;

    mfxStatus sts;
    sts = pipeline->DeliverLoop();

    return 0;
}

void CDecodingPipeline::PrintPerFrameStat(bool force)
{
	const int MY_COUNT =1 ;// TODO: this will be cmd option
	const double MY_THRESHOLD =10000.0;
    if ((!(m_output_count % MY_COUNT) ) || force) 
	{
        double fps, fps_fread, fps_fwrite;

        m_timer_overall.Sync();

        fps = (m_tick_overall)? m_output_count/CTimer::ConvertToSeconds(m_tick_overall): 0.0;
        fps_fread = (m_tick_fread)? m_output_count/CTimer::ConvertToSeconds(m_tick_fread): 0.0;
        fps_fwrite = (m_tick_fwrite)? m_output_count/CTimer::ConvertToSeconds(m_tick_fwrite): 0.0;
        // decoding progress
        msdk_printf(MSDK_STRING("Frame number: %4d, fps: %0.3f, fread_fps: %0.3f, fwrite_fps: %.3f\r"),
            m_output_count,
            fps,
            (fps_fread < MY_THRESHOLD)? fps_fread: 0.0,
            (fps_fwrite < MY_THRESHOLD)? fps_fwrite: 0.0);
        fflush(NULL);

    }
}

mfxStatus CDecodingPipeline::RunDecoding()
{
    mfxFrameSurface1*   pOutSurface = NULL;
    mfxBitstream*       pBitstream = &m_mfxBS;
    mfxStatus           sts = MFX_ERR_NONE;
    bool                bErrIncompatibleVideoParams = false;
    CTimeInterval<>     decodeTimer(m_bIsCompleteFrame);
    time_t start_time = time(0);
    MSDKThread * pDeliverThread = NULL;


    m_pDeliverOutputSemaphore = new Win32Semaphore(sts);
    m_pDeliveredEvent = new Win32Event(TRUE);
    pDeliverThread = new MSDKThread(sts, DeliverThreadFunc, this);
    if (!pDeliverThread || !m_pDeliverOutputSemaphore || !m_pDeliveredEvent) {
        MSDK_SAFE_DELETE(pDeliverThread);
        MSDK_SAFE_DELETE(m_pDeliverOutputSemaphore);
        MSDK_SAFE_DELETE(m_pDeliveredEvent);
        return MFX_ERR_MEMORY_ALLOC;
    }

	IOFrameSurface * pFreeSurf;
    while (((sts == MFX_ERR_NONE) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts)) && (m_nFrames > m_output_count))
	{
        if (MFX_ERR_NONE != m_error) {
            msdk_printf(MSDK_STRING("DeliverOutput return error = %d\n"),m_error);
            break;
        }

        if (pBitstream && ((MFX_ERR_MORE_DATA == sts) || (m_bIsCompleteFrame && !pBitstream->DataLength))) {
            CAutoTimer timer_fread(m_tick_fread);
            sts = m_FileReader->ReadNextFrame(pBitstream); // read more data to input bit stream

            if (MFX_ERR_MORE_DATA == sts) {
				pBitstream = NULL;
				sts = MFX_ERR_NONE;

            } else if (MFX_ERR_NONE != sts) {
                break;
            }
        }

		pFreeSurf = NULL;
        if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts))
		{
			recycleUsedSurface();

			pFreeSurf = dequeFreeSurface();

            if (!pFreeSurf)
			{
                // we stuck with no free surface available, now we will sync...
                sts = syncOutSurface(30 * 1000);
                
				if (MFX_ERR_MORE_DATA == sts) 
				{
                    if (m_synced_count != m_output_count) {
						sts = m_pDeliveredEvent->Wait(MSDK_DEC_WAIT_INTERVAL)?MFX_ERR_NONE:MFX_ERR_NOT_FOUND;
                    } else {
                        sts = MFX_ERR_NOT_FOUND;
                    }
                    if (MFX_ERR_NOT_FOUND == sts) {
                        msdk_printf(MSDK_STRING("fatal: failed to find output surface, that's a bug!\n"));
                        break;
                    }
                }
                // note: MFX_WRN_IN_EXECUTION will also be treated as an error at this point
                continue;
            }

            if (!m_pCurrentFreeOutputSurface) {
                m_pCurrentFreeOutputSurface = GetFreeOutputSurface();
            }
            if (!m_pCurrentFreeOutputSurface) {
                sts = MFX_ERR_NOT_FOUND;
                break;
            }
        }

        // exit by timeout
        if ((MFX_ERR_NONE == sts) &&  (time(0)-start_time) >= m_nTimeout) {
            sts = MFX_ERR_NONE;
            break;
        }

        if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts))
		{
            if (m_bIsCompleteFrame) {
                m_pCurrentFreeSurface->submit = m_timer_overall.Sync();
            }
            pOutSurface = NULL;
            do {
                sts = m_pmfxDEC->DecodeFrameAsync(pBitstream, &(m_pCurrentFreeSurface->frame), &pOutSurface, &(m_pCurrentFreeOutputSurface->syncp));
                if (pBitstream && MFX_ERR_MORE_DATA == sts && pBitstream->MaxLength == pBitstream->DataLength)
                {
                    mfxStatus status = ExtendMfxBitstream(pBitstream, pBitstream->MaxLength * 2);
                    MSDK_CHECK_RESULT(status, MFX_ERR_NONE, status);
                }

                if (MFX_WRN_DEVICE_BUSY == sts) {
                    if (m_bIsCompleteFrame) {
                        //in low latency mode device busy leads to increasing of latency
                        //msdk_printf(MSDK_STRING("Warning : latency increased due to MFX_WRN_DEVICE_BUSY\n"));
                    }
                    mfxStatus _sts = SyncOutputSurface(MSDK_DEC_WAIT_INTERVAL);
                    // note: everything except MFX_ERR_NONE are errors at this point
                    if (MFX_ERR_NONE == _sts) {
                        sts = MFX_WRN_DEVICE_BUSY;
                    } else {
                        sts = _sts;
                        if (MFX_ERR_MORE_DATA == sts) {
                            // we can't receive MFX_ERR_MORE_DATA and have no output - that's a bug
                            sts = MFX_WRN_DEVICE_BUSY;//MFX_ERR_NOT_FOUND;
                        }
                    }
                }
            } while (MFX_WRN_DEVICE_BUSY == sts);

            if (sts > MFX_ERR_NONE) {
                // ignoring warnings...
                if (m_pCurrentFreeOutputSurface->syncp) {
                    MSDK_SELF_CHECK(pOutSurface);
                    // output is available
                    sts = MFX_ERR_NONE;
                } else {
                    // output is not available
                    sts = MFX_ERR_MORE_SURFACE;
                }
            } else if ((MFX_ERR_MORE_DATA == sts) && pBitstream) {
                if (m_bIsCompleteFrame && pBitstream->DataLength)
                {
                    // In low_latency mode decoder have to process bitstream completely
                    msdk_printf(MSDK_STRING("error: Incorrect decoder behavior in low latency mode (bitstream length is not equal to 0 after decoding)\n"));
                    sts = MFX_ERR_UNDEFINED_BEHAVIOR;
                    continue;
                }
            } else if ((MFX_ERR_MORE_DATA == sts) && !pBitstream) {
                // that's it - we reached end of stream; now we need to render bufferred data...
                do {
                    sts = SyncOutputSurface(MSDK_DEC_WAIT_INTERVAL);
                } while (MFX_ERR_NONE == sts);

                while (m_synced_count != m_output_count) {
                    m_pDeliveredEvent->Wait();
                }

                if (MFX_ERR_MORE_DATA == sts) {
                    sts = MFX_ERR_NONE;
                }
                break;
            } else if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts) {
                bErrIncompatibleVideoParams = true;
                // need to go to the buffering loop prior to reset procedure
                pBitstream = NULL;
                sts = MFX_ERR_NONE;
                continue;
            }
        }

        if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts)) {
            // if current free surface is locked we are moving it to the used surfaces array
            /*if (m_pCurrentFreeSurface->frame.Data.Locked)*/ {
                m_UsedSurfacesPool.AddSurface(m_pCurrentFreeSurface);
                m_pCurrentFreeSurface = NULL;
            }
        }
        if (MFX_ERR_NONE == sts) {
            
            msdkFrameSurface* surface = FindUsedSurface(pOutSurface);

            msdk_atomic_inc16(&(surface->render_lock));

            m_pCurrentFreeOutputSurface->surface = surface;
            m_OutputSurfacesPool.AddSurface(m_pCurrentFreeOutputSurface);
            m_pCurrentFreeOutputSurface = NULL;
            
        }
    } //while processing

    PrintPerFrameStat(true);

    if (m_bPrintLatency && m_vLatency.size() > 0) {
        unsigned int frame_idx = 0;
        msdk_tick sum = 0;
        for (std::vector<msdk_tick>::iterator it = m_vLatency.begin(); it != m_vLatency.end(); ++it)
        {
            sum += *it;
            msdk_printf(MSDK_STRING("Frame %4d, latency=%5.5f ms\n"), ++frame_idx, CTimer::ConvertToSeconds(*it)*1000);
        }
        msdk_printf(MSDK_STRING("\nLatency summary:\n"));
        msdk_printf(MSDK_STRING("\nAVG=%5.5f ms, MAX=%5.5f ms, MIN=%5.5f ms"),
            CTimer::ConvertToSeconds((msdk_tick)((mfxF64)sum/m_vLatency.size()))*1000,
            CTimer::ConvertToSeconds(*std::max_element(m_vLatency.begin(), m_vLatency.end()))*1000,
            CTimer::ConvertToSeconds(*std::min_element(m_vLatency.begin(), m_vLatency.end()))*1000);
    }

    m_bStopDeliverLoop = true;
    m_pDeliverOutputSemaphore->Post();
    if (pDeliverThread)
        pDeliverThread->Wait();

    MSDK_SAFE_DELETE(m_pDeliverOutputSemaphore);
    MSDK_SAFE_DELETE(m_pDeliveredEvent);
    MSDK_SAFE_DELETE(pDeliverThread);

    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // if we exited main decoding loop with ERR_INCOMPATIBLE_PARAM we need to send this status to caller
    if (bErrIncompatibleVideoParams) {
        sts = MFX_ERR_INCOMPATIBLE_VIDEO_PARAM;
    }

    return sts; // ERR_NONE or ERR_INCOMPATIBLE_VIDEO_PARAM
}

void CDecodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Decoding Sample Version %s\n\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput video\t%s\n"), CodecIdToStr(m_mfxVideoParams.mfx.CodecId).c_str());

	msdk_printf(MSDK_STRING("Output format\t%s\n"), CodecIdToStr(m_mfxVideoParams.mfx.FrameInfo.FourCC).c_str());


    mfxFrameInfo Info = m_mfxVideoParams.mfx.FrameInfo;
    msdk_printf(MSDK_STRING("Input:\n"));
    msdk_printf(MSDK_STRING("  Resolution\t%dx%d\n"), Info.Width, Info.Height);
    msdk_printf(MSDK_STRING("  Crop X,Y,W,H\t%d,%d,%d,%d\n"), Info.CropX, Info.CropY, Info.CropW, Info.CropH);
    msdk_printf(MSDK_STRING("Output:\n"));

	msdk_printf(MSDK_STRING("  Resolution\t%dx%d\n"), Info.Width, Info.Height);

    mfxF64 dFrameRate = CalculateFrameRate(Info.FrameRateExtN, Info.FrameRateExtD);
    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), dFrameRate);

    const msdk_char* sMemType = m_memType == D3D9_MEMORY  ? MSDK_STRING("d3d")
                             : (m_memType == D3D11_MEMORY ? MSDK_STRING("d3d11")
                                                          : MSDK_STRING("system"));
    msdk_printf(MSDK_STRING("Memory type\t\t%s\n"), sMemType);


    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(m_impl)) ? MSDK_STRING("hw_d3d11")
                           : (MFX_IMPL_HARDWARE & m_impl)  ? MSDK_STRING("hw")
                                                         : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("MediaSDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("MediaSDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));

    return;
}
