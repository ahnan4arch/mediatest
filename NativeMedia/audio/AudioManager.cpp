///////////////////////////////////////////////////////////
//  Audio device management 
//   
#include "stdafx.h"

#include <objbase.h>
#include <functiondiscoverykeys.h>
#include "AudioManager.h"
#include "GlobalFun.h"
#include "NativeMediaLog.h"

#pragma comment (lib, "avrt.lib")
#pragma comment (lib, "comsuppw.lib ")

// The macro to enable UT
//#define AUDIO_UT

namespace WinRTCSDK {

// The context ID for session events
GUID AudioManager::eventContext_ = { 0x83c4ac17, 0x4f12, 0x4325, { 0xa9, 0x2e, 0xa4, 0x6b, 0x28, 0x54, 0x82, 0xaa } };

AudioManager::AudioManager (INativeMediaCallbacks ^ callbacks) :
	mmDevEnumerator_(NULL),
	mmCaptureDevCollection_(NULL),
	mmRenderDevCollection_(NULL),
	useDefaultCaptureDev_(true),
	useDefaultRenderDev_(true),
	captureEndpointId_(L""),
	renderEndpointId_(L""),
	capture_(NULL),
	loopbackCap_(NULL),
	render_(NULL),
	// current audio mode
	renderStreamType_(eNone),
	captureStreamType_(eNone),
	// datasourceID 
	captureSourceID_(""),
	renderSourceID_(""),
	mmNotificationClient_(NULL),
	dispatcher_("AudioNotifHandler"),
	nativeMediaCallbacks_(callbacks)
{
}

AudioManager::~AudioManager () 
{
	// ensure all callbacks finished
	dispatcher_.stopAndJoin();
}

void AudioManager::_HandleSessionEvents(SessionEventType evt, SessionEventParams param)
{
	MediaDevType devType;
	if (param.dataFlow == eCapture){
		devType = MediaDevType::eAudioCapture;
	}else if (param.dataFlow == eRender){
		devType = MediaDevType::eAudioRender;
	}

	switch (evt){
	case DisplayNameChanged:
		break;
	case IconPathChanged:
		break;
	case SimpleVolumeChanged:
		if ( param.eventContext != eventContext_)
		{
			nativeMediaCallbacks_->onAudioVolumeChanged(devType, (int )( param.newSimpleVolume * 100.0f), (param.newMuteState==TRUE));
		}
		break;
	case ChannelVolumeChanged:
		break;
	case GroupingParamChanged:
		break;
	case StateChanged:
		break;
	case SessionDisconnected:
		if ( param.disconnectReason == DisconnectReasonDeviceRemoval){
			// we will handle the device removal in MMNotification
			// so ignore this one
		}else if ( param.disconnectReason == DisconnectReasonFormatChanged){
			// device format changed (by user), let's reset device
			NativeMediaLogger::log(LogLevel::Info, "AudioMgr", "SessionDisconnected::DisconnectReasonFormatChanged");
			if ( param.dataFlow == eRender ){
				_ResetRender();
			}else if ( param.dataFlow == eCapture ){
				_ResetCapture();
			}
		}
		break;
		
	}
}

void AudioManager::_HandleMMNotification(MMNotificationType notif, MMNotificationParams param)
{

	MediaDevType devType;
	if (param.dataFlow == eCapture){
		devType = MediaDevType::eAudioCapture;
	}else if (param.dataFlow == eRender){
		devType = MediaDevType::eAudioRender;
	}

	switch (notif){
	case DeviceStateChanged:
		break;
	case DeviceAdded:
		nativeMediaCallbacks_->onDeviceChanged(devType, MediaDevEvent::eNewDevArrived);
		break;
	case DeviceRemoved:
		nativeMediaCallbacks_->onDeviceChanged(devType, MediaDevEvent::eDevRemoved);
		if ( param.deviceId == renderEndpointId_){
			_DestroyRender();
			nativeMediaCallbacks_->onDeviceChanged(devType, MediaDevEvent::eCurrentDevLost);
		}else if ( param.deviceId == captureEndpointId_){
			_DestroyCapture();
			nativeMediaCallbacks_->onDeviceChanged(devType, MediaDevEvent::eCurrentDevLost);
		}
		break;
	case DefaultDeviceChanged: // default device changed, reset devices
		if (param.role == eCommunications ){ // we care about communication devices only
			if ( param.dataFlow == eCapture && useDefaultCaptureDev_){
				_ResetCapture();
			}else if ( param.dataFlow == eRender && useDefaultRenderDev_){
				_ResetRender();
			}
		}
		break;
	case PropertyValueChange:
		break;
	}

}

// IMMNotificationHandler, this callback should not be blocked
void AudioManager::OnMMNotification (MMNotificationType notif, MMNotificationParams &param)
{
	LooperTask t = [=] () { this->_HandleMMNotification (notif, param); };
	dispatcher_.runAsyncTask(t);
}

// ISessionEventHandler, this callback should not be blocked
void AudioManager::OnAudioSessionEvent (SessionEventType evt, SessionEventParams &param)
{
	LooperTask t = [=] () { this->_HandleSessionEvents (evt, param); };
	dispatcher_.runAsyncTask(t);
}

///////////////////////////////////////////
// IAudioClientCallbacks
bool AudioManager::OnWriteAudioData(const void * data, int len, uint32_t sampleRate, uint64_t timestamp)
{
    // This method is called by capture, write audio data to the audio send pipeline
    if ( captureStreamType_ == eVoice ){
        DS_PutAudioData(captureSourceID_, data, len, sampleRate, timestamp);
    } else if ( captureStreamType_ == eTesting ) {
        // Mic is testing, user likes to see the peak meter
    }
	return true;
}

bool AudioManager::OnWriteLoopbackData(const void * data, int len, uint32_t sampleRate, uint64_t timestamp)
{
	return DS_PutLoopbackData(captureSourceID_, data, len, sampleRate, timestamp);
}

bool AudioManager::OnReadAudioData(void * data, int len/*in bytes*/)
{
	bool ret;
	// This method is called by render, read audio data from the audio recv pipeline
	if ( renderStreamType_ == eVoice ){
		ret = DS_GetAudioData(renderSourceID_, data, len, 48000);
	}else if( renderStreamType_ == eRingTone ) {
		//DS_GetRingToneData(data, len, 48000);
	}
	return ret;
}

void AudioManager::OnAudioClientEvent(EDataFlow dataFlow, AudioClientEvent evt, BOOL isLoopbackCap)
{
	LooperTask t = [=](){ _HandleAudioClientEvent (dataFlow, evt, isLoopbackCap);};
	dispatcher_.runAsyncTask(t);
}
	
void AudioManager::_HandleAudioClientEvent(EDataFlow dataFlow, AudioClientEvent evt, BOOL isLoopbackCap)
{
	AudioDeviceTypeManaged devType;
	AudioDeviceParamManaged ^ devParam = gcnew AudioDeviceParamManaged();
	const WAVEFORMATEX * wfx = NULL;
	switch (evt)
	{
	case AudioClientEvent::AudioClientStarted:
		switch (dataFlow)
		{
		case eRender:
			devType = AudioDeviceTypeManaged::AudioDeviceRender;
			wfx = render_->GetFormat();
			break;
		case eCapture:
			devType = AudioDeviceTypeManaged::AudioDeviceCapture;
			if ( isLoopbackCap == FALSE){ // only care capture device
				wfx = capture_->GetFormat();
			}
			break;
		}

		if ( wfx)
		{
			devParam->analogVolumeSupport = true;
			devParam->format->bitsPerSample = wfx->wBitsPerSample;
			devParam->format->containerSize = wfx->wBitsPerSample;
			devParam->format->formatType = 0;
			devParam->format->numChannels = 1; // we will remove capture/render side resampling later..
			devParam->format->samplesPerSec = wfx->nSamplesPerSec;
			devParam->format->channelMask = KSAUDIO_SPEAKER_MONO;

			// fire an event
			nativeMediaCallbacks_->onAudioDeviceReset(devType, devParam);
		}
		break;

	}
}

// IAudioClientCallbacks End     
////////////////////////////////////////////////////

HRESULT AudioManager::Init ()
{
    HRESULT hr;
	LooperTask t = [=,&hr]()
	{

		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&mmDevEnumerator_));
		if (FAILED(hr)) return ;
		// update device collections
		_UpdateDeviceList();
		mmNotificationClient_ = new MMNotificationClient(this);
		hr = mmDevEnumerator_->RegisterEndpointNotificationCallback(mmNotificationClient_);

		// create default devices
		_ResetCapture();
		_ResetRender();

		// setup initial value
		speakerVolume_ = this->GetSpeakerVolume( );
		speakerMuted_ = false;
		microphoneMuted_ = false;

	};
	dispatcher_.runSyncTask(t);
	return hr;
}

HRESULT AudioManager::UnInit ()
{
	LooperTask t = [=]()
	{
		// stop capture and render (if they exist)
		StopAudioCapture();
		StopAudioRender();
		_DestroyRender();
		_DestroyCapture();

		mmDevEnumerator_->UnregisterEndpointNotificationCallback(mmNotificationClient_);
		delete mmNotificationClient_;
		mmNotificationClient_ = NULL;

		mmDevEnumerator_ = NULL;
		mmCaptureDevCollection_ = NULL;
		mmRenderDevCollection_ = NULL;
	};
	
	dispatcher_.runSyncTask(t);

	return S_OK;
}

HRESULT AudioManager::_UpdateDeviceList ()
{
    HRESULT hr = mmDevEnumerator_->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &mmCaptureDevCollection_);
    if (FAILED(hr)) return hr;

    hr = mmDevEnumerator_->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &mmRenderDevCollection_);
	return hr;
}

HRESULT AudioManager::_GetEndpointID(_com_ptr_IMMDevice mmDev, std::wstring& id)
{
    HRESULT hr;
	// Device id
    LPWSTR deviceId;
    hr = mmDev->GetId(&deviceId);
    if (FAILED(hr)) return hr;

	id = deviceId;
    ::CoTaskMemFree(deviceId);
	return hr;
}

// I have to define this key myself!
static const PROPERTYKEY priv_PKEY_AudioEndpoint_FormFactor ={ {0x1da5d803, 0xd492, 0x4edd, 0x8c, 0x23, 0xe0, 0xc0, 0xff, 0xee, 0x7f, 0x0e}, 1}; 

HRESULT AudioManager::_FillDevInfo(_com_ptr_IMMDevice mmDev, MediaDevInfo & info)
{
    HRESULT hr;
    PROPVARIANT var;
    _com_ptr_t< _com_IIID<IPropertyStore, &__uuidof(IPropertyStore)> > propertyStore;
    PropVariantInit(&var);

	hr =_GetEndpointID(mmDev, info.symbolicLink);
    if (FAILED(hr)) goto done;

	// Let's open property store
    hr = mmDev->OpenPropertyStore(STGM_READ, &propertyStore);
    if (FAILED(hr)) goto done;
	
	// Friendly name
    hr = propertyStore->GetValue(PKEY_Device_FriendlyName, &var);
	if (SUCCEEDED(hr))
	{
		info.devFriendlyName = var.vt != VT_LPWSTR ? L"Unknown" : var.pwszVal;
		PropVariantClear(&var);
	}

	// form factor 
	hr = propertyStore->GetValue(priv_PKEY_AudioEndpoint_FormFactor, &var);
	if (SUCCEEDED(hr))
	{
		info.audioDevInfo.formFactor = var.vt!=VT_UI4? UnknownFormFactor: var.uintVal;
		PropVariantClear(&var);
	}

	// interface friendly name
	hr = propertyStore->GetValue(PKEY_DeviceInterface_FriendlyName, &var);
	if (SUCCEEDED(hr))
	{
		info.audioDevInfo.devInterfaceName = var.vt != VT_LPWSTR ? L"Unknown" : var.pwszVal;
		PropVariantClear(&var);
	}

	// container ID (device enclosure ID)
	hr = propertyStore->GetValue(PKEY_Device_ContainerId, &var);
	if (SUCCEEDED(hr))
	{
		info.audioDevInfo.containerId = var.vt != VT_LPWSTR ? L"Unknown" : var.pwszVal;
		PropVariantClear(&var);
	}
	
done:
    return S_OK;
}

HRESULT AudioManager::_GetDevList (EDataFlow dataFlow/*eRender/eCapture*/, std::vector<MediaDevInfo> & list)
{
	MediaDevInfo info;
	_com_ptr_IMMDeviceCollection  col;
	if ( dataFlow ==eRender){
		col = mmRenderDevCollection_;
	}else{
		col = mmCaptureDevCollection_;
	}
    _com_ptr_IMMDevice  mmdev = NULL;

	UINT deviceCount;
	HRESULT hr = col->GetCount(&deviceCount);
	if (FAILED(hr)) return hr;

	for (UINT i = 0 ; i < deviceCount ; i++)
	{
		hr = col->Item (i, &mmdev);
		_FillDevInfo(mmdev, info );
		list.push_back(info);
	}

	//////////////////////////////////////////////////////////////
	// check devices roles
	bool hasConsoleDev=false, hasCommunicationsDev=false, hasMultimediaDev=false;
	std::wstring consoleDev, communicationsDev, multimediaDev;

	hr = mmDevEnumerator_->GetDefaultAudioEndpoint(dataFlow, eConsole, &mmdev);
	if (SUCCEEDED(hr))   {
		hasConsoleDev = true;
		_GetEndpointID(mmdev, consoleDev);
	}
	hr = mmDevEnumerator_->GetDefaultAudioEndpoint(dataFlow, eCommunications, &mmdev);
	if (SUCCEEDED(hr))   {
		hasCommunicationsDev = true;
		_GetEndpointID(mmdev, communicationsDev);
	}
	hr = mmDevEnumerator_->GetDefaultAudioEndpoint(dataFlow, eMultimedia, &mmdev);
	if (SUCCEEDED(hr))   {
		hasMultimediaDev = true;
		_GetEndpointID(mmdev, multimediaDev);
	}

	size_t n = list.size();
	for(size_t i =0; i<n; i++){
		if ( hasConsoleDev &&        list[i].symbolicLink == consoleDev ) 
			list[i].audioDevInfo.bConsoleRole = true;

		if ( hasCommunicationsDev && list[i].symbolicLink == communicationsDev ) 
			list[i].audioDevInfo.bCommunicationsRole = true;

		if ( hasMultimediaDev &&     list[i].symbolicLink == multimediaDev ) 
			list[i].audioDevInfo.bMultimediaRole = true;
    }

	return S_OK;
}

HRESULT AudioManager::GetDevList (EDataFlow dataFlow/*eRender/eCapture*/, std::vector<MediaDevInfo> & list)
{
	HRESULT hr;
	LooperTask t = [=,&hr, &list]()
	{
		hr = _GetDevList(dataFlow, list);
	};
	dispatcher_.runSyncTask(t);
	return hr;
}

// update default devices
HRESULT AudioManager::_UpdateDefaultDevice()
{
	if ( useDefaultCaptureDev_)
	{
		_com_ptr_IMMDevice  mmdev = NULL;
		HRESULT hr = mmDevEnumerator_->GetDefaultAudioEndpoint(eCapture, eCommunications, &mmdev);
		if ( SUCCEEDED(hr)){
			_GetEndpointID(mmdev, captureEndpointId_);
		}
	}

	if ( useDefaultRenderDev_)
	{
		_com_ptr_IMMDevice  mmdev = NULL;
		HRESULT hr = mmDevEnumerator_->GetDefaultAudioEndpoint(eRender, eCommunications, &mmdev);
		if ( SUCCEEDED(hr)){
			_GetEndpointID(mmdev, renderEndpointId_);
		}
	}
	return S_OK;
}

WASAPICapture* AudioManager::_CreateCapture (_com_ptr_IMMDevice mmDev, bool loopback)
{
	WASAPICapture* cap = new WASAPICapture(mmDev, this, this, loopback);
	bool ret = cap->Initialize();
	if ( !ret ) {
		delete cap;
		cap = NULL;
	}
	return cap;
}

WASAPIRenderer* AudioManager::_CreateRender (_com_ptr_IMMDevice mmDev)
{
	WASAPIRenderer* pRender = new WASAPIRenderer(mmDev, this, this, eventContext_);
	bool ret = pRender->Initialize();
	if ( !ret) {
		delete pRender;
		pRender = NULL;
	}
	return pRender;
}

HRESULT  AudioManager::_DestroyRender ()
{
	if (render_)
	{
		render_->Stop();
		render_->Shutdown();
		delete render_;
		render_ = NULL;
	}

	if (loopbackCap_)
	{
		loopbackCap_->Stop();
		loopbackCap_->Shutdown();
		delete loopbackCap_;
		loopbackCap_ = NULL;
	}
	return S_OK;
}

HRESULT  AudioManager::_DestroyCapture ()
{
	if (capture_)
	{
		capture_->Stop();
		capture_->Shutdown();
		delete capture_;
		capture_ = NULL;
	}
	return S_OK;
}

HRESULT  AudioManager::_ResetRender()
{
	_DestroyRender();
	_UpdateDefaultDevice();
	_com_ptr_IMMDevice  mmdev = NULL;
	HRESULT hr = mmDevEnumerator_->GetDevice(renderEndpointId_.c_str(), &mmdev);
	if ( SUCCEEDED(hr)){
		render_ = _CreateRender(mmdev);

		// log render dev info
		MediaDevInfo info;
		hr = AudioManager::_FillDevInfo( mmdev, info);
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioRender", L"current dev - name=%s", info.devFriendlyName.c_str());
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioRender", L"current dev - containerId=%s", info.audioDevInfo.containerId.c_str());
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioRender", L"current dev - formFactor=%d", info.audioDevInfo.formFactor);

		// also create loopback capture for render for AEC.
		loopbackCap_ = _CreateCapture(mmdev, true);
	}

	if (renderStreamType_ != eNone) {
		if (render_) render_->Start();
		if (loopbackCap_) loopbackCap_->Start();
	}

	return S_OK;
}

HRESULT  AudioManager::_ResetCapture()
{
	_DestroyCapture ();
	_UpdateDefaultDevice();
	_com_ptr_IMMDevice  mmdev = NULL;
	HRESULT hr = mmDevEnumerator_->GetDevice(captureEndpointId_.c_str(), &mmdev);
	if ( SUCCEEDED(hr)){
		MediaDevInfo info;
		hr = AudioManager::_FillDevInfo( mmdev, info);
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioCapture", L"current dev - name=%s", info.devFriendlyName.c_str());
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioCapture", L"current dev - containerId=%s", info.audioDevInfo.containerId.c_str());
		NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioCapture", L"current dev - formFactor=%d", info.audioDevInfo.formFactor);
		capture_ = _CreateCapture(mmdev, false);
	}
	if (captureStreamType_ != eNone) {
		if (capture_) capture_->Start();
	}
	return S_OK;
}

HRESULT AudioManager::ChooseRenderDev(bool useDefault,  std::wstring endpointId)
{ 
	LooperTask t = [=]()
	{
		useDefaultRenderDev_ = useDefault;
		renderEndpointId_ = endpointId;
		_ResetRender();
	};
	dispatcher_.runSyncTask(t);
	return S_OK;
}

HRESULT AudioManager::ChooseCaptureDev(bool useDefault,  std::wstring endpointId)
{ 
	LooperTask t = [=]()
	{
		useDefaultCaptureDev_ = useDefault;
		captureEndpointId_ = endpointId;
		_ResetCapture();
	};
	dispatcher_.runSyncTask(t);
	return S_OK;
}

void AudioManager::StartAudioCapture(std::string sourceID)
{
	LooperTask t = [=]()
	{
		captureStreamType_ = eVoice;
		captureSourceID_ = sourceID;
		_ResetCapture();
		if( microphoneMuted_ ){
			this->SetMicMute(true);
		}
	};
	dispatcher_.runSyncTask(t);
}

void AudioManager::StartAudioRender(std::string sourceID)
{
	LooperTask t = [=]()
	{
		renderStreamType_ = eVoice;
		renderSourceID_ = sourceID;
		_ResetRender();
		if( speakerMuted_){
			this->SetSpeakerMute(true);
		}else{
			// restore the last speaker volume
			this->SetSpeakerVolume(speakerVolume_);
		}
	};
	dispatcher_.runSyncTask(t);
}

void AudioManager::StartCaptureTest()
{
	LooperTask t = [=]()
	{
		captureStreamType_ = eTesting;
		_ResetCapture();
	};
	dispatcher_.runSyncTask(t);
}

void AudioManager::StartRingtone()
{
	LooperTask t = [=]()
	{
		renderStreamType_ = eRingTone;
		_ResetRender();
	};
	dispatcher_.runSyncTask(t);
}

// This method will remove the capture stream (captureStreamType_ = eNone)
void AudioManager::StopAudioCapture()
{
	LooperTask t = [=]()
	{
		captureStreamType_ = eNone;
		captureSourceID_ = "";
		if (capture_)capture_->Stop(); // Just stop the device. User may still in a meeting and need to mute the device. 
	};
	dispatcher_.runSyncTask(t);
}

// This method will remove the render stream (renderStreamType_ = eNone)
void AudioManager::StopAudioRender()
{
	LooperTask t = [=]()
	{
		renderStreamType_ = eNone;
		renderSourceID_ = "";
		if (loopbackCap_)loopbackCap_->Stop();
		if (render_)render_->Stop(); // Just stop the device. User may still in a meeting and need to mute the device. 
	};
	dispatcher_.runSyncTask(t);
}

float AudioManager::GetCapturePeakMeter()
{
	float ret = 0.0;
	LooperTask t = [=,&ret](){ if(capture_) ret = capture_->GetPeakMeter();};
	dispatcher_.runSyncTask(t);
	return ret;
}

float AudioManager::GetRenderPeakMeter ()
{
	float ret = 0.0;
	LooperTask t = [=,&ret](){ if (render_) ret = render_->GetPeakMeter();};
	dispatcher_.runSyncTask(t);
	return ret;
}

//////////////////////////////////////
// Volume mute control
int AudioManager::GetSpeakerVolume()/* [0, 100] */
{
	int vol=0;
	LooperTask t = [=,&vol](){ if (render_) render_->GetSimpleVolume(vol);};
	dispatcher_.runSyncTask(t);
	return vol;
}

void AudioManager::SetSpeakerVolume(int level /* [0, 100] */)
{
	LooperTask t = [=]()
	{
		speakerVolume_ = level;
		if (render_ == NULL) return;
		render_->SetSimpleVolume(level);
	};
	dispatcher_.runSyncTask(t);
}

void AudioManager::SetSpeakerMute(bool bMute)
{
	LooperTask t = [=]()
	{
		speakerMuted_ = bMute;
		if (render_ == NULL) return;
		render_->SetMute(bMute);
	};
	dispatcher_.runSyncTask(t);
}

void AudioManager::SetMicMute(bool bMute)
{
	LooperTask t = [=]()
	{
		microphoneMuted_ = bMute;
		if ( capture_ == NULL) return;
		capture_->SetMicMute(bMute);
	};
	dispatcher_.runSyncTask(t);
}

} // namespace WinRTCSDK
