#pragma once

#include "resource.h"
#include "camera.h"
#include "decode_test.h"
#include "encode_test.h"
#include "bitstream_buffering.h"

class LoopbackPipeline : public ICameraSink
{
public:
	LoopbackPipeline ():
		encodeTest_(),decodeTest_(), bsBuffering_()
	{
	}
	~LoopbackPipeline() 
	{
	}

	bool Init( DXVAProcessor* processor, 
		int w, int h, int frNumerator, int frDenomerator)
	{
		bsBuffering_.Init(3); // three buffers for buffering...

		encodeTest_.Init(processor, &bsBuffering_, w, h, frNumerator, frDenomerator, true);
		decodeTest_.Init(w, h, (IBitstreamSource*)&bsBuffering_);

		return true;
	}

	bool Start()
	{
		decodeTest_.Start();
		encodeTest_.Start();
	}

	bool Stop()
	{
		// set stop flag
		decodeTest_.Stop();
		encodeTest_.Stop();
		
		// the encoder & decoder may be trying to dequeue buffers from bsBuffering_.
		// so unblock them now!
		bsBuffering_.Shutdown();

		decodeTest_.Join();
		encodeTest_.Join();
		return true;
	}

	bool UnInit()
	{
		encodeTest_.UnInit();
		decodeTest_.UnInit();
		return true;
	}

public:
	virtual bool PutSurface (VideoFrameInfo & frame)
	{
		return true;
	}


private:
	EncodeTest encodeTest_;
	DecodeTest decodeTest_;
	BitstreamBuffering bsBuffering_;
};

class LoopbackTest
{
public:
	LoopbackTest ():camera_(1280,720)
	{
	}
	~LoopbackTest() {}

	bool Init()
	{
		camera_.Init();
		processor_ = new DXVAProcessor(camera_.GetDeviceManager());
		processor_->Init();
		return true;
	}

	bool UnInit()
	{
		processor_->UnInit();
		delete processor_;
		camera_.UnInit();
		return true;
	}

	bool StartLoopbackMax()
	{
	}

private:
	LoopbackPipeline loopback_max_;
	LoopbackPipeline loopback_360p_;
	LoopbackPipeline loopback_180p_;

	Camera           camera_;
	DXVAProcessor*   processor_;
};

static std::map<HWND, void *> _hwnd_obj_map;// map HWND to an object

class LoopbackTestUI : public IDeviceEventCallback
{
public:
	LoopbackTestUI (DWORD dialogId)
	{
		dialogId_ = dialogId;
	}

	void Run()
	{
		hDlg_ = ::CreateDialogParam(NULL, MAKEINTRESOURCE(dialogId_), NULL, DialogProcThunk, (LPARAM)this);
		if (hDlg_){
			::ShowWindow(hDlg_, SW_SHOW);

			// enter message pump
			messagePump();
		}
	}


private:
	INT messagePump()
	{
		MSG msg;
		while(::GetMessage(&msg, NULL, 0, 0))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		return 0;
	}

	// thiscall version
	INT_PTR  DialogProc( HWND   hwndDlg,UINT   uMsg,  WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg){
		case WM_INITDIALOG: // store this pointer on this message
			Init();
			break;
		default:
			break;
		}
		return TRUE;
	}

	// stdcall version
	static INT_PTR CALLBACK DialogProcThunk( HWND   hwndDlg,UINT   uMsg,  WPARAM wParam, LPARAM lParam)
	{
		LoopbackTestUI * pThis;
		switch (uMsg){
		case WM_INITDIALOG: // store this pointer on this message
			_hwnd_obj_map[hwndDlg] = (void *)lParam;
			break;
		default:
			break;
		}
		pThis = (LoopbackTestUI *)_hwnd_obj_map[hwndDlg] ;

		return pThis->DialogProc(hwndDlg, uMsg, wParam, lParam);
	}

	void Init()
	{
		// device event manager
		deviceEvents_ = new DeviceEvents(this);
		deviceEvents_->Startup();
	}
	
	void UnInit()
	{
		delete deviceEvents_;
	}

public:
	// IDeviceEventCallback. This callback is response to WM_DEVICECHANGE, so don't do much thing here
	void OnVideoCapDevChanged (UINT nEventType, DWORD_PTR dwData  )
	{
		DEV_BROADCAST_HDR *pHdr = (DEV_BROADCAST_HDR *)dwData;
		if(!pHdr) return;
		if (pHdr->dbch_devicetype != DBT_DEVTYP_DEVICEINTERFACE ) return ;

		DEV_BROADCAST_DEVICEINTERFACE *pDi = (DEV_BROADCAST_DEVICEINTERFACE *)dwData;
		std::wstring  name = &pDi->dbcc_name[0];
		switch (nEventType)
		{
		case DBT_DEVICEARRIVAL://   A device has been inserted and is now available.
			// fire the event to GUI
			break;
		case DBT_DEVICEQUERYREMOVE://   Permission to remove a device is requested. 
		case DBT_DEVICEQUERYREMOVEFAILED://   Request to remove a device has been canceled.
		case DBT_DEVICEREMOVEPENDING ://  Device is about to be removed. Cannot be denied.
			break;
		case DBT_DEVICEREMOVECOMPLETE ://  Device has been removed.
			// fire the event to GUI
			break;
		case DBT_DEVICETYPESPECIFIC ://  Device-specific event.
			break;
		case DBT_CONFIGCHANGED ://  Current configuration has changed.
			break;
		}
	}

private:
	LoopbackTest  loopbackTest_;
	DeviceEvents    *deviceEvents_;
	DWORD dialogId_;
	HWND hDlg_;
};