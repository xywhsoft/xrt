


// 创建线程
#if defined(_WIN32) || defined(_WIN64)
	// windows 方案
	static DWORD WINAPI __pri_ThreadProxyProc(xthread objThread)
	{
		return 0;
	}
#else
	// 其他平台方案
#endif
XXAPI xthread xrtThreadCreate(ptr pProc, ptr pParam, size_t iStackSize)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		DWORD iThreadID;
		xthread pRet = CreateThread(NULL, iStackSize, pProc, pParam, 0, &iThreadID);
		return pRet;
	#else
		// 其他平台方案
	#endif
}



// 等待线程
XXAPI void xrtThreadWait(xthread pThread)
{
	#if defined(_WIN32) || defined(_WIN64)
		// windows 方案
		if ( pThread ) {
			WaitForSingleObject(pThread, INFINITE);
			CloseHandle(pThread);
		}
	#else
		// 其他平台方案
	#endif
}



// 强行结束线程



// 暂停线程



// 恢复线程



// 创建互斥体



// 销毁互斥体



// 锁定互斥体



// 解锁互斥体


