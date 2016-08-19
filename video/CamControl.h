#pragma once
///////////////////////////////////////////////////////////////////////
//Provides access to following interfaces of a WDM Video Capture.
//    IAMCameraControl, IAMVideoProcAmp , IAMVideoControl 


#include "ComPtrDefs.h"

namespace WinRTCSDK{

class CameraControl 
{
public:
	CameraControl(){};
	~CameraControl(){};

	// We are still able to query following DShow interfaces from MFMediaSource
	HRESULT Create( _com_ptr_IMFMediaSource pMediaSource )
	{
		HRESULT hr;
		// Get IAMVideoProcAmp
		hr = pMediaSource->QueryInterface(IID_IAMVideoProcAmp, (void**)&pAMVideoProcAmp_);
		if ( FAILED (hr) ) goto done;

		// Get IAMCameraControl
		hr = pMediaSource->QueryInterface(IID_IAMCameraControl, (void**)&pAMCameraControl_);
		if ( FAILED (hr) ) goto done;

done:
		return hr;	
	}

	// just release the COM interfaces
	void Destroy () 
	{
		pAMCameraControl_=NULL;
		pAMVideoProcAmp_ =NULL;
	}

	///////////////////////////////////////////////////////////
	// camera control
	bool SetCameraFocus ()
	{
		long Min, Max, Step, Default, Flags;
		HRESULT hr = pAMCameraControl_->GetRange(CameraControl_Focus, &Min, &Max, &Step,&Default, &Flags);
		return true;
	}

	///////////////////////////////////////////////////
	// qualities of video signal
	bool SetVideoBrightness (float percentage/*0.0-1.0*/)
	{
		long Min, Max, Step, Default, Flags;
		// Get the range and default value. 
		HRESULT hr = pAMVideoProcAmp_->GetRange(VideoProcAmp_Brightness, &Min, &Max, &Step, &Default, &Flags);
		if (SUCCEEDED(hr))
		{
			if ( percentage == 0.0){
				hr = pAMVideoProcAmp_->Set(VideoProcAmp_Brightness, Min, VideoProcAmp_Flags_Manual);
			}else{
				hr = pAMVideoProcAmp_->Set(VideoProcAmp_Brightness, Max, VideoProcAmp_Flags_Manual);
			}
		}
		return true;
	}


private:
	_com_ptr_IAMCameraControl pAMCameraControl_;
	_com_ptr_IAMVideoProcAmp  pAMVideoProcAmp_;
};

} // namesapce WinRTCSDK