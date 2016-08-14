// mediatest32.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>

#include "Looper.h"
#include "D3D9Renderer.h"
#include "decode_pipeline.h"

using namespace WinRTCSDK;
using namespace MP;

#pragma comment (lib, "avrt.lib")
#pragma comment (lib, "comsuppw.lib ")

#pragma comment (lib, "gdi32.lib")
#pragma comment (lib, "d3d9.lib")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mf.lib")
#pragma comment (lib, "Mfuuid.lib")
#pragma comment (lib, "Strmiids.lib")

#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")
#pragma comment (lib, "shlwapi.lib")
#pragma comment (lib, "comctl32.lib")
#pragma comment (lib, "Dxva2.lib")

#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "dwmapi.lib")

BYTE _image [320*240*3/2];

void make_fake_image ( BYTE * image)
{
	::memset(image, 0, sizeof(image));
	for ( int j=0; j<240; j++)
	{
		for ( int i=0; i< 320; i++){
			if ( i<160)
				image[i+j*320]=30;
			else 
				image[i+j*320]= 64;
			if ( j<120) image[i+j*320]+=30;
			else  image[i+j*320] -= 30;
		}
	}

	BYTE * pU = &image[240*320];
	for ( int j=0; j<120; j++)
	{
		for ( int i=0; i< 160; i++){
			if ( i<80)
				pU[i+j*160]=40;
			else 
				pU[i+j*160]= 50;

			if ( j<60)
				pU[i+j*160]+=10;
			else 
				pU[i+j*160] -= 20;
		}
	}

	BYTE * pV = &image[240*320*5/4];
	for ( int j=0; j<120; j++)
	{
		for ( int i=0; i< 160; i++){
			if ( i<80)
				pV[i+j*160]=160;
			else 
				pV[i+j*160]= 150;

			if ( j<60)
				pV[i+j*160]+=20;
			else 
				pV[i+j*160] -= 20;
		}
	}
}

void testFakeImage()
{
	BaseWnd * dummyWnd_ = new BaseWnd(TRUE);
	dummyWnd_->Create(0, L"DUMMYD3DRENDER", WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, NULL, 0, NULL);
	::ShowWindow(*dummyWnd_, SW_SHOW);

	D3D9Renderer render;
	render.Create();
	render.CreateVideo("testvid", *dummyWnd_);
	VideoFrameInfo frame;
	frame.width = 320;
	frame.height = 240;
	frame.image = _image;

	render.DrawVideo("testvid", frame);
}

class  DecodeTest
{
public:
	DecodeTest ():dispatcher_("DecodeTest"),params_(),pipeline_()
	{
	}
	int TestProc()
	{

		msdk_printf(MSDK_STRING("Decoding started\n"));

		params_.bUseHWLib = true;
		params_.videoType = MFX_CODEC_AVC;
		params_.nAsyncDepth = 4;

		mfxStatus sts = pipeline_.Init(&params_, NULL);
		MSDK_CHECK_RESULT(sts, MFX_ERR_NONE, 1);

		// print stream info
		pipeline_.PrintInfo();

		pipeline_.Run();
		msdk_printf(MSDK_STRING("\nDecoding finished\n"));
		return 0;
	}

	void start ()
	{
		dispatcher_.startUp();
	}
	void stop()
	{
		dispatcher_.stopAndJoin();
	}
private:
	Looper              dispatcher_;
    sInputParams        params_;   // input parameters from command line
    CDecodingPipeline   pipeline_; // pipeline for decoding, includes input file reader, decoder and output file writer

};


INT messagePump()
{
	MSG msg;
	while(::GetMessage(&msg, NULL, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
//	testFakeImage();


	messagePump();
	return 0;
}

