#include "StdAfx.h"

#include <assert.h>
#include <avrt.h>
#include <stdio.h>

#include "WASAPIRenderer.h"
#include "NativeMediaLog.h"

namespace WinRTCSDK{

WASAPIRenderer::WASAPIRenderer(_com_ptr_IMMDevice endpoint, IAudioClientCallbacks *callback, 
		ISessionEventHandler *sessEvthandler, GUID eventContext) : 
    endpoint_(endpoint),
    audioClient_(NULL),
    audioSessionControl_(NULL),
	audioMeterInfo_(NULL),
	simpleVolume_(NULL),
	sessionEvents_(NULL),
	callback_ (callback),
    renderClient_(NULL),
    renderThread_(NULL),
    shutdownEvent_(NULL),
    mixFormat_(NULL),
    samplesReadyEvent_(NULL),
	resampler_(),
	lastLevelScalar_(0.0),
	eventContext_(eventContext)
{
	sessionEvents_ = new AudioSessionEvents(sessEvthandler, eRender);
}

WASAPIRenderer::~WASAPIRenderer(void) 
{
	delete sessionEvents_;
}

const WAVEFORMATEX * WASAPIRenderer::GetFormat()
{
	return &format_;
}

bool WASAPIRenderer::_SetupFormat()
{
    HRESULT hr = audioClient_->GetMixFormat(&mixFormat_);
    if (FAILED(hr)) return false;

	WAVEFORMATEXTENSIBLE * pfmtext;
	bool isInt;

	// Debug the mix format for shared-mode
    switch ( mixFormat_->wFormatTag )
	{
	case WAVE_FORMAT_PCM :
		isInt = true;
		break;
	case WAVE_FORMAT_IEEE_FLOAT:
		isInt = false;
		break;
	case WAVE_FORMAT_EXTENSIBLE:
		pfmtext = reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mixFormat_);
		if (pfmtext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM){
			isInt = false;
		}else if (pfmtext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT){
			isInt = true;
		}
	}

	// Set up WAV format . 
    memset(&format_, 0, sizeof(WAVEFORMATEX)); 
    format_.wFormatTag = WAVE_FORMAT_PCM; 
    format_.nChannels = 2;
    format_.wBitsPerSample = 16; // we should use 16 bits data only
    format_.nSamplesPerSec = mixFormat_->nSamplesPerSec; 
    format_.nBlockAlign = (format_.wBitsPerSample / 8) * format_.nChannels; 
    format_.nAvgBytesPerSec = format_.nSamplesPerSec * format_.nBlockAlign; 

    frameSize_ = format_.nBlockAlign;

	WAVEFORMATEX *ppClosestMatch;
	hr =  audioClient_->IsFormatSupported(
				AUDCLNT_SHAREMODE_SHARED, &format_, &ppClosestMatch);

	if (ppClosestMatch)::CoTaskMemFree(ppClosestMatch);

	if (hr == S_OK){
		switch (format_.nSamplesPerSec){
		case 48000:
		case 44100:
			return true;
		default:
			break;
		}
	}

    return false;
}

bool WASAPIRenderer::Initialize()
{
	bMuted_ = false;
    shutdownEvent_ = ::CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
    samplesReadyEvent_ = ::CreateEventEx(NULL, NULL, 0, EVENT_MODIFY_STATE | SYNCHRONIZE);
 
	// activate the audio client
    HRESULT hr = endpoint_->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER, NULL, 
		reinterpret_cast<void **>(&audioClient_));
	if(FAILED(hr)) return false;

	// Get endpoint volume interface
	hr = endpoint_->Activate(__uuidof(IAudioEndpointVolume), 
		CLSCTX_ALL, NULL, reinterpret_cast<void**>(&endpointVolume_));
	if(FAILED(hr)) return false;

	if ( !_SetupFormat ()) return false;

    engineLatency_ = 0 * 10000 ;// use device default
    hr = audioClient_->Initialize(AUDCLNT_SHAREMODE_SHARED, 
        AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST, 
        engineLatency_, 0, &format_, NULL);

	// Get simple volume interface
	hr = audioClient_->GetService(__uuidof(ISimpleAudioVolume), (void**)&simpleVolume_);
	if(FAILED(hr)) return false;
	simpleVolume_->SetMasterVolume(0.8f, &eventContext_); // default to 80%

    if (FAILED(hr)) return false;

    hr = audioClient_->GetBufferSize(&bufferSize_);
    if(FAILED(hr)){ return false; }

    hr = audioClient_->SetEventHandle(samplesReadyEvent_);
    if(FAILED(hr)){ return false; }

    hr = audioClient_->GetService(IID_PPV_ARGS(&renderClient_));
    if(FAILED(hr)){ return false; }

	hr= audioClient_->GetService(IID_PPV_ARGS(&audioSessionControl_));
    if(FAILED(hr)){ return false; }

    hr = audioSessionControl_->RegisterAudioSessionNotification(sessionEvents_);
    if(FAILED(hr)){ return false; }
	
    hr = endpoint_->Activate(__uuidof(IAudioMeterInformation), 
		CLSCTX_INPROC_SERVER, NULL, reinterpret_cast<void **>(&audioMeterInfo_));
    if(FAILED(hr)){ return false; }
   
	return true;
}

void WASAPIRenderer::Shutdown()
{
    if (renderThread_)
    {
        SetEvent(shutdownEvent_);
        WaitForSingleObject(renderThread_, INFINITE);
        CloseHandle(renderThread_);
        renderThread_ = NULL;
    }

    if (shutdownEvent_)
    {
        CloseHandle(shutdownEvent_);
        shutdownEvent_ = NULL;
    }

    if (samplesReadyEvent_)
    {
        CloseHandle(samplesReadyEvent_);
        samplesReadyEvent_ = NULL;
    }

    endpoint_     = NULL;
    audioClient_  = NULL;
    renderClient_ = NULL;
	simpleVolume_ = NULL;


    if (mixFormat_){
        CoTaskMemFree(mixFormat_);
        mixFormat_ = NULL;
    }

    if( audioSessionControl_ != NULL)
    {
        HRESULT hr = audioSessionControl_->UnregisterAudioSessionNotification(sessionEvents_);
        if (FAILED(hr)) {
            printf("Unable to unregister for session notifications: %x\n", hr);
        }
    }

    audioSessionControl_ = NULL;
}

bool WASAPIRenderer::Start()
{
	HRESULT hr;
	BYTE *pData;

	hr = renderClient_->GetBuffer(bufferSize_, &pData);
	if (FAILED(hr)){
		return false;
	}

	hr = renderClient_->ReleaseBuffer(bufferSize_, AUDCLNT_BUFFERFLAGS_SILENT);
	if (FAILED(hr)){
		return false;
	}

    renderThread_ = CreateThread(NULL, 0, _RenderThreadEntry, this, 0, NULL);
    if (renderThread_ == NULL){
        return false;
    }

    hr = audioClient_->Start();
    if (FAILED(hr)){
        return false;
    }

	NativeMediaLogger4CPP::log(LogLevel::Info, "AudioRender", "capture started.");
	// log audio volumes
	float dbVal=0, minDB=0, maxDB=0, stepDB=0;
	hr = endpointVolume_->GetVolumeRange(&minDB, &maxDB, &stepDB);
	if ( SUCCEEDED(hr)){
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioRender", "EndpointVolume range: min=%f(db) max=%f(db) step=%f(db)\n", minDB, maxDB, stepDB);
		endpointVolume_->GetMasterVolumeLevel(&dbVal);
		NativeMediaLogger4CPP::log(LogLevel::Info, "AudioRender", "EndpointVolume current: val=%f(db)\n", dbVal);
	}

	// fire an client event, to audio manager
	callback_->OnAudioClientEvent(eRender, AudioClientStarted, FALSE);

    return true;
}

void WASAPIRenderer::Stop()
{
    HRESULT hr;
    if (shutdownEvent_)
    {
        SetEvent(shutdownEvent_);
    }

    if (renderThread_)
    {
        WaitForSingleObject(renderThread_, INFINITE);
        CloseHandle(renderThread_);
        renderThread_ = NULL;
    }

    hr = audioClient_->Stop();
    if (FAILED(hr))
    {
		NativeMediaLogger4CPP::log(LogLevel::Error, "AudioRender", "failed to stop auido client hr=0x%x", hr);
    }

	// fire an client event, to audio manager
	callback_->OnAudioClientEvent(eRender, AudioClientStopped, FALSE);

}

float WASAPIRenderer::GetPeakMeter()
{
	if( audioMeterInfo_)
	{
		float peak;
		HRESULT hr = audioMeterInfo_->GetPeakValue(&peak);
		if (SUCCEEDED(hr)) return peak;
	}
	return 0.0;
}

void WASAPIRenderer::SetMute (bool bMute)
{
	if (simpleVolume_==NULL)return;
	if (bMuted_ == bMute) return; // NOP
	HRESULT hr;
	bMuted_ = bMute;
	if ( bMute ){
		// store last level
		HRESULT hr = simpleVolume_->GetMasterVolume(&lastLevelScalar_);
		// set level to zero
		hr = simpleVolume_->SetMasterVolume(0.0, &eventContext_);
	}else{
		// restore the volume level
		hr = simpleVolume_->SetMasterVolume(lastLevelScalar_, &eventContext_);
	}	
}

HRESULT WASAPIRenderer::SetSimpleVolume( int volume/*[0, 100]*/)
{
	if (bMuted_) return S_OK; // doesn't change volume when muted

	if (simpleVolume_ == NULL) return E_FAIL;

	float flevel = volume * 1.0f / 100.0f ; //[0.0, 1.0] 
	HRESULT hr = simpleVolume_->SetMasterVolume(flevel, &eventContext_);

	return hr;
}

HRESULT WASAPIRenderer::GetSimpleVolume( int &volume/*[0, 100]*/)
{
	float flevel=0.0f;
	HRESULT hr = simpleVolume_->GetMasterVolume(&flevel);

	volume = (int)( flevel * 100.0f);
	return hr;
}

// Win32 thread entry
DWORD WASAPIRenderer::_RenderThreadEntry(LPVOID Context)
{
    WASAPIRenderer *renderer = static_cast<WASAPIRenderer *>(Context);
    return renderer->_RenderThread();
}

int WASAPIRenderer::_InputData ( BYTE *pData, UINT32 frames)
{
	bool ret;
	int actFrames ;
	// we will remove this ugly code, when audio pipeline can do resampling things...
	// the render is always initialized in stereo format
	if ( format_.nSamplesPerSec == 48000 ){
		ret = callback_->OnReadAudioData(pData, frames*2/*the pipeline always provide mono data*/);
		short * pStereo = resampler_.ReSamplingMonoToStereo((short*)pData, frames);
		::memcpy(pData, pStereo, frames*frameSize_);
		actFrames = frames;
	}else if (format_.nSamplesPerSec == 44100){
		int toReadFrames48k = frames /147 * 160; // 480:441=160:147
		ret = callback_->OnReadAudioData(pData, toReadFrames48k * 2/*the pipeline always provide mono data*/);
		short * p441k = resampler_.ReSampling48KMonoTo44p1KMono((short*)pData, toReadFrames48k, actFrames);
		short * pStereo = resampler_.ReSamplingMonoToStereo((short*)p441k, actFrames);
		::memcpy(pData, pStereo, actFrames*frameSize_);
	}
	return actFrames;
}

DWORD WASAPIRenderer::_RenderThread()
{
    bool stillPlaying = true;
    HANDLE waitArray[2] = {shutdownEvent_, samplesReadyEvent_};
    HANDLE mmcssHandle = NULL;
    DWORD mmcssTaskIndex = 0;

    HRESULT hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(hr)){
        return hr;
    }

    mmcssHandle = ::AvSetMmThreadCharacteristics(L"Audio", &mmcssTaskIndex);
    if (mmcssHandle ){
		BOOL ret = ::AvSetMmThreadPriority(mmcssHandle, AVRT_PRIORITY_HIGH );
    }

    while (stillPlaying)
    {
        HRESULT hr;
        DWORD waitResult = WaitForMultipleObjects(2, waitArray, FALSE, INFINITE);
        switch (waitResult)
        {
        case WAIT_OBJECT_0 + 0:     // ShutdownEvent_
            stillPlaying = false;
            break;

        case WAIT_OBJECT_0 + 1:     // samplesReadyEvent_
            BYTE *pData;
            UINT32 padding;
            UINT32 framesAvailable;

            hr = audioClient_->GetCurrentPadding(&padding);
			if (FAILED(hr) ) stillPlaying = false; // always end the loop on error!

            if (SUCCEEDED(hr))
            {
				// padding is the frames still need to play
                framesAvailable = bufferSize_ - padding;
				switch (format_.nSamplesPerSec)
				{ 
				case 48000:
					if (framesAvailable >480) framesAvailable = 480; // input 10ms data
					break;
				case 44100: // need to resample
					break;
				}

                hr = renderClient_->GetBuffer(framesAvailable, &pData);
                if (SUCCEEDED(hr) && pData != NULL){
					int actFrames = _InputData(pData, framesAvailable);
					hr = renderClient_->ReleaseBuffer(actFrames, 0);
                    
					if (!SUCCEEDED(hr))stillPlaying = false;

                }else{
                    stillPlaying = false;
                }
            }
            break;
        }
    } // while

    if (mmcssHandle){
        ::AvRevertMmThreadCharacteristics(mmcssHandle);
    }

    CoUninitialize();
    return 0;
}

} // namespace WinRTCSDK