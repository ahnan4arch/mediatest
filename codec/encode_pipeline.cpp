
#include "stdafx.h"

#include "encode_pipeline.h"


CEncTaskPool::CEncTaskPool()
{
    m_pTasks  = NULL;
    m_pmfxSession       = NULL;
    m_nTaskBufferStart  = 0;
    m_nPoolSize         = 0;
}

CEncTaskPool::~CEncTaskPool()
{
    Close();
}

mfxStatus CEncTaskPool::Init(MFXVideoSession* pmfxSession, CSmplBitstreamWriter* pWriter,IBitstreamSink *pSink,
							 mfxU32 nPoolSize, mfxU32 nBufferSize )
{
    MSDK_CHECK_POINTER(pmfxSession, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pWriter, MFX_ERR_NULL_PTR);

    MSDK_CHECK_ERROR(nPoolSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);
    MSDK_CHECK_ERROR(nBufferSize, 0, MFX_ERR_UNDEFINED_BEHAVIOR);

    m_pmfxSession = pmfxSession;
	m_pWriter = pWriter;
	m_pBitstreamSink = pSink;
    m_nPoolSize = nPoolSize;

    m_pTasks = new EncTask [m_nPoolSize];
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_MEMORY_ALLOC);

    mfxStatus sts = MFX_ERR_NONE;
    for (mfxU32 i = 0; i < m_nPoolSize; i++)
    {
        sts = m_pTasks[i].Init(nBufferSize);
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    }

    return MFX_ERR_NONE;
}

mfxStatus CEncTaskPool::SynchronizeFirstTask()
{
    m_statOverall.StartTimeMeasurement();
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_POINTER(m_pmfxSession, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts  = MFX_ERR_NONE;

    // non-null sync point indicates that task is in execution
    if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
    {
        sts = m_pmfxSession->SyncOperation(m_pTasks[m_nTaskBufferStart].EncSyncP, MSDK_WAIT_INTERVAL);

        if (MFX_ERR_NONE == sts)
        {
            m_statFile.StartTimeMeasurement();

			if ( m_pBitstreamSink ){ // loopback test
				m_pBitstreamSink->PutBittream(&m_pTasks[m_nTaskBufferStart].mfxBS);
			}else{
				if (m_pWriter)
				{
					sts = m_pWriter->WriteNextFrame(&m_pTasks[m_nTaskBufferStart].mfxBS);
				}
			}

            m_statFile.StopTimeMeasurement();
            MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

            m_pTasks[m_nTaskBufferStart].Reset();

            // move task buffer start to the next executing task
            // the first transform frame to the right with non zero sync point
            for (mfxU32 i = 0; i < m_nPoolSize; i++)
            {
                m_nTaskBufferStart = (m_nTaskBufferStart + 1) % m_nPoolSize;
                if (NULL != m_pTasks[m_nTaskBufferStart].EncSyncP)
                {
                    break;
                }
            }
        }

        return sts;
    }
    else
    {
        sts = MFX_ERR_NOT_FOUND; // no tasks left in task buffer
    }
    m_statOverall.StopTimeMeasurement();
    return sts;
}

mfxU32 CEncTaskPool::GetFreeTaskIndex()
{
    mfxU32 off = 0;

    if (m_pTasks)
    {
        for (off = 0; off < m_nPoolSize; off++)
        {
            if (NULL == m_pTasks[(m_nTaskBufferStart + off) % m_nPoolSize].EncSyncP)
            {
                break;
            }
        }
    }

    if (off >= m_nPoolSize)
        return m_nPoolSize;

    return (m_nTaskBufferStart + off) % m_nPoolSize;
}

mfxStatus CEncTaskPool::GetFreeTask(EncTask **ppTask)
{
    MSDK_CHECK_POINTER(ppTask, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pTasks, MFX_ERR_NOT_INITIALIZED);

    mfxU32 index = GetFreeTaskIndex();

    if (index >= m_nPoolSize)
    {
        return MFX_ERR_NOT_FOUND;
    }

    // return the address of the task
    *ppTask = &m_pTasks[index];

    return MFX_ERR_NONE;
}

void CEncTaskPool::Close()
{
    if (m_pTasks)
    {
        for (mfxU32 i = 0; i < m_nPoolSize; i++)
        {
            m_pTasks[i].Close();
        }
    }

    MSDK_SAFE_DELETE_ARRAY(m_pTasks);

    m_pmfxSession = NULL;
    m_nTaskBufferStart = 0;
    m_nPoolSize = 0;
}

mfxStatus CEncodingPipeline::InitMfxEncParams(EncInitParams *pInParams)
{
    m_mfxEncParams.mfx.CodecId                 = pInParams->CodecId;
    m_mfxEncParams.mfx.TargetUsage             = pInParams->nTargetUsage; // trade-off between quality and speed
    m_mfxEncParams.mfx.TargetKbps              = pInParams->nBitRate; // in Kbps
    m_mfxEncParams.mfx.RateControlMethod       = MFX_RATECONTROL_VBR;
	
	m_mfxEncParams.mfx.GopRefDist = pInParams->nGopRefDist;

    m_mfxEncParams.mfx.FrameInfo.FrameRateExtN = pInParams->nFrameRateExtN;
    m_mfxEncParams.mfx.FrameInfo.FrameRateExtD = pInParams->nFrameRateExtD;

    m_mfxEncParams.mfx.EncodedOrder            = 0; // binary flag, 0 signals encoder to take frames in display order
	
    // specify memory type
    m_mfxEncParams.IOPattern = MFX_IOPATTERN_IN_VIDEO_MEMORY;

    // frame info parameters
    m_mfxEncParams.mfx.FrameInfo.FourCC       = MFX_FOURCC_NV12;
    m_mfxEncParams.mfx.FrameInfo.ChromaFormat = MFX_CHROMAFORMAT_YUV420;
    m_mfxEncParams.mfx.FrameInfo.PicStruct    = MFX_PICSTRUCT_PROGRESSIVE;

    // set frame size and crops
    // width must be a multiple of 16
    // height must be a multiple of 16 in case of frame picture and a multiple of 32 in case of field picture
    m_mfxEncParams.mfx.FrameInfo.Width  = MSDK_ALIGN16(pInParams->nDstWidth);
    m_mfxEncParams.mfx.FrameInfo.Height = MSDK_ALIGN16(pInParams->nDstHeight) ;

    m_mfxEncParams.mfx.FrameInfo.CropX = 0;
    m_mfxEncParams.mfx.FrameInfo.CropY = 0;
    m_mfxEncParams.mfx.FrameInfo.CropW = pInParams->nDstWidth;
    m_mfxEncParams.mfx.FrameInfo.CropH = pInParams->nDstHeight;

    if (!m_EncExtParams.empty())
    {
        m_mfxEncParams.ExtParam = &m_EncExtParams[0]; // vector is stored linearly in memory
        m_mfxEncParams.NumExtParam = (mfxU16)m_EncExtParams.size();
    }

    m_mfxEncParams.AsyncDepth = pInParams->nAsyncDepth;

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocFrames()
{
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;
    mfxFrameAllocRequest EncRequest;

    MSDK_ZERO_MEMORY(EncRequest);

    // Calculate the number of surfaces for components.
    // QueryIOSurf functions tell how many surfaces are required to produce at least 1 output.
    // To achieve better performance we provide extra surfaces.
    // 1 extra surface at input allows to get 1 extra output.
    sts = m_pmfxENC->QueryIOSurf(&m_mfxEncParams, &EncRequest);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    if (EncRequest.NumFrameSuggested < m_mfxEncParams.AsyncDepth)
        return MFX_ERR_MEMORY_ALLOC;

    // prepare allocation requests
    EncRequest.NumFrameSuggested = EncRequest.NumFrameMin = EncRequest.NumFrameSuggested;
    MSDK_MEMCPY_VAR(EncRequest.Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));

    // alloc frames for encoder
    sts = m_pMFXAllocator->Alloc(m_pMFXAllocator->pthis, &EncRequest, &m_EncResponse);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // prepare mfxFrameSurface1 array for encoder
    m_pEncSurfaces = new mfxFrameSurface1 [m_EncResponse.NumFrameActual];
    MSDK_CHECK_POINTER(m_pEncSurfaces, MFX_ERR_MEMORY_ALLOC);

    for (int i = 0; i < m_EncResponse.NumFrameActual; i++)
    {
        memset(&(m_pEncSurfaces[i]), 0, sizeof(mfxFrameSurface1));
        MSDK_MEMCPY_VAR(m_pEncSurfaces[i].Info, &(m_mfxEncParams.mfx.FrameInfo), sizeof(mfxFrameInfo));
		m_pEncSurfaces[i].Data.MemId = m_EncResponse.mids[i];

    }

	return MFX_ERR_NONE;
}

void CEncodingPipeline::DeleteFrames()
{
    // delete surfaces array
    MSDK_SAFE_DELETE_ARRAY(m_pEncSurfaces);

    // delete frames
    if (m_pMFXAllocator)
    {
        m_pMFXAllocator->Free(m_pMFXAllocator->pthis, &m_EncResponse);
    }
}

CEncodingPipeline::CEncodingPipeline()
{
    m_pmfxENC = NULL;
    m_pMFXAllocator = NULL;
    m_pEncSurfaces = NULL;

    MSDK_ZERO_MEMORY(m_CodingOption);
    m_CodingOption.Header.BufferId = MFX_EXTBUFF_CODING_OPTION;
    m_CodingOption.Header.BufferSz = sizeof(m_CodingOption);

    MSDK_ZERO_MEMORY(m_CodingOption2);
    m_CodingOption2.Header.BufferId = MFX_EXTBUFF_CODING_OPTION2;
    m_CodingOption2.Header.BufferSz = sizeof(m_CodingOption2);


    MSDK_ZERO_MEMORY(m_mfxEncParams);

    MSDK_ZERO_MEMORY(m_EncResponse);
}

CEncodingPipeline::~CEncodingPipeline()
{
    Close();
}

mfxStatus CEncodingPipeline::Init(EncInitParams *pParams, MP::IDXVAVideoProcessor* pDXVAProcessor, IBitstreamSink *pSink)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(pDXVAProcessor, MFX_ERR_NULL_PTR);

	m_Params = *pParams;
	m_pDXVAProcessor = pDXVAProcessor;
	m_pBitstreamSink = pSink;

    mfxStatus sts = MFX_ERR_NONE;

    // prepare input/output files
	if (! m_Params.bLoopBack)
	{
		sts = m_FileReader.Init(pParams->strSrcFile, MFX_FOURCC_YV12 , 1, std::vector<msdk_char*>());
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

		// prepare output file writer
		sts = m_FileWriter.Init( pParams->strDstFile);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	}

	// prepare session param
    mfxInitParam initPar;

    MSDK_ZERO_MEMORY(initPar);

    // we set version to 1.0 and later we will query actual version of the library which will got leaded
    initPar.Version.Major = 1;
    initPar.Version.Minor = 0;
    initPar.GPUCopy = MFX_GPUCOPY_DEFAULT;

    // Init session
    initPar.Implementation = MFX_IMPL_HARDWARE_ANY;
    initPar.Implementation |= MFX_IMPL_VIA_D3D9;
    sts = m_mfxSession.InitEx(initPar);

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = MFXQueryVersion(m_mfxSession , &m_Version); // get real API version of the loaded library
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
	
	// set device handle
	mfxHDL hdl = (mfxHDL)m_pDXVAProcessor->getHandle();
	mfxHandleType hdl_t = MFX_HANDLE_D3D9_DEVICE_MANAGER;
	sts = m_mfxSession.SetHandle(hdl_t, hdl);
	MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// set frame allocator
	m_pMFXAllocator = (mfxFrameAllocator*)m_pDXVAProcessor->getFrameAllocator();
	sts = m_mfxSession.SetFrameAllocator(m_pMFXAllocator);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// init encoder params
    sts = InitMfxEncParams(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // create encoder
    m_pmfxENC = new MFXVideoENCODE(m_mfxSession);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_MEMORY_ALLOC);

    sts = ResetMFXComponents(pParams);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);



    return MFX_ERR_NONE;
}

void CEncodingPipeline::Close()
{
	if(!m_Params.bLoopBack)
	{
        msdk_printf(MSDK_STRING("Frame number: %u\r\n"), m_FileWriter.m_nProcessedFramesNum);
        mfxF64 ProcDeltaTime = m_statOverall.GetDeltaTime() - m_statFile.GetDeltaTime() - m_TaskPool.GetFileStatistics().GetDeltaTime();
        msdk_printf(MSDK_STRING("Encoding fps: %.0f"), m_FileWriter.m_nProcessedFramesNum / ProcDeltaTime);
	}

    MSDK_SAFE_DELETE(m_pmfxENC);


    DeleteFrames();


    m_TaskPool.Close();
    m_mfxSession.Close();

	if(!m_Params.bLoopBack)
	{
		m_FileReader.Close();
		m_FileWriter.Close();
	}

}

mfxStatus CEncodingPipeline::ResetMFXComponents(EncInitParams* pParams)
{
    MSDK_CHECK_POINTER(pParams, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    sts = m_pmfxENC->Close();
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_INITIALIZED);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // free allocated frames
    DeleteFrames();

    m_TaskPool.Close();

    sts = AllocFrames();
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    sts = m_pmfxENC->Init(&m_mfxEncParams);
    if (MFX_WRN_PARTIAL_ACCELERATION == sts)
    {
        msdk_printf(MSDK_STRING("WARNING: partial acceleration\n"));
        MSDK_IGNORE_MFX_STS(sts, MFX_WRN_PARTIAL_ACCELERATION);
    }

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

	// now init the task pool
    mfxU32 nEncodedDataBufferSize = m_mfxEncParams.mfx.FrameInfo.Width * m_mfxEncParams.mfx.FrameInfo.Height * 4;
    
	sts = m_TaskPool.Init(&m_mfxSession, &m_FileWriter,m_pBitstreamSink,
		m_mfxEncParams.AsyncDepth, nEncodedDataBufferSize);

    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::AllocateSufficientBuffer(mfxBitstream* pBS)
{
    MSDK_CHECK_POINTER(pBS, MFX_ERR_NULL_PTR);
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxVideoParam par;
    MSDK_ZERO_MEMORY(par);

    // find out the required buffer size
    mfxStatus sts = m_pmfxENC->GetVideoParam(&par);
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // reallocate bigger buffer for output
    sts = ExtendMfxBitstream(pBS, par.mfx.BufferSizeInKB * 1000);
    MSDK_CHECK_RESULT_SAFE(sts, MFX_ERR_NONE, sts, WipeMfxBitstream(pBS));

    return MFX_ERR_NONE;
}

mfxStatus CEncodingPipeline::GetFreeTask(EncTask **ppTask)
{
    mfxStatus sts = MFX_ERR_NONE;

    sts = m_TaskPool.GetFreeTask(ppTask);
    if (MFX_ERR_NOT_FOUND == sts)
    {
        sts = m_TaskPool.SynchronizeFirstTask();
        MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

        // try again
        sts = m_TaskPool.GetFreeTask(ppTask);
    }

    return sts;
}

mfxStatus CEncodingPipeline::Run()
{
	m_StopFlag = false;

    while(!m_StopFlag)
    {
        mfxStatus sts = RunEncoding();

        if (MFX_ERR_DEVICE_LOST == sts || MFX_ERR_DEVICE_FAILED == sts)
        {
            msdk_printf(MSDK_STRING("\nERROR: Hardware device was lost or returned an unexpected error. Recovering...\n"));

            sts = ResetMFXComponents(&m_Params);
            if (sts != MFX_ERR_NONE) return  MFX_ERR_UNKNOWN;
            continue;
        }
        else
        {
            if (sts != MFX_ERR_NONE) return  MFX_ERR_UNKNOWN;
            break;
        }
    }

	Close();
	return MFX_ERR_NONE;

}

// input captured frame
bool  CEncodingPipeline::Input (void * pSurface/*d3d9surface*/)
{
	if ( !m_Params.bLoopBack) return false; // not in loopback mode, data is input from a file
	if (!pSurface) return false;
	if ( !m_pDXVAProcessor) return false;

	MP::VideoFrameData src;
	MP::VideoFrameData dst;
	src.type = MP::D3D9_SURFACE;
	src.pSurface = pSurface;

	mfxU16 nEncSurfIdx =MSDK_INVALID_SURF_IDX;

	// find free surface for encoder input
	nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
	if (nEncSurfIdx == MSDK_INVALID_SURF_IDX) return false;

	mfxFrameSurface1* pSurf = NULL; // dispatching pointer
	// point pSurf to encoder surface
	pSurf = &m_pEncSurfaces[nEncSurfIdx];

	dst.type = MP::MFX_FRAME_SURFACE;
	dst.pSurface = pSurf;

	bool ret = m_pDXVAProcessor->videoProcessBlt(&src, &dst);
		m_ReadyQueue.put(nEncSurfIdx);
	if (ret){ // add surface to ready queue

	}
	return true;
}
	
void  CEncodingPipeline::Stop ()
{ 
	m_ReadyQueue.put(0);
	m_StopFlag = true;
}

mfxU16  CEncodingPipeline::GetNextSurface ()
{
	mfxU16 nEncSurfIdx =MSDK_INVALID_SURF_IDX;
	if ( !m_Params.bLoopBack)
	{ // not in loopback mode, data is input from a file

		// find free surface for encoder input
		nEncSurfIdx = GetFreeSurface(m_pEncSurfaces, m_EncResponse.NumFrameActual);
		MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);

		mfxFrameSurface1* pSurf = NULL; // dispatching pointer
		// point pSurf to encoder surface
		pSurf = &m_pEncSurfaces[nEncSurfIdx];

		// get YUV pointers
		mfxStatus sts = m_pMFXAllocator->Lock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
		if (MFX_ERR_NONE > sts) return nEncSurfIdx;

		m_statFile.StartTimeMeasurement();
		sts = m_FileReader.LoadNextFrame(pSurf);
		m_statFile.StopTimeMeasurement();

		// after we're done call Unlock
		sts = m_pMFXAllocator->Unlock(m_pMFXAllocator->pthis, pSurf->Data.MemId, &(pSurf->Data));
	}
	else // loopback mode
	{
		// dequeue one surface from the ready queue.
		// and it is a blockingqueue, so calling thread may be blocked
		m_ReadyQueue.get(nEncSurfIdx);
	
	}

	return nEncSurfIdx;

}

mfxStatus CEncodingPipeline::RunEncoding()
{
    m_statOverall.StartTimeMeasurement();
    MSDK_CHECK_POINTER(m_pmfxENC, MFX_ERR_NOT_INITIALIZED);

    mfxStatus sts = MFX_ERR_NONE;

    mfxFrameSurface1* pSurf = NULL; // dispatching pointer

    EncTask *pCurrentTask = NULL; // a pointer to the current task
    mfxU16 nEncSurfIdx = 0;     // index of free surface for encoder input (vpp output)

    sts = MFX_ERR_NONE;

    // main loop, preprocessing and encoding
    while (MFX_ERR_NONE <= sts || MFX_ERR_MORE_DATA == sts)
    {
		if ( m_StopFlag ) break; // stop the loop

        // get a pointer to a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

		//////////////////////////////////////////////////
		// Get next frame to encode
        nEncSurfIdx = GetNextSurface();

		// a fake item in blocking queue may be used to unblock this thread!
		if ( m_StopFlag ) break; // stop the loop

        MSDK_CHECK_ERROR(nEncSurfIdx, MSDK_INVALID_SURF_IDX, MFX_ERR_MEMORY_ALLOC);


        for (;;)
        {
            // at this point surface for encoder contains either a frame from file or a frame processed by vpp
            sts = m_pmfxENC->EncodeFrameAsync(NULL, &m_pEncSurfaces[nEncSurfIdx], &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else
            {
                // get next surface and new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }
    }

    // means that the input file has ended, need to go to buffering loops
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // loop to get buffered frames from encoder
    while (MFX_ERR_NONE <= sts)
    {
        // get a free task (bit stream and sync point for encoder)
        sts = GetFreeTask(&pCurrentTask);
        MSDK_BREAK_ON_ERROR(sts);

        for (;;)
        {
            sts = m_pmfxENC->EncodeFrameAsync(NULL, NULL, &pCurrentTask->mfxBS, &pCurrentTask->EncSyncP);

            if (MFX_ERR_NONE < sts && !pCurrentTask->EncSyncP) // repeat the call if warning and no output
            {
                if (MFX_WRN_DEVICE_BUSY == sts)
                    MSDK_SLEEP(1); // wait if device is busy
            }
            else if (MFX_ERR_NONE < sts && pCurrentTask->EncSyncP)
            {
                sts = MFX_ERR_NONE; // ignore warnings if output is available
                break;
            }
            else if (MFX_ERR_NOT_ENOUGH_BUFFER == sts)
            {
                sts = AllocateSufficientBuffer(&pCurrentTask->mfxBS);
                MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
            }
            else
            {
                // get new task for 2nd bitstream in ViewOutput mode
                MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_BITSTREAM);
                break;
            }
        }
        MSDK_BREAK_ON_ERROR(sts);
    }

    // MFX_ERR_MORE_DATA is the correct status to exit buffering loop with
    // indicates that there are no more buffered frames
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_MORE_DATA);
    // exit in case of other errors
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);

    // synchronize all tasks that are left in task pool
    while (MFX_ERR_NONE == sts)
    {
        sts = m_TaskPool.SynchronizeFirstTask();
    }

    // MFX_ERR_NOT_FOUND is the correct status to exit the loop with
    // EncodeFrameAsync and SyncOperation don't return this status
    MSDK_IGNORE_MFX_STS(sts, MFX_ERR_NOT_FOUND);
    // report any errors that occurred in asynchronous part
    MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, sts);
    m_statOverall.StopTimeMeasurement();
    return sts;
}

void CEncodingPipeline::PrintInfo()
{
    msdk_printf(MSDK_STRING("Encoding Sample Version %s\n"), MSDK_SAMPLE_VERSION);
    msdk_printf(MSDK_STRING("\nInput file format\t%s\n"), ColorFormatToStr(m_FileReader.m_ColorFormat));
    msdk_printf(MSDK_STRING("Output video\t\t%s\n"), CodecIdToStr(m_mfxEncParams.mfx.CodecId).c_str());

    mfxFrameInfo DstPicInfo = m_mfxEncParams.mfx.FrameInfo;

    msdk_printf(MSDK_STRING("Destination picture:\n"));
    msdk_printf(MSDK_STRING("\tResolution\t%dx%d\n"), DstPicInfo.Width, DstPicInfo.Height);
    msdk_printf(MSDK_STRING("\tCrop X,Y,W,H\t%d,%d,%d,%d\n"), DstPicInfo.CropX, DstPicInfo.CropY, DstPicInfo.CropW, DstPicInfo.CropH);

    msdk_printf(MSDK_STRING("Frame rate\t%.2f\n"), DstPicInfo.FrameRateExtN * 1.0 / DstPicInfo.FrameRateExtD);
    msdk_printf(MSDK_STRING("Bit rate(Kbps)\t%d\n"), m_mfxEncParams.mfx.TargetKbps);
    msdk_printf(MSDK_STRING("Gop size\t%d\n"), m_mfxEncParams.mfx.GopPicSize);
    msdk_printf(MSDK_STRING("Ref dist\t%d\n"), m_mfxEncParams.mfx.GopRefDist);
    msdk_printf(MSDK_STRING("Ref number\t%d\n"), m_mfxEncParams.mfx.NumRefFrame);
    msdk_printf(MSDK_STRING("Idr Interval\t%d\n"), m_mfxEncParams.mfx.IdrInterval);
    msdk_printf(MSDK_STRING("Target usage\t%s\n"), TargetUsageToStr(m_mfxEncParams.mfx.TargetUsage));

    const msdk_char* sMemType = MSDK_STRING("d3d");
    msdk_printf(MSDK_STRING("Memory type\t%s\n"), sMemType);

    mfxIMPL impl;
    m_mfxSession.QueryIMPL(&impl);

    const msdk_char* sImpl = (MFX_IMPL_VIA_D3D11 == MFX_IMPL_VIA_MASK(impl)) ? MSDK_STRING("hw_d3d11")
                     : (MFX_IMPL_HARDWARE & impl)  ? MSDK_STRING("hw")
                                                   : MSDK_STRING("sw");
    msdk_printf(MSDK_STRING("Media SDK impl\t\t%s\n"), sImpl);

    mfxVersion ver;
    m_mfxSession.QueryVersion(&ver);
    msdk_printf(MSDK_STRING("Media SDK version\t%d.%d\n"), ver.Major, ver.Minor);

    msdk_printf(MSDK_STRING("\n"));
}
