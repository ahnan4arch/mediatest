#include "stdafx.h"
#include "AudioManager.h"
#define _USE_MATH_DEFINES
#include <math.h>
#include <limits.h>

namespace WinRTCSDK {
template<typename T> T Convert(double Value);

template <typename T>
void GenerateSineSamples(BYTE *Buffer, size_t BufferLength, DWORD Frequency, WORD ChannelCount, DWORD SamplesPerSecond, double *InitialTheta)
{
    double sampleIncrement = (Frequency * (M_PI*2)) / (double)SamplesPerSecond;
    T *dataBuffer = reinterpret_cast<T *>(Buffer);
    double theta = (InitialTheta != NULL ? *InitialTheta : 0);

    for (size_t i = 0 ; i < BufferLength / sizeof(T) ; i += ChannelCount)
    {
        double sinValue = sin( theta );
        for(size_t j = 0 ;j < ChannelCount; j++)
        {
            dataBuffer[i+j] = Convert<T>(sinValue);
        }
        theta += sampleIncrement;
    }

    if (InitialTheta != NULL)
    {
        *InitialTheta = theta;
    }
}

template<> 
float Convert<float>(double Value)
{
    return (float)(Value);
};

template<> 
short Convert<short>(double Value)
{
    return (short)(Value * _I16_MAX);
};

class AudioUT 
{
public:
	AudioUT()
	{
		double theta=0;
		GenerateSineSamples<short>((BYTE*)_ringTone, 4800 * 2, 880,
					   1, 48000, &theta);
	}
	~AudioUT() { }

public:
	short _ringTone[4800 * 2]; // 100 ms
};

static AudioUT _ut;

bool UT_DS_GetAudioData(std::string sourceID, void *buffer, int length, uint32_t sampleRate)
{
	::CopyMemory(buffer, _ut._ringTone, length);
	return true;
}

}