
#pragma once

#include "stdafx.h"

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
	EncodeTest ():dispatcher_("DecodeTest"),params_(),pipeline_(), processor_(NULL)
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
		pParams->nAsyncDepth = 4;
		pParams->nGopRefDist=1;
		pParams->CodecId = MFX_CODEC_AVC;
		pParams->nTargetUsage = MFX_TARGETUSAGE_BALANCED;
		pParams->nFrameRateExtN = 30;
		pParams->nFrameRateExtD = 1;


		// calculate default bitrate based on the resolution (a parameter for encoder, so Dst resolution is used)
		mfxF64 dFrameRate = CalculateFrameRate (pParams->nFrameRateExtN, pParams->nFrameRateExtD);

		pParams->nBitRate = CalculateDefaultBitrate(pParams->CodecId, 
			pParams->nTargetUsage, pParams->nDstWidth,
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
