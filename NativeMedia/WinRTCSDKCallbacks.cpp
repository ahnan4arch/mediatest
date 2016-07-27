#include "stdafx.h"
#include "WinRTCSDKCallbacks.h"


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

#define IMPL_OBSERVER_METHOD( method )\
	void IRTCSDKSerializableContextObserverImpl::##method(const BOOAT::Dictionary &para){\
        System::String^ p1 = gcnew System::String(BOOAT::Serialization::serialize(para).c_str());\
        managedCallbacks_->##method(p1);\
    }
	
	IRTCSDKSerializableContextObserverImpl::IRTCSDKSerializableContextObserverImpl ( IWinRTCSDKSerializedCallbacks^ callbacks):
		managedCallbacks_(callbacks)
	{
	}

	IMPL_OBSERVER_METHOD(onCallStateChanged)
	IMPL_OBSERVER_METHOD(onCallReleased)
    IMPL_OBSERVER_METHOD(onCallMsgSendRequest)
    IMPL_OBSERVER_METHOD(onCallException)
    IMPL_OBSERVER_METHOD(onContentStateChanged)
    IMPL_OBSERVER_METHOD(onIncommingCall)
	IMPL_OBSERVER_METHOD(onCallEventReport)
	IMPL_OBSERVER_METHOD(onEventReport)	
    IMPL_OBSERVER_METHOD(onRecordingStateChanged)  
    IMPL_OBSERVER_METHOD(onConfCall)
    IMPL_OBSERVER_METHOD(onConfereeStateChanged)  
    IMPL_OBSERVER_METHOD(onCallReplace)   
	IMPL_OBSERVER_METHOD(onWhiteboardStateChanged)
    IMPL_OBSERVER_METHOD(onConfMgmtStateChanged)
    IMPL_OBSERVER_METHOD(onVideoStreamRequested)
    IMPL_OBSERVER_METHOD(onVideoStreamReleased)
    IMPL_OBSERVER_METHOD(onAudioStreamRequested)
    IMPL_OBSERVER_METHOD(onAudioStreamReleased)
    IMPL_OBSERVER_METHOD(onAudioStreamReceived)
    IMPL_OBSERVER_METHOD(onAudioStreamRemoved)
    IMPL_OBSERVER_METHOD(onCDRReport)   
    IMPL_OBSERVER_METHOD(onLayoutChanged)
    IMPL_OBSERVER_METHOD(onFileReady)
    IMPL_OBSERVER_METHOD(onRecordExpired)
    IMPL_OBSERVER_METHOD(onFaceDetected)
    IMPL_OBSERVER_METHOD(onMotionDetected)
    IMPL_OBSERVER_METHOD(onHowlingDetected)
    IMPL_OBSERVER_METHOD(onSetMicVolumeRequest)
    IMPL_OBSERVER_METHOD(onStartMotor)
    IMPL_OBSERVER_METHOD(onStopMotor)
    IMPL_OBSERVER_METHOD(onStopTiltCamera)
    IMPL_OBSERVER_METHOD(onUploadFile)
    IMPL_OBSERVER_METHOD(onEVBiasChange)
    IMPL_OBSERVER_METHOD(onEVBiasReset)
    IMPL_OBSERVER_METHOD(onVideoStatusChange)
    IMPL_OBSERVER_METHOD(onRelayInfoChanged)
    IMPL_OBSERVER_METHOD(onMediaException)
    IMPL_OBSERVER_METHOD(onDummyKeyFrameRequest)

} //name space

