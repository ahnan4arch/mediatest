#pragma once

#include "stdafx.h"

class ID3DRender
{
public:
	void * GetID3DDeviceManager() = 0;
	int Draw(void * pID3DSurface) = 0;
}