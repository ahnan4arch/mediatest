///////////////////////////////////////////////////////
// Managed types and Native types (structs) goes here
//  please only define types that could be
//  used in multiple modules here!
#pragma once
#include <string>
#include <stdint.h>

using namespace System;
using System::String;
using System::Runtime::InteropServices::Marshal;

namespace WinRTCSDK{

public delegate void LoggerDelegate (int level, String^ tag, String^ logstr);

/////////////////////////////////////////////////////////
// clr types. can be used in c#(NemoFramework.dll)
public enum class MediaDevType
{
	eAudioCapture,
	eAudioRender,
	eVideoCapture
};

public enum class MediaInitResult
{
	ENMEDIA_INIT_OK = 0,
	ENMEDIA_INIT_VIDEO_FAILED,
	ENMEDIA_INIT_AUDIO_FAILED,
	ENMEDIA_INIT_UNKNOWN_FAILED
};

public enum class MediaDevEvent
{
	eNewDevArrived,
	eDevRemoved,
	eCurrentDevLost
};

public ref class VideoStreamInfo 
{
public:
	String ^ sourceID;
	bool     bAudioMuted;
};

public ref class MediaDevInfoManaged
{
public:
	MediaDevInfoManaged() { audioDevInfo = gcnew __AudioDevInfo();}
public :
	String ^ symbolicLink; // dev symbolic link
	String ^ devFriendlyName;
	MediaDevType devType;
	ref class __AudioDevInfo
	{
public :
		String ^ devInterfaceName; // the audio adapter name, used to check homologous device	
		String ^ containerId; // dev enclosure ID, for audio devices, used to check homologous device
		int      formFactor;
		bool     bConsoleRole;
		bool     bMultimediaRole;
		bool     bCommunicationsRole;
	}^ audioDevInfo;

};

///////////////////////////////////////////////////////////////////////////////////
// following types are just managed versions of RTCSDK types (to be used in c#)
//   please always check the the rtcsdk_interface.h header to keep them match!
public ref struct WinRTCSDKParam
{
    String ^     logpath;
    int          logLevel; // map to LogLevel
    int          sysType;
    String ^     nemoMachineType;
    String ^     deviceModel;
    String ^     osVersion;
    String ^     swVersion;
    String ^     hwVersion;
    String ^     mpDebugFodler;
    unsigned int mpDebugMask;
    String ^     recordingStorageFolder;
    String ^     sdkFolder;
};

public enum class AudioDeviceTypeManaged
{
    AudioDeviceCapture,
    AudioDeviceRender
};

public enum class  AudioDeviceVolumeTypeManaged
{
    AudioDeviceVolumeCaptureEndpoint,
    AudioDeviceVolumeCaptureBoost,
    AudioDeviceVolumeRenderEndpoint,
    AudioDeviceVolumeRenderSession
};

public ref class PCMFormatManaged
{
public:
	UInt32 formatType; // 0 : PCM, 1: IEEE float
    UInt32 samplesPerSec;
    UInt32 numChannels;
    UInt32 channelMask;
    UInt32 bitsPerSample; // valid size per sample in bits
    UInt32 containerSize; // container size per sample in bits
};

public ref class AudioDeviceParamManaged
{
public :
	AudioDeviceParamManaged() { 
		deviceId = gcnew String ("");
		format = gcnew PCMFormatManaged();
	}
    String ^ deviceId;
	PCMFormatManaged ^ format;

    float minEndpointLevelDB;
    float maxEndpointLevelDB;
    float endpointSteppingDB;

    bool analogVolumeSupport;
    float minBoostLevelDB;
    float maxBoostLevelDB;
    float boostSteppingDB;
};

// values must comply with RTCSDK::MediaType
public enum class MediaMuteType
{
    PeopleVideo = 0,
    ContentVideo,
    Audio,
    Speaker,
};

////////////////////////////////////////////////////////
// Native types only used in c++ code in this module
struct MediaDevInfo
{
	std::wstring symbolicLink; // dev symbolic link
	std::wstring devFriendlyName;
	MediaDevType devType;
	struct {
		std::wstring devInterfaceName; // the audio adapter name, used to check homologous device	
		std::wstring containerId; // dev enclosure ID, for audio devices, used to check homologous device
		uint32_t     formFactor; // please refer to EndpointFormFactor in Mmdeviceapi.h
		bool         bConsoleRole;
		bool         bMultimediaRole;
		bool         bCommunicationsRole;
	} audioDevInfo;

	MediaDevInfo(){
		audioDevInfo.formFactor = 0;
		audioDevInfo.bConsoleRole = false;
		audioDevInfo.bMultimediaRole = false;
		audioDevInfo.bCommunicationsRole = false;
	}
};

}