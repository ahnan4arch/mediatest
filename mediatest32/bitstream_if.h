#pragma once
#include "mfxstructures.h"


// IBitstreamSink, for producor
struct IBitstreamSink
{
	virtual  bool PutBittream ( mfxBitstream* bs /*in*/) = 0;
};

//IBitstreamSource, for consumer
struct IBitstreamSource
{
	// caller must allocate the bs
	virtual  bool  GetBittream (mfxBitstream* bs/*out*/) = 0;
};
