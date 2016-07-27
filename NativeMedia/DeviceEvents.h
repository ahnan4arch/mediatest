///////////////////////////////////////////////////////////////////////
// A util class to monitor system device notification.
// Currently we only care video capture devs. Windows send dev notification 
//  via HWND, so a message pump is also included.

#pragma once
#include "Stdafx.h"
#include "BaseWnd.h"
#include "Looper.h"

namespace WinRTCSDK{

class IDeviceEventCallback{
public:
	// handler for WM_DEVICECHANGE
	virtual void OnVideoCapDevChanged (UINT nEventType, DWORD_PTR dwData  )= 0;
};

class DeviceEvents{

public:
	DeviceEvents ( IDeviceEventCallback * callback)
		:callback_(callback),dispatcher_("WM_DEVICECHANGE listener"){
	}
	
	~DeviceEvents(){
	}

	void Startup(){
		dispatcher_.startUp();
		LooperTask t = [this](){ messageLoop(); } ;
		dispatcher_.runAsyncTask(t);
	}

	void Shutdown (){
		DWORD tid = dispatcher_.threadID();
		::PostThreadMessage(tid, WM_QUIT, 0, 0);// end the message pump
		dispatcher_.stopAndJoin(); // stop the dispatcher thread
	}

private:

	class DummyWnd: public BaseWnd {
	public:
		DummyWnd (IDeviceEventCallback * callback):BaseWnd(),callback_(callback) {}
	public:
		LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			switch (msg){
			case WM_DEVICECHANGE:
				callback_->OnVideoCapDevChanged(wParam, lParam);
				break;
			default:
				return BaseWnd::WndProc(hWnd, msg, wParam, lParam);
			}
			return 0;
		}
	private:
		IDeviceEventCallback * callback_;
	};

	// The message pump thread
	void messageLoop() // just pump Windows system broadcast
	{
		DummyWnd dummyWnd (callback_); // create the dummy Wnd inside this message pump
		dummyWnd.Create(0, L"WND_DEVEVENTS", WS_OVERLAPPEDWINDOW, 0, 0, 0, 0, NULL, 0, NULL);

		// Register this window to get device notification messages.
		DEV_BROADCAST_DEVICEINTERFACE di = { 0 };
		di.dbcc_size = sizeof(di);
		di.dbcc_devicetype  = DBT_DEVTYP_DEVICEINTERFACE;
		di.dbcc_classguid  = KSCATEGORY_VIDEO; // capture dev

		HDEVNOTIFY hDevnotify = ::RegisterDeviceNotification(dummyWnd, &di, DEVICE_NOTIFY_WINDOW_HANDLE);

		MSG msg = {0};
		while (::GetMessage(&msg, NULL, 0, 0))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}

		::UnregisterDeviceNotification(hDevnotify);
	}

	Looper                 dispatcher_;
	IDeviceEventCallback * callback_;
};

}