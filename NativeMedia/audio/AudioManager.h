////////////////////////////////////////////////////////////////
//   Audio device management 
//
//   This class provides following functions:
//
//    1. using MMDeviceEnumerator to emumerate capture/render devices
//    2. handles MMNotifications
//    3. handles AuioSessionEvents
//    4. capture/render endpoints creation and deletion
//    5. acts as a source/sink of audio data of the render/capture endpoints
//

// standard librariesd
#include <string>
#include <vector>
#include <set>

#include <comip.h>
#include <vcclr.h>

// headers of current module
#include "AutoLock.h"
#include "Looper.h"
#include "WASAPIComPtrDefs.h"
#include "AudioClientCallbacks.h"
#include "MMNotificationClient.h"
#include "SessionEvents.h"
#include "WASAPICapture.h"
#include "WASAPIRenderer.h"
#include "NativeMediaCallbacks.h"
#include "WinRTCSDKTypes.h"

namespace WinRTCSDK {

enum AudioStreamType
{
	eNone,
	eVoice,
	eRingTone, 
	eTesting // mic testing
};

class AudioManager :
	public IMMNotificationHandler,
	public ISessionEventHandler,
	public IAudioClientCallbacks
{

public:
	AudioManager(INativeMediaCallbacks ^ callbacks);
	~AudioManager();

public: 
	// IMMNotificationHandler. run by dispatcher_
	virtual void OnMMNotification (MMNotificationType notif, MMNotificationParams &param);

	// ISessionEventHandler. run by dispatcher_
	virtual void OnAudioSessionEvent (SessionEventType evt, SessionEventParams &param);

	// IAudioClientCallbacks
	virtual bool OnWriteAudioData(const void * data, int len/*in bytes*/, uint32_t sampleRate, uint64_t timestamp);
	virtual bool OnWriteLoopbackData(const void * data, int len/*in bytes*/, uint32_t sampleRate, uint64_t timestamp);
	virtual bool OnReadAudioData(void * data, int len/*in bytes*/);
	virtual void OnAudioClientEvent(EDataFlow dataFlow, AudioClientEvent evt, BOOL isLoopbackCap);

public:
	////////////////////////////////////////////////////////
	// NOTE: All calls to public method  are dispatched by 
	// the internal dispatcher, which is MTA enabled. So
	// we can comply with COM requirments.

	HRESULT  Init (); // To init MMDeviceEnumerator
	HRESULT  UnInit ();
	HRESULT  GetDevList (EDataFlow dataFlow/*eRender/eCapture*/, std::vector<MediaDevInfo> & list);
	
	////////////////////////////////////////////////////
	// all following public threads must be thread-safe

	// if useDefault is true then endpointId is ignored
	HRESULT  ChooseRenderDev(bool useDefault,  std::wstring endpointId);
	HRESULT  ChooseCaptureDev(bool useDefault,  std::wstring endpointId);

	void     StartAudioCapture(std::string sourceID);
	void     StartAudioRender(std::string sourceID);
	void     StartCaptureTest();
	void     StartRingtone();
	void     StopAudioCapture();
	void     StopAudioRender();

	// volume/mute control
	int      GetSpeakerVolume();/* [0, 100] */
	void     SetSpeakerVolume(int level /* [0, 100] */);
	void     SetSpeakerMute(bool bMute);
	void     SetMicMute(bool bMute);
	float    GetCapturePeakMeter();
	float    GetRenderPeakMeter ();

private: // internal methods
	HRESULT  _GetDevList (EDataFlow dataFlow/*eRender/eCapture*/, std::vector<MediaDevInfo> & list);
	HRESULT  _DestroyRender ();
	HRESULT  _DestroyCapture ();
	HRESULT  _ResetRender();
	HRESULT  _ResetCapture();
	WASAPICapture  * _CreateCapture (_com_ptr_IMMDevice mmDev, bool loopback);
	WASAPIRenderer * _CreateRender (_com_ptr_IMMDevice mmDev);
	HRESULT _UpdateDefaultDevice();
	HRESULT _FillDevInfo(_com_ptr_IMMDevice mmDev, MediaDevInfo & info);
	HRESULT _GetEndpointID(_com_ptr_IMMDevice mmDev, std::wstring& id);
	HRESULT _UpdateDeviceList ();

	// events handlers
	void    _HandleSessionEvents(SessionEventType evt, SessionEventParams param);
	void    _HandleMMNotification(MMNotificationType notif, MMNotificationParams param);
	void    _HandleAudioClientEvent(EDataFlow dataFlow, AudioClientEvent evt, BOOL isLoopbackCap);

private: // data members
	_com_ptr_IMMDeviceEnumerator     mmDevEnumerator_;
	_com_ptr_IMMDeviceCollection     mmCaptureDevCollection_;
	_com_ptr_IMMDeviceCollection     mmRenderDevCollection_;

	// current used devices 
	bool                             useDefaultCaptureDev_;
	bool                             useDefaultRenderDev_;
	std::wstring                     captureEndpointId_;
	std::wstring                     renderEndpointId_;

	// render/capture
	WASAPICapture                   *capture_;
	WASAPICapture                   *loopbackCap_; // render loopback capture, for AEC
	WASAPIRenderer                  *render_;

	// current audio mode
	AudioStreamType                  renderStreamType_;
	AudioStreamType                  captureStreamType_;

	// datasourceID 
	std::string                      captureSourceID_;
	std::string                      renderSourceID_;

	// system events 
	MMNotificationClient            *mmNotificationClient_;

	// message queue to handle AudioSessionEvents and MMNotification
	Looper                           dispatcher_; // to execute a task asynchronously

	// managed callback
	gcroot<INativeMediaCallbacks ^>  nativeMediaCallbacks_;

	// mute/volume management
	static GUID                      eventContext_;
	int                              speakerVolume_;
	bool                             speakerMuted_;
	bool                             microphoneMuted_;
};

} // namespace WinRTCSDK