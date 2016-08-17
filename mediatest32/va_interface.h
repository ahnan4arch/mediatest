#pragma once

#include <stdint.h>

namespace MP{

enum VideoFrameType{
	SOFT_I420,
	SOFT_NV12,
	D3D9_SURFACE,
	MFX_FRAME_SURFACE /* surface type is mfxFrameSurface1* of IntelMediaSDK */
};

struct VideoFrameData
{
	VideoFrameType type;
	int width;
	int height;
	int rotation;
	int stride0;
	union {
		void* pSurface;
		void* pData0;
	};
	int stride1;
	void* pData1;
	int stride2;
	void* pData2;
};

struct IVideoRender 
{
	virtual bool drawFrame(VideoFrameData * pFrame) = 0;
};

struct IDXVAVideoRender : public IVideoRender
{
	// teturn type : mfxHDL of IntelMediaSDK
	// handle type : MFX_HANDLE_D3D9_DEVICE_MANAGER
	virtual void * getHandle() = 0; 

	// return type : mfxFrameAllocator* of IntelMediaSDK
	virtual void * getFrameAllocator() =0;  
};


struct IVideoProcessor 
{
	virtual bool videoProcessBlt(VideoFrameData * pSrc, VideoFrameData * pTarget) = 0;
};

struct IDXVAVideoProcessor : public IVideoProcessor
{
	// teturn type : mfxHDL of IntelMediaSDK
	// handle type : MFX_HANDLE_D3D9_DEVICE_MANAGER
	virtual void * getHandle() = 0; 

	// return type : mfxFrameAllocator* of IntelMediaSDK
	virtual void * getFrameAllocator() =0;  
};

}