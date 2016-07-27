// define smart pointer types of WASAPI Interfaces
#pragma once
#include <comip.h>
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include <Endpointvolume.h>

// I'm not planning to use ATL/MFC, so that you see _com_ptr_t instead of ATL::CComPtr
namespace WinRTCSDK{

typedef _com_ptr_t < _com_IIID<ISimpleAudioVolume, &__uuidof(ISimpleAudioVolume) > > _com_ptr_ISimpleAudioVolume;
typedef _com_ptr_t < _com_IIID<IAudioEndpointVolume, &__uuidof(IAudioEndpointVolume) > > _com_ptr_IAudioEndpointVolume;
typedef _com_ptr_t < _com_IIID<IAudioSessionControl, &__uuidof(IAudioSessionControl) > > _com_ptr_IAudioSessionControl;
typedef _com_ptr_t < _com_IIID<IAudioClient, &__uuidof(IAudioClient) > > _com_ptr_IAudioClient;
typedef _com_ptr_t < _com_IIID<IAudioCaptureClient, &__uuidof(IAudioCaptureClient) > > _com_ptr_IAudioCaptureClient;
typedef _com_ptr_t < _com_IIID<IAudioRenderClient, &__uuidof(IAudioRenderClient) > > _com_ptr_IAudioRenderClient;
typedef _com_ptr_t < _com_IIID<IAudioMeterInformation, &__uuidof(IAudioMeterInformation) > > _com_ptr_IAudioMeterInformation;
typedef _com_ptr_t < _com_IIID<IMMEndpoint, &__uuidof(IMMEndpoint) > > _com_ptr_IMMEndpointe;
typedef _com_ptr_t < _com_IIID<IMMDevice, &__uuidof(IMMDevice) > > _com_ptr_IMMDevice;
typedef _com_ptr_t < _com_IIID<IMMDeviceEnumerator, &__uuidof(IMMDeviceEnumerator) > > _com_ptr_IMMDeviceEnumerator;
typedef _com_ptr_t < _com_IIID<IMMDeviceCollection, &__uuidof(IMMDeviceCollection) > > _com_ptr_IMMDeviceCollection;
typedef _com_ptr_t < _com_IIID<IPart, &__uuidof(IPart) > > _com_ptr_IPart;
typedef _com_ptr_t < _com_IIID<IPartsList, &__uuidof(IPartsList) > > _com_ptr_IPartsList;
typedef _com_ptr_t < _com_IIID<IConnector, &__uuidof(IConnector) > > _com_ptr_IConnector;
typedef _com_ptr_t < _com_IIID<IDeviceTopology, &__uuidof(IDeviceTopology) > > _com_ptr_IDeviceTopology;
typedef _com_ptr_t < _com_IIID<IAudioVolumeLevel, &__uuidof(IAudioVolumeLevel) > > _com_ptr_IAudioVolumeLevel;
typedef _com_ptr_t < _com_IIID<IAudioLoudness, &__uuidof(IAudioLoudness) > > _com_ptr_IAudioLoudness;
typedef _com_ptr_t < _com_IIID<IAudioAutoGainControl, &__uuidof(IAudioAutoGainControl) > > _com_ptr_IAudioAutoGainControl;
	
}