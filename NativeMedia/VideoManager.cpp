#include "stdafx.h"
#include "ComPtrDefs.h"
#include "VideoManager.h"
#include "GlobalFun.h"
#include "NativeMediaLog.h"

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

namespace WinRTCSDK{

VideoManager::VideoManager(INativeMediaCallbacks ^ callbacks, HWND hwndPreview) :
	previewID_("capture_preview"),
	hwndPreview_(hwndPreview),
	mutex_(), 
	render_(NULL), 
	vidCap_(NULL),
	dispatcher_ ("VideoManagerDispatcher", true),
	videoBlitter_ ( [=]() { _DrawVideo();} ),
	renderIsResetting_(false),
	useDefaultCamera_(true),
	cameraId_ (L""),
	cameraRunning_(false),
	nativeMediaCallbacks_(callbacks)
{
}

VideoManager::~VideoManager()
{
}

void  VideoManager::StartVideoCapture(string sourceID, VideoCaptureParam &p)
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		streamCaptureMap_[sourceID] = p;
	};
	dispatcher_.runAsyncTask(t);
}

void  VideoManager::StopVideoCapture(string sourceID)
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		streamCaptureMap_.erase(sourceID);
	};
	dispatcher_.runAsyncTask(t);
}

void  VideoManager::AddVideoStream(string sourceID, VideoRenderParam& p)
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		streamRenderList_[sourceID] = p;
		render_->CreateVideo(sourceID, p.hwnd);
	};
	dispatcher_.runAsyncTask(t);
}
	
void VideoManager::RemoveVideoStream(string sourceID)
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		streamRenderList_.erase(sourceID);
		render_->RemoveVideo(sourceID);
	};
	dispatcher_.runAsyncTask(t);
}

void VideoManager::UpdateVideoStream(string sourceID, VideoRenderParam& p)
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		streamRenderList_[sourceID] = p;
	};
	dispatcher_.runAsyncTask(t);
}

void VideoManager::_RemoveAllVideoStreams()
{
	LooperTask t = [=]()
	{
		AutoLock l (mutex_);
		map<string, VideoRenderParam>::iterator i = streamRenderList_.begin();
		for ( ; i!=streamRenderList_.end(); i++) render_->RemoveVideo(i->first);
		streamRenderList_.clear();
	};
	dispatcher_.runAsyncTask(t);
}

// IVideoCaptureCallback
void VideoManager::OnVideoCaptureData(VideoFrameInfo & frame) 
{
	bool ret;
	HRESULT hr;
	D3DSURFACE_DESC desc;

	static int cnt = 0;
	cnt++;

	if ( frame.pSample == NULL
		&& frame.image == NULL) return;

	// draw preview
	render_->DrawVideo(previewID_, frame);
	return;
	map<string, VideoCaptureParam>  capMap;
	{
		AutoLock l (mutex_);
		capMap = streamCaptureMap_;
	}

	// put video frame to data store (in RTCSDK), for software encoder
	std::map<string, VideoCaptureParam>::iterator i = capMap.begin();
	if ( i == capMap.end() ) return;
	
	if ( frame.pSample )
	{
		frame.pSample->GetDesc(&desc);
		switch (desc.Format)
		{
		case D3DFMT_YUY2:
		case D3DFMT_I420:
			break;
		default:
			return; // format not supported
		}
		D3DLOCKED_RECT lr;
		hr = frame.pSample->LockRect(&lr, NULL, D3DLOCK_NOSYSLOCK);
		if (SUCCEEDED(hr)){
			ret = DS_PutVideoData(desc.Format, i->first, lr.pBits, lr.Pitch, desc.Width, desc.Height);
			frame.pSample->UnlockRect();
		}
	}
	else if ( frame.image != NULL)
	{
		ret = DS_PutVideoData(frame.format, i->first, frame.image
			, frame.stride, frame.width, frame.height);
	}
	
}

// IDeviceEventCallback. This callback is response to WM_DEVICECHANGE, so don't do much thing here
void VideoManager::OnVideoCapDevChanged (UINT nEventType, DWORD_PTR dwData  )
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
		this->nativeMediaCallbacks_->onDeviceChanged(MediaDevType::eVideoCapture, MediaDevEvent::eNewDevArrived);
		break;
	case DBT_DEVICEQUERYREMOVE://   Permission to remove a device is requested. 
		break;
	case DBT_DEVICEQUERYREMOVEFAILED://   Request to remove a device has been canceled.
		break;
	case DBT_DEVICEREMOVEPENDING ://  Device is about to be removed. Cannot be denied.
		break;
	case DBT_DEVICEREMOVECOMPLETE ://  Device has been removed.
		// fire the event to GUI
		this->nativeMediaCallbacks_->onDeviceChanged(MediaDevType::eVideoCapture, MediaDevEvent::eDevRemoved);
		BOOL bDevLost;
		vidCap_->CheckDeviceLost(name, &bDevLost);
		if ( bDevLost)
		{
			// must call this->CloseCamera. 
			// this->CloseCamera will also remove the preview stream inside the video render
			CloseCamera();
			// fire the event to GUI
			this->nativeMediaCallbacks_->onDeviceChanged(MediaDevType::eVideoCapture, MediaDevEvent::eCurrentDevLost);
		}
		break;
	case DBT_DEVICETYPESPECIFIC ://  Device-specific event.
		break;
	case DBT_CONFIGCHANGED ://  Current configuration has changed.
		break;
	}
}

int VideoManager::Init()
{
	int iRet = 0;

	LooperTask t = [=, &iRet]()
	{
		render_ = new D3D9Renderer;
		HRESULT hr = render_->Create();
		if(FAILED(hr))
		{
			NativeMediaLogger::log(LogLevel::Error, "VideoManager", "Init(): failed to Create D3D9 Render.");
			iRet = -1;
		}

		// camera using DXVA now
		vidCap_ = new MFVidCapture(this);

		// device event manager
		deviceEvents_ = new DeviceEvents(this);
		deviceEvents_->Startup();

		// kick-off the video blitter
		videoBlitter_.Startup();

		// create preview video
		render_->CreateVideo(previewID_, hwndPreview_);


	}; // end lambda

	dispatcher_.runSyncTask(t);
	
	return iRet;
}

int VideoManager::UnInit()
{
	// if there are video streams, remove them first
	_RemoveAllVideoStreams();
	// if a camera is opened, close it first!
	CloseCamera();
	// Also remove the video buffer for cameraPreview in the render!
	render_->RemoveVideo(previewID_);
	
	// shutdown video blitter
	videoBlitter_.Shutdown();

	// shutdown device event monitor
	deviceEvents_->Shutdown();

	// delete all sub objects
	LooperTask t = [=]()
	{
		delete vidCap_; // must delete capture before render
		delete render_;
		delete deviceEvents_;
		vidCap_ = NULL;
		render_ = NULL;
		deviceEvents_ = NULL;
	};

	dispatcher_.runSyncTask(t);
	
	return 0;
}

// the video blitter run this method at fixed pace
DWORD  VideoManager::_DrawVideo ()
{
	map<string, VideoRenderParam> list;
	{
		// just make a copy of the list
		AutoLock l (mutex_);
		list = streamRenderList_;
	}

	map<string, VideoRenderParam>::iterator i;
	int cnt = list.size(); // for debugging
	for ( i = list.begin(); i != list.end(); i++)
	{
		bool ret;
		void * buff;
		int len, width, height, rotation;
		VideoRenderParam p = i->second;
		ret = DS_GetVideoData(i->first,&buff, len, width, height, rotation);
		const char * srcid = i->first.c_str(); // for debugging
		if ( ret )
		{
			VideoFrameInfo frame;
			frame.format = D3DFMT_I420;
			frame.image = (BYTE*)buff;
			frame.width = width;
			frame.height = height;
			frame.rotation = rotation;
			// need mute icon
			frame.bMicMuteIcon = p.bAudioMuted;

			render_->DrawVideo(i->first, frame);
			DS_FreeVideoData(&buff);
		}
	}

	return 0;
}
	
std::vector<MediaDevInfo> VideoManager::GetVideoCaptureDevList ()
{
	std::vector<MediaDevInfo> list;
	LooperTask t = [=, &list]()
	{
		list = vidCap_->GetDevList();
	};
	// local var list is passed by ref, so must await the task to be finished ...	
	dispatcher_.runSyncTask(t);
	return list;
}

// check if the camera running. restart it using new divice id if running.
void  VideoManager::_ResetCamera()
{
	// check if running
	if ( !cameraRunning_ ) return;
	if (useDefaultCamera_)
	{
		std::vector<MediaDevInfo> list;
		list = vidCap_->GetDevList();
		if (list.size()>0) cameraId_=list[0].symbolicLink;
	}
	if ( !cameraId_.empty())
	{
		// close old one
		vidCap_->CloseCamera();
		vidCap_->SetD3DDevManager(NULL);

		vidCap_->SetD3DDevManager( render_->GetD3DDeviceManager9(previewID_));
		vidCap_->OpenCamera(cameraId_);
	}
}

void VideoManager::ChooseCamera(std::wstring symbolicLink, bool useDefault)
{
	LooperTask t = [=]()
	{
		useDefaultCamera_ = useDefault;
		cameraId_ = symbolicLink;
		// reset camera
		_ResetCamera();
	};
	dispatcher_.runAsyncTask(t);

}

void VideoManager::OpenCamera ()
{
	LooperTask t = [=]()
	{
		// set running flag
		cameraRunning_ = true;
		// reset the camera
		_ResetCamera();
	};
	dispatcher_.runAsyncTask(t);

}

void VideoManager::CloseCamera ()
{
	LooperTask t = [=]()
	{
		vidCap_->CloseCamera();
		vidCap_->SetD3DDevManager(NULL);
		cameraRunning_ = false;
	};
	dispatcher_.runAsyncTask(t);
}

void VideoManager::SetMicMuteIcon(BYTE* argb, int w, int h)
{
	render_->SetMicMuteIcon(argb, w, h);
}
}// namespace WinRTCSDK