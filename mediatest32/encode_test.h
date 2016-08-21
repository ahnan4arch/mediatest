
#pragma once

#include <string>
#include "encode_pipeline.h"
#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "decode_pipeline.h"
#include "va_interface.h"
#include "dxva_processor.h"

using namespace WinRTCSDK;
using namespace MP;

class  EncodeTest
{
public:
	EncodeTest ():dispatcher_("EncodeTest"),params_(),pipeline_(), processor_(NULL)
	{
	}

	void Init( DXVAProcessor* processor, IBitstreamSink* pSink,
		int w, int h, int frNumerator, int frDenomerator, bool loopBack)
	{
		processor_ = processor;
		pSink_ = pSink;


		// default implementation
		params_.bUseHWLib = true;
		params_.nAsyncDepth = 1;
		params_.nGopRefDist=1;
		params_.CodecId = MFX_CODEC_AVC;
		params_.nTargetUsage = MFX_TARGETUSAGE_BALANCED;
		params_.nDstWidth =w;
		params_.nDstHeight =h;
		params_.nFrameRateExtN = frNumerator;
		params_.nFrameRateExtD = frDenomerator;
		
		params_.bLoopBack = true;

		// calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
		mfxF64 dFrameRate = CalculateFrameRate (params_.nFrameRateExtN, params_.nFrameRateExtD);

		params_.nBitRate = CalculateDefaultBitrate(params_.CodecId, 
			params_.nTargetUsage, params_.nDstWidth,
			params_.nDstHeight, dFrameRate);

		wcscpy_s(params_.strSrcFile, L"d:\\test_en.yuv");
		wcscpy_s(params_.strDstFile, L"d:\\test_en.h264");


	}

	void UnInit()
	{
		processor_ =NULL;
		pSink_ = NULL;
	}


	void Input (void * pSurface/*d3d9surface*/)
	{
		pipeline_.Input(pSurface);
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

		msdk_printf(MSDK_STRING("Encoding started\n"));
		
		mfxStatus sts = pipeline_.Init(&params_, processor_, pSink_);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

		// print stream info
		pipeline_.PrintInfo();

		pipeline_.Run();
		msdk_printf(MSDK_STRING("\nEncoding finished\n"));
		return 0;
	}

private:
	Looper              dispatcher_;
    EncInitParams       params_;   // input parameters from command line
    CEncodingPipeline   pipeline_; // pipeline for decoding, includes input file reader, decoder and output file writer
	
	DXVAProcessor*      processor_;
	IBitstreamSink*     pSink_;  // for loopbabck test
};
