// mediatest32.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>

#include "Looper.h"
#include "D3D9Renderer.h"

#include "camera.h"
#include "decode_test.h"
#include "encode_test.h"
#include "loopback_pipeline.h"

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


int testDecode()
{
	DecodeTest decodeTest;
	decodeTest.init();
	decodeTest.start();
	messagePump();
	decodeTest.stop();
	return 0;
}

int testEncode()
{
	EncodeTest encodeTest;
	encodeTest.init();
	encodeTest.start();
	messagePump();
	encodeTest.stop();
	return 0;
}

int testCamera()
{
	Camera cam(1280,720);
	cam.Init();
	vector<MediaDevInfo> devList = cam.GetCameraList();
	cam.OpenCamera(devList[0].symbolicLink);
	messagePump();
	cam.CloseCamera();
	cam.UnInit();
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{

//	return testDecode();
//	return testEncode();
	return testCamera();
}

