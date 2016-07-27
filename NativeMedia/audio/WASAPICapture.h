
#pragma once

#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include "WASAPIComPtrDefs.h"
#include "SessionEvents.h"
#include "AudioClientCallbacks.h"
#include "AudioResampler.h"
#include "WavHelper.h"
#include "MicControl.h"

namespace WinRTCSDK{

class WASAPICapture 
{
public:
    WASAPICapture(_com_ptr_IMMDevice endpoint, IAudioClientCallbacks *callback, 
		ISessionEventHandler *sessEvthandler, bool loopbackMode);

    ~WASAPICapture(); 

	bool Initialize();
    void Shutdown();

    bool Start();
    void Stop();
	float GetPeakMeter();
	void  SetMicMute(bool bMute);
	const WAVEFORMATEX * GetFormat();

private:
    static DWORD WINAPI CaptureThreadEntry(LPVOID Context);
    bool    _SetupFormat();
	BOOL    _OutputData(BYTE *pData, UINT32 framesAvailable, UINT64  u64QPCPosition);
    DWORD   _CaptureThread();
	DWORD   _CaptureLoop();
	DWORD   _LoopbackCaptureLoop();

private:
	// interfaces
    _com_ptr_IMMDevice             endpoint_;
    _com_ptr_IAudioClient          audioClient_;
    _com_ptr_IAudioCaptureClient   captureClient_;
    _com_ptr_IAudioSessionControl  audioSessionControl_;
	_com_ptr_IAudioMeterInformation  audioMeterInfo_;
	AudioSessionEvents            *sessionEvents_;
	IAudioClientCallbacks     *callback_;
	MicControl                    *micControl_;
	// working thread
    HANDLE                         captureThread_;
    HANDLE                         shutdownEvent_;
    HANDLE                         samplesReadyEvent_;
	HANDLE                         hWakeUp_; // a waitable timer for loopback capture mode
	// audio data
	REFERENCE_TIME                 hnsDevicePeriod_;
    WAVEFORMATEX *                 mixFormat_;
	WAVEFORMATEX                   format_;
    size_t                         frameSize_;
    UINT32                         bufferSize_;
	AudioResampler                 resampler_;
	// loopback mode
	bool                           loopbackCapMode_;
	// Just for UT
	WavHelper                      wavFile_;
	// The original volume, need to restore on exit
	float                          originalVolume_;
};
} // namespace WinRTCSDK