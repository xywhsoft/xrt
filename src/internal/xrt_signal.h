#ifndef XRT_INTERNAL_SIGNAL_H
#define XRT_INTERNAL_SIGNAL_H

#include "xrt_internal.h"

#include <xrt/atomic.h>
#include <xrt/signal.h>
#include <xrt/sync.h>
#include <xrt/thread.h>



#if defined(XRT_FEATURE_SIGNAL)

#define XRT_SIGNAL_SLOT_COUNT 7u



typedef enum xsignalslotmode {
	XRT_SIGNAL_SLOT_DEFAULT = 0,
	XRT_SIGNAL_SLOT_WATCH = 1,
	XRT_SIGNAL_SLOT_IGNORE = 2
} xsignalslotmode;



typedef struct xsignalslot {
	xsignal Code;
	int32 SystemCode;
	cstr Name;
	bool Supported;
	xatomic32 Mode;
	xatomic32 Pending;
	xatomic32 LastSystemCode;
	uint64 Total;
} xsignalslot;



struct xsignalwatch {
	volatile int32 RefCount;
	uint64 Id;
	xsignal Code;
	xsignalproc Proc;
	ptr Data;
	xsignalfreeproc Free;
	bool Once;
	bool Active;
	bool Linked;
	bool CallbackActive;
	struct xsignalwatch* Previous;
	struct xsignalwatch* Next;
};



typedef struct xsignalstate {
	xmutex Lock;
	xcond Idle;
	bool PlatformReady;
	bool Running;
	bool Stopping;
	bool DispatchFailed;
	int DispatchSystemCode;
	uint64 NextId;
	uint64 DispatcherId;
	xthread* Dispatcher;
	xsignalwatch* Head;
	xsignalwatch* Callback;
	xsignalslot Slots[XRT_SIGNAL_SLOT_COUNT];
	xatomic32 HandlerActive;
} xsignalstate;



extern xsignalstate __xrtSignalState;



/* 原生处理器只调用此入口记录数量和唤醒调度线程。 */
void __xrtSignalNotify(uint32 iSlot, int32 iSystemCode);



/* 初始化当前平台的无锁唤醒资源。 */
bool __xrtSignalPlatformInit(void);



/* 释放当前平台的唤醒资源。 */
void __xrtSignalPlatformUnit(void);



/* 阻塞等待原生处理器或关闭过程唤醒调度线程。 */
bool __xrtSignalPlatformWait(int* pSystemCode);



/* 无锁唤醒调度线程，可从原生信号处理器调用。 */
void __xrtSignalPlatformWake(void);



/* 设置一个信号槽的原生监听或忽略方式。 */
bool __xrtSignalPlatformMode(
	uint32 iSlot,
	xsignalslotmode Mode,
	int* pSystemCode
);



/* 恢复一个信号槽在 XRT 接管前的原生处理方式。 */
bool __xrtSignalPlatformRestore(uint32 iSlot, int* pSystemCode);



/* 向当前进程发送指定信号。 */
bool __xrtSignalPlatformRaise(uint32 iSlot, int* pSystemCode);

#endif

#endif
