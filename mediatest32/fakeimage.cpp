#include "Stdafx.h"

void make_fake_image ( BYTE * image)
{
	::memset(image, 0, sizeof(image));
	for ( int j=0; j<240; j++)
	{
		for ( int i=0; i< 320; i++){
			if ( i<160)
				image[i+j*320]=30;
			else 
				image[i+j*320]= 64;
			if ( j<120) image[i+j*320]+=30;
			else  image[i+j*320] -= 30;
		}
	}

	BYTE * pU = &image[240*320];
	for ( int j=0; j<120; j++)
	{
		for ( int i=0; i< 160; i++){
			if ( i<80)
				pU[i+j*160]=40;
			else 
				pU[i+j*160]= 50;

			if ( j<60)
				pU[i+j*160]+=10;
			else 
				pU[i+j*160] -= 20;
		}
	}

	BYTE * pV = &image[240*320*5/4];
	for ( int j=0; j<120; j++)
	{
		for ( int i=0; i< 160; i++){
			if ( i<80)
				pV[i+j*160]=160;
			else 
				pV[i+j*160]= 150;

			if ( j<60)
				pV[i+j*160]+=20;
			else 
				pV[i+j*160] -= 20;
		}
	}
}
