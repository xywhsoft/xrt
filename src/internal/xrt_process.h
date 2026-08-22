#ifndef XRT_INTERNAL_PROCESS_H
#define XRT_INTERNAL_PROCESS_H

#include "xrt_internal.h"

#include <xrt/charset.h>
#include <xrt/process.h>
#include <xrt/sync.h>
#include <xrt/thread.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <sys/types.h>
#endif



#if defined(XRT_FEATURE_PROCESS)

/* Process 对象只保存平台句柄和稳定生命周期状态，不内嵌输出捕获缓冲。 */
struct xprocess {
	volatile int32 RefCount;
	volatile int32 UserRefs;
	xmutex Lock;
	xcond Changed;
	xthread* Waiter;
	xprocessstate State;
	xprocessstatus Status;
	xprocessstop RequestedStop;
	xerror* Error;
	bool NewGroup;
	bool Terminal;
	#if defined(XRT_FEATURE_PROCESS_FUTURE)
		xfuture* WaitFuture;
		xpromise* WaitPromise;
		xprocessstatus* WaitStatus;
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		HANDLE Process;
		HANDLE Job;
		#if defined(XRT_FEATURE_PROCESS_TERMINAL)
			HANDLE TerminalHandle;
		#endif
		HANDLE Stdin;
		HANDLE Stdout;
		HANDLE Stderr;
		DWORD Id;
	#else
		pid_t Id;
		int Stdin;
		int Stdout;
		int Stderr;
	#endif
};



/* 在当前执行上下文建立并设置 xrt.process 结构化错误。 */
void __xrtProcessErrorSet(
	xerrkind Kind,
	xprocesserror Code,
	cstr sOperation,
	cstr sMessage,
	int iSystemCode
);



/* 平台启动只在全部子进程资源就绪后把句柄提交给对象。 */
bool __xrtProcessPlatformSpawn(
	xprocess* pProcess,
	const xprocessconfig* pConfig
);



/* 平台等待填充临时状态，调用方负责原子发布。 */
bool __xrtProcessPlatformWait(
	xprocess* pProcess,
	xprocessstatus* pStatus
);



/* 平台读写函数只消费 Process 自己拥有的父端管道。 */
int64 __xrtProcessPlatformRead(
	xprocess* pProcess,
	xprocessstream Stream,
	void* pData,
	size_t iSize
);

int64 __xrtProcessPlatformWrite(
	xprocess* pProcess,
	const void* pData,
	size_t iSize
);



/* 幂等关闭一个父端管道。 */
bool __xrtProcessPlatformClose(
	xprocess* pProcess,
	xprocessstream Stream
);



/* 释放 Process 持有的全部平台对象。 */
void __xrtProcessPlatformUnit(xprocess* pProcess);



/* 返回平台进程和标准流的借用标识。 */
uint64 __xrtProcessPlatformId(const xprocess* pProcess);
intptr_t __xrtProcessPlatformNative(const xprocess* pProcess);
intptr_t __xrtProcessPlatformStream(
	const xprocess* pProcess,
	xprocessstream Stream
);



/* 平台停止操作不等待退出。 */
bool __xrtProcessPlatformInterrupt(xprocess* pProcess);
bool __xrtProcessPlatformTerminate(xprocess* pProcess);
bool __xrtProcessPlatformKill(xprocess* pProcess);
bool __xrtProcessPlatformKillTree(xprocess* pProcess);



#if defined(XRT_FEATURE_PROCESS_FUTURE)
/* 在终态发布后完成已安装的共享等待 Future。 */
void __xrtProcessFutureComplete(xprocess* pProcess);
#endif



#if defined(XRT_FEATURE_PROCESS_TERMINAL)
/* 平台终端能力、启动与尺寸调整由对应 Process 后端实现。 */
bool __xrtProcessTerminalSupportedPlatform(void);
bool __xrtProcessTerminalResizePlatform(
	xprocess* pProcess,
	uint32 iColumns,
	uint32 iRows
);
#endif

#endif

#endif
