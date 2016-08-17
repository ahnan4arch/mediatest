// mediatest32.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>

#include "Looper.h"
#include "D3D9Renderer.h"
#include "decode_test.h"

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
void make_fake_image ( BYTE * image);

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
	DecodeTest decodeTest;
	decodeTest.init();
	decodeTest.start();
	messagePump();
	decodeTest.stop();
	return 0;
}

