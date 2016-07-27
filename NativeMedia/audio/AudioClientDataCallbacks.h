#pragma once
#include "stdafx.h"

namespace WinRTCSDK {

// The audio render/capture use this callback interface to
// write/read audio data to/from their container object -- the AudioManager.
//
// Callback functions are invoked by working thread of the capture/render.
// The data buffer is provided by audio engine, so the buffer pointer should not be passed to other threads.
//
//  currently we input/output 48k mono data only with audio pipelines
//
class IAudioClientDataCallbacks {
public:
	// timeStamp is in 100ns unit
    virtual bool OnWriteAudioData(const void * data, int len/*in bytes*/, uint32_t sampleRate, uint64_t timeStamp) = 0;
    virtual bool OnWriteLoopbackData(const void * data, int len/*in bytes*/, uint32_t sampleRate, uint64_t timeStamp) = 0;
    virtual bool OnReadAudioData(void * data, int len/*in bytes*/) = 0;
};
} // namespace WinRTCSDK
