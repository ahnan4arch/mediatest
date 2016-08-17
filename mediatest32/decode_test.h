#include "stdafx.h"
#include <string>
#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "decode_pipeline.h"
#include "va_interface.h"


using namespace WinRTCSDK;
using namespace MP;

class DXVARender : public MP::IDXVAVideoRender
{
public:
	DXVARender(int w, int h):render_(),width_(w),height_(h)
	{
	}
	bool Init()
	{
		videoWindow_ = new BaseWnd(TRUE);
		videoWindow_->Create(0, L"DUMMYD3DRENDER", WS_OVERLAPPEDWINDOW, 0, 0, width_, height_, NULL, 0, NULL);
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
	int width_;
	int height_;

};
class  DecodeTest
{
public:
	DecodeTest ():dispatcher_("DecodeTest"),params_(),pipeline_(),render_(1280,720)
	{
	}
	void init()
	{
		render_.Init();
	}
	void UnInit()
	{
		render_.UnInit();
	}
	int TestProc()
	{

		msdk_printf(MSDK_STRING("Decoding started\n"));

		params_.bUseHWLib = true;
		params_.videoType = MFX_CODEC_AVC;
		params_.nAsyncDepth = 5;
		params_.nMaxFPS=40;
		wcscpy_s(params_.strSrcFile, L"d:\\test.h264");
		wcscpy_s(params_.strDstFile, L"d:\\test.nv12");
		
		mfxStatus sts = pipeline_.Init(&params_, &render_);
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
		LooperTask t = [=](){ TestProc();};
		dispatcher_.runAsyncTask(t);
	}
	void stop()
	{
		dispatcher_.stopAndJoin();
	}
private:
	Looper              dispatcher_;
    DecodeParams        params_;   // input parameters from command line
    CDecodingPipeline   pipeline_; // pipeline for decoding, includes input file reader, decoder and output file writer
	DXVARender          render_;

};
