#include "stdafx.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "AudioResampler.h"
#include "ResampleInterpTabs.h"

// The interpolation filter is 16 taps
#define kFIR44p1kFilterHistorySize  15

namespace WinRTCSDK{

AudioResampler::AudioResampler()
{
	// we use sepatate buffers for different methods, 
	// so that the returned pointer of one method can be used as the input of another method
	out48k_ = (short *)malloc(48000 * 2 * 100/1000) ;// 100 ms
	out44p1k_ = (short *)malloc(48000 * 2 * 100/1000) ;// 100 ms
	outStereo_ = (short *)malloc(48000 * 4 * 100/1000) ;// 100 ms

	sizeOfWorkBuffer_ = kFIR44p1kFilterHistorySize + 960;
	pWorkBuffer_ = (short *)malloc(sizeOfWorkBuffer_ * sizeof(short));
    memset(pWorkBuffer_, 0, sizeOfWorkBuffer_ * sizeof(short));

	reset();
}

void AudioResampler::reset ()
{
	remainder44p1k_ = 0;
	remainder48k_ = 0;
}

// This public method will do alignment at 160 bytes boundary
short * AudioResampler::ReSampling48KMonoTo44p1KMono(short *pin, int sample_len, int& outSamplelen)
{
	// we assume n > 160;
	int n = sample_len;
	int e = (160 - remainder48k_);

	::memcpy(&remainderBuf48k_[remainder48k_], pin, e * sizeof(short) );
	outSamplelen = _ReSampling48KMonoto44p1KMono (remainderBuf48k_, 160, out44p1k_);

	remainder48k_ = (n-e) % 160 ;

	int m = n - e - remainder48k_;
	outSamplelen += _ReSampling48KMonoto44p1KMono ( pin + e, m, out44p1k_+outSamplelen);

	// save remainder for next call
	::memcpy(&remainderBuf48k_[0], pin + e + m, remainder48k_* sizeof(short));

	return out44p1k_;
}

// This public method will do data alignment at 147 bytes boundary
short * AudioResampler::ReSampling44p1KMonoTo48KMono(short *pin, int sample_len, int& outSamplelen)
{
	int n = sample_len;
	int e = (147 - remainder44p1k_);

	// too few samples to make one resampling operation. just save to reminder for next call
	if ( n < e ){
		::memcpy(&remainderBuf44p1k_[remainder44p1k_], pin, n * sizeof(short) );
		remainder44p1k_ +=n;
		return NULL;
	}

	::memcpy(&remainderBuf44p1k_[remainder44p1k_], pin, e * sizeof(short) );
	outSamplelen = _ReSampling44p1KMonoto48KMono (remainderBuf44p1k_, 147, out48k_);

	remainder44p1k_ = (n-e) % 147 ;

	int m = n - e - remainder44p1k_;
	outSamplelen += _ReSampling44p1KMonoto48KMono ( pin + e, m, out48k_+outSamplelen);

	// save remainder for next call
	::memcpy(&remainderBuf44p1k_[0], pin + e + m, remainder44p1k_* sizeof(short));

	return out48k_;
}

// this is an in-place operation. 
short * AudioResampler::ReSamplingStereoToMono(short *pin, int sample_len)
{
	// merge 2 channels into mono
	int i =0;
	int n = sample_len * 2;
	for (i=0; i<n; i+=2){
		pin[i/2] = pin[i]/2+pin[i+1]/2; // must divide every sample by 2 to avoid overflow
	}
	return pin;
}
	
short * AudioResampler::ReSamplingMonoToStereo(short *pin, int sample_len)
{
	// mono to stereo
	int i =0;
	for (i=0; i<sample_len; i++)
	{
		outStereo_[i*2+0] = pin[i];
		outStereo_[i*2+1] = pin[i];
	}
	return outStereo_;
}

AudioResampler::~AudioResampler()
{
	if (out48k_) free (out48k_);
	if (outStereo_) free (outStereo_);
	if (out44p1k_) free (out44p1k_);
    if (pWorkBuffer_) free(pWorkBuffer_);
}

int AudioResampler::_ReSampling44p1KMonoto48KMono(short *pin, int sample_len, short *pout)
{
    int i, j, k, l, IntegerPart;
    float OutAcc0;

    if (sample_len + kFIR44p1kFilterHistorySize > sizeOfWorkBuffer_)
    {
        sizeOfWorkBuffer_ = kFIR44p1kFilterHistorySize + sample_len;
        pWorkBuffer_ = (short *)realloc(pWorkBuffer_, sizeOfWorkBuffer_ * sizeof(short));
        assert(pWorkBuffer_);
    }

    for (i = 0; i < sample_len; ++i)
    {
        pWorkBuffer_[i + kFIR44p1kFilterHistorySize] = pin[i];
    }

    for (i = 0; i < sample_len / 147; ++i)
    {
        l = 0;

        for (j = 0; j < 160; ++j)
        {
            IntegerPart = InterpIndex44to48[j] + i * 147;
            OutAcc0 = 0;

            for (k = 0; k < 16; ++k)
            {
                // Perform the FIR interpolation filter
                OutAcc0 += (pWorkBuffer_[IntegerPart - k] * Interpolation44to48[l++]) >> 15;
            }

            pout[j + i * 160] = (short)OutAcc0; // output at 48 kHz sample rate
        }
    }

    // Copy the oldest samples at the end of the buffer to the beginning
    for (i = 0; i < kFIR44p1kFilterHistorySize; ++i)
    {
        pWorkBuffer_[i] = pWorkBuffer_[sample_len + i];
    }

    return sample_len * 160 / 147;
}

int AudioResampler::_ReSampling48KMonoto44p1KMono(short *pin, int sample_len, short *pout)
{

    int i, j, k, l, IntegerPart;
    float OutAcc0;

    if (sample_len + kFIR44p1kFilterHistorySize > sizeOfWorkBuffer_)
    {
        sizeOfWorkBuffer_ = kFIR44p1kFilterHistorySize + sample_len;
        pWorkBuffer_ = (short *)realloc(pWorkBuffer_, sizeOfWorkBuffer_ * sizeof(short));
        assert(pWorkBuffer_);
    }

    for (i = 0; i < sample_len; ++i)
    {
        pWorkBuffer_[i + kFIR44p1kFilterHistorySize] = pin[i];
    }

    for (i = 0; i < sample_len / 160; ++i)
    {
        l = 0;

        for (j = 0; j < 147; ++j)
        {
            IntegerPart = InterpIndex48to44[j] + i * 160;
            OutAcc0 = 0;

            for (k = 0; k < 16; ++k)
            {
                // Perform the FIR interpolation filter
                OutAcc0 += (pWorkBuffer_[IntegerPart - k] * Interpolation48to44[l++]) >> 15;
            }

            pout[j + i * 147] = (short)OutAcc0; // output at 48 kHz sample rate
        }
    }

    // Copy the oldest samples at the end of the buffer to the beginning
    for (i = 0; i < kFIR44p1kFilterHistorySize; ++i)
    {
        pWorkBuffer_[i] = pWorkBuffer_[sample_len + i];
    }

    return sample_len * 147 / 160;
}
} // namespace WinRTCSDK