#pragma once
using System::String;
using System::Runtime::InteropServices::Marshal;

#include "WinRTCSDKTypes.h"

namespace WinRTCSDK
{

/////////////////////////////////////////////////////////////////////
//  callbacks of NativeMedia 
//  this callback must be implemented by NativeMedia Api consumer
public interface class INativeMediaCallbacks
{
public :
    void onAudioDeviceReset(AudioDeviceTypeManaged type, AudioDeviceParamManaged ^ param);
    void onAudioVolumeChanged(MediaDevType type, int volume/*[0,100]*/,  bool bMute);
    void onDeviceChanged(MediaDevType type, MediaDevEvent evt);
};

}