#include <string>
#include <stdint.h>

// global functions
namespace WinRTCSDK{

// audio data
bool DS_PutAudioData(std::string sourceID, const void *buffer, int length, uint32_t sampleRate, uint64_t timestamp);
bool DS_PutLoopbackData(std::string sourceID, const void *buffer, int length, uint32_t sampleRate, uint64_t timestamp);
bool DS_GetAudioData(std::string sourceID, void *buffer, int length, uint32_t sampleRate);

// video data
bool DS_PutVideoData(D3DFORMAT fmt, std::string sourceID, const void *buffer, int srcStride, int width, int height);
bool DS_GetVideoData(std::string sourceID, void **buffer, int &length, int& width, int& height, int &degree);
bool DS_FreeVideoData( void **buffer); 

}