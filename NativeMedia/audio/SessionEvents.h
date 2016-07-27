//////////////////////////////////////////////////////////////////////
// WASAPI requires AudioSessionEvents to be handled asynchronously

#pragma once

#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include <string>

namespace WinRTCSDK{

enum SessionEventType 
{
	DisplayNameChanged,
	IconPathChanged,
	SimpleVolumeChanged,
	ChannelVolumeChanged,
	GroupingParamChanged,
	StateChanged,
	SessionDisconnected
};

struct SessionEventParams
{
	EDataFlow                    dataFlow; // to distinguish render/capture session
	std::wstring                 newDisplayName;
	GUID                         eventContext;
	BOOL                         newMuteState;
	float                        newSimpleVolume;
	AudioSessionState            newState;
	AudioSessionDisconnectReason disconnectReason;
};

// the handler to handle events asynchronously
// the AudioManager class is an implementation of this interface
class ISessionEventHandler 
{
public:
	virtual void OnAudioSessionEvent (SessionEventType evt, SessionEventParams &param) = 0;
};

// This AudioSessionEvents callback is used for both capture and render
class AudioSessionEvents : public IAudioSessionEvents
{
public:
	AudioSessionEvents (ISessionEventHandler *handler, EDataFlow dataFlow);
	~AudioSessionEvents();
public:
	// IUnknown
	STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();
    STDMETHOD(QueryInterface)(REFIID iid, void **pvObject);

	// IAudioSessionEvents
    STDMETHOD(OnDisplayNameChanged) (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/);
    STDMETHOD(OnIconPathChanged) (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/);
    STDMETHOD(OnSimpleVolumeChanged) (float /*NewSimpleVolume*/, BOOL /*NewMute*/, LPCGUID /*EventContext*/);
    STDMETHOD(OnChannelVolumeChanged) (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/);
    STDMETHOD(OnGroupingParamChanged) (LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/);
    STDMETHOD(OnStateChanged) (AudioSessionState /*NewState*/);
    STDMETHOD(OnSessionDisconnected) (AudioSessionDisconnectReason DisconnectReason);	
private:
	ULONG                  refCount_;
	EDataFlow              dataFlow_;
	ISessionEventHandler * handler_;
};
} // Nemo namespace