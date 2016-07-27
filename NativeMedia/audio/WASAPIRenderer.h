
#pragma once
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include "WASAPIComPtrDefs.h"
#include "SessionEvents.h"
#include "AudioClientCallbacks.h"
#include "AudioResampler.h"
namespace WinRTCSDK {

class WASAPIRenderer
{
public:

    WASAPIRenderer(_com_ptr_IMMDevice endpoint, IAudioClientCallbacks *callback, 
		ISessionEventHandler *sessEvthandler, GUID eventContext);
    ~WASAPIRenderer(void);

    bool Initialize();
    void Shutdown();

    bool Start();
    void Stop();
	float GetPeakMeter();
	void  SetMute(bool bMute);
	HRESULT SetSimpleVolume( int volume/*[0, 100]*/);
	HRESULT GetSimpleVolume( int &volume/*[0, 100]*/);
	const WAVEFORMATEX * GetFormat();
private:
    static DWORD __stdcall _RenderThreadEntry(LPVOID Context);
    DWORD _RenderThread();
    bool  _SetupFormat();
	int   _InputData ( BYTE *pData, UINT32 frames); // this function returns acctual frames

private:
    _com_ptr_IMMDevice               endpoint_;
    _com_ptr_IAudioClient            audioClient_;
    _com_ptr_IAudioRenderClient      renderClient_;
    _com_ptr_IAudioSessionControl    audioSessionControl_;
	_com_ptr_IAudioMeterInformation  audioMeterInfo_;
	_com_ptr_ISimpleAudioVolume      simpleVolume_;
	_com_ptr_IAudioEndpointVolume    endpointVolume_; // we don't use this interface the set volume. this is used for debugging!
	AudioSessionEvents              *sessionEvents_;
	IAudioClientCallbacks       *callback_;

    HANDLE                           renderThread_;
    HANDLE                           shutdownEvent_;
    HANDLE                           samplesReadyEvent_;

    WAVEFORMATEX                    *mixFormat_;
    WAVEFORMATEX                     format_;
    UINT32                           frameSize_;
    UINT32                           bufferSize_;
    LONG                             engineLatency_; // in 100ns unit
	AudioResampler                   resampler_;
	
	// mute control
	bool                             bMuted_; // this is for mute operation
	float                            lastLevelScalar_; // this is for mute operation
	GUID                             eventContext_;
};

} // Nemo