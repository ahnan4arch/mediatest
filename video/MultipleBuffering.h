#pragma once
#include <queue>
#include "Win32Event.h"
#include "AutoLock.h"

// I am resembling the buffer queue logic from Android::Surfaceflinger
namespace WinRTCSDK{

// add some buffers to the producer queue, to begin use.
// buffer id is just key to a buffer pool(or array)
class MultipleBuffering
{
public:
	MultipleBuffering():lock_(){};
	///////////////////////////////////////////////////
	//  methods for the consumer
	bool acquire (int timeOut /*in millisec*/, int& id)
	{
		id = -1;
		{  
			AutoLock al (lock_); 
			if ( !consumerQueue_.empty() ){
				id = consumerQueue_.front();
				consumerQueue_.pop();
				return true;
			}
		}
		// wait for next buffer to consume
		if ( evtEnqueued_.Wait(timeOut)){
			AutoLock al (lock_); 
			if ( !consumerQueue_.empty() ){
				id = consumerQueue_.front();
				consumerQueue_.pop();
				return true;
			}
		}
		return false;
	}

	void release (int id){
		AutoLock al (lock_); 
		// return to producer queue
		producerQueue_.push(id);
		evtReleased_.SetEvent();
	}

	///////////////////////////////////////////////////
	// methods for the producer
	bool dequeue (int timeOut, int&id)
	{
		id = -1;
		{  
			AutoLock al (lock_); 
			if ( !producerQueue_.empty() ){
				id = producerQueue_.front();
				producerQueue_.pop();
				return true;
			}
		}
		// wait for next buffer to consume
		if ( evtReleased_.Wait(timeOut)){
			AutoLock al (lock_); 
			if ( !producerQueue_.empty() ){
				id = producerQueue_.front();
				producerQueue_.pop();
				return true;
			}
		}
		return false;
	}

	void enqueue(int id)
	{
		AutoLock al (lock_); 
		// return to producer queue
		consumerQueue_.push(id);
		evtEnqueued_.SetEvent();
	}

	// call this to shutdown the running threads quickly
	void unblockWaitingThreads()
	{
		evtEnqueued_.SetEvent();
		evtReleased_.SetEvent();
	}

	void reset()
	{
		while (!producerQueue_.empty())producerQueue_.pop();
		while (!consumerQueue_.empty())consumerQueue_.pop();
	}

private:
	std::queue<int> producerQueue_;
	std::queue<int> consumerQueue_;
	
	Win32Event evtEnqueued_;
	Win32Event evtReleased_;
	Mutex lock_;
};

} // namespace WinRTCSDK