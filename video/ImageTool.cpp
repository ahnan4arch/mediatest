#include "stdafx.h"
#include "ImageTool.h"

static bool _tbl_initialized = false;
static long int crv_tab[256];
static long int cbu_tab[256];
static long int cgu_tab[256];
static long int cgv_tab[256];
static long int tab_76309[256];
static unsigned char clp[1024];            //for clip in CCIR601

static void InitConvertTableYUV420_RGB()
{
	if(_tbl_initialized ) return;

    long int crv,cbu,cgu,cgv;
    int i,ind;   
    
    crv = 104597; cbu = 132201;  // fra matrise i global.h
    cgu = 25675;  cgv = 53279;
    
    for (i = 0; i < 256; i++) 
    {
        crv_tab[i] = (i-128) * crv;
        cbu_tab[i] = (i-128) * cbu;
        cgu_tab[i] = (i-128) * cgu;
        cgv_tab[i] = (i-128) * cgv;
        tab_76309[i] = 76309*(i-16);
    }
    
    for (i=0; i<384; i++)
        clp[i] =0;
    ind=384;
    for (i=0;i<256; i++)
        clp[ind++]=i;
    ind=640;
    for (i=0;i<384;i++)
        clp[ind++]=255;

	_tbl_initialized = true;
}

//  Convert from YUV420 to ARGB32 (small endian, B first in memory)
void ConvertYUV420ToARGB(unsigned char *src0,unsigned char *src1,unsigned char *src2,
                         unsigned char *dst_ori, int width,int height)
{
	InitConvertTableYUV420_RGB(); // init once

    int y1,y2,u,v; 
    unsigned char *py1,*py2;
    int i,j, c1, c2, c3, c4;
    unsigned char *d1, *d2;
    
    py1=src0;
    py2=py1+width;
    d1=dst_ori;
    d2=d1+4*width;
    //    d1=dst_ori;
    //    d2=d1+3*width;
    for (j = 0; j < height; j += 2) 
    { 
        for (i = 0; i < width; i += 2) 
        {
            u = *src1++;
            v = *src2++;
            
            //c1 = crv_tab[v];
            //c2 = cgu_tab[u];
            //c3 = cgv_tab[v];
            //c4 = cbu_tab[u];
            c1 = cbu_tab[u];
            c2 = cgu_tab[u];
            c3 = cgv_tab[v];
            c4 = crv_tab[v];
            
            //up-left
            y1 = tab_76309[*py1++];    
            *d1++ = clp[384+((y1 + c1)>>16)];  
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];
            *d1++ = clp[384+((y1 + c4)>>16)];
            *d1++ = 0xFF;
            
            //down-left
            y2 = tab_76309[*py2++];
            *d2++ = clp[384+((y2 + c1)>>16)];  
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];
            *d2++ = clp[384+((y2 + c4)>>16)];
            *d2++ = 0xFF;
            
            //up-right
            y1 = tab_76309[*py1++];
            *d1++ = clp[384+((y1 + c1)>>16)];  
            *d1++ = clp[384+((y1 - c2 - c3)>>16)];
            *d1++ = clp[384+((y1 + c4)>>16)];
            *d1++ = 0xFF;
            
            //down-right
            y2 = tab_76309[*py2++];
            *d2++ = clp[384+((y2 + c1)>>16)];  
            *d2++ = clp[384+((y2 - c2 - c3)>>16)];
            *d2++ = clp[384+((y2 + c4)>>16)];
            *d2++ = 0xFF;
        }
        //        d1 += 3*width;
        //        d2 += 3*width;
        d1 += 4*width;
        d2 += 4*width;
        py1+=   width;
        py2+=   width;
    }       
}

// C version. we use table lookup method to avoid multiplication
void ConvertARGBToI420 (unsigned char *argb, int width, int height, unsigned char *yuv)
{
    static unsigned short y_tbl[3][256] = {{0}, {0}, {0}};
    static short u_tbl[3][256] = {{0}, {0}, {0}};
    static short v_tbl[3][256] = {{0}, {0}, {0}};

    // should add multi-thread protection
    if ( y_tbl[0][255] == 0 )  // lookup table should only be initialized once
    {
        for ( int i = 0; i < 256; i++)
        {
            y_tbl[0][i] = 66  * i;
            y_tbl[1][i] = 129 * i;
            y_tbl[2][i] = 25  * i;
            u_tbl[0][i] = -38 * i;
            u_tbl[1][i] = -74 * i;
            u_tbl[2][i] = 112 * i;
            v_tbl[0][i] = 112 * i;
            v_tbl[1][i] = -94 * i;
            v_tbl[2][i] = -18 * i;
        }
    }

    bool need_uv_vertically;
    unsigned char *u = yuv + width * height;
    unsigned char *v = u   +  width * height / 4;

    for ( int i = 0; i < height; i++)
    {
        need_uv_vertically = (i % 2 == 0); // calculate once per stride

        for ( int j = 0; j < width ; j++)
        {
            // for big endian:
            // R = *(argb+1) , G = *(argb+2) , B = *(argb+3)

            // for little endian:
            // R = *(argb+2) , G = *(argb+1) , B = *(argb+0)
            int R_off=2, G_off=1, B_off=0;

            *yuv = (unsigned char)((y_tbl[0][*(argb + R_off)] + y_tbl[1][*(argb + G_off)] + y_tbl[2][*(argb + B_off)] + 128) >> 8) + 16;
            yuv ++;

            if ( need_uv_vertically && ( j % 2 == 0 ) )
            {
                *u = (unsigned char)((u_tbl[0][*(argb + R_off)] + u_tbl[1][*(argb + G_off)] + u_tbl[2][*(argb + B_off)] + 128) >> 8 ) + 128;
                *v = (unsigned char)((v_tbl[0][*(argb + R_off)] + v_tbl[1][*(argb + G_off)] + v_tbl[2][*(argb + B_off)] + 128) >> 8 ) + 128;
                u++;
                v++;
            }

            argb += 4; // next ARGB pixel
        }
    }

}

void CutImageI420 ( unsigned char * src, int src_w, int src_h, unsigned char * dest, int org_x, int org_y, int dest_w, int dest_h )
{
    unsigned char *  y = src;
    unsigned char *  u = y + src_w*src_h;
    unsigned char *  v = u + src_w*src_h/4;
    unsigned char *  y2 = dest;
    unsigned char *  u2 = y2 + dest_w*dest_h;
    unsigned char *  v2 = u2 + dest_w*dest_h/4;
    // cut Y' component
    y = y + org_y  * src_w + org_x;
    for ( int i=0; i< dest_h; i++){
        memcpy ( y2, y, dest_w );
        y  += src_w;
        y2 += dest_w;
    }
    // cut Cb component
    u = u + org_y/2  * src_w/2 + org_x/2;
    for ( int i=0; i< dest_h/2; i++){
        memcpy ( u2, u, dest_w/2 );
        u  += src_w/2;
        u2 += dest_w/2;
    }
    // cut Cr component
    v = v + org_y/2  * src_w/2 + org_x/2;
    for ( int i=0; i< dest_h/2; i++){
        memcpy ( v2, v, dest_w/2 );
        v  += src_w/2;
        v2 += dest_w/2;
    }

}


void _down_scale_component (unsigned char * src_comp, unsigned char * dest_comp, int w, int h )
{
    int i, j;
    for ( j=0; j<h; j+=2){
        for (i=0; i<w; i+=2){
            *dest_comp = *src_comp;
            dest_comp++;
            src_comp+=2; // skip on pixel horizontally
        }
        src_comp  += w ; // skip one line vertically
    }
}

void DownScaleImage2X ( unsigned char * src, unsigned char * dest, int w, int h )
{
    int w2 = w/2;
    int h2 = h/2;
    unsigned char *  y = src;
    unsigned char *  u = y + w*h;
    unsigned char *  v = u + w*h/4;
    unsigned char *  y2 = dest;
    unsigned char *  u2 = y2 + w2*h2;
    unsigned char *  v2 = u2 + w2*h2/4;
    // down scal Y-comp
    _down_scale_component (y, y2, w, h);
    // scal U-comp
    _down_scale_component (u, u2, w2, h2);
    // scal V-comp
    _down_scale_component (v, v2, w2, h2);

}

void ConvertYUY2ToYV12(unsigned char *src, int width, int height, unsigned char *dest)
{
    unsigned char *v_plane = dest + width * height;
    unsigned char *u_plane = v_plane  +  width * height / 4;

    for ( int i = 0 ; i < height; i++)
    {
        if ( i % 2 == 0 )   // it's odd scan line.
        {
            for ( int j = 0; j < width / 2; j++) // process 2 pixels per iteration
            {
                *dest++   = *src++;
                *u_plane++ = *src++;
                *dest++   = *src++;
                *v_plane++ = *src++;
            }
        }
        else
        {
            for ( int j = 0; j < width / 2; j++)
            {
                *dest++   = *src++;
                src++;
                *dest++   = *src++;
                src++;
            }
        }
    }
}

void ConvertI420ToYV12(BYTE *dest, LONG destStride, LONG destHeight, BYTE *src, LONG width, LONG height, BOOL bottomUp)
{
	int i = 0;
	BYTE * pScan = src;
	int    stride = width;
	if (bottomUp)
	{
		pScan = src + (height -1) * width;
		stride = (-1) * width;
	}
	for(i = 0;i < height;i ++)
	{
		memcpy(dest + i * destStride, pScan, width);
		pScan += stride;
	}

	// copy 3rd plane of src to 2nd plane of dest
	pScan = src + width * height + width * height / 4;
	stride = width / 2;
	if (bottomUp)
	{
		pScan = src + width * height + width * height / 4 + ( (height/2 - 1) * width / 2) ;
		stride = (-1) *width / 2;
	}
	for(i = 0;i < height/2;i ++)
	{
		memcpy(dest + destStride * destHeight + i * destStride / 2, pScan, width / 2);
		pScan += stride;
	}

	// copy 2nd plane of src to 3rd plane of dest
	pScan = src + width * height;
	stride = width / 2;
	if (bottomUp)
	{
		pScan = src + width * height  + ( (height/2 - 1) * width / 2) ;
		stride = (-1) *width / 2;
	}
	for(i = 0;i < height/2;i ++)
	{
		memcpy(dest + destStride * destHeight + destStride * destHeight / 4 + i * destStride / 2, pScan, width / 2);
		pScan += stride;
	}
}

void ReverseVertical(BYTE * dst,int dstpitch,BYTE *src,int srcpitch,int width,int height)
{
	dst = dst+height*dstpitch - dstpitch;
	for(int i=0;i<height;i++)
	{
		memcpy(dst,src,width);
		src += srcpitch;
		dst -= dstpitch;
	}
}

static inline BYTE Clip(int clr)
{
    return (BYTE)(clr < 0 ? 0 : ( clr > 255 ? 255 : clr ));
}

static inline RGBQUAD ConvertYCrCbToRGB( int y, int cr,  int cb)
{
    RGBQUAD rgbq;

    int c = y - 16;
    int d = cb - 128;
    int e = cr - 128;

    rgbq.rgbRed =   Clip(( 298 * c           + 409 * e + 128) >> 8);
    rgbq.rgbGreen = Clip(( 298 * c - 100 * d - 208 * e + 128) >> 8);
    rgbq.rgbBlue =  Clip(( 298 * c + 516 * d           + 128) >> 8);

    return rgbq;
}

void TransformImage_YUY2_BGRA(
    BYTE*       pDest,
    LONG        lDestStride,
    const BYTE* pSrc,
    LONG        lSrcStride,
    DWORD       dwWidthInPixels,
    DWORD       dwHeightInPixels
    )
{
    for (DWORD y = 0; y < dwHeightInPixels; y++)
    {
        RGBQUAD *pDestPel = (RGBQUAD*)pDest;
        WORD    *pSrcPel = (WORD*)pSrc;

        for (DWORD x = 0; x < dwWidthInPixels; x += 2)
        {
            // Byte order is U0 Y0 V0 Y1

            int y0 = (int)LOBYTE(pSrcPel[x]);
            int u0 = (int)HIBYTE(pSrcPel[x]);
            int y1 = (int)LOBYTE(pSrcPel[x + 1]);
            int v0 = (int)HIBYTE(pSrcPel[x + 1]);

            pDestPel[x] = ConvertYCrCbToRGB(y0, v0, u0);
            pDestPel[x + 1] = ConvertYCrCbToRGB(y1, v0, u0);
        }

        pSrc += lSrcStride;
        pDest += lDestStride;
    }

}

// revert rotated images sent from mobile devices
//   if you are facing the front camera, the degree is increased at anti-clockwise direction
static void revert_rotation_270degree_plane ( char * psrc, char * pdest, int w, int h, int scaler)
{
    int i, j;
    for (j=0; j<w; j++)
        for (i=0 ; i< h; i++)
            memcpy(pdest+ (j*h+i) * scaler,  psrc + (w-j-1 + i*w) * scaler, scaler);
}

static void revert_rotation_180degree_plane ( char * psrc, char * pdest, int w, int h, int scaler)
{
    int j;
    for (j=0; j<h; j++)
        memcpy(pdest+ (j*w) * scaler,  psrc + ((h-j-1)*w) * scaler, w*scaler);
}

static void revert_rotation_90degree_plane ( char * psrc, char * pdest, int w, int h, int scaler)
{
    int i, j;
    for (j=0; j<w; j++)
        for (i=0 ; i< h; i++)
            memcpy(pdest+ (j*h+i) * scaler,  psrc + (j + (h-i-1)*w) * scaler, scaler);
}

void RevertRotationI420 ( char * psrc, char * pdest, int w, int h, int degree, int &w_out,int &h_out)
{
    char *y = psrc, *u = psrc+w*h, *v = psrc+w*h + w*h/4;
    char *y1 = pdest, *u1 = pdest+w*h, *v1 = pdest+w*h + w*h/4;
    switch (degree ) 
    {
    case 1: // 90
        revert_rotation_90degree_plane (y, y1, w, h, 1 );
        revert_rotation_90degree_plane (u, u1, w/2, h/2, 1 );
        revert_rotation_90degree_plane (v, v1, w/2, h/2, 1 );
        w_out = h; h_out =w;
        break;
    case 2: // 180 degree
        revert_rotation_180degree_plane (y, y1, w, h, 1 );
        revert_rotation_180degree_plane (u, u1, w/2, h/2, 1 );
        revert_rotation_180degree_plane (v, v1, w/2, h/2, 1 );
        w_out = w; h_out =h;
        break;
    case 3: // 270
        revert_rotation_270degree_plane (y, y1, w, h, 1 );
        revert_rotation_270degree_plane (u, u1, w/2, h/2, 1 );
        revert_rotation_270degree_plane (v, v1, w/2, h/2, 1 );
        w_out = h; h_out =w;
        break;
    }
}
