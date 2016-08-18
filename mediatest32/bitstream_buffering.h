#pragma once

#include "bitstream_io.h"

#include "MultipleBuffering.h"
#include "codec_utils.h"


class BitstreamBuffering : public IBitstreamSink, IBitstreamSource
{

public:
	BitstreamBuffering (int poolSize)
	{
		size_ = poolSize;
		pool_ = new mfxBitstream[size_];
		for ( int i = 0; i< poolSize;i++)
		{
			::memset(&pool_[i], 0, sizeof (mfxBitstream));
			InitMfxBitstream(&pool_[i], 1024*1024*2);

			// add to buffering (producer side)
			buffering_.release(i);
		}
	}

	~BitstreamBuffering()
	{
		delete [] pool_;
	}

public:
	// IBitstreamSink, for producor
	virtual  bool PutBittream ( mfxBitstream* bs /*in*/) 
	{
		int buffId;
		bool ret = buffering_.dequeue(60*1000, buffId);
		if (ret){
			CopyBitstream(&pool_[buffId], bs );
			buffering_.enqueue(buffId);
		}
	}
	
	//IBitstreamSource, for consumer
	virtual  bool  GetBittream (mfxBitstream* bs/*out*/) 
	{
		int buffId;
		bool ret = buffering_.acquire(60*1000, buffId);
		if (ret){
			CopyBitstream(bs, &pool_[buffId] );
			buffering_.release(buffId);
		}
	}

	// unblock both consumer and producer thread
	void shutdown()
	{
		buffering_.unblockWaitingThreads();
	}

private:
	void CopyBitstream( mfxBitstream* dst,  mfxBitstream* src)
	{
		// we assume every BS just contain one frame data, so offset is always 0
		memcpy_s(dst->Data, dst->MaxLength, src->Data, src->DataLength);

		dst->DataOffset = 0;
	}

private:
	WinRTCSDK::MultipleBuffering buffering_;
	int size_;
	mfxBitstream * pool_;
};