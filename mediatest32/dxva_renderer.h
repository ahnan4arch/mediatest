#pragma once

#include <string>
#include "va_interface.h"

#include "D3D9Renderer.h"
#include "d3d_allocator.h"

using namespace WinRTCSDK;
using namespace MP;

class DXVARender : public MP::IDXVAVideoRender
{
public:
	DXVARender(int w, int h, std::wstring title):render_(),width_(w),height_(h),title_(title)
	{
	}
	bool Init()
	{
		videoWindow_ = new BaseWnd(TRUE);
		videoWindow_->Create(0, title_.c_str(), WS_OVERLAPPEDWINDOW, 0, 0, width_, height_, NULL, 0, NULL);
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
	std::wstring title_;
	int width_;
	int height_;

};