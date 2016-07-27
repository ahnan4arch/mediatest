//////////////////////////////////////////////////////////////////////
// WASAPI requires AudioSessionEvents to be handled asynchronously

#pragma once
#include "stdafx.h"
#include "MMNotificationClient.h"

namespace WinRTCSDK{
	
MMNotificationClient::MMNotificationClient (IMMNotificationHandler *handler):
	refCount_(1),handler_(handler)
{
}

MMNotificationClient::~MMNotificationClient()
{
}

////////////////////////////////////////////////////////
// IMMNotificationClient
HRESULT MMNotificationClient::OnDeviceStateChanged (LPCWSTR /*DeviceId*/, DWORD /*NewState*/) 
{ 
	return S_OK; 
}

HRESULT MMNotificationClient::OnDeviceAdded (LPCWSTR id/*DeviceId*/) 
{ 
	MMNotificationParams p;
	p.deviceId = id;
	handler_->OnMMNotification(DeviceAdded, p);
	return S_OK;
}

HRESULT MMNotificationClient::OnDeviceRemoved (LPCWSTR id/*DeviceId(*/) 
{ 
	MMNotificationParams p;
	p.deviceId = id;
	handler_->OnMMNotification(DeviceRemoved, p);
	return S_OK;
}

HRESULT MMNotificationClient::OnDefaultDeviceChanged (EDataFlow flow, ERole role, LPCWSTR newDefaultDeviceId)
{ 
	MMNotificationParams p;
	p.dataFlow = flow;
	p.role = role;
	p.deviceId = newDefaultDeviceId;
	handler_->OnMMNotification(DefaultDeviceChanged, p);
	return S_OK; 
}

HRESULT MMNotificationClient::OnPropertyValueChanged (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/)
{
	return S_OK; 
}

///////////////////////////////////////////////////////
//  IUnknown
HRESULT MMNotificationClient::QueryInterface(REFIID Iid, void **Object)
{
    if (Object == NULL)
    {
        return E_POINTER;
    }
    *Object = NULL;

    if (Iid == IID_IUnknown)
    {
        *Object = static_cast<IUnknown *>(this);
        AddRef();
    }
    else if (Iid == __uuidof(IMMNotificationClient))
    {
        *Object = static_cast<IMMNotificationClient *>(this);
        AddRef();
    }
    else
    {
        return E_NOINTERFACE;
    }
    return S_OK;
}

ULONG MMNotificationClient::AddRef()
{
    return InterlockedIncrement(&refCount_);
}

ULONG MMNotificationClient::Release()
{
    ULONG returnValue = InterlockedDecrement(&refCount_);
    if (returnValue == 0)
    {
        delete this;
    }
    return returnValue;
}

} // Nemo namespace