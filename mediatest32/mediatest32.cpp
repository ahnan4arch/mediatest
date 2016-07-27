// mediatest32.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>

#include "D3D9Renderer.h"
using namespace WinRTCSDK;

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


BYTE _image [320*240*3/2];
int _tmain(int argc, _TCHAR* argv[])
{
//	testCompiler();
	BaseWnd * dummyWnd_ = new BaseWnd(TRUE);
	dummyWnd_->Create(0, L"DUMMYD3DRENDER", WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, NULL, 0, NULL);
	::ShowWindow(*dummyWnd_, SW_SHOW);
	BaseWnd * dummyWnd1_ = new BaseWnd(TRUE);
	dummyWnd1_->Create(0, L"DUMMYD3DRENDER", WS_OVERLAPPEDWINDOW, 0, 0, 400, 400, NULL, 0, NULL);
	::ShowWindow(*dummyWnd1_, SW_SHOW);

	D3D9Renderer render;
	render.Create();
	render.CreateVideo("testvid", *dummyWnd_);
	render.CreateVideo("testvid1", *dummyWnd1_);
	VideoFrameInfo frame;
	frame.width = 320;
	frame.height = 240;
	frame.image = _image;
	::memset(_image, 0, sizeof(_image));
	for ( int j=0; j<240; j++)
	{
		for ( int i=0; i< 320; i++){
			if ( i<160)
				_image[i+j*320]=30;
			else 
				_image[i+j*320]= 64;
			if ( j<120) _image[i+j*320]+=30;
			else  _image[i+j*320] -= 30;
		}
	}

	BYTE * pU = &_image[240*320];
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

	BYTE * pV = &_image[240*320*5/4];
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
	render.DrawVideo("testvid", frame);
	render.DrawVideo("testvid1", frame);
	getchar();
	return 0;
}

