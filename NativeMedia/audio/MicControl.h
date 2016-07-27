#pragma once
////////////////////////////////////////////////////////////////////////
//  Microphone Controll ( for AEC)
//
//  This class provide following control functions for a capture device
//   1. EndpointVolumeControl
//   2. traverse the dev topology to find following parts
//       MicBOOST, AGC (will be disabled if found)
//

#include "WASAPIComPtrDefs.h"

namespace WinRTCSDK{

class MicControl
{
public:
	MicControl ( _com_ptr_IMMDevice mmdev ):mmdev_(mmdev)
	{
		bMuted_ = false;
		lastLevelDB_ = 0.0f;
		bAGCFound_ = FALSE;
		bBoostFound_ = FALSE;
		hr_ = _Init();
	}

	~MicControl()
	{
	}
	// The mute function is used by App
	void    SetMicMute (bool bMute);
	BOOL    IsHardwareVolume();
	BOOL    HasBoostControl();
	
	// following two functions are called by application to save/restore system mic volume
	HRESULT GetEndpointVolumeScalar(float &fLevel);
	HRESULT SetEndpointVolumeScalar(float fLevel);
	
	// following functions are used by AEC module, to set mic boost 
	// When Mic is muted, volume functions are disabled
	HRESULT SetMicBoost(float dbVal, bool absolute);
	HRESULT GetMicBoost(float& dbVal);
	HRESULT GetMicBoostRange(float& dbMin, float& dbMax, float& dbStep);
	HRESULT SetEndpointVolume(float dbVal, bool absolute);
	HRESULT GetEndpointVolume(float& dbVal);
	HRESULT GetEndpointVolumeRange(float& dbMin, float& dbMax, float& dbStep);

private:
	HRESULT _Init();

private:
	bool                           bMuted_; // this is for mute operation
	float                          lastLevelDB_; // this is for mute operation

	BOOL                           bAGCFound_;
	BOOL                           bBoostFound_;
	_com_ptr_IMMDevice             mmdev_;
	_com_ptr_IAudioEndpointVolume  endpointVolume_; // endpoint volume
	_com_ptr_IAudioVolumeLevel     micBoost_; // this is the boost control
	_com_ptr_IAudioLoudness        micLoudness_; // Qin said this is also a kind of mic boost
	HRESULT                        hr_;
};

}// namespace WinRTCSDK