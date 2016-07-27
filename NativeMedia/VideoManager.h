#pragma once

#include <vcclr.h>

#include "basewnd.h"
#include "Looper.h"
#include "mfVidcapture.h"
#include "d3d9renderer.h"
#include "DeviceEvents.h"
#include "NativeMediaCallbacks.h"

namespace WinRTCSDK{

struct VideoCaptureParam
{
	int width;
	int height;
	int frameRate;
};

struct VideoRenderParam
{
	VideoRenderParam():hwnd(NULL), bAudioMuted(false){}
	// the render window, when in windowed mode
	HWND     hwnd;
	bool     bAudioMuted;
};

class VideoBlitter 
{
public: // ctor/dtor
	VideoBlitter(LooperTask job):
		stopFlag_(false),dispatcher_("VideoBlitter", false), job_(job){}
	~VideoBlitter() {}

public: // public methods
	void Startup ()
	{
		LooperTask t = [=]() { _blitter_run(); };
		// kick off the loop
		stopFlag_ = false;
		dispatcher_.startUp();
		dispatcher_.runAsyncTask(t);
	}

	void Shutdown (){
		stopFlag_ = true;
		dispatcher_.stopAndJoin();
	}

private:
	void _blitter_run()
	{
		// use MMCSS
		HANDLE mmcssHandle = NULL;
		DWORD mmcssTaskIndex = 0;
		mmcssHandle = ::AvSetMmThreadCharacteristics(L"Playback ", &mmcssTaskIndex);
		if (mmcssHandle != NULL){
			BOOL ret = ::AvSetMmThreadPriority(mmcssHandle,AVRT_PRIORITY_HIGH );
		}

		DWORD timestampBeforeJob , timestampAfterJob; 
		while ( !stopFlag_) 
		{
			timestampBeforeJob = ::GetTickCount();
			job_(); // execute the job
			timestampAfterJob = ::GetTickCount();
			DWORD duration = timestampBeforeJob-timestampAfterJob;
			if ( duration < 33) {
				::Sleep( 33 - (duration));
			}
		}
		if ( mmcssHandle){
			::AvRevertMmThreadCharacteristics(mmcssHandle);
		}
	};

private: 
	AtomicVar<bool>   stopFlag_;
	Looper            dispatcher_;
	LooperTask        job_;
};

class VideoManager : public IVideoCaptureCallback, public IDeviceEventCallback
{

public:
	VideoManager(INativeMediaCallbacks ^ callbacks, HWND hwndPreview);
	~VideoManager();

public: // interfaces
	//IVideoCaptureCallback, called by an internal thread of the MFVidCapture
	virtual void OnVideoCaptureData(VideoFrameInfo & pSample);

	//IDeviceEventCallback
	virtual void OnVideoCapDevChanged (UINT nEventType, DWORD_PTR dwData  );

public:
	void  StartVideoCapture(string sourceID, VideoCaptureParam &p);
	void  StopVideoCapture(string sourceID);

	void  AddVideoStream(string sourceID, VideoRenderParam& p);
	void  RemoveVideoStream(string sourceID);
	
	// only mute state changed, we just update the state
	// without removing/adding the stream again
	void  UpdateVideoStream(string sourceID, VideoRenderParam& p);

	// initialization
	int Init();
	int UnInit();

	// device list
	std::vector<MediaDevInfo> GetVideoCaptureDevList ();

	// camera operation
	void ChooseCamera(std::wstring symbolicLink, bool useDefault= true);
	void OpenCamera ();
	void CloseCamera ();

	// set mute icon
	void SetMicMuteIcon(BYTE* argb, int w, int h);

private: // internal functions
	void  _RemoveAllVideoStreams();
	DWORD _DrawVideo (); // run by videoBliter_
	void  _ResetCamera();

private:
	const string     previewID_;
	HWND             hwndPreview_;
	Mutex            mutex_;
	D3D9Renderer    *render_;
	MFVidCapture    *vidCap_;
	DeviceEvents    *deviceEvents_;

	// requested streams to capture
	map<string, VideoCaptureParam>  streamCaptureMap_;
	// streams to render
	map<string, VideoRenderParam>  streamRenderList_;
	
	//dispatcher_ £º this MediaFoundation enabled thread is used to run camera related methods
	Looper            dispatcher_;

	// video blitter: a dedicate thread to draw video at 30fps
	VideoBlitter      videoBlitter_;

	// display lost and trying to recreate
	AtomicVar<bool>   renderIsResetting_; 

	// data members used to restart camera
	// camera restart occurs when device (either capture or render) is changed(added/removed)
	bool              useDefaultCamera_;
	std::wstring      cameraId_;
	AtomicVar<bool>   cameraRunning_;

	// managed callback
	gcroot<INativeMediaCallbacks ^>  nativeMediaCallbacks_;

};

}// namespace WinRTCSDK