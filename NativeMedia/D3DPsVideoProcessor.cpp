#include "stdafx.h"
#include <string>
#include <stdio.h>
#include <d3dcompiler.h>

#include "D3DPsVideoProcessor.h"

#pragma comment(lib,"d3dcompiler.lib")

namespace WinRTCSDK{

D3DPsVideoProcessor::D3DPsVideoProcessor()
{
	nWidth_ = 0;
	nHeight_ = 0;
    bFlipHorizontally_ =false;
	bFlipVertically_=false;
}

D3DPsVideoProcessor::~D3DPsVideoProcessor()
{
}

void D3DPsVideoProcessor::UnInit()
{
	// just force the smart pointers to release objects now
	pd3dDevice_=NULL;
	pd3dTextureY_=NULL;
	pd3dTextureU_=NULL;
	pd3dTextureV_=NULL;
	pd3dTextureRGB32_=NULL;
	pd3dVertexBuffer_=NULL;
	pd3dVertexBuffer4Icon_=NULL;
	pPixelShader_=NULL;
}

HRESULT D3DPsVideoProcessor::_CompileShader( _In_ LPCVOID  src, SIZE_T size, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob )
{
    if ( !src || !entryPoint || !profile || !blob )
       return E_INVALIDARG;

    *blob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS| D3DCOMPILE_DEBUG;

    const D3D_SHADER_MACRO defines[] = 
    {
        "EXAMPLE_DEFINE", "1",
        NULL, NULL
    };

    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    HRESULT hr = ::D3DCompile( src, size, NULL, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entryPoint, profile,
		flags, 0, &shaderBlob, &errorBlob );

    if ( FAILED(hr) )
    {
        if ( errorBlob )
        {
            OutputDebugStringA( (char*)errorBlob->GetBufferPointer() );
            errorBlob->Release();
        }

        if ( shaderBlob )
           shaderBlob->Release();
        return hr;
    }    

    *blob = shaderBlob;
    return hr;
}

static std::string ps_yuy2=
	"float4 texOffsets;\n"
	"SamplerState texSampler {MipFilter = LINEAR;MinFilter = LINEAR; MagFilter = LINEAR;};\n"
	"struct PSInput\n"
	"{\n"
	"float2 uv : TEXCOORD0;\n"
	"};\n"

	"float4 main(PSInput input) : SV_TARGET\n"
	"{\n"
	"float4 pix = tex2D(texSampler, input.uv);\n"
	"return pix;"\
	"}";

HRESULT D3DPsVideoProcessor::Init(_com_ptr_IDirect3DDevice9Ex pd3dDevice, UINT nWidth, UINT nHeight, D3DFORMAT nPixelFormat)
{
	HRESULT hr;
	pd3dDevice_ = pd3dDevice;
	nWidth_ = nWidth;
	nHeight_ = nHeight;
	nPixelFormat_ = nPixelFormat;

	if ( pd3dDevice_ == NULL) return E_INVALIDARG;


	std::string	 strPixelShaderCode = 
		"SamplerState YTextue;"\
		"SamplerState UTextue;"\
		"SamplerState VTextue;"\
		"float4 main( float2 texCoord : TEXCOORD0 ) : COLOR0"\
		"{"\
		"float3 yuvColor;"\
		"float3 delYuv = float3(-16.0/255.0 , -128.0/255.0 , -128.0/255.0);"\
		"yuvColor.x = tex2D( YTextue, texCoord ).x;"\
		"yuvColor.y = tex2D( UTextue, texCoord ).x;"\
		"yuvColor.z = tex2D( VTextue, texCoord ).x;"\
		"yuvColor += delYuv;"\
		"float3 matYUVRGB1 = float3(1.164,  2.018 ,   0.0   );"\
		"float3 matYUVRGB2 = float3(1.164, -0.391 , -0.813  );"\
		"float3 matYUVRGB3 = float3(1.164,    0.0 ,  1.596  );"\
		"float4 rgbColor;"\
		"rgbColor.x = dot(yuvColor,matYUVRGB1);"\
		"rgbColor.y = dot(yuvColor,matYUVRGB2);"\
		"rgbColor.z = dot(yuvColor,matYUVRGB3);"\
		"rgbColor.w = 1.0f;"\
		"return rgbColor;"\
		"}";


	ID3DBlob *psBlob = nullptr;
	hr = _CompileShader(strPixelShaderCode.c_str(), strPixelShaderCode.size(),  "main", "ps_2_0", &psBlob);
	if (FAILED(hr)) goto DONE;

	hr = pd3dDevice_->CreatePixelShader( (DWORD*)psBlob->GetBufferPointer(), &pPixelShader_ );
	if (FAILED(hr)) goto DONE;
	psBlob->Release();

	// shader for yuy2
	psBlob = nullptr;
	hr = _CompileShader(ps_yuy2.c_str(), ps_yuy2.size(),  "main", "ps_2_0", &psBlob);
	if (FAILED(hr)) goto DONE;
	hr = pd3dDevice_->CreatePixelShader( (DWORD*)psBlob->GetBufferPointer(), &pPixelShaderYUY2_ );
	if (FAILED(hr)) goto DONE;
	psBlob->Release();

	// Texture Addressing mode
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP );
	pd3dDevice_->SetSamplerState( 1, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP );
	pd3dDevice_->SetSamplerState( 1, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP );  
	pd3dDevice_->SetSamplerState( 2, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP );
	pd3dDevice_->SetSamplerState( 2, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP );

	// Texture filter
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
	pd3dDevice_->SetSamplerState( 1, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 1, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 1, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
	pd3dDevice_->SetSamplerState( 2, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 2, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 2, D3DSAMP_MIPFILTER, D3DTEXF_POINT );
	
	// Cull mode
	pd3dDevice_->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

	// Turn off some states
	pd3dDevice_->SetRenderState(D3DRS_ZENABLE, FALSE );
	pd3dDevice_->SetRenderState(D3DRS_ZWRITEENABLE, FALSE );
	pd3dDevice_->SetRenderState(D3DRS_LIGHTING,FALSE);
	pd3dDevice_->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	pd3dDevice_->SetRenderState(D3DRS_STENCILENABLE, FALSE);

	// Enable alpha blending (frame buffer).
	pd3dDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
	pd3dDevice_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

	
	hr = pd3dDevice_->CreateTexture( nWidth, nHeight,     1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTextureY_, NULL);
	hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTextureU_, NULL);
	hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTextureV_, NULL);

	if( FAILED(hr)) goto DONE;

	// YUY2 tex
	hr = pd3dDevice_->CreateTexture( nWidth, nHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_YUY2, D3DPOOL_DEFAULT, &pd3dTextureYUY2_, NULL);
	
	CustomVertex* pVertices = NULL;

	// Vertex buffer for video
	hr = pd3dDevice_->CreateVertexBuffer(4*sizeof(CustomVertex), 
		D3DUSAGE_DYNAMIC,  D3DFVF_CUSTOMVERTEX, 
		D3DPOOL_DEFAULT,  &pd3dVertexBuffer_, NULL ); 
	if( FAILED(hr)) goto DONE;

	hr = pd3dVertexBuffer_->Lock(0, 4 * sizeof(CustomVertex), (void**)&pVertices, 0); 
	if( FAILED(hr)) goto DONE;
	_MakeQuadVertices(nWidth, nHeight, bFlipHorizontally_, bFlipVertically_, pVertices);
	hr = pd3dVertexBuffer_->Unlock();  
	
	// Vertex buffer for icon
	hr = pd3dDevice_->CreateVertexBuffer(4*sizeof(CustomVertex), 
		D3DUSAGE_DYNAMIC,  D3DFVF_CUSTOMVERTEX, 
		D3DPOOL_DEFAULT,  &pd3dVertexBuffer4Icon_, NULL ); 
	if( FAILED(hr)) goto DONE;

	hr = pd3dVertexBuffer4Icon_->Lock(0, 4 * sizeof(CustomVertex), (void**)&pVertices, 0); 
	if( FAILED(hr)) goto DONE;
	_MakeQuadVertices(100, 100, false, false, pVertices);
	hr = pd3dVertexBuffer4Icon_->Unlock();  

	// setup constants for PixelShader
	float texOffset[] ={ 1.0f/(nWidth*2.0f), 1.0f/(nWidth*4.0f), 0.0, 0.0,   };
	pd3dDevice_->SetPixelShaderConstantF(0, texOffset, 1);

DONE:
	return SUCCEEDED(hr);
}
	
HRESULT D3DPsVideoProcessor::_LoadTexture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int height)
{
	HRESULT hr;
	D3DLOCKED_RECT d3dlr;
	hr = tex->LockRect( 0, &d3dlr, 0, D3DLOCK_DISCARD  );
	if( FAILED(hr)) return hr;
	for (int i=0; i<height; i++){
		::memcpy((BYTE*)d3dlr.pBits+i*d3dlr.Pitch, (BYTE*)data+i*stride, stride);
	}
	hr = tex->UnlockRect(0);
	return hr;
}

// pVertex must point to a buffer of 4 CustomVertex
void D3DPsVideoProcessor::_MakeQuadVertices(int w, int h, 
				bool bFlipHorizontally, bool bFlipVertically,  CustomVertex * pVertex)
{
	float u1, u2, v1, v2;
	if ( !bFlipHorizontally){
		u1=0.0f, u2=1.0f;
	}else{
		u1=1.0f, u2=0.0f;
	}

	if ( !bFlipHorizontally){
		v1=0.0f, v2=1.0f;
	}else{
		v1=1.0f, v2=0.0f;
	}

	CustomVertex vertices[] =
	{
		{0.0f,	 0.0f,	 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v1},
		{w*1.0f, 0.0f,	 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v1},
		{w*1.0f, h*1.0f, 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v2},
		{0.0f,	 h*1.0f, 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v2}
	};
	::memcpy(pVertex, vertices, sizeof(vertices));	
}

HRESULT D3DPsVideoProcessor::UpdateTextureYUY2(BYTE* pYUY2, int stride)
{
	//Y plain
	HRESULT hr = _LoadTexture(pd3dTextureYUY2_, pYUY2, stride, nHeight_);
	return hr;
}

HRESULT D3DPsVideoProcessor::UpdateTexture(BYTE* pI420, int stride, bool flipVertically, bool flipHorizontally)
{
	HRESULT hr;
	LPBYTE lpY = NULL;
	LPBYTE lpU = NULL;
	LPBYTE lpV = NULL;

	if (nPixelFormat_ == (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0'))
	{
		lpY = pI420;
		lpU = pI420 + nWidth_ * nHeight_;
		lpV = pI420 + nWidth_ * nHeight_ * 5 / 4;
	}
	else if (nPixelFormat_ == (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2'))
	{
		lpY = pI420;
		lpV = pI420 + nWidth_ * nHeight_;
		lpU = pI420 + nWidth_ * nHeight_ * 5 / 4;
	}
	else
	{
		return E_INVALIDARG;
	}

	if (!pd3dTextureY_ || !pd3dTextureU_ || !pd3dTextureV_)
	{
		return FALSE;
	}

	if ( bFlipHorizontally_ != flipHorizontally || bFlipVertically_!= flipVertically)
	{
		bFlipHorizontally_ = flipHorizontally;
		bFlipVertically_ = flipVertically;
		CustomVertex *  pVertices;
		hr = pd3dVertexBuffer_->Lock(0, 4 * sizeof(CustomVertex), (void**)&pVertices, 0); 
		if( FAILED(hr)) goto DONE;
		_MakeQuadVertices(nWidth_, nHeight_, bFlipHorizontally_, bFlipVertically_, pVertices);
		hr = pd3dVertexBuffer_->Unlock();  
	}

	//Y plain
	hr = _LoadTexture(pd3dTextureY_, lpY, nWidth_,nHeight_);
	if( FAILED(hr)) goto DONE;

	//U plain
	hr = _LoadTexture(pd3dTextureU_, lpU, nWidth_/2, nHeight_/2);
	if( FAILED(hr)) goto DONE;

	//V Plain
	hr = _LoadTexture(pd3dTextureV_, lpV, nWidth_/2, nHeight_/2);
	if( FAILED(hr)) goto DONE;

DONE:
	return hr;
}

// need to be called between a BeginScene/EndScene pair
HRESULT D3DPsVideoProcessor::DrawVideo ()
{
	HRESULT hr;
	pd3dDevice_->SetStreamSource( 0, pd3dVertexBuffer_, 0, sizeof(CustomVertex) );
	pd3dDevice_->SetFVF( D3DFVF_CUSTOMVERTEX );


	hr = pd3dDevice_->SetTexture( 0, pd3dTextureYUY2_ );
	//hr = pd3dDevice_->SetTexture( 1, pd3dTextureV_ );
	//hr = pd3dDevice_->SetTexture( 2, pd3dTextureU_ );
	hr = pd3dDevice_->SetPixelShader( pPixelShaderYUY2_ );

	hr = pd3dDevice_->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
	if( FAILED(hr)) goto DONE;

	hr = pd3dDevice_->SetTexture( 0, NULL );
	hr = pd3dDevice_->SetTexture( 1, NULL );
	hr = pd3dDevice_->SetTexture( 2, NULL );  	
	hr = pd3dDevice_->SetPixelShader( NULL );
DONE:
	return hr;
}

// This method must be called between a BeginScene/EndScene pair!
HRESULT D3DPsVideoProcessor::AlphaBlendingIcon(_com_ptr_IDirect3DTexture9  pIcon, RECT destRC)
{
	HRESULT hr;
	unsigned long w = destRC.right-destRC.left;
	unsigned long h = destRC.bottom-destRC.top;
	
	// Fill Vertex Buffer with new position
	CustomVertex *pVertex;
	hr = pd3dVertexBuffer4Icon_->Lock( 0, 4 * sizeof(CustomVertex), (void**)&pVertex, 0 );
	if (FAILED(hr)) { goto done;}
	_MakeQuadVertices(w, h, false, false, pVertex);
	hr = pd3dVertexBuffer4Icon_->Unlock();

	// setup texture
	hr = pd3dDevice_->SetTexture( 0, pIcon );
	if (FAILED(hr)) { goto done;}	
	// binds a vertex buffer to a device data stream.
	hr = pd3dDevice_->SetStreamSource( 0, pd3dVertexBuffer4Icon_, 0, sizeof(CustomVertex));
	if (FAILED(hr)) { goto done;}
	// sets the current vertex stream declaration.
	hr = pd3dDevice_->SetFVF( D3DFVF_CUSTOMVERTEX );
	if (FAILED(hr)) { goto done;}
	// draw the primitive
	hr = pd3dDevice_->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
	if (FAILED(hr)) { goto done;}

	hr = pd3dDevice_->SetTexture( 0, NULL);

done:

	return hr;
}

}