#pragma once

#include <stdint.h>

namespace MP{

enum VideoFrameType{
	SOFT_I420,
	SOFT_NV12,
	D3D9_SURFACE,
	MFX_FRAME_SURFACE
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
	virtual void * getHandle() = 0; // device manager 
	virtual void * getFrameAllocator() =0; 
};


}