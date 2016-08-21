
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
		return true;
	}

public:
	static bool _YUY2_TO_NV12(unsigned char *src, int srcStride, 
		int width, int height, unsigned char *dest, int dstStride)
	{
		// setup default stride
		// for hardware decoder(of camera), srcStride > width*2
		if ( srcStride == 0){
			srcStride = width * 2;
		}
		if ( dstStride == 0){
			dstStride = width ;
		}

		unsigned char *y_plane = dest;
		unsigned char *u_plane = dest + width * height;
		unsigned char *v_plane = u_plane+1;


		unsigned char *scan = src;

		for ( int i = 0 ; i < height; i++)
		{
			scan = src + i * srcStride;
			unsigned char * y_scan = y_plane + i*dstStride;
			unsigned char * u_scan = u_plane + i/2*dstStride;
			unsigned char * v_scan = v_plane + i/2*dstStride;
			if ( i % 2 == 0 )   // it's odd scan line.
			{
				for ( int j = 0; j < width / 2; j++) // process 2 pixels per iteration
				{
					*y_scan++   = *scan++;
					*u_scan++ = *scan++;
					u_scan++;
					*y_scan++   = *scan++;
					*v_scan++ = *scan++;
					v_scan++;
				}
			}
			else
			{
				for ( int j = 0; j < width / 2; j++)
				{
					*y_scan++   = *scan++;
					scan++;
					*y_scan++   = *scan++;
					scan++;
				}
			}
		}

		return true;
	}


	virtual bool videoProcessBlt(VideoFrameData * pSrc, VideoFrameData * pTarget) 
	{
		if ( D3D9_SURFACE != pSrc->type ) return false;
		if ( MFX_FRAME_SURFACE != pTarget->type ) return false;

		HANDLE hDev;
		IDirect3DDevice9 *pDev;
		HRESULT hr = pDevMan_->OpenDeviceHandle(&hDev);
		if ( FAILED (hr) ) goto DONE;
		
		hr = pDevMan_->LockDevice(hDev, &pDev, FALSE);
		if ( FAILED (hr) ){
			::CloseHandle(hDev);
			goto DONE;
		}

		// setup source
		IDirect3DSurface9 * pSrcSurf, *pDstSurf;
		pSrcSurf = (IDirect3DSurface9 *)pSrc->pSurface;

		// setup dest
		mfxFrameSurface1 * pFrameSurf = (mfxFrameSurface1 *)pTarget->pSurface;
		mfxHDL pSurf;
		allocator_->GetHDL(allocator_->pthis, pFrameSurf->Data.MemId, &pSurf);
		pDstSurf = (IDirect3DSurface9 *)pSurf;
		D3DSURFACE_DESC desc1;
		D3DSURFACE_DESC desc2;
		pSrcSurf->GetDesc(&desc1);
		pDstSurf->GetDesc(&desc2);

//		hr = pDev->StretchRect(pSrcSurf, NULL, pDstSurf, NULL, D3DTEXF_NONE );
		D3DLOCKED_RECT lr_src,lr_dst;
		hr = pSrcSurf->LockRect(&lr_src, NULL, 0);
		if ( FAILED (hr) ){ goto CLEAR;}

		hr = pDstSurf->LockRect(&lr_dst, NULL, 0);
		if ( FAILED (hr) ){ goto CLEAR;}

		_YUY2_TO_NV12((unsigned char *)lr_src.pBits, lr_src.Pitch,
			desc1.Width, desc1.Height, (unsigned char *)lr_dst.pBits, lr_dst.Pitch);

		pSrcSurf->UnlockRect();
		pDstSurf->UnlockRect();

CLEAR:
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
