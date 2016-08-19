#include "mfxdefs.h"
#include "windows.h"

typedef unsigned int (MFX_STDCALL * msdk_thread_callback)(void*);

class MSDKThread
{
public:
	MSDKThread(mfxStatus &sts, LPTHREAD_START_ROUTINE func, void* arg)
	{
		sts = MFX_ERR_NONE;
		m_thread = (void*)::CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)func, arg, 0, NULL);
	}

	~MSDKThread(void)
	{
		CloseHandle(m_thread);
	}

	mfxStatus Wait(void)
	{
		return (WaitForSingleObject(m_thread, INFINITE) != WAIT_OBJECT_0) ? MFX_ERR_UNKNOWN : MFX_ERR_NONE;
	}

	mfxStatus TimedWait(mfxU32 msec)
	{
		if(MFX_INFINITE == msec) return MFX_ERR_UNSUPPORTED;

		mfxStatus mfx_res = MFX_ERR_NONE;
		DWORD res = WaitForSingleObject(m_thread, msec);

		if(WAIT_OBJECT_0 == res) mfx_res = MFX_ERR_NONE;
		else if (WAIT_TIMEOUT == res) mfx_res = MFX_TASK_WORKING;
		else mfx_res = MFX_ERR_UNKNOWN;

		return mfx_res;
	}

	mfxStatus GetExitCode()
	{
		mfxStatus mfx_res = MFX_ERR_NOT_INITIALIZED;

		DWORD code = 0;
		int sts = 0;
		sts = GetExitCodeThread(m_thread, &code);

		if (sts == 0) mfx_res = MFX_ERR_UNKNOWN;
		else if (STILL_ACTIVE == code) mfx_res = MFX_TASK_WORKING;
		else mfx_res = MFX_ERR_NONE;

		return mfx_res;
	}
private:
	HANDLE m_thread;
	MSDKThread(const MSDKThread&);
	void operator=(const MSDKThread&);
};
