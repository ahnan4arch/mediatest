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
		return true;
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
	// ICameraSink
	virtual bool PutSurface (VideoFrameInfo & frame)
	{
		encodeTest_.Input(frame.pSample);
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
	LoopbackTest (  Camera & cam):camera_(cam)
	{
	}
	~LoopbackTest() {}

	bool Init()
	{
		processor_ = new DXVAProcessor(camera_.GetDeviceManager());
		processor_->Init();
		processor1_ = new DXVAProcessor(camera_.GetDeviceManager());
		processor1_->Init();
		return true;
	}

	bool UnInit()
	{
		processor_->UnInit();
		delete processor_;
		processor1_->UnInit();
		delete processor1_;
		return true;
	}

	bool StartLoopbackMax()
	{
		StartLoopbackPipeline(processor_, loopback_max_, 1280, 720, 5, 1);
		StartLoopbackPipeline(processor_, loopback_360p_, 1280, 720, 5, 1);
		StartLoopbackPipeline(processor_, loopback_180p_, 1280, 720, 5, 1);
		return true;
	}
	
	bool StopLoopbackMax()
	{
		return StopLoopbackPipeline(loopback_max_);
		return StopLoopbackPipeline(loopback_360p_);
		return StopLoopbackPipeline(loopback_180p_);
	}

	//bool StartLoopback360p()
	//{
	//}
	//
	//bool StopLoopback360p()
	//{
	//}

private:
	bool StartLoopbackPipeline(DXVAProcessor* processor, LoopbackPipeline& pipeline, int w, int h, int frN, int frD)
	{
		pipeline.Init(processor, w, h, frN, frD);
		pipeline.Start();
		camera_.AddSink(&pipeline);
		return true;
	}

	bool StopLoopbackPipeline(LoopbackPipeline& pipeline)
	{
		camera_.RemoveSink(&pipeline);
		pipeline.Stop();
		pipeline.UnInit();
		return true;
	}


private:
	LoopbackPipeline loopback_max_;
	LoopbackPipeline loopback_360p_;
	LoopbackPipeline loopback_180p_;
	Camera &camera_;
	DXVAProcessor*   processor_;
	DXVAProcessor*   processor1_;
};

static std::map<HWND, void *> _hwnd_obj_map;// map HWND to an object
#define UserMsg_DevChanged WM_USER + 1
class LoopbackTestUI : public IDeviceEventCallback
{
public:
	LoopbackTestUI (DWORD dialogId):
		camera_(1280,720), 
		loopbackTest_(camera_)
	{
		dialogId_ = dialogId;
	}

	~LoopbackTestUI(){
		if (hDlg_)::DestroyWindow(hDlg_);
	}

	void Run()
	{
		::CreateDialogParam(NULL, MAKEINTRESOURCE(dialogId_), NULL, DialogProcThunk, (LPARAM)this);

		if (hDlg_){ // hDlg_ is set in the WM_INITDIALOG message!
			::ShowWindow(hDlg_, SW_SHOW);

			// enter message pump
			MSG msg;
			while(::GetMessage(&msg, NULL, 0, 0))
			{
				if (! ::IsDialogMessage(hDlg_, &msg))
				{
					::TranslateMessage(&msg);
					::DispatchMessage(&msg);
				}
			}
		}
	}


private:

	// thiscall version
	INT_PTR  DialogProc( HWND   hwndDlg,UINT   uMsg,  WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg){
		case UserMsg_DevChanged: // user defined message
			FillCamList();
			break;
		case WM_INITDIALOG: // store this pointer on this message
			hDlg_ = hwndDlg; // must init memeber here!
			Init();
			break;
		case WM_CLOSE:
			UnInit();
			::PostQuitMessage(0);//End message pump
			break;
		case WM_COMMAND :
			if ( LOWORD( wParam) == IDC_CAMLIST){
				if ( HIWORD(wParam) == CBN_SELCHANGE){
					ChooseCamera();
					loopbackTest_.StartLoopbackMax();
				}
			}
			break;
		default:
			return FALSE;
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
		if ( _hwnd_obj_map.find(hwndDlg) != _hwnd_obj_map.end())
		{
			pThis = (LoopbackTestUI *)_hwnd_obj_map[hwndDlg] ;
			return pThis->DialogProc(hwndDlg, uMsg, wParam, lParam);
		}else{
			return FALSE;
		}
	}

	void Init()
	{
		camera_.Init();
		// device event manager
		deviceEvents_ = new DeviceEvents(this);
		deviceEvents_->Startup();
		loopbackTest_.Init();
		// fill combo
		FillCamList();
	}
	
	void UnInit()
	{
		deviceEvents_->Shutdown();
		delete deviceEvents_;
		camera_.UnInit();
		loopbackTest_.UnInit();
	}

	void FillCamList()
	{
		HWND hCtrl = ::GetDlgItem(hDlg_, IDC_CAMLIST);
		// clear items
		ComboBox_ResetContent(hCtrl);
		// update list
		camList_ = camera_.GetCameraList();
		for ( int i =0 ; i< camList_.size(); i++)
		{
			//camList_[i].devFriendlyName.c_str()
			int idx= ComboBox_AddString(hCtrl,camList_[i].devFriendlyName.c_str());
			ComboBox_SetItemData(hCtrl, idx, i); // store the camera idx
		}
		ComboBox_SetCurSel(hCtrl, 0);// select zero
	}

	void ChooseCamera()
	{
		// no camera
		if (camList_.empty()) return;

		HWND hCtrl = ::GetDlgItem(hDlg_, IDC_CAMLIST);
		int idx = ComboBox_GetCurSel(hCtrl);
		int camidx = ComboBox_GetItemData(hCtrl, idx);
		camera_.CloseCamera();
		camera_.OpenCamera(camList_[camidx].symbolicLink);
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
		case DBT_DEVICEARRIVAL:
			::PostMessage(hDlg_, UserMsg_DevChanged, 0, 0); // fire event to UI thread
			break;
		case DBT_DEVICEQUERYREMOVE:
		case DBT_DEVICEQUERYREMOVEFAILED:
		case DBT_DEVICEREMOVEPENDING :
			break;
		case DBT_DEVICEREMOVECOMPLETE :
			::PostMessage(hDlg_, UserMsg_DevChanged, 0, 0); // fire event to UI thread
			break;
		case DBT_DEVICETYPESPECIFIC :
			break;
		case DBT_CONFIGCHANGED :
			break;
		}
	}

private:
	Camera           camera_;
	std::vector<MediaDevInfo> camList_;
	LoopbackTest  loopbackTest_;
	DeviceEvents    *deviceEvents_;
	DWORD dialogId_;
	HWND hDlg_;
};