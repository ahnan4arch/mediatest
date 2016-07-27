#include "stdafx.h"
#include "MicControl.h"
#include "NativeMediaLog.h"

namespace WinRTCSDK{

// we will traverse the capture device topology to find MicBoost and AGC control
HRESULT MicControl::_Init()
{
	HRESULT hr;
	UINT nConnector;
	DataFlow                  dataFlow;
	BOOL                      bIsConnected;
	ConnectorType             conType;
	LPWSTR                    pwstrName = NULL;

	_com_ptr_IDeviceTopology  devTopology_;
	_com_ptr_IConnector       spConnector;
	_com_ptr_IConnector       spConnTo;
	_com_ptr_IPart            spPartPrev;
	_com_ptr_IPart            spPartNext;

	// check mmdev 
	if ( mmdev_ == NULL) return E_INVALIDARG;

	// Get endpoint volume interface
	hr = mmdev_->Activate(__uuidof(IAudioEndpointVolume), 
		CLSCTX_ALL, NULL, reinterpret_cast<void**>(&endpointVolume_));
	if(FAILED(hr)) return hr;

	// Get the Device Topology interface
	hr = mmdev_->Activate(__uuidof(IDeviceTopology), 
		CLSCTX_ALL, NULL, reinterpret_cast<void**>(&devTopology_));
	if(FAILED(hr)) return hr;

	// for endpoint device, there should be only 1 connector
	hr = devTopology_->GetConnectorCount(&nConnector);
	if(FAILED(hr)) return hr;
	if (nConnector == 0) return E_FAIL;

	hr = devTopology_->GetConnector(0, &spConnector);
	if(FAILED(hr)) return hr;

	// make sure this is a capture device
	hr = spConnector->GetDataFlow(&dataFlow);
	if(FAILED(hr)) return hr;
	if (dataFlow != DataFlow::Out) return E_FAIL;

	// outer loop all connector on the data path
	while(!bAGCFound_ || !bBoostFound_)
	{
		spConnTo = NULL;
		hr = spConnector->IsConnected(&bIsConnected);
		if(FAILED(hr)) return hr;
		if (bIsConnected == false)
		{
			hr = spConnector->GetType(&conType);
			if(FAILED(hr)) return hr;
			// the end of the topology must be a software IO connector
			if (conType != ConnectorType::Software_IO) return E_FAIL;
			break;  // finished
		}

		hr = spConnector->GetConnectedTo(&spConnTo);
		if(FAILED(hr)) return hr;

		// query interface for IPart
		spPartPrev = spConnTo;

		// inner loop over all Subunits looking for a specified topology
		while(!bAGCFound_ || !bBoostFound_)
		{
			PartType partType;
			_com_ptr_IPartsList spParts;
			GUID SubType = {0};
			spPartNext = NULL;

			hr = spPartPrev->EnumPartsOutgoing(&spParts);
			if(FAILED(hr)) return hr;

			hr = spParts->GetPart(0, &spPartNext);
			if(FAILED(hr)) return hr;
			hr = spPartNext->GetPartType(&partType);
			if(FAILED(hr)) return hr;

			if (partType == PartType::Connector)
			{
				// this is a connector
				spConnector = spPartNext;
				break;
			}

			hr = spPartNext->GetSubType(&SubType);
			if(FAILED(hr)) return hr;

			// check if this is the AGC part
			if (SubType == KSNODETYPE_AGC)
			{
				_com_ptr_IAudioAutoGainControl spAGC;
				hr = spPartNext->Activate(CLSCTX_ALL, __uuidof(IAudioAutoGainControl), (void**)&spAGC);
				if (hr == S_OK) // just disable the AGC
				{
					bAGCFound_ = TRUE;
					spAGC->SetEnabled( FALSE, NULL);
				}
			}

			hr = spPartNext->GetName(&pwstrName);
			if(FAILED(hr)) return hr;
			// check if this is the MicBoost
			if (pwstrName ){
				NativeMediaLogger4CPP::logW(LogLevel::Info, L"AudioCapture MicControl", L"Found part in capture topology%s\n", pwstrName);
				CoTaskMemFree(pwstrName);
				pwstrName = NULL;
			}

			if (SubType == KSNODETYPE_LOUDNESS)
			{
				hr = spPartNext->Activate(CLSCTX_ALL, __uuidof(IAudioLoudness), (void**)&micLoudness_);
				if (hr == S_OK) bBoostFound_ = TRUE;
			}
			else if (SubType == KSNODETYPE_VOLUME)
			{
				hr = spPartNext->Activate(CLSCTX_ALL, __uuidof(IAudioVolumeLevel), (void**)&micBoost_);
				if (hr == S_OK) bBoostFound_ = TRUE;
			}

			spPartPrev = spPartNext;
			spPartNext = NULL;
		}
	}

	return S_OK;
}

void MicControl::SetMicMute (bool bMute)
{
	if (endpointVolume_==NULL)return;
	if (bMuted_ == bMute) return; // NOP
	HRESULT hr;
	bMuted_ = bMute;
	if ( bMute ){
		// store last level
		HRESULT hr = endpointVolume_->GetMasterVolumeLevel(&lastLevelDB_);
		// set level to minimal 
		float mindb, maxdb, stepdb;
		hr = endpointVolume_->GetVolumeRange(&mindb, &maxdb, &stepdb);
		hr = endpointVolume_->SetMasterVolumeLevel(mindb, NULL);
	}else{
		// restore the volume level
		hr = endpointVolume_->SetMasterVolumeLevel(lastLevelDB_, NULL);
	}	
}

BOOL MicControl::IsHardwareVolume ()
{
	DWORD flags = 0;
	if (endpointVolume_!=NULL){
		endpointVolume_->QueryHardwareSupport(&flags);
	}
	return ( (flags & ENDPOINT_HARDWARE_SUPPORT_VOLUME) == ENDPOINT_HARDWARE_SUPPORT_VOLUME);
}

BOOL MicControl::HasBoostControl ()
{
	return bBoostFound_;
}

HRESULT MicControl::SetMicBoost(float dbVal, bool absolute)
{
	if (bMuted_) return S_OK; // doesn't change volume when muted

	if ( bBoostFound_ == FALSE) return E_FAIL;
	if ( micBoost_ ){
		float lvl;
		micBoost_->GetLevel(0, &lvl);
		if (!absolute) dbVal += lvl; // dvVla is a delta val
		micBoost_->SetLevel(0, dbVal, NULL);
	}else if ( micLoudness_ ){
		if (dbVal > 0.0){ // only two levels
			micLoudness_->SetEnabled(TRUE, NULL);
		}else{
			micLoudness_->SetEnabled(FALSE, NULL);
		}
	}
	return S_OK;
}

HRESULT MicControl::SetEndpointVolume(float dbVal, bool absolute)
{
	if (bMuted_) return S_OK; // doesn't change volume when muted

	if (endpointVolume_ == NULL) return E_FAIL;
	float lvl;
	endpointVolume_->GetMasterVolumeLevel(&lvl);
	if ( absolute){
		endpointVolume_->SetMasterVolumeLevel(dbVal, NULL);
	}else{
		endpointVolume_->SetMasterVolumeLevel(lvl+dbVal, NULL);
	}

	return S_OK;
}

HRESULT MicControl::GetEndpointVolumeScalar(float &fLevel)
{
	if ( endpointVolume_ == NULL) return E_FAIL;
	HRESULT hr = endpointVolume_->GetMasterVolumeLevelScalar(&fLevel);
	return hr;
}

HRESULT MicControl::SetEndpointVolumeScalar(float fLevel)
{
	if ( endpointVolume_ == NULL) return E_FAIL;
	HRESULT hr = endpointVolume_->SetMasterVolumeLevelScalar(fLevel, NULL);
	return hr;
}

HRESULT MicControl::GetEndpointVolumeRange(float& dbMin, float& dbMax, float& dbStep)
{
	if ( endpointVolume_ == NULL) return E_FAIL;
	endpointVolume_->GetVolumeRange(&dbMin, &dbMax, &dbStep);
	return S_OK;
}

HRESULT MicControl::GetMicBoostRange(float& dbMin, float& dbMax, float& dbStep)
{
	if (!bBoostFound_) return E_FAIL;
	if (micBoost_) {
		micBoost_->GetLevelRange(0, &dbMin, &dbMax, &dbStep);
	}else if (micLoudness_){ // this is a switch control
		dbMin = -10.0;
		dbMax = 10.0;
		dbStep = 20.0; // only one step to simulate the switch
	}
	return S_OK;

}

HRESULT MicControl::GetMicBoost(float& dbVal)
{
	if (micBoost_==NULL) return E_FAIL;
	micBoost_->GetLevel(0, &dbVal);
	return S_OK;
}

HRESULT MicControl::GetEndpointVolume(float& dbVal)
{
	if ( endpointVolume_ == NULL) return E_FAIL;
	endpointVolume_->GetMasterVolumeLevel(&dbVal);
	return S_OK;
}

} // namespace WinRTCSDK