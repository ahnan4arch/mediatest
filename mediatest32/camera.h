#pragma once

#include <string>
#include "va_interface.h"
#include "MFVidCapture.h"
#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "DeviceEvents.h"

using namespace WinRTCSDK;
using namespace MP;

struct ICameraSink 
{
	virtual bool PutSurface (VideoFrameInfo & frame) = 0;
};

class Camera : public IVideoCaptureCallback
{
public:
	Camera(int w, int h):render_(),width_(w),height_(h)
	{
	}

	bool Init()
	{
		videoWindow_ = new BaseWnd(TRUE);
		videoWindow_->Create(0, L"CameraView", WS_OVERLAPPEDWINDOW, 0, 0, width_, height_, NULL, 0, NULL);
		::ShowWindow(*videoWindow_, SW_SHOW);

		render_.Create();
		render_.CreateVideo("testvid", *videoWindow_);

		// camera using DXVA now
		vidCap_ = new MFVidCapture(this);
		vidCap_->SetD3DDevManager(render_.GetD3DDeviceManager9("testvid"));

		return true;
		
	}

	bool UnInit()
	{
		delete videoWindow_;
		delete vidCap_;
		return true;
	}

	bool AddSink (ICameraSink* sink)
	{
		WinRTCSDK::AutoLock l (m_lock);
		m_SinkList.push_back(sink);
		return true;
	}

	bool RemoveSink(ICameraSink* sink)
	{
		WinRTCSDK::AutoLock l (m_lock);
		auto pred = [=](ICameraSink* item)->bool { return item == sink ;};
		std::remove_if(m_SinkList.begin(), m_SinkList.end(), pred);
		return true;
	}


public:
	virtual _com_ptr_IDirect3DDeviceManager9 GetDeviceManager() 
	{ 
		return render_.GetD3DDeviceManager9("testvid");
	}

	// IVideoCaptureCallback
	void OnVideoCaptureData(VideoFrameInfo & frame) 
	{
		bool ret;
		HRESULT hr;

		static int cnt = 0;
		cnt++;

		if ( frame.pSample == NULL
			&& frame.image == NULL) return;

		// draw preview
		hr = render_.DrawVideo("testvid", frame);

		// feed to loopback pipelines
		ret = FeedToSink(frame);
	
	}


	vector<MediaDevInfo> GetCameraList()
	{
		return vidCap_->GetDevList();
	}
	void OpenCamera(std::wstring devid)
	{
		vidCap_->OpenCamera(devid);
		return;
	}

	void CloseCamera()
	{
		vidCap_->CloseCamera();
	}

private:

	bool FeedToSink(VideoFrameInfo & frame)
	{
		WinRTCSDK::AutoLock l (m_lock);
		int n = m_SinkList.size();
		for (  int i=0; i<n; i++)
		{
			m_SinkList[i]->PutSurface(frame);
		}

		return true;
	}

private:

	WinRTCSDK::Mutex m_lock;
	std::vector<ICameraSink*> m_SinkList;

	D3D9Renderer     render_;
	MFVidCapture    *vidCap_;
	BaseWnd         *videoWindow_ ;
	int width_;
	int height_;

};

