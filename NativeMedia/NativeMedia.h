#pragma once
/////////////////////////////////////////////////////////////////////
//  This is the public managed interface for UI to access Native media

#include "VideoHWND.h"
#include "VideoManager.h"
#include "AudioManager.h"
#include "WinRTCSDKTypes.h"

using System::String;
using System::Runtime::InteropServices::Marshal;
using System::Collections::Generic::Dictionary;
using System::Collections::Generic::Queue;
using System::Collections::Generic::List;

namespace WinRTCSDK
{

//////////////////////////////////////////////////////////////////////
// following  Methods in this class must be called by main UI thread 
// (not the service handler):
//    Startup GetVideoElement GetVideoElement4Camera
public ref class NativeMedia
{
public:
	// NativeMedia is a singleton
	static NativeMedia ^ GetInstance () ;
	~NativeMedia(void);
	!NativeMedia(void);

public: // video elements (hwndhost)
	FrameworkElement ^ GetVideoElement(String ^ videoId);
	FrameworkElement ^ GetVideoElement4Camera();

public: // public interface
	MediaInitResult Startup(INativeMediaCallbacks^ callback);
	void  Shutdown();
	
	void  StartVideoCapture(String ^ sourceID, int w, int h, int frameRate);
	void  StopVideoCapture(String ^ sourceID);
	void  StartAudioCapture(String ^  sourceID);
	void  StartAudioRender(String ^  sourceID);
	void  StopAudioCapture();
	void  StopAduioRender();
	float GetCapturePeakMeter(); // Audio meter for Microphone
	float GetRenderPeakMeter (); // Audio meter for Speaker

	// mute operation
	void SetVideoMute(bool bMute);
	int GetSpeakerVolume();/*[0, 100]*/
	void SetSpeakerVolume( int volume/*[0, 100]*/);
	void SetSpeakerMute( bool bMute);
	void SetMicMute(bool bMute);

	// For video stream changing
	void  UpdateVideoStreams (array<VideoStreamInfo^> ^ videoList);

	// device lists
	array<MediaDevInfoManaged^> ^ GetDevList (MediaDevType t);

	///////////////////////////////////////////////////////////////
	// choose device.
	// when useDefault is true:
	//     1) symbolicLink is ignored. 
	//     2) if t is audio device, default communication dev used.
	//     3) if t is camera, the first camera is used.
	//     4) if device lost, device lost event is fired
	void  ChooseDev(MediaDevType t, String ^ symbolicLink, bool useDefault);

private:
	NativeMedia(void);

	///////////////////////////////////////////////////////////////
	// Data marshalling
	MediaDevInfoManaged ^ _MediaDevInfo_N2M(MediaDevInfo&info);
	array<MediaDevInfoManaged^> ^ _MediaDevInfoList_N2M(std::vector<MediaDevInfo> &list);

private: // Managed data member
	Queue < int >                         ^videoHwndFreeList_;
	Dictionary<String ^ , int>            ^videoHwndMap_;
	array <VideoHWND^>                    ^videoHwndPool_;

	String                                ^cameraSymbolicLink_;
	static NativeMedia                    ^instance_ = nullptr;
	static int                             maxVideoNumber_ = 13;

private:  // Native members
	NativeVideoHWND                       *nativeVideoHWND_;
	VideoManager                          *videoManager_;
	AudioManager                          *audioManager_;
};

}