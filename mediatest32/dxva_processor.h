
#pragma once

#include "stdafx.h"

#include "D3D9Renderer.h"
#include "d3d_allocator.h"
#include "decode_pipeline.h"
#include "va_interface.h"

using namespace WinRTCSDK;
using namespace MP;

class DXVAProcessor : public MP::IDXVAVideoProcessor
{
public:
	DXVAProcessor(_com_ptr_IDirect3DDeviceManager9 pMan):pDevMan_(pMan)
	{
	}

	bool Init()
	{
		allocator_ = new D3DFrameAllocator();
		allocator_->Init(pDevMan_);
		return true;
		
	}

	bool UnInit()
	{
		delete allocator_;
	}

public:

	virtual bool videoProcessBlt(VideoFrameData * pSrc, VideoFrameData * pTarget) 
	{
		if ( D3D9_SURFACE != pSrc->type ) return false;
		if ( MFX_FRAME_SURFACE != pSrc->type ) return false;

		HANDLE hDev;
		IDirect3DDevice9 *pDev;
		HRESULT hr = pDevMan_->OpenDeviceHandle(&hDev);
		if ( FAILED (hr) ) goto DONE;
		
		hr = pDevMan_->LockDevice(hDev, &pDev, FALSE);
		if ( FAILED (hr) ){
			::CloseHandle(hDev);
			goto DONE;
		}

		IDirect3DSurface9 * pSrcSurf, *pDstSurf;
		pSrcSurf = (IDirect3DSurface9 *)pSrc->pSurface;
		pDstSurf = (IDirect3DSurface9 *)pTarget->pSurface;

		hr = pDev->StretchRect(pSrcSurf, NULL, pDstSurf, NULL, D3DTEXF_LINEAR );
		pDevMan_->UnlockDevice(hDev, FALSE);
		pDev->Release();
DONE:
		return hr==S_OK;
	}

	virtual void * getHandle() 
	{ 
		return pDevMan_.GetInterfacePtr();
	}

	virtual void * getFrameAllocator() 
	{
		return (mfxFrameAllocator*)allocator_;
	}

private:
	_com_ptr_IDirect3DDeviceManager9 pDevMan_;
	D3DFrameAllocator * allocator_;

};
