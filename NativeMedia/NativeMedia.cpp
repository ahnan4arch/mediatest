#include "stdafx.h"
#include "NativeMedia.h"
#include "ResourceHelper.h"
#include <msclr\lock.h>

using namespace System;

namespace WinRTCSDK
{

static std::string   MarshalString ( String ^ s);
static std::wstring  MarshalStringW( String ^ s);

// NativeMedia is a singleton
NativeMedia ^ NativeMedia::GetInstance () 
{
	if (instance_ == nullptr ){
		instance_ = gcnew NativeMedia();
	}
	return instance_;
}

NativeMedia::NativeMedia(void)
{	
	nativeVideoHWND_ = nullptr;
	videoManager_ = nullptr;
	audioManager_ = nullptr;
}

NativeMedia::~NativeMedia(void)
{
}

NativeMedia::!NativeMedia(void)
{
}

FrameworkElement ^ NativeMedia::GetVideoElement(String ^ videoId)
{
	msclr::lock l(this);
	if ( videoId == nullptr) return nullptr;
	int idx;
	if ( videoHwndMap_->ContainsKey(videoId) ){
		bool ret = videoHwndMap_->TryGetValue(videoId, idx);
		if ( ret ){
			return videoHwndPool_[idx];
		}
	}
	return nullptr;
}

FrameworkElement ^ NativeMedia::GetVideoElement4Camera()
{
	return videoHwndPool_[maxVideoNumber_-1];
}

MediaInitResult NativeMedia::Startup(INativeMediaCallbacks^ callback)
{			
	// prepare video window list
	nativeVideoHWND_ = new NativeVideoHWND(maxVideoNumber_);
	videoHwndPool_ = gcnew array <VideoHWND^> (maxVideoNumber_);
	videoHwndFreeList_ = gcnew Queue <int> ;
	for ( int i = 0; i< maxVideoNumber_; i++)
	{
		videoHwndPool_[i] = gcnew VideoHWND(nativeVideoHWND_, i);
	}
	for ( int i = 0; i< maxVideoNumber_-1/*the last one is used for camera*/; i++)
	{
		videoHwndFreeList_->Enqueue(i);
	}

	// mapping video source to videoHwnd 
	videoHwndMap_ = gcnew Dictionary<String ^ , int> ();

	// video manager
	// use the last HWND for camera preview
	videoManager_ = new VideoManager(callback, (HWND)(void*)videoHwndPool_[maxVideoNumber_-1]->GetHwnd());
	if (videoManager_->Init() != 0)
	{
		return MediaInitResult::ENMEDIA_INIT_VIDEO_FAILED;
	}

	// audio manager
	audioManager_ = new AudioManager(callback);
	if (audioManager_->Init() != 0)
	{
		return MediaInitResult::ENMEDIA_INIT_AUDIO_FAILED;
	}

	// setup mute icon
	int w, h;
//	IntPtr ptr =ResourceHelper::GetBitmapDataById(w, h, gcnew String("imgOpenMicBtnNorm"));
	IntPtr ptr =ResourceHelper::GetBitmapDataById(w, h, gcnew String("imgMicMuteOnVideo"));
	if ( ptr != IntPtr::Zero){
		videoManager_->SetMicMuteIcon((BYTE*)ptr.ToPointer(), w, h);
		Marshal::FreeHGlobal(ptr);
	}

	return MediaInitResult::ENMEDIA_INIT_OK;
}

void NativeMedia::Shutdown()
{
	// delete window list
	for ( int i = 0; i< maxVideoNumber_; i++)
	{
		videoHwndPool_[i]->ClearNative();
	}
	if (nativeVideoHWND_)
	{
		delete nativeVideoHWND_;
	}

	// video
	if (videoManager_)
	{
		videoManager_->UnInit();
		delete videoManager_;
		videoManager_ = nullptr;
	}
	// audio
	if (audioManager_)
	{
		audioManager_->UnInit();
		delete audioManager_ ;
		audioManager_ = nullptr;
	}
}

void NativeMedia::StartVideoCapture(String ^ sourceID, int w, int h, int frameRate)
{
	std::string sid = MarshalString (sourceID);
	VideoCaptureParam p;
	p.width = w;
	p.height = h;
	p.frameRate = frameRate;
	videoManager_->StartVideoCapture(sid, p);
}

void  NativeMedia::StopVideoCapture(String ^ sourceID)
{
	std::string sid = MarshalString (sourceID);
	videoManager_->StopVideoCapture(sid);
}

void  NativeMedia::UpdateVideoStreams (array<VideoStreamInfo^> ^ videoList)
{
	msclr::lock l(this);
	List <VideoStreamInfo^> ^items2Add = gcnew List <VideoStreamInfo^>;
	List <VideoStreamInfo^> ^items2Update = gcnew List <VideoStreamInfo^>;
	List <String ^> ^items2Del = gcnew List <String ^>;
	int n = videoList->Length;

	// find streams to add/update
	for (int i=0; i<n; i++ )
	{
		if ( videoHwndMap_->ContainsKey(videoList[i]->sourceID) == false){
			items2Add->Add(videoList[i]);
		}else{
			items2Update->Add(videoList[i]);
		}
	}

	// find streams to del
	for each (String ^id in videoHwndMap_->Keys)
	{
		bool toDel=true;
		for (int i=0; i<n; i++ ){
			if ( id->Equals(videoList[i]->sourceID)){
				toDel = false;
				break;
			}
		}
		if ( toDel){
			items2Del->Add(id);
		}
	}

	// delete obsolete streams
	for each (String ^ id in items2Del)
	{
		int hwndkey=-1;
		if (videoHwndMap_->TryGetValue(id, hwndkey)){
			// add hwnd to free list
			videoHwndFreeList_->Enqueue(hwndkey);
		}
		// remove the stream from video->hwnd map
		videoHwndMap_->Remove(id);
		// remove the stream from Native videoManager
		videoManager_->RemoveVideoStream(MarshalString(id));
	}

	// add new streams
	for each ( VideoStreamInfo^ info in items2Add)
	{
		int hwndkey=-1;
		if ( videoHwndFreeList_->Count == 0){ // error state!
			break;
		}
		hwndkey = videoHwndFreeList_->Dequeue();
		videoHwndMap_->Add(info->sourceID, hwndkey);
		VideoRenderParam  p;
		p.bAudioMuted = info->bAudioMuted;
		p.hwnd = (HWND)(void*)videoHwndPool_[hwndkey]->GetHwnd();
		videoManager_->AddVideoStream( MarshalString(info->sourceID), p);
	}

	// update streams
	for each ( VideoStreamInfo^ info in items2Update)
	{
		int hwndkey=-1;
		videoHwndMap_->TryGetValue(info->sourceID, hwndkey);
		VideoRenderParam  p;
		p.bAudioMuted = info->bAudioMuted;
		p.hwnd = (HWND)(void*)videoHwndPool_[hwndkey]->GetHwnd();
		videoManager_->UpdateVideoStream( MarshalString(info->sourceID), p);
	}

}

void  NativeMedia::StartAudioCapture(String ^  sourceID)
{
	std::string sid = MarshalString (sourceID);
	audioManager_->StartAudioCapture(sid);
}

void  NativeMedia::StartAudioRender(String ^  sourceID)
{
	std::string sid  = MarshalString (sourceID);
	audioManager_->StartAudioRender(sid);
}

void  NativeMedia::StopAudioCapture()
{
	audioManager_->StopAudioCapture();
}

void  NativeMedia::StopAduioRender()
{
	audioManager_->StopAudioRender();
}
	
float NativeMedia::GetCapturePeakMeter()
{
	return audioManager_->GetCapturePeakMeter();
}

float NativeMedia::GetRenderPeakMeter ()
{
	return audioManager_->GetRenderPeakMeter();
}

//////////////////////////////////////////////////////////////////////
// mute operation
void NativeMedia::SetVideoMute(bool bMute)
{
	if ( bMute ){
		videoManager_->CloseCamera();
	}else{
		videoManager_->OpenCamera();
	}
}
	
int NativeMedia::GetSpeakerVolume()/*[0, 100]*/
{
	return audioManager_->GetSpeakerVolume();
}

void NativeMedia::SetSpeakerVolume( int volume/*[0, 100]*/)
{
	audioManager_->SetSpeakerVolume(volume);
}

void NativeMedia::SetSpeakerMute( bool bMute)
{
	audioManager_->SetSpeakerMute(bMute);
}

void NativeMedia::SetMicMute(bool bMute)
{
	audioManager_->SetMicMute(bMute);
}

/////////////////////////////////////////////////////////////////////
// device list
array<MediaDevInfoManaged^> ^ NativeMedia::GetDevList (MediaDevType t)
{
	HRESULT hr =S_OK;
	std::vector<MediaDevInfo> list;
	switch (t)
	{
	case MediaDevType::eAudioCapture:
		hr = audioManager_->GetDevList(eCapture, list);
		break;
	case MediaDevType::eAudioRender:
		hr = audioManager_->GetDevList(eRender, list);
		break;
	case MediaDevType::eVideoCapture:
		list = videoManager_->GetVideoCaptureDevList();
		break;
	}
	return _MediaDevInfoList_N2M (list);
}

///////////////////////////////////////////////////////////////////////
// choose device
void  NativeMedia::ChooseDev(MediaDevType t, String ^ symbolicLink, bool useDefault)
{
	std::wstring id =  MarshalStringW(symbolicLink);
	switch (t)
	{
	case MediaDevType::eAudioCapture:
		audioManager_->ChooseCaptureDev(useDefault, id);
		break;
	case MediaDevType::eAudioRender:
		audioManager_->ChooseRenderDev(useDefault, id);
		break;
	case MediaDevType::eVideoCapture:
		videoManager_->ChooseCamera(id, useDefault);
		break;
	}
}

MediaDevInfoManaged ^ NativeMedia::_MediaDevInfo_N2M(MediaDevInfo&info)
{
	MediaDevInfoManaged ^  infoM = gcnew MediaDevInfoManaged();
	infoM->devFriendlyName = gcnew System::String (info.devFriendlyName.c_str());
	infoM->symbolicLink = gcnew System::String (info.symbolicLink.c_str());
	// audio dev only info
	infoM->audioDevInfo->containerId = gcnew System::String (info.audioDevInfo.containerId.c_str());
	infoM->audioDevInfo->devInterfaceName = gcnew System::String (info.audioDevInfo.devInterfaceName.c_str());
	infoM->audioDevInfo->formFactor = info.audioDevInfo.formFactor;
	infoM->audioDevInfo->bCommunicationsRole = info.audioDevInfo.bCommunicationsRole;
	infoM->audioDevInfo->bConsoleRole = info.audioDevInfo.bConsoleRole;
	infoM->audioDevInfo->bMultimediaRole = info.audioDevInfo.bMultimediaRole;

	infoM->devType = info.devType;
	return infoM;
}
	
array<MediaDevInfoManaged^> ^ NativeMedia::_MediaDevInfoList_N2M(std::vector<MediaDevInfo> &list)
{
	array<MediaDevInfoManaged^> ^ listM = gcnew array<MediaDevInfoManaged^> (list.size());
	for ( int i=0; i<list.size();i++){
		listM[i] = _MediaDevInfo_N2M (list[i] );
	}
	return listM;
}

//////////////////////////////////////
// some global functions

std::string  MarshalString ( String ^ s)
{
	const char* chars = 
		(const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
	std::string os = chars;
	Marshal::FreeHGlobal(IntPtr((void*)chars));
	return os;
}
std::wstring  MarshalStringW( String ^ s)
{
	const wchar_t* chars = 
		(const wchar_t*)(Marshal::StringToHGlobalUni(s)).ToPointer();
	std::wstring os = chars;
	Marshal::FreeHGlobal(IntPtr((void*)chars));
	return os;
}

}