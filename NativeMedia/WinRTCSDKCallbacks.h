#pragma once
#include "rtcsdk/include/rtcsdk_interface.h"
#include "rtcsdk/src/serialize/sdk_serialize_key.h"
#include "rtcsdk/include/rtcsdk_serializable_interface.h"
#include "booat/src/serialization.h"
#include "booat/src/log.h"

#include <vcclr.h>

using namespace std;
using namespace RTCSDK;
using namespace System;

namespace WinRTCSDK {

// This is the interface to be implemented in managed(c#) code
public interface class IWinRTCSDKSerializedCallbacks
{
public :
    void onCallMsgSendRequest(System::String^ message);
    void onCallReleased(System::String^ message);
    void onCallException(System::String^ message);
    void onCallStateChanged(System::String^ message);
    void onRecordingStateChanged(System::String^ message);
    void onIncommingCall(System::String^ message);
    void onConfCall(System::String^ message);
    void onConfereeStateChanged(System::String^ message); // result of invitation
    void onCallEventReport(System::String^ message); //
    void onCallReplace(System::String^ message); // meeting/call switched
    void onContentStateChanged(System::String^ message);
	void onWhiteboardStateChanged(System::String^ message);
    void onConfMgmtStateChanged(System::String^ message);
    void onVideoStreamRequested(System::String^ message);
    void onVideoStreamReleased(System::String^ message);
    void onAudioStreamRequested(System::String^ message);
    void onAudioStreamReleased(System::String^ message);
    void onAudioStreamReceived(System::String^ message);
    void onAudioStreamRemoved(System::String^ message);
    void onCDRReport(System::String^ message);
    void onEventReport(System::String^ message);
    void onLayoutChanged(System::String^ message);
    void onFileReady(System::String^ message);
    void onRecordExpired(System::String^ message);
    void onFaceDetected(System::String^ message);
    void onMotionDetected(System::String^ message);
    void onHowlingDetected(System::String^ message);
    void onSetMicVolumeRequest(System::String^ message);
    void onStartMotor(System::String^ message);
    void onStopMotor(System::String^ message);
    void onStopTiltCamera(System::String^ message);
    void onUploadFile(System::String^ message);
    void onEVBiasChange(System::String^ message);
    void onEVBiasReset(System::String^ message);
    void onVideoStatusChange(System::String^ message);
    void onRelayInfoChanged(System::String^ message);
    void onMediaException(System::String^ message);
	void onDummyKeyFrameRequest(System::String^ message);
};

// This is the c++ callback for RTCSDK 
class IRTCSDKSerializableContextObserverImpl : public IRTCSDKSerializableContextObserver
{
private:
	gcroot<IWinRTCSDKSerializedCallbacks^> managedCallbacks_;

public :
	IRTCSDKSerializableContextObserverImpl ( IWinRTCSDKSerializedCallbacks^ callbacks) ;

public:
	
    virtual void onCallMsgSendRequest(const BOOAT::Dictionary &para);
    virtual void onCallReleased(const BOOAT::Dictionary &para);
    virtual void onCallException(const BOOAT::Dictionary &para);
    virtual void onCallStateChanged(const BOOAT::Dictionary &para);
    virtual void onRecordingStateChanged(const BOOAT::Dictionary &para);
    virtual void onIncommingCall(const BOOAT::Dictionary &para);
    virtual void onConfCall(const BOOAT::Dictionary &para);
    virtual void onConfereeStateChanged(const BOOAT::Dictionary &para);
    virtual void onCallEventReport(const BOOAT::Dictionary &para);
    virtual void onCallReplace(const BOOAT::Dictionary &para);
    virtual void onContentStateChanged(const BOOAT::Dictionary &para);
	virtual void onWhiteboardStateChanged(const BOOAT::Dictionary &para);
    virtual void onConfMgmtStateChanged(const BOOAT::Dictionary &para);
    virtual void onVideoStreamRequested(const BOOAT::Dictionary &para);
    virtual void onVideoStreamReleased(const BOOAT::Dictionary &para);
    virtual void onAudioStreamRequested(const BOOAT::Dictionary &para);
    virtual void onAudioStreamReleased(const BOOAT::Dictionary &para);
    virtual void onAudioStreamReceived(const BOOAT::Dictionary &para);
    virtual void onAudioStreamRemoved(const BOOAT::Dictionary &para);
    virtual void onCDRReport(const BOOAT::Dictionary &para);
    virtual void onEventReport(const BOOAT::Dictionary &para);
    virtual void onLayoutChanged(const BOOAT::Dictionary &para);
    virtual void onFileReady(const BOOAT::Dictionary &para);
    virtual void onRecordExpired(const BOOAT::Dictionary &para);
    virtual void onFaceDetected(const BOOAT::Dictionary &para);
    virtual void onMotionDetected(const BOOAT::Dictionary &para);
    virtual void onHowlingDetected(const BOOAT::Dictionary &para);
    virtual void onSetMicVolumeRequest(const BOOAT::Dictionary &para);
    virtual void onStartMotor(const BOOAT::Dictionary &para);
    virtual void onStopMotor(const BOOAT::Dictionary &para);
    virtual void onStopTiltCamera(const BOOAT::Dictionary &para);
    virtual void onUploadFile(const BOOAT::Dictionary &para);
    virtual void onEVBiasChange(const BOOAT::Dictionary &para);
    virtual void onEVBiasReset(const BOOAT::Dictionary &para);
    virtual void onVideoStatusChange(const BOOAT::Dictionary &para);
    virtual void onRelayInfoChanged(const BOOAT::Dictionary &para);
    virtual void onMediaException(const BOOAT::Dictionary &para);
	virtual void onDummyKeyFrameRequest(const BOOAT::Dictionary &para);
	  
	virtual void onCcsNotification(const BOOAT::Dictionary &para) {};

};

}
