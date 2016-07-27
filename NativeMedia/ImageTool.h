#pragma once

void ConvertYUV420ToARGB(unsigned char *src0,unsigned char *src1,unsigned char *src2, unsigned char *dst_ori, int width,int height);
void ConvertARGBToI420 (unsigned char *argb, int width, int height, unsigned char *yuv);
void DownScaleImage2X ( unsigned char * src, unsigned char * dest, int w, int h );
void CutImageI420 ( unsigned char * src, int src_w, int src_h, unsigned char * dest, int x_org, int y_org, int w, int h );
void ConvertYUY2ToYV12(unsigned char *src, int width, int height, unsigned char *dest);
void ReverseVertical(BYTE * dst,int dstpitch,BYTE *src,int srcpitch,int width,int height);
void RevertRotationI420 ( char * psrc, char * pdest, int w, int h, int degree, int &w_out,int &h_out);
void TransformImage_YUY2_BGRA(BYTE* pDest, LONG lDestStride, const BYTE* pSrc, LONG lSrcStride, DWORD dwWidth,  DWORD dwHeight);
void ConvertI420ToYV12(BYTE *dest, LONG destStride, LONG destHeight, BYTE *src, LONG width, LONG height, BOOL bottomUp=FALSE);
