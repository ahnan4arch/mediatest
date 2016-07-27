#include "stdafx.h"
#include "d3d9renderer.h"
#include "ImageTool.h"
#include <sstream>  
#include "NativeMediaLog.h"

namespace WinRTCSDK{

D3D9Video::D3D9Video ( _com_ptr_IDirect3D9Ex pD3DEx,  HWND hwnd)
{
    width_ = 1;
    height_ = 1;  
    pD3DEx_ = pD3DEx;
	hwnd_ = hwnd;
	pPsVideoProcessor_ = new D3DPsVideoProcessor();
}

D3D9Video::~D3D9Video ()
{
	_Destroy();
	delete pPsVideoProcessor_;
}

HRESULT D3D9Video::_Destroy ()
{
	pSwapChain_ = NULL;
    pDevice_ = NULL;
    pD3DDevManager_ = NULL;
	return S_OK;
}

HRESULT D3D9Video::Create(const IconData * iconData)
{
	HRESULT hr;

    memset (&d3dpp_, 0, sizeof (D3DPRESENT_PARAMETERS));
    d3dpp_.BackBufferFormat     = D3DFMT_X8R8G8B8;
    d3dpp_.SwapEffect           = D3DSWAPEFFECT_FLIPEX;
    d3dpp_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dpp_.Windowed             = TRUE;
    d3dpp_.hDeviceWindow        = hwnd_;
	d3dpp_.BackBufferHeight     = 1;
	d3dpp_.BackBufferWidth      = 1;
	d3dpp_.BackBufferCount      = 4;
	d3dpp_.Flags                = D3DPRESENTFLAG_VIDEO;

	//Get Adapter Information
	D3DDISPLAYMODE mode = { 0 };
    hr = pD3DEx_->GetAdapterDisplayMode( D3DADAPTER_DEFAULT,  &mode );
	if (FAILED(hr)) goto DONE;

	D3DCAPS9 caps;
	hr = pD3DEx_->GetDeviceCaps(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&caps);
	if (FAILED(hr)) goto DONE;

    hr = pD3DEx_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		hwnd_, D3DCREATE_HARDWARE_VERTEXPROCESSING |D3DCREATE_MULTITHREADED|
		D3DCREATE_ENABLE_PRESENTSTATS | D3DCREATE_FPU_PRESERVE,
        &d3dpp_, NULL, &pDevice_ );

	if (FAILED(hr)) goto DONE;
	
	// get the swapchain
	hr = pDevice_->GetSwapChain(0, &pSwapChain_);
	if (FAILED(hr)) goto DONE;

	// create d3d manager, for DXVA decoders
	UINT resetToken = 0;
    hr = ::DXVA2CreateDirect3DDeviceManager9(&resetToken, &pD3DDevManager_);
	if (FAILED(hr)) goto DONE;

    hr = pD3DDevManager_->ResetDevice(pDevice_, resetToken);
	if (FAILED(hr)) goto DONE;

	// create the mute icon
	if ( iconData != NULL)
	{
		HRESULT hr = pDevice_->CreateTexture(iconData->width, iconData->height, 1, D3DUSAGE_DYNAMIC,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &micMuteIcon_, NULL );
		if (FAILED(hr)) { goto DONE;}
		D3DLOCKED_RECT lr;
		hr = micMuteIcon_->LockRect(0, &lr, NULL, D3DLOCK_DISCARD );
		if (FAILED(hr)) { goto DONE;}
		BYTE * srcline = iconData->argb;
		BYTE * destline = (BYTE*)lr.pBits;
		for (int j=0; j<iconData->height; j++)
		{
			::memcpy(destline, srcline, iconData->width*4);
			srcline += iconData->width*4;
			destline += lr.Pitch;
		}
		hr = micMuteIcon_->UnlockRect(0);
	}

DONE:
	return hr;

}

VOID D3D9Video::Reset(UINT width, UINT height)
{
    width_ = width;
    height_ = height;  
	d3dpp_.BackBufferWidth = width_;
	d3dpp_.BackBufferHeight = height_;
	pDevice_->ResetEx(&d3dpp_, NULL);
	pPsVideoProcessor_->UnInit();
	pPsVideoProcessor_->Init(pDevice_, width, height, D3DFMT_I420);
}

// for software decoder, using shader
HRESULT D3D9Video::_DrawVideoPS(VideoFrameInfo &frame)
{
	HRESULT hr;
	if ( frame.image == NULL) return E_INVALIDARG;
	
	RECT rcIcon;
	// calculate icon position
	if(frame.bMicMuteIcon && micMuteIcon_!=NULL){
		RECT rcHwnd;
		::GetWindowRect(hwnd_, &rcHwnd);
		D3DSURFACE_DESC surf_desc;
		hr = micMuteIcon_->GetLevelDesc(0, &surf_desc);
		if ( FAILED(hr)) goto done;

		float r = 1.0;
		if ( rcHwnd.right - rcHwnd.left > 0){
			r =  0.75f *(1.0f * width_)/(1.0f * (rcHwnd.right - rcHwnd.left)) ; 
		}
		::SetRect(&rcIcon, 0, 0, surf_desc.Width * r, surf_desc.Height * r);//Target rect of the icon
	}
		
	// update texture with YUV data
//	pPsVideoProcessor_->UpdateTexture(frame.image, frame.stride, true, true);
	pPsVideoProcessor_->UpdateTextureYUY2(frame.image, frame.stride);

	// begin drawing ...
	hr = pDevice_->BeginScene();
	if ( FAILED(hr)) goto done;
	hr = pPsVideoProcessor_->DrawVideo();
	if ( FAILED(hr)) goto done;

	 // need to blending an icon...
	if(frame.bMicMuteIcon && micMuteIcon_!=NULL){
		hr = pPsVideoProcessor_->AlphaBlendingIcon(micMuteIcon_, rcIcon);
	}
	if ( FAILED(hr)) goto done;

	hr = pDevice_->EndScene();
	if ( FAILED(hr)) goto done;
	// present now!
	hr = pDevice_->PresentEx(NULL, NULL, NULL, NULL, 0);

done:
	return hr;
}

// for DXVA
HRESULT D3D9Video::_DrawVideoDXVA(VideoFrameInfo &frame)
{
	HRESULT hr;	
	if (frame.pSample == NULL) return E_INVALIDARG;

	_com_ptr_IDirect3DSurface9 pRT = NULL;
	hr = pSwapChain_->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO , &pRT);
	if (FAILED(hr)) { goto DONE;}
	hr = pDevice_->StretchRect(frame.pSample, NULL, pRT, NULL,  D3DTEXF_NONE);
	if (FAILED(hr)) { goto DONE;}
	// we will use DXVA later for flexibility
	hr = pDevice_->PresentEx(NULL, NULL, NULL, NULL, 0);

DONE:
	return hr;
}

HRESULT D3D9Video::DrawVideo(VideoFrameInfo &frame)
{
	HRESULT hr;	
	if (frame.pSample){// DXVA decoder
		hr = _DrawVideoDXVA(frame);
	}else{// software decoder
		hr = _DrawVideoPS(frame);
	}
	return hr;
}

INT D3D9Video::GetWidth() 
{ 
	return width_; 
}

INT D3D9Video::GetHeight() 
{ 
	return height_; 
}

_com_ptr_IDirect3DDeviceManager9 D3D9Video::GetD3DDeviceManager9()
{
	return pD3DDevManager_;
}

//-------------------------------------------------------------------
// D3D9Renderer  Class
//-------------------------------------------------------------------
D3D9Renderer::D3D9Renderer():
	dispatcher_("d3ddev_creator"),
    pD3DEx_(NULL)
{
}

D3D9Renderer::~D3D9Renderer()
{
    _Destroy();
}

HRESULT D3D9Renderer::Create()
{
	HRESULT hr = S_OK;

	if (pD3DEx_ != NULL) return S_OK;

    // Create the Direct3D object.
	hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx_);
	if (FAILED(hr) )  goto DONE;

DONE:
    return hr;
}

D3D9Video* D3D9Renderer::_GetOrResetD3DVideo(string videoId, int width, int height)
{
	D3D9Video* pD3DVideo = NULL;
	if (videoList_.find(videoId) != videoList_.end())
	{
		pD3DVideo = videoList_[videoId];
		int w = pD3DVideo->GetWidth();
		int h = pD3DVideo->GetHeight();
		if (w!=width || h!= height)
		{ 
			// video resolution changed, resize the device
			// d3d device need to be created/rest by a same thread.
			LooperTask t = [=]{ pD3DVideo->Reset(width, height);};
			dispatcher_.runSyncTask(t);
		}
	}

	return pD3DVideo;
}

HRESULT D3D9Renderer::DrawVideo(string videoId, VideoFrameInfo &frame)
{
	HRESULT hr;
    if (pD3DEx_ == NULL) return S_OK;

	D3D9Video*  pD3DVideo = NULL;	
	shared_ptr<Mutex> delLock = NULL;
	///////////////////////////////////////
	// locking scope of the video list
	{ 
		AutoLock autolock(maplock_);
		if ( frame.pSample ) // DXVA decoder
		{
			D3DSURFACE_DESC surf_desc;
			frame.pSample->GetDesc(&surf_desc);
			pD3DVideo = _GetOrResetD3DVideo(videoId, surf_desc.Width, surf_desc.Height);
		}
		else // software decoder
		{
			pD3DVideo = _GetOrResetD3DVideo(videoId, frame.width, frame.height);
		}
		if (pD3DVideo)
		{
			delLock = deletionLocks_[pD3DVideo];
		}
	}

	hr = E_FAIL;
	///////////////////////////////////////////////
	// locking scope of the individual video stream 
	if (pD3DVideo != NULL )
	{
		AutoLock al (*delLock);
		hr =  pD3DVideo->DrawVideo(frame);
	}

	return hr;
}

void D3D9Renderer::RemoveVideo(string videoId)
{
	AutoLock autolock(maplock_); // lock the video list
	if (videoList_.find(videoId) != videoList_.end())
	{
		D3D9Video *	pD3DVideo = videoList_[videoId];
		shared_ptr<Mutex> delLock = deletionLocks_[pD3DVideo];
		// must lock the delete lock, 
		// for we allow seperate threads to do drawing with the D3D9Video object
		AutoLock al ( *delLock); 
		// delete the video stream
		videoList_.erase(videoId);  // erase the map item		
		deletionLocks_.erase(pD3DVideo);// also erase the delete lock
		delete pD3DVideo;
	}
}

// create the video stream
void D3D9Renderer::CreateVideo(string videoId, HWND hwnd)
{
	AutoLock autolock(maplock_);
	RemoveVideo(videoId); // always delete old one
	// create new one
	D3D9Video *	pD3DVideo = new  D3D9Video (pD3DEx_, hwnd); 
	videoList_[videoId] = pD3DVideo;
	deletionLocks_ [pD3DVideo] = std::make_shared<Mutex>();
	// d3d device need to be created/rest by a same thread.
	LooperTask t = [=]{ pD3DVideo->Create( &muteIconData_);};
	dispatcher_.runSyncTask(t);
}

// provide DXVA for other modules
_com_ptr_IDirect3DDeviceManager9 D3D9Renderer::GetD3DDeviceManager9(string videoId)
{
	AutoLock autolock(maplock_);
	D3D9Video * pD3DVideo = NULL;
	if (videoList_.find(videoId) != videoList_.end())
	{
		pD3DVideo = videoList_[videoId];
		return pD3DVideo->GetD3DDeviceManager9();
	}
	return NULL;
}

void D3D9Renderer::SetMicMuteIcon(BYTE* argb, int w, int h)
{
	////// make a fake icon
	//DWORD *data = new DWORD[100 * 100];
	//DWORD *pixel = data;
	//for (int j=0; j<100 * 100; j++){
	//	*pixel = D3DCOLOR_ARGB(100, 70, 80, 90);
	//	pixel++;
	//}
	//muteIconData_.Set((BYTE*)data, 100, 100);
	//delete [] data;
	////// fake icon for test

	muteIconData_.Set(argb, w, h);

done:
	return;
}

HRESULT D3D9Renderer::Reset()
{
	// TODO: reset individual devices
	return S_OK;
}

void D3D9Renderer::_Destroy()
{
	AutoLock autolock(maplock_); // lock the video list
	map <string, D3D9Video *>::iterator i = videoList_.begin();
	while ( i != videoList_.end()) // delete all video streams
	{
		D3D9Video *	pD3DVideo = i->second;
		if ( pD3DVideo != NULL)
		{
			shared_ptr<Mutex> delLock = deletionLocks_[pD3DVideo];
			// must lock the delete lock, 
			// for we allow seperate threads to do drawing with the D3D9Video object
			AutoLock al ( *delLock); 
			delete pD3DVideo;
		}
		i++;
	}

	deletionLocks_.clear();
    videoList_.clear();
	pD3DEx_ = NULL;
}

} // namespace WinRTCSDK

