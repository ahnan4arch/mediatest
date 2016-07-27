// This is the main DLL file.

#include "stdafx.h"
#include <new.h>

#include "WinRTCSDK.h"

// Win32 libs
#pragma comment (lib, "User32.lib")
#pragma comment (lib, "Advapi32.lib")

// The static libs needed by this DLL. This libs used at source level
#pragma comment (lib, "../release/algorithm.lib")
#pragma comment (lib, "../release/booat.lib")
#pragma comment (lib, "../release/callcontrol.lib")
#pragma comment (lib, "../release/mp.lib")
#pragma comment (lib, "../release/rtcsdk.lib")
#pragma comment (lib, "../release/audio_net_adaptor.lib") // ANA depends on booat, so we use it at source level!
// NetTool
#pragma comment (lib, "../Release/NetTool.lib")

// Thirdparty libs
#pragma comment (lib, "../../../../../nemolib/thirdparty/json-c/dist/windows/lib/json.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/libb64/dist/windows/lib/b64.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/sqlite/dist/windows/lib/sqlite.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/openssl/dist/windows/lib/libeay32.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/aac/dist/windows/lib/aac.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/zlib/dist/windows/lib/zlib.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/libjpeg/dist/windows/lib/jpeglib.lib")
#pragma comment (lib, "../../../../../nemolib/thirdparty/libyuv/dist/windows/lib/yuv.lib")
// Audio libs
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_enhancement/library/windows/audio_enhancement.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_codec/lark/library/windows/lark.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_resampler/library/windows/audio_resampler.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_resampler2/library/windows/audio_resampler2.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_common/library/windows/audio_common.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/util/library/windows/mojoutil.lib")
// Audio libs - opus
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_codec/opus/library/windows/celt.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_codec/opus/library/windows/opus.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_codec/opus/library/windows/silk_common.lib")
#pragma comment (lib, "../../../../media_processor/algorithm/audio/audio_codec/opus/library/windows/silk_fixed.lib")

// video libs
//#pragma comment (lib, "../lib/NemoCodec.lib")

// new video libs
#pragma comment (lib, "../lib/codec_wrapper.lib")
#pragma comment (lib, "../lib/common.lib")
#pragma comment (lib, "../lib/h264_core.lib")
#pragma comment (lib, "../lib/h264_encoder.lib")
#pragma comment (lib, "../lib/h26x_core.lib")
#pragma comment (lib, "../lib/image_proc.lib")
#pragma comment (lib, "../lib/palm_decoder.lib")

#define HEAP_DEBUGGING

namespace WinRTCSDK {

static gcroot <LoggerDelegate^> __sdk_logger_delegate; // must keep a ref on native heap
static void __sdk_logger_callback(int level, const char* tag, const char* buffer)
{
    System::String^ strTag = gcnew System::String(tag);
    System::String^ strBuf = gcnew System::String(buffer);
	static_cast<LoggerDelegate^>(__sdk_logger_delegate)(level, strTag, strBuf);
}

// This is used for heap debugging,
static int _handle_program_memory_depletion( size_t )
{
   return 1; // keep retry
}

WinRTCSDK::WinRTCSDK (WinRTCSDKParam ^ p, IWinRTCSDKSerializedCallbacks ^ mngedCallback, LoggerDelegate^ logger)
{
    RTCSDKParam param;
    param.logpath =         MarshalString(p->logpath);

    param.logLevel =        (RTCSDK::LogLevel)p->logLevel;
    param.sysType =         (SoftSysType)p->sysType;

    param.nemoMachineType = MarshalString(p->nemoMachineType);
    param.deviceModel =     MarshalString(p->deviceModel);
    param.osVersion =       MarshalString(p->osVersion);
    param.swVersion =       MarshalString(p->swVersion);
    param.hwVersion =       MarshalString(p->hwVersion);
    param.mpDebugFodler =   MarshalString(p->mpDebugFodler);
    param.mpDebugMask =     p->mpDebugMask;
    param.recordingStorageFolder = MarshalString(p->recordingStorageFolder);
    param.sdkFolder =       MarshalString(p->sdkFolder);

	// This is for heap debugging
#ifdef HEAP_DEBUGGING
	_set_new_handler( _handle_program_memory_depletion );
#endif

	// setup logger first
	__sdk_logger_delegate = logger;
	BOOAT::Log::setLogCallback(__sdk_logger_callback);

	// native sdk callbacks
	sdkObserver_ = new IRTCSDKSerializableContextObserverImpl( mngedCallback );

	// native context
	sdkContext_ = IRTCSDKContext::createContext(sdkObserver_->getContextObserver(), param);

}

WinRTCSDK::!WinRTCSDK ()
{
}

WinRTCSDK::~WinRTCSDK ()
{
}

void WinRTCSDK::shutdown()
{
    IRTCSDKContext::destoryContext(sdkContext_);
	delete sdkObserver_;
	sdkContext_ = NULL;
	sdkObserver_ = NULL;
	__sdk_logger_delegate = nullptr;
}

void WinRTCSDK::setConfig( String ^config)
{
	sdkContext_->setConfig(MarshalString(config).c_str());
}

void WinRTCSDK::notifyNetworkChanged( String ^ address, int networkType, String ^ netKey)
{
	sdkContext_->notifyNetworkChanged(MarshalString(address).c_str(), 
		(RTCSDK::NetWorkType)networkType, MarshalString(netKey).c_str());
}

void WinRTCSDK::updateConfig( String ^config)
{
	sdkContext_->updateStatus(MarshalString(config).c_str());
}

void WinRTCSDK::notifyCallMsgReceived( String ^callMsg)
{

	sdkContext_->notifyCallMsgReceived(MarshalString(callMsg));
}

void WinRTCSDK::makeCall( int callIndex, String ^ uri, int callmode, String ^ append, String ^ localType)
{
	std::vector<std::string> moreUris; moreUris.clear();
	sdkContext_->makeCall(callIndex, MarshalString(uri), moreUris, 
		(RTCSDK::CallMode)callmode,MarshalString( append), MarshalString(localType));
}

void WinRTCSDK::answerCall( int callIndex, String ^ uri, int rate, int peerType, int callMode, bool isReplace)
{
	sdkContext_->answerCall(callIndex, MarshalString(uri), rate, (RTCSDK::PeerType)peerType, (RTCSDK::CallMode)callMode, isReplace);
}

void WinRTCSDK::dropCall( int callIndex, String ^ reason)
{
	sdkContext_->dropCall(callIndex, MarshalString(reason));
}

void WinRTCSDK::startRecording( String ^ author, String ^ deviceId, int res, String ^ extraMeteData)
{
	sdkContext_->startRecording(MarshalString(author), MarshalString(deviceId),
		(RTCSDK::VideoResolution)res, MarshalString(extraMeteData));
}

void WinRTCSDK::stopRecording( String ^ author)
{
	sdkContext_->stopRecording(MarshalString(author));
}

void WinRTCSDK::checkRecordingResult( int callIndex, String ^ controlUri, int reason)
{
	sdkContext_->checkRecordingResult(callIndex, MarshalString(controlUri), (RTCSDK::RecordReason)reason);
}

void WinRTCSDK::getStatistics(String ^% atx,String ^% arx,String ^% vtx,String ^% vrx, String ^% dba, String ^% etc)
{
	RTCSDK::GeneralStatistics st=sdkContext_->getStatistics();
	std::string str_dba = st.dbaStatistics ;
	
	std::string arx_parm = st.mediaParam.arxParam.getStr();
	std::string str_arx = arx_parm + st.mediaStatistics.AudioRxStatistics2String(st.mediaStatistics.audioRxStatisticsList);

	std::string atx_parm = st.mediaParam.atxParam.getStr();
	std::string str_atx = atx_parm + st.mediaStatistics.AudioTxStatistics2String(st.mediaStatistics.audioTxStatisticsList);

	std::string vtx_parm = st.mediaParam.pvtxParam.getStr();
	std::string str_vtx = vtx_parm + st.mediaStatistics.VideoTxStatistics2String(st.mediaStatistics.videoTxStatisticsList);

	std::string vrx_parm = st.mediaParam.pvrxParam.getStr();
	std::string str_vrx = vrx_parm + st.mediaStatistics.VideoRxStatistics2String(st.mediaStatistics.videoRxStatisticsList);

	std::string str_etc = st.otherStatistics2String(st);
	
	dba = gcnew String (str_dba.c_str());
	arx = gcnew String (str_arx.c_str());
	atx = gcnew String (str_atx.c_str());
	vtx = gcnew String (str_vtx.c_str());
	vrx = gcnew String (str_vrx.c_str());
	etc = gcnew String (str_etc.c_str());
}

void WinRTCSDK::enableLipSync( bool enabled)
{
	sdkContext_->enableLipSync(enabled);
}

void WinRTCSDK::enableRD( bool enabled)
{
	sdkContext_->enableRD(enabled);
}

void WinRTCSDK::enableDBA( bool enabled)
{
	sdkContext_->featureControl(RTCSDK::FeatureId::FeatureId_DBA, enabled);
}

void WinRTCSDK::enableAEC( bool enabled)
{
	sdkContext_->enableAudioAEC(enabled);
}

void WinRTCSDK::enableDRC( bool enabled)
{
	sdkContext_->enableDRC(enabled);
}

void WinRTCSDK::setAudioEvent( String ^ audioEvent)
{
	sdkContext_->setAudioEvent(MarshalString(audioEvent));
}

void WinRTCSDK::setAudioConfig( String ^ name, int value)
{
	sdkContext_->setAudioConfig( MarshalString(name), value);
}

int WinRTCSDK::prepareCall()
{
	return sdkContext_->prepareCall();
}

void WinRTCSDK::mute( int callIndex, MediaMuteType mediaType, bool isMute)
{
	sdkContext_->mute(callIndex, (RTCSDK::MediaType)mediaType, isMute);
}

void WinRTCSDK::farEndHardwareControl( int participantId, int command, int angle)
{
	sdkContext_->farEndHardwareControl(participantId, (RTCSDK::FECCCommand)command, angle);
}

void WinRTCSDK::reset()
{
	sdkContext_->reset();
}

void WinRTCSDK::heartbeat( long timestamp)
{
	sdkContext_->heartbeat(timestamp);
}

void WinRTCSDK::updateVideoRelayConfig( String ^ config)
{
	sdkContext_->updateVideoRelayConfig(MarshalString(config));
}

void WinRTCSDK::contentStart( int callIndex, int clientId, int type)
{
	sdkContext_->contentStart(callIndex, clientId, (RTCSDK::ShareType)type);
}

void WinRTCSDK::contentStop( int callIndex, int clientId)
{
	sdkContext_->contentStop(callIndex, clientId);
}

void WinRTCSDK::notifyCDRResult( bool isSuccess, int cdrId)
{
	sdkContext_->notifyCDRResult(isSuccess, cdrId);
}

void WinRTCSDK::dialDtmf( int callIndex, String ^ peerUri, String ^ content)
{
	sdkContext_->dialDtmf(callIndex, MarshalString(peerUri), MarshalString(content));
}

// aec
void WinRTCSDK::setAudioLpbSupported(bool isSupport)
{
	sdkContext_->setAudioLpbSupported(isSupport);
}

void WinRTCSDK::setAudioFeature(unsigned int audioFeature)
{
	sdkContext_->setAudioFeature(audioFeature);
}

void WinRTCSDK::setAudioDeviceParam(AudioDeviceTypeManaged deviceType, AudioDeviceParamManaged^ param)
{
	AudioDeviceParam paramN;
	// wave format
	paramN.format.formatType= param->format->formatType;
	paramN.format.samplesPerSec= param->format->samplesPerSec;
	paramN.format.numChannels= param->format->numChannels;
	paramN.format.channelMask= param->format->channelMask;
	paramN.format.bitsPerSample= param->format->bitsPerSample;
	paramN.format.containerSize= param->format->containerSize;
	// device info
	paramN.deviceId = MarshalString (param->deviceId);
	paramN.minEndpointLevelDB = param->minEndpointLevelDB;
	paramN.maxEndpointLevelDB = param->maxEndpointLevelDB;
	paramN.endpointSteppingDB = param->endpointSteppingDB;
	paramN.analogVolumeSupport = param->analogVolumeSupport;
	paramN.minBoostLevelDB = param->minBoostLevelDB;
	paramN.maxBoostLevelDB = param->maxBoostLevelDB;
	paramN.boostSteppingDB = param->boostSteppingDB;

	sdkContext_->setAudioDeviceParam((AudioDeviceType)deviceType, paramN);
}

void WinRTCSDK::addOtherCallee( int callIndex, array< String ^> ^ calleeUri, int uriNumber)
{
	std::vector<std::string> uris;
	for (int i = 0; i < uriNumber; ++ i)
	{
		uris.push_back( MarshalString(calleeUri[i]));
	}
	sdkContext_->addOtherCallee(callIndex, "", uris);
}

void WinRTCSDK::saveDump()
{
	sdkContext_->saveDump();
}

void WinRTCSDK::setContentMode( bool isThumbnail)
{
	sdkContext_->setContentMode(isThumbnail);
}

void WinRTCSDK::setLayoutForceTarget(unsigned int participantId)
{
	sdkContext_->setLayoutForceTarget(participantId);
}

//////////////////////////////////////
// some global functions
std::string  MarshalString ( String ^ s)
{
	const char* chars = 
		(const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
	std::string os = chars;
	Marshal::FreeHGlobal(IntPtr((void*)chars));
	return os;
}

std::wstring MarshalStringW ( String ^ s)
{
	const wchar_t* chars = 
		(const wchar_t*)(Marshal::StringToHGlobalUni(s)).ToPointer();
	std::wstring  os = chars;
	Marshal::FreeHGlobal(IntPtr((void*)chars));
	return os;
}

}
