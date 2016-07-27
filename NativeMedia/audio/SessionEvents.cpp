// Implementation of IAudioSessionEvents.
// 
#include "stdafx.h"

#include "SessionEvents.h"

namespace WinRTCSDK{
	
AudioSessionEvents::AudioSessionEvents (ISessionEventHandler *handler, EDataFlow dataFlow): 
	refCount_(1),handler_(handler), dataFlow_(dataFlow)
{
};

AudioSessionEvents::~AudioSessionEvents()
{
}

///////////////////////////////////////////////////////
// IAudioSessionEvents
HRESULT AudioSessionEvents::OnDisplayNameChanged (LPCWSTR /*NewDisplayName*/, LPCGUID /*EventContext*/) 
{
	return S_OK; 
}
HRESULT AudioSessionEvents::OnIconPathChanged (LPCWSTR /*NewIconPath*/, LPCGUID /*EventContext*/) 
{ 
	return S_OK; 
}
HRESULT AudioSessionEvents::OnSimpleVolumeChanged (float newVol, BOOL newMute, LPCGUID ctxt /*EventContext*/)
{ 
	SessionEventParams param;
	param.dataFlow = dataFlow_;
	param.newSimpleVolume = newVol;
	param.newMuteState = newMute;
	param.eventContext = *ctxt;
	handler_->OnAudioSessionEvent(SimpleVolumeChanged, param);
	return S_OK;
}
HRESULT AudioSessionEvents::OnChannelVolumeChanged (DWORD /*ChannelCount*/, float /*NewChannelVolumes*/[], DWORD /*ChangedChannel*/, LPCGUID /*EventContext*/) 
{ 
	return S_OK; 
}
HRESULT AudioSessionEvents::OnGroupingParamChanged(LPCGUID /*NewGroupingParam*/, LPCGUID /*EventContext*/) 
{
	return S_OK; 
}
HRESULT AudioSessionEvents::OnStateChanged (AudioSessionState /*NewState*/) 
{ 
	return S_OK; 
}

HRESULT AudioSessionEvents::OnSessionDisconnected(AudioSessionDisconnectReason disconnectReason)
{
	SessionEventParams param;
	param.dataFlow = dataFlow_;
	param.disconnectReason = disconnectReason;
	handler_->OnAudioSessionEvent(SessionDisconnected, param);
	return S_OK;
}
	
///////////////////////////////////////////////////////
//  IUnknown
HRESULT AudioSessionEvents::QueryInterface(REFIID Iid, void **Object)
{
    if (Object == NULL) {
        return E_POINTER;
    }
    *Object = NULL;

    if (Iid == IID_IUnknown){
        *Object = static_cast<IUnknown *>(this);
        AddRef();
    }else if (Iid == __uuidof(IAudioSessionEvents)){
        *Object = static_cast<IAudioSessionEvents *>(this);
        AddRef();
    }  else {
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG AudioSessionEvents::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

ULONG AudioSessionEvents::Release()
{
    ULONG returnValue = InterlockedDecrement(&refCount_);
    if (returnValue == 0)
    {
        delete this;
    }
    return returnValue;
}
} // namespace WinRTCSDK