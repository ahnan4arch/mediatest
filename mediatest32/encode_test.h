
#pragma once

#include "stdafx.h"

#include <string>
#include "encode_pipeline.h"
#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "decode_pipeline.h"
#include "va_interface.h"

using namespace WinRTCSDK;
using namespace MP;

class DXVAProcessor : public MP::IDXVAVideoProcessor
{
public:
	DXVAProcessor(int w, int h):render_(),width_(w),height_(h)
	{
	}
	bool Init()
	{
		videoWindow_ = new BaseWnd(TRUE);
		videoWindow_->Create(0, L"Camera", WS_OVERLAPPEDWINDOW, 0, 0, width_, height_, NULL, 0, NULL);
		::ShowWindow(*videoWindow_, SW_SHOW);

		render_.Create();
		render_.CreateVideo("testvid", *videoWindow_);

		allocator_ = new D3DFrameAllocator();

		allocator_->Init(render_.GetD3DDeviceManager9("testvid"));
		return true;
		
	}
	bool UnInit()
	{
		delete allocator_;
		delete videoWindow_;
	}
public:
	virtual bool drawFrame(VideoFrameData * pFrame) 
	{
		if ( MFX_FRAME_SURFACE != pFrame->type ) return false;
		VideoFrameInfo frame;
		mfxFrameSurface1 * pFrameSurf = (mfxFrameSurface1 *)pFrame->pSurface;
		mfxHDL pSurf;
		allocator_->GetHDL(allocator_->pthis, pFrameSurf->Data.MemId, &pSurf);
		frame.pSample = ( IDirect3DSurface9*)pSurf;
		render_.DrawVideo("testvid", frame);
		return true;
	}

	virtual void * getHandle() 
	{ 
		return render_.GetD3DDeviceManager9("testvid");
	}

	virtual void * getFrameAllocator() 
	{
		return (mfxFrameAllocator*)allocator_;
	}

private:
	D3D9Renderer render_;
	BaseWnd * videoWindow_ ;
	D3DFrameAllocator * allocator_;
	int width_;
	int height_;

};

class  EncodeTest
{
public:
	EncodeTest ():dispatcher_("DecodeTest"),params_(),pipeline_(), processor_(1280,720)
	{
	}
	void init()
	{
		processor_.Init();
	}
	void UnInit()
	{
		processor_.UnInit();
	}
	int TestProc()
	{

		msdk_printf(MSDK_STRING("Encoding started\n"));
		SetupParam (&params_);
		
		mfxStatus sts = pipeline_.Init(&params_, &processor_);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

		// print stream info
		pipeline_.PrintInfo();

		pipeline_.Run();
		msdk_printf(MSDK_STRING("\nEncoding finished\n"));
		return 0;
	}

	void start ()
	{
		dispatcher_.startUp();
		LooperTask t = [=](){ TestProc();};
		dispatcher_.runAsyncTask(t);
	}

	void stop()
	{
		dispatcher_.stopAndJoin();
	}

private:

	void SetupParam(EncodeParams *pParams )
	{
		// default implementation
		pParams->bUseHWLib = true;
		pParams->nDstWidth =1280;
		pParams->nDstHeight =720;
		pParams->bUseHWLib = true;
		pParams->nAsyncDepth = 4;
		pParams->nBitRate =0;
		pParams->nGopRefDist=1;
		pParams->CodecId = MFX_CODEC_AVC;
		pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
		pParams->nFrameRateExtN = 30;
		pParams->nFrameRateExtD = 1;


		// calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
		mfxF64 dFrameRate = CalculateFrameRate (pParams->nFrameRateExtN, pParams->nFrameRateExtD);
		pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, pParams->nTargetUsage, pParams->nDstWidth,
			pParams->nDstHeight, dFrameRate);


		wcscpy_s(params_.strSrcFile, L"d:\\test_en.yuv");
		wcscpy_s(params_.strDstFile, L"d:\\test_en.h264");

	}

private:

	Looper              dispatcher_;
    EncodeParams        params_;   // input parameters from command line
    CEncodingPipeline   pipeline_; // pipeline for decoding, includes input file reader, decoder and output file writer
	DXVAProcessor       processor_;

};
