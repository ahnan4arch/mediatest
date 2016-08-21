#pragma once

#include "bitstream_if.h"

#include "MultipleBuffering.h"
#include "codec_utils.h"


class BitstreamBuffering : public IBitstreamSink, IBitstreamSource
{

public:
	BitstreamBuffering ()
	{
	}

	void Init (int poolSize)
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
		return ret;
	}
	
	//IBitstreamSource, for consumer
	virtual  bool  GetBittream (mfxBitstream* bs/*out*/) 
	{
		int buffId;
		bool ret = buffering_.acquire(60*1000, buffId);
		if (ret){
			CopyBitstream2(bs, &pool_[buffId] );
			buffering_.release(buffId);
		}
		return ret;
	}

	// unblock both consumer and producer thread
	void Shutdown()
	{
		buffering_.unblockWaitingThreads();
	}

private:
	void CopyBitstream( mfxBitstream* dst,  mfxBitstream* src)
	{
		// we assume every BS just contain one frame data, so offset is always 0
		memcpy_s(dst->Data, dst->MaxLength, src->Data, src->DataLength);
		dst->DataLength = src->DataLength;
		dst->DataOffset = 0;
	}

	mfxStatus CopyBitstream2(mfxBitstream *dest, mfxBitstream *src)
	{
		if (!dest || !src)
			return MFX_ERR_NULL_PTR;

		if (!dest->DataLength)
		{
			dest->DataOffset = 0;
		}
		else
		{
			memmove(dest->Data, dest->Data + dest->DataOffset, dest->DataLength);
			dest->DataOffset = 0;
		}

		if (src->DataLength > dest->MaxLength - dest->DataLength - dest->DataOffset)
			return MFX_ERR_NOT_ENOUGH_BUFFER;

		MSDK_MEMCPY_BITSTREAM(*dest, dest->DataOffset, src->Data, src->DataLength);
		dest->DataLength = src->DataLength;

		dest->DataFlag = src->DataFlag;

		//common Extended buffer will be for src and dest bit streams
		dest->EncryptedData = src->EncryptedData;

		return MFX_ERR_NONE;
	}

private:
	WinRTCSDK::MultipleBuffering buffering_;
	int size_;
	mfxBitstream * pool_;
};