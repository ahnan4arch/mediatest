#pragma once

#include "MultipleBuffering.h"
#include "mfxstructures.h"


struct IBitstreamSink
{
	virtual  bool PutBittream ( mfxBitstream* bs) = 0;
};

struct IBitstreamSource
{
	virtual  const  mfxBitstream* GetBittream () = 0;
};

class BitstreamBuffering
{
};