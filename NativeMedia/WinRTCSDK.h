#pragma once
#include "WinRTCSDKCallbacks.h"
#include "WinRTCSDKTypes.h"
using namespace System;
using System::String;
using System::Runtime::InteropServices::Marshal;

namespace WinRTCSDK {

public ref class WinRTCSDK
{
public:
	WinRTCSDK (WinRTCSDKParam ^ p, IWinRTCSDKSerializedCallbacks ^ mngedCallback, LoggerDelegate^ logger) ;
	!WinRTCSDK ();
	~WinRTCSDK (); // invoked by dispose
public:
	void shutdown(); // an extra method to do cleanup

public:
	void setConfig( String ^config);
	void notifyNetworkChanged( String ^ address, int networkType, String ^ netKey);
	void updateConfig( String ^config);
	void notifyCallMsgReceived( String ^callMsg);
	void makeCall( int callIndex, String ^ uri, int callmode, String ^ append, String ^ localType);
	void answerCall( int callIndex, String ^ uri, int rate, int peerType, int callMode, bool isReplace);
	void dropCall( int callIndex, String ^ reason);
	void startRecording( String ^ author, String ^ deviceId, int res, String ^ extraMeteData);
	void stopRecording( String ^ author);
	void checkRecordingResult( int callIndex, String ^ controlUri, int reason);
	void getStatistics(String ^% atx,String ^% arx,String ^% vtx,String ^% vrx, String ^% dba, String ^% etc);
	void enableLipSync( bool enabled);
	void enableRD( bool enabled);
	void enableDBA( bool enabled);
	void enableAEC( bool enabled);
	void enableDRC( bool enabled);
	void setAudioEvent( String ^ audioEvent);
	void setAudioConfig( String ^ name, int value);
	int  prepareCall();
	void mute( int callIndex, MediaMuteType mediaType, bool isMute);
	void farEndHardwareControl( int participantId, int command, int angle);
	void reset();
	void heartbeat( long timestamp);
	void updateVideoRelayConfig( String ^ config);
	void contentStart( int callIndex, int clientId, int type);
	void contentStop( int callIndex, int clientId);
	void notifyCDRResult( bool isSuccess, int cdrId);
	void dialDtmf( int callIndex, String ^ peerUri, String ^ content);
	// aec
	void setAudioLpbSupported(bool isSupport);
	void setAudioFeature(unsigned int audioFeature);
	void setAudioDeviceParam(AudioDeviceTypeManaged deviceType, AudioDeviceParamManaged^ param);
	void addOtherCallee( int callIndex, array< String ^> ^ calleeUri, int uriNumber);
	void saveDump();
	void setContentMode( bool isThumbnail);
    // layout
	void setLayoutForceTarget(unsigned int participantId);
 
private:
	IRTCSDKSerializableContextObserverImpl * sdkObserver_;
	IRTCSDKContext * sdkContext_; 
};

// some global functions, for data marshalling
// used only inside this assembly
std::string  MarshalString ( String ^ s);
std::wstring MarshalStringW ( String ^ s);

}
