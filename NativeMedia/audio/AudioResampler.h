#pragma once
namespace WinRTCSDK{

// This resampler process precision of 16bit only
// verey call to the public methods must pass audio data less than 100ms
class AudioResampler
{
public:
	AudioResampler();
	~AudioResampler();
	void reset ();
	short * ReSampling44p1KMonoTo48KMono(short *pin, int sample_len, int& outSamplelen);
	short * ReSampling48KMonoTo44p1KMono(short *pin, int sample_len, int& outSamplelen);
	short * ReSamplingStereoToMono(short *pin, int sample_len);
	short * ReSamplingMonoToStereo(short *pin, int sample_len);
private:
	// sample_len must be a multiple of 147
	int _ReSampling44p1KMonoto48KMono(short *pin, int sample_len, short *pout);
	// sample_len must be a multiple of 160
	int _ReSampling48KMonoto44p1KMono(short *pin, int sample_len, short *pout);

private:
	int    remainder44p1k_;
	short  remainderBuf44p1k_[147] ; 
	int    remainder48k_;
	short  remainderBuf48k_[160] ;
	// output buffers
	short *out48k_; 
	short *outStereo_;
	short *out44p1k_;
	
	// converting buffer
	short *pWorkBuffer_;
	int    sizeOfWorkBuffer_;   // num samples
};

}