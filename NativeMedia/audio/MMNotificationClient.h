//////////////////////////////////////////////////////////////////////
// WASAPI requires MMNotification to be handled asynchronously

#pragma once
#include <MMDeviceAPI.h>
#include <AudioClient.h>
#include <AudioPolicy.h>
#include <string>

namespace WinRTCSDK{

enum MMNotificationType
{
	DeviceStateChanged,
	DeviceAdded,
	DeviceRemoved,
	DefaultDeviceChanged,
	PropertyValueChange
};

struct MMNotificationParams
{
	std::wstring  deviceId;
	DWORD         newState;
	EDataFlow     dataFlow;
	ERole         role;
	PROPERTYKEY   propertyKey;
};

// the handler to handle events asynchronously
// the AudioManager class is an implementation of this interface
class IMMNotificationHandler 
{
public:
	virtual void OnMMNotification (MMNotificationType notif, MMNotificationParams &param) = 0;
};

class MMNotificationClient : public IMMNotificationClient
{
public:
	MMNotificationClient (IMMNotificationHandler *handler);
	~MMNotificationClient();
public:
	// IUnknown
	STDMETHOD_(ULONG, AddRef)();
    STDMETHOD_(ULONG, Release)();
    STDMETHOD(QueryInterface)(REFIID iid, void **pvObject);

	// IMMNotificationClient
	STDMETHOD(OnDeviceStateChanged) (LPCWSTR /*DeviceId*/, DWORD /*NewState*/);
    STDMETHOD(OnDeviceAdded) (LPCWSTR /*DeviceId*/);
    STDMETHOD(OnDeviceRemoved) (LPCWSTR /*DeviceId(*/);
    STDMETHOD(OnDefaultDeviceChanged) (EDataFlow Flow, ERole Role, LPCWSTR NewDefaultDeviceId);
    STDMETHOD(OnPropertyValueChanged) (LPCWSTR /*DeviceId*/, const PROPERTYKEY /*Key*/);

private:
	ULONG                    refCount_;
	IMMNotificationHandler * handler_;
};
} // Nemo namespace