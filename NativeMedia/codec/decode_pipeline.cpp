 
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

    MSDK_ZERO_MEMORY(m_mfxResponse);

    m_pDeliverOutputSemaphore = NULL;
    m_pDeliveredEvent = NULL;
    m_error = MFX_ERR_NONE;
    m_bStopDeliverLoop = false;

    m_bPrintLatency = false;

    m_nMaxFps = 0;


    m_vLatency.reserve(1000); // reserve some space to reduce dynamic reallocation impact on pipeline execution

}

CDecodingPipeline::~CDecodingPipeline()
{
    Close();
}

mfxStatus CDecodingPipeline::Init(sInputParams *pParams, MP::IDXVAVideoRender* pRender)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
	m_Params=*pParams;

	// va interface
	m_pRender = pRender;

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input stream file reader
    switch (m_Params.videoType)
    {
    case MFX_CODEC_HEVC:
    case MFX_CODEC_AVC:
        m_FileReader.reset(new CH264FrameReader());
        m_bPrintLatency = true;
        break;
    default:
        return MFX_ERR_UNSUPPORTED; // latency mode is supported only for H.264 and JPEG codecs
	}


    m_nMaxFps = m_Params.nMaxFPS;
    m_nFrames =  MFX_INFINITE;

    sts = m_FileReader->Init(m_Params.strSrcFile);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    mfxInitParam initPar;
    mfxVersion version;     // real API version with which library is initialized

    MSDK_ZERO_MEMORY(initPar);

    // we set version to 1.0 and later we will query actual version of the library which will got leaded
    initPar.Version.Major = 1;
    initPar.Version.Minor = 0;

    initPar.GPUCopy = MFX_GPUCOPY_DEFAULT;

    // Init session
    if (m_Params.bUseHWLib) {
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

    // create decoder
    m_pmfxDEC = new MFXVideoDECODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_MEMORY_ALLOC);

    // set video type in parameters
    m_mfxVideoParams.mfx.CodecId = m_Params.videoType;

    // prepare bit stream
    if (MFX_CODEC_CAPTURE != m_Params.videoType)
    {
        sts = InitMfxBitstream(&m_mfxBS, 1024 * 1024);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    // Populate parameters. Involves DecodeHeader call
    sts = InitMfxParams(&m_Params);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
       
    // prepare YUV file writer
    sts = m_FileWriter.Init(m_Params.strDstFile, 1);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// set device handle
	mfxHDL hdl = (mfxHDL)m_pRender->getHandle();
	mfxHandleType hdl_t = MFX_HANDLE_D3D9_DEVICE_MANAGER;
	sts = m_mfxSession.SetHandle(hdl_t, hdl);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	// set frame allocator
	mfxFrameAllocator* pAlloc = (mfxFrameAllocator*) m_pRender->getFrameAllocator();
	sts = m_mfxSession.SetFrameAllocator(pAlloc);
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

    return;
}

mfxStatus CDecodingPipeline::InitMfxParams(sInputParams *pParams)
{
    MSDK_CHECK_POINTER(m_pmfxDEC, MFX_ERR_NULL_PTR);
    mfxStatus sts = MFX_ERR_NONE;

    while(true)
    {
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
	mfxFrameAllocator* pAlloc = (mfxFrameAllocator*) m_pRender->getFrameAllocator();
    sts = pAlloc->Alloc(pAlloc->pthis, &Request, &m_mfxResponse);
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

		// setup mapping
		m_SurfaceMap[&m_IOSurfacePool[i].frame]  = &m_IOSurfacePool[i];
    }

    return MFX_ERR_NONE;
}

void CDecodingPipeline::DeleteFrames()
{

	// clear containers
	while( m_OutQueue.empty() == false) m_OutQueue.pop();
	while( m_FreeQueue.empty() == false) m_FreeQueue.pop();
	while( m_SyncQueue.empty() == false) m_SyncQueue.pop();
	m_UsedQueue.clear();
	m_SurfaceMap.clear();

	// delete IO pool
	delete m_IOSurfacePool;

    // delete frames
	mfxFrameAllocator* pAlloc = (mfxFrameAllocator*) m_pRender->getFrameAllocator();
    pAlloc->Free(pAlloc->pthis, &m_mfxResponse);

    return;
}

mfxStatus CDecodingPipeline::ResetDecoder(sInputParams *pParams)
{
    mfxStatus sts = MFX_ERR_NONE;
	m_Params = *pParams;
    // close decoder
    sts = m_pmfxDEC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();

    // initialize parameters with values from parsed header
    sts = InitMfxParams(&m_Params);
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
    // alloc frames for decoder
	mfxFrameAllocator* pAlloc = (mfxFrameAllocator*) m_pRender->getFrameAllocator();
    res = pAlloc->Lock(pAlloc->pthis, frame->Data.MemId, &(frame->Data));
    if (MFX_ERR_NONE == res) {
        res = m_FileWriter.WriteNextFrame(frame);
        sts = pAlloc->Unlock(pAlloc->pthis, frame->Data.MemId, &(frame->Data));
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

IOFrameSurface * CDecodingPipeline::dequeOutSurface()
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

void CDecodingPipeline::enqueueOutSurface(IOFrameSurface * pSurf)
{
	WinRTCSDK::AutoLock l (m_lock);
	m_OutQueue.push(pSurf);
}

void CDecodingPipeline::returnOutSurface(IOFrameSurface * pSurf)
{
	WinRTCSDK::AutoLock l (m_lock);
	if ( pSurf->frame.Data.Locked ==0 ){// locked by decoder
		m_FreeQueue.push(pSurf);
	}else{
		m_UsedQueue.push_back(pSurf);
	}
}

IOFrameSurface * CDecodingPipeline::dequeFreeSurface()
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

void CDecodingPipeline::recycleUsedSurface()
{
	WinRTCSDK::AutoLock l (m_lock);
	std::vector<IOFrameSurface*>::iterator i = m_UsedQueue.begin();
	while (i != m_UsedQueue.end()){
		if ( (*i)->frame.Data.Locked ==0 ){ // freed by decoder
			m_FreeQueue.push(*i);
			i = m_UsedQueue.erase(i);
			continue;
		}else{
			i++;
		}
	}
}

mfxStatus  CDecodingPipeline::syncOutSurface ( int waitTime)
{
	IOFrameSurface* pSurf = NULL;
	if ( m_SyncQueue.empty()) return MFX_ERR_MORE_DATA;

	pSurf = m_SyncQueue.front();

	mfxStatus sts = m_mfxSession.SyncOperation(pSurf->syncPoint, waitTime);

	if (MFX_WRN_IN_EXECUTION == sts) {
		return sts;
	}

	if (MFX_ERR_NONE != sts){
		sts = MFX_ERR_UNKNOWN;
		return sts;
	}
	// we got completely decoded frame - pushing it to the delivering thread...
	++m_synced_count;
	if (m_bPrintLatency) {
		m_vLatency.push_back(m_timer_overall.Sync() - pSurf->submit);
	}else {
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

	return sts;
}

mfxStatus CDecodingPipeline::RunDecoding()
{
    mfxBitstream*       pBitstream = &m_mfxBS;
    mfxStatus           sts = MFX_ERR_NONE;
    bool                bErrIncompatibleVideoParams = false;
    CTimeInterval<>     decodeTimer(true);
    time_t start_time = time(0);
    MSDKThread * pDeliverThread = NULL;


    m_pDeliverOutputSemaphore = new Win32Semaphore(sts);
    m_pDeliveredEvent = new Win32Event(TRUE);
    pDeliverThread = new MSDKThread(sts, DeliverThreadFunc, this);
    if (!pDeliverThread || !m_pDeliverOutputSemaphore || !m_pDeliveredEvent)
	{
        MSDK_SAFE_DELETE(pDeliverThread);
        MSDK_SAFE_DELETE(m_pDeliverOutputSemaphore);
        MSDK_SAFE_DELETE(m_pDeliveredEvent);
        return MFX_ERR_MEMORY_ALLOC;
    }

	mfxSyncPoint      syncPoint = 0;
	mfxFrameSurface1* pOutSurface = NULL;
	IOFrameSurface *  pFreeSurf = NULL;

    while (((sts == MFX_ERR_NONE) || (MFX_ERR_MORE_DATA == sts) || (MFX_ERR_MORE_SURFACE == sts)) )
	{
        if (MFX_ERR_NONE != m_error) {
            msdk_printf(MSDK_STRING("DeliverOutput return error = %d\n"),m_error);
            break;
        }

		if (m_nFrames < m_output_count){
			break;
		}

		// read more bits
        if (pBitstream && ((MFX_ERR_MORE_DATA == sts) || (0==pBitstream->DataLength))) 
		{
            CAutoTimer timer_fread(m_tick_fread);
            mfxStatus status_read = m_FileReader->ReadNextFrame(pBitstream); // read more data to input bit stream

            if (MFX_ERR_MORE_DATA == status_read) {
				pBitstream = NULL;
            } else if (MFX_ERR_NONE != status_read) {
                break;
            }
        }

		// check if surface unlocked by decoder component
		recycleUsedSurface();

		// schedule more working frame
        if ((MFX_ERR_NONE == sts) || (MFX_ERR_MORE_SURFACE == sts))
		{
			pFreeSurf = dequeFreeSurface();
            if (!pFreeSurf)
			{
                // we stuck with no free surface available, now we will sync...
                mfxStatus sync_sts = syncOutSurface(MSDK_DEC_WAIT_INTERVAL);
                
				// sync queue is empty
				if (MFX_ERR_MORE_DATA == sync_sts) 
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
        }


		do 
		{
			pFreeSurf->submit = m_timer_overall.Sync();
			pOutSurface = NULL;
			syncPoint = 0;
			sts = m_pmfxDEC->DecodeFrameAsync(pBitstream, &(pFreeSurf->frame), &pOutSurface, &syncPoint);
			if (pBitstream && MFX_ERR_MORE_DATA == sts && pBitstream->MaxLength == pBitstream->DataLength)
			{
				mfxStatus status = ExtendMfxBitstream(pBitstream, pBitstream->MaxLength * 2);
				MSDK_CHECK_RESULT(status, MFX_ERR_NONE, status);
			}

			if (MFX_WRN_DEVICE_BUSY == sts)
			{
				mfxStatus sync_sts = syncOutSurface(MSDK_DEC_WAIT_INTERVAL);
				// note: everything except MFX_ERR_NONE are errors at this point
				switch (sync_sts)
				{
				case MFX_ERR_NONE:
				case MFX_ERR_MORE_DATA:
					sts = MFX_WRN_DEVICE_BUSY;
					break;
				default:
					sts = sync_sts;
					break;
				}
			}
		} while (MFX_WRN_DEVICE_BUSY == sts);

		if (sts > MFX_ERR_NONE) {
			// ignoring warnings...
			if (syncPoint && pOutSurface) {
				// output is available
				sts = MFX_ERR_NONE;
			} else {
				// output is not available
				sts = MFX_ERR_MORE_SURFACE;
			}
		} else if ((MFX_ERR_MORE_DATA == sts) && pBitstream) {
			continue;
		} else if ((MFX_ERR_MORE_DATA == sts) && !pBitstream) {
			// that's it - we reached end of stream; now we need to render bufferred data...
			do {
				sts = syncOutSurface(MSDK_DEC_WAIT_INTERVAL);
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
        
		// has one syncpoint
        if (MFX_ERR_NONE == sts) {

			IOFrameSurface * pSync = m_SurfaceMap[pOutSurface];
			pSync->syncPoint = syncPoint;
			m_SyncQueue.push(pSync);            
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

mfxStatus CDecodingPipeline::Run()
{
	mfxStatus sts = MFX_ERR_NONE; // return value check

    for (;;)
    {
        sts = RunDecoding();

        if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts || MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            if (MFX_ERR_INCOMPATIBLE_VIDEO_PARAM == sts)
            {
                msdk_printf(MSDK_STRING("\nERROR: Incompatible video parameters detected. Recovering...\n"));
            }
            else
            {
                msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned unexpected error. Recovering...\n"));
            }

            sts = ResetDecoder(&m_Params);
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_UNKNOWN);
            continue;
        }
        else
        {
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, MFX_ERR_UNKNOWN);
            break;
        }
    }

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

    const msdk_char* sMemType =  MSDK_STRING("d3d") ;
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
