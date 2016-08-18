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

class Camera : public IVideoCaptureCallback, public IDeviceEventCallback
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

		// device event manager
		deviceEvents_ = new DeviceEvents(this);
		deviceEvents_->Startup();

		return true;
		
	}

	bool UnInit()
	{
		delete videoWindow_;
		delete deviceEvents_;
		delete vidCap_;
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
		D3DSURFACE_DESC desc;

		static int cnt = 0;
		cnt++;

		if ( frame.pSample == NULL
			&& frame.image == NULL) return;

		// draw preview
		render_.DrawVideo("testvid", frame);
		return;

		// put video frame to data store (in RTCSDK), for software encoder	
		//if ( frame.pSample )
		//{
		//	frame.pSample->GetDesc(&desc);
		//	switch (desc.Format)
		//	{
		//	case D3DFMT_YUY2:
		//	case D3DFMT_I420:
		//		break;
		//	default:
		//		return; // format not supported
		//	}
		//}
	
	}

	// IDeviceEventCallback. This callback is response to WM_DEVICECHANGE, so don't do much thing here
	void OnVideoCapDevChanged (UINT nEventType, DWORD_PTR dwData  )
	{
		DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR *)dwData;
		if(!pHdr) return;
		if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE ) return ;

		DEV_BROADCAST_DEVICEINTERFACE *pDi = (DEV_BROADCAST_DEVICEINTERFACE *)dwData;
		std::wstring  name = &pDi->dbcc_name[0];
		switch (nEventType)
		{
		case DBT_DEVICEARRIVAL://   A device has been inserted and is now available.
			// fire the event to GUI
			break;
		case DBT_DEVICEQUERYREMOVE://   Permission to remove a device is requested. 
			break;
		case DBT_DEVICEQUERYREMOVEFAILED://   Request to remove a device has been canceled.
			break;
		case DBT_DEVICEREMOVEPENDING ://  Device is about to be removed. Cannot be denied.
			break;
		case DBT_DEVICEREMOVECOMPLETE ://  Device has been removed.
			// fire the event to GUI
			break;
		case DBT_DEVICETYPESPECIFIC ://  Device-specific event.
			break;
		case DBT_CONFIGCHANGED ://  Current configuration has changed.
			break;
		}
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
	D3D9Renderer     render_;
	MFVidCapture    *vidCap_;
	DeviceEvents    *deviceEvents_;
	BaseWnd         *videoWindow_ ;
	int width_;
	int height_;

};