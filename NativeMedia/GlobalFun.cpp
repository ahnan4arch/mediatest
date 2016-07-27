// audio/video buffer related global functions
//  and other utility functions
#include "Stdafx.h"
#include <map>
#include <memory>


#include "GlobalFun.h"

// global funcions to access data source managers, used by NativeMedia only
namespace WinRTCSDK{

// we use same algo to calculate the time stamp for video as the WASAPI api
// please refer to IAudioCaptureClient::GetBuffer method
static DWORD __get_timestamp()
{
	LARGE_INTEGER t;
	LARGE_INTEGER f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency (&f);
	double tdou = t.QuadPart * 1.0f;
	double fdou = f.QuadPart * 1.0f;
	LONGLONG _100nanos = (tdou * 10000000.0f) / fdou;
	// conver to milliseconds
	return (DWORD)(_100nanos/(10 * 1000));
}

// note£º the timestamp is in hns(100ns) unit now, for AEC
bool DS_PutAudioData(std::string sourceID, const void *buffer, int length, uint32_t sampleRate, uint64_t timestamp)
{
	return false;
}

// note£º the timestamp is in hns(100ns) unit now, for AEC
bool DS_PutLoopbackData(std::string sourceID, const void *buffer, int length, uint32_t sampleRate, uint64_t timestamp)
{
	return false;
}

bool DS_GetAudioData(std::string sourceID, void *buffer, int length, uint32_t sampleRate)
{
	return false;
}

bool DS_PutVideoData(D3DFORMAT fmt, std::string sourceID, const void *buffer, int srcStride, int width, int height)
{
	return false;
}

/*
* Note: the NativeMedia module doesn't use booat, 
* so we need this workaround to adapt the SharedBufferPtr type
*/
bool DS_GetVideoData(std::string sourceID, void **ppbuffer, int &length, int& width, int& height, int &degree)
{
	return false;
}

bool DS_FreeVideoData( void **ppbuffer)
{
	return false;
}

}//namespace WinRTCSDK
