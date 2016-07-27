
#include "StdAfx.h"
#include <stdio.h>

#include <assert.h>
#include <avrt.h>
#include "WASAPICapture.h"
#include "NativeMediaLog.h"

//#define AUDIO_UT

namespace WinRTCSDK {

WASAPICapture::WASAPICapture(_com_ptr_IMMDevice endpoint, IAudioClientCallbacks *callback, 
		ISessionEventHandler *sessEvthandler, bool loopbackMode):
    endpoint_(endpoint),
	callback_(callback),
    audioClient_(NULL),
    captureClient_(NULL),
	audioSessionControl_(NULL),
	audioMeterInfo_(NULL),
    captureThread_(NULL),
    shutdownEvent_(NULL),
    mixFormat_(NULL),
    samplesReadyEvent_(NULL),
	hWakeUp_(NULL),
	resampler_(),
	loopbackCapMode_(loopbackMode),
	originalVolume_(0.0f)
{
	sessionEvents_ = new AudioSessionEvents(sessEvthandler, eCapture);
	micControl_ = new MicControl(endpoint_);
}


WASAPICapture::~WASAPICapture(void) 
{
	delete micControl_;
	delete sessionEvents_;
}

const WAVEFORMATEX * WASAPICapture::GetFormat()
{
	return &format_;
}

bool WASAPICapture::_SetupFormat()
{
	// we must use the same sampling-rate as the shared-mode format
	HRESULT hr = audioClient_->GetMixFormat(&mixFormat_);
	if (FAILED(hr)) return false;

	// Set up WAV format. 
	memset(&format_, 0, sizeof(WAVEFORMATEX)); 
	format_.wFormatTag = WAVE_FORMAT_PCM; 
	format_.nChannels = mixFormat_->nChannels; 
	format_.nSamplesPerSec = mixFormat_->nSamplesPerSec; 
	format_.wBitsPerSample = 16;
	format_.nBlockAlign = (format_.wBitsPerSample / 8) * format_.nChannels; 
	format_.nAvgBytesPerSec = format_.nSamplesPerSec * format_.nBlockAlign; 

	frameSize_ = format_.nBlockAlign;

	WAVEFORMATEX      *ppClosestMatch;
	hr =  audioClient_->IsFormatSupported(
		AUDCLNT_SHAREMODE_SHARED, &format_, &ppClosestMatch);
	if (ppClosestMatch)::CoTaskMemFree(ppClosestMatch);
	if (hr == S_OK)
	{  
		switch ( format_.nSamplesPerSec )
		{
		case 44100:
		case 48000:
		case 32000:
		case 16000:
		case 8000:
			return true;
		default:
			break;
		}
	}
	return false;
}

bool WASAPICapture::Initialize()
{
    shutdownEvent_ = ::CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    samplesReadyEvent_ = ::CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
	hWakeUp_ = ::CreateWaitableTimer (NULL, FALSE, NULL); // for loopback capture mode
 
    HRESULT hr = endpoint_->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL,(void**) &audioClient_);
    if (FAILED(hr)) return false;

	bool ret = _SetupFormat();
    if (!ret) return false;

	int latency = 30;
	// initalize the audio client
    hnsDevicePeriod_ = 0;
    DWORD streamFlags = AUDCLNT_STREAMFLAGS_NOPERSIST;
	if ( loopbackCapMode_ ) {
		streamFlags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
		audioClient_->GetDevicePeriod(&hnsDevicePeriod_, NULL);
	}else{
		streamFlags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
		hnsDevicePeriod_ = latency * 10000;
	}

	hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, streamFlags, 
		hnsDevicePeriod_, 0, &format_, NULL); 
    if (FAILED(hr)) return false;
	
    hr = audioClient_->GetBufferSize(&bufferSize_);
    if(FAILED(hr)) return false;
	
	// don't enable event-driven mode for loopback capture!
	if ( ! loopbackCapMode_ ) {
		hr = audioClient_->SetEventHandle(samplesReadyEvent_);
		if (FAILED(hr)) return false;
	}

    hr = audioClient_->GetService(IID_PPV_ARGS(&captureClient_));
    if (FAILED(hr)) return false;

    hr = audioClient_->GetService(IID_PPV_ARGS(&audioSessionControl_));
    if (FAILED(hr)) return false;

    hr = audioSessionControl_->RegisterAudioSessionNotification(sessionEvents_);
    if (FAILED(hr)) return false;

    hr = endpoint_->Activate(__uuidof(IAudioMeterInformation), 
		CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&audioMeterInfo_));
    if (FAILED(hr)) return false;

	// we need store the original volume of system
	micControl_->GetEndpointVolumeScalar(originalVolume_);
	if ( !loopbackCapMode_){ // set default microphone volume
		micControl_->SetEndpointVolumeScalar(0.8f);
	}
    return true;
}

void WASAPICapture::Shutdown()
{
	HRESULT hr ;

	// we need restore the original volume of system, after using the mic
	micControl_->SetEndpointVolumeScalar(originalVolume_);

	if ( audioSessionControl_ ) hr = audioSessionControl_->UnregisterAudioSessionNotification(sessionEvents_);

	// release the interface
	audioSessionControl_ = NULL;

    // Shutdown the thread
	if (captureThread_)
    {
        SetEvent(shutdownEvent_);
        ::WaitForSingleObject(captureThread_, INFINITE);
        ::CloseHandle(captureThread_);
        captureThread_ = NULL;
    }

	// close handles
    ::CloseHandle(shutdownEvent_);
    shutdownEvent_ = NULL;
    ::CloseHandle(samplesReadyEvent_);
    samplesReadyEvent_ = NULL;
	::CloseHandle(hWakeUp_);
	hWakeUp_ = NULL;

    endpoint_ = NULL;
    audioClient_ = NULL;
    captureClient_ = NULL;

    if (mixFormat_)
    {
        CoTaskMemFree(mixFormat_);
        mixFormat_ = NULL;
    }
}

bool WASAPICapture::Start()
{
    HRESULT hr;
    captureThread_ = CreateThread(NULL, 0, CaptureThreadEntry, this, 0, NULL);
    if (captureThread_ == NULL)  return false;

#ifdef AUDIO_UT
	if ( loopbackCapMode_) {
		wavFile_.CreateWavFile(L"d:\\loopback.wav", &format_);
	}else{
		wavFile_.CreateWavFile(L"d:\\capture.wav", &format_);
	}
#endif

	resampler_.reset();
    hr = audioClient_->Start();
    if (FAILED(hr))  return false;

	NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "capture started. isloopback=%s\n", loopbackCapMode_?"true":"false");
	// log audio volumes
	float dbVal=0, minDB=0, maxDB=0, stepDB=0;
	hr = micControl_->GetMicBoostRange(minDB, maxDB, stepDB);
	if ( FAILED (hr) ){
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "Has no mic boost\n");
	}else{
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "MicBoost range: min=%f(db) max=%f(db) step=%f(db)\n", minDB, maxDB, stepDB);
		micControl_->GetMicBoost(dbVal);
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "MicBoost current: val=%f(db)\n", dbVal);
	}
	hr = micControl_->GetEndpointVolumeRange(minDB, maxDB, stepDB);
	if ( SUCCEEDED(hr)){
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "EndpointVolume range: min=%f(db) max=%f(db) step=%f(db)\n", minDB, maxDB, stepDB);
		micControl_->GetEndpointVolume(dbVal);
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioCapture", "EndpointVolume current: val=%f(db)\n", dbVal);
	}

	// fire an client event, to audio manager
	callback_->OnAudioClientEvent(eCapture, AudioClientStarted, loopbackCapMode_?TRUE:FALSE);

    return true;
}

void WASAPICapture::Stop()
{
    HRESULT hr;

    if (shutdownEvent_)
    {
        SetEvent(shutdownEvent_);
    }


    if ( captureThread_)
    {
        ::WaitForSingleObject(captureThread_, INFINITE);
        ::CloseHandle(captureThread_);
        captureThread_ = NULL;
    }

    hr = audioClient_->Stop();
    if (FAILED(hr))
    {
		NativeMediaLogger4CPP::log(LogLevel::Error, "AudioCapure", "failed to stop auido client hr=0x%x", hr);
    }

	// fire an client event, to audio manager
	callback_->OnAudioClientEvent(eCapture, AudioClientStopped, loopbackCapMode_?TRUE:FALSE);

#ifdef AUDIO_UT
	if ( loopbackCapMode_) {
		wavFile_.Close();
	}
#endif

}

float WASAPICapture::GetPeakMeter()
{
	if( audioMeterInfo_)
	{
		float peak;
		HRESULT hr = audioMeterInfo_->GetPeakValue(&peak);
		if (SUCCEEDED(hr)) return peak;
	}
	return 0.0;
}

void  WASAPICapture::SetMicMute(bool bMute)
{
	micControl_->SetMicMute(bMute);
}

DWORD WASAPICapture::CaptureThreadEntry(LPVOID Context)
{
    WASAPICapture *capturer = static_cast<WASAPICapture *>(Context);
    return capturer->_CaptureThread();
}

BOOL WASAPICapture::_OutputData(BYTE *pData, UINT32 framesAvailable, UINT64  u64QPCPosition)
{
	void * pout = NULL;
	int len = 0;
	uint32_t sampleRate = 0;

#ifdef AUDIO_UT
	wavFile_.WriteData(pData, framesAvailable*frameSize_);
#endif
	if ( format_.nChannels > 2 ) return FALSE;

	// we output mono data only, we will remove all resampling code later...
	short * pMono =  (short*)pData;
	if ( format_.nChannels == 2) {
		pMono = resampler_.ReSamplingStereoToMono ( (short*)pData, framesAvailable);
	}

	// setup out param
	pout = pMono;
	len = framesAvailable*2;
	sampleRate = format_.nSamplesPerSec;

	if ( pout != NULL && len > 0 ){
		if (loopbackCapMode_){
			callback_->OnWriteLoopbackData(pout, len, sampleRate, u64QPCPosition);
		}else{
			callback_->OnWriteAudioData(pout, len, sampleRate, u64QPCPosition);
		}
	}
	return TRUE;
}

DWORD WASAPICapture::_CaptureLoop()
{
    bool stillPlaying = true;
    HRESULT hr;
    HANDLE waitArray[2] = {shutdownEvent_, samplesReadyEvent_ };
    while (stillPlaying)
    {
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:  // shutdownEvent_
            stillPlaying = false;   
            break;
        case WAIT_OBJECT_0 + 1:  // samplesReadyEvent_
            BYTE *pData;
            UINT32 framesAvailable;
            DWORD  flags;
			UINT64  u64QPCPosition;
            hr = captureClient_->GetBuffer(&pData, &framesAvailable, &flags, NULL, &u64QPCPosition);
            if (SUCCEEDED(hr))
            {
				_OutputData (pData, framesAvailable, u64QPCPosition);
                hr = captureClient_->ReleaseBuffer(framesAvailable);
                if (FAILED(hr)){
                    printf("Unable to release capture buffer: %x!\n", hr);
                }
            }
			else if ( AUDCLNT_S_BUFFER_EMPTY!=hr) // always exit on  error case
			{
				stillPlaying = false;
				break;
			}

            break;
        }
    }

	return 0;
}

DWORD WASAPICapture::_LoopbackCaptureLoop()
{
    bool    stillPlaying = true;
    HANDLE  waitArray[2] = {shutdownEvent_, hWakeUp_/*this is a waitable timer*/ };
	HRESULT hr;
	BYTE   *pData;
	UINT32  framesAvailable;
	DWORD   flags;
	DWORD   waitResult;
    UINT32  nNextPacketSize;
	UINT64  u64QPCPosition;

	// set the waitable timer
	LARGE_INTEGER liFirstFire;
	liFirstFire.QuadPart = -hnsDevicePeriod_ / 2; // negative means relative time
	LONG lTimeBetweenFires = (LONG)hnsDevicePeriod_ / 2 / (10 * 1000); // convert to milliseconds
	BOOL bOK = ::SetWaitableTimer(hWakeUp_, &liFirstFire, lTimeBetweenFires, NULL, NULL, FALSE);
	if ( bOK == FALSE) return -1;

    while (stillPlaying)
    {
		// let's drain data when they are available
        for ( hr = captureClient_->GetNextPacketSize(&nNextPacketSize);
            SUCCEEDED(hr) && nNextPacketSize > 0;
            hr = captureClient_->GetNextPacketSize(&nNextPacketSize))
		{
			hr = captureClient_->GetBuffer(&pData, &framesAvailable, &flags, NULL, &u64QPCPosition);
			if (SUCCEEDED(hr))
			{
				_OutputData (pData, framesAvailable, u64QPCPosition);
				hr = captureClient_->ReleaseBuffer(framesAvailable);
				if (FAILED(hr)){
					printf("Unable to release capture buffer: %x!\n", hr);
				}
			}
			else if ( AUDCLNT_S_BUFFER_EMPTY!=hr) // always exit on  error case
			{
				stillPlaying = false;
				break;
			}
		}
		// GetNextPacketSize failed!
		if (FAILED(hr)) stillPlaying = false;

		// wait for next wakeup
        waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:  // shutdownEvent_
            stillPlaying = false;   
            break;
        case WAIT_OBJECT_0 + 1:  // hWakeUp_
            break;
		default: // This is the error case!
            stillPlaying = false;   
            break;
        }
    }

	// cancel the waitable
	::CancelWaitableTimer(hWakeUp_); 
	return 0;
}

DWORD WASAPICapture::_CaptureThread()
{
    HANDLE mmcssHandle = NULL;
    DWORD mmcssTaskIndex = 0;

    HRESULT hr = ::CoInitialize(NULL);
    if (FAILED(hr)) return hr;
 
    mmcssHandle = ::AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
    if (mmcssHandle != NULL){
		BOOL ret = ::AvSetMmThreadPriority(mmcssHandle,AVRT_PRIORITY_HIGH );
    }

	if (loopbackCapMode_){
		_LoopbackCaptureLoop();
	}else{
		_CaptureLoop();
	}

	if ( mmcssHandle){
        ::AvRevertMmThreadCharacteristics(mmcssHandle);
    }

    CoUninitialize();
    return 0;
}

} // namespace WinRTCSDK
