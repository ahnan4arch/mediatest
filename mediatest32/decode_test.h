#pragma once

#include <string>
#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "decode_pipeline.h"
#include "dxva_renderer.h"

using namespace WinRTCSDK;
using namespace MP;

class  DecodeTest
{
public:
	DecodeTest ():dispatcher_("DecodeTest"),params_(),pipeline_(),render_(NULL)
	{
	}
	
	void Init(int viewWidth, int viewHeight, IBitstreamSource* pBSSource)
	{
		pBSSource_ = pBSSource;
		render_ = new DXVARender(viewWidth,viewHeight);
		render_->Init();

		params_.bLoopback = true;

		params_.bUseHWLib = true;
		params_.videoType = MFX_CODEC_AVC;
		params_.nAsyncDepth = 5;
		params_.nMaxFPS=40;
		wcscpy_s(params_.strSrcFile, L"d:\\test_dec.h264");
		wcscpy_s(params_.strDstFile, L"d:\\test_dec.nv12");
	}

	void UnInit()
	{
		pBSSource_ = NULL;
		render_->UnInit();
		delete render_;
	}


	void Start ()
	{
		dispatcher_.startUp();
		LooperTask t = [=](){ TestProc();};
		dispatcher_.runAsyncTask(t);
	}

	void Stop()
	{
		pipeline_.Stop();
	}

	void Join()
	{
		dispatcher_.stopAndJoin();
	}

private:

	int TestProc()
	{

		msdk_printf(MSDK_STRING("Decoding started\n"));

		
		mfxStatus sts = pipeline_.Init(&params_, render_);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

		// print stream info
		pipeline_.PrintInfo();

		pipeline_.Run();
		msdk_printf(MSDK_STRING("\nDecoding finished\n"));
		return 0;
	}

private:
	Looper              dispatcher_;
    DecInitParams       params_;   // input parameters from command line
    CDecodingPipeline   pipeline_; // pipeline for decoding, includes input file reader, decoder and output file writer
	DXVARender  *       render_;
	IBitstreamSource*   pBSSource_;
};
