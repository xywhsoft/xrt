#ifndef XRT_INTERNAL_TLS_STREAM_H
#define XRT_INTERNAL_TLS_STREAM_H

#include "xrt_tcp.h"
#include "xrt_tls_session.h"
#include <xrt/tls_stream.h>



#if defined(XRT_FEATURE_TLS_STREAM)

#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
typedef struct __xrt_tls_stream_async __xrt_tls_stream_async;
#endif

#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
typedef struct __xrt_tls_listener_stream __xrt_tls_listener_stream;
#endif

/* 组合对象只在所属 Worker 修改协议字段，原子字段提供跨线程快照和门。 */
struct xtlsstream {
	volatile int32 References;
	xatomic32 State;
	xatomic32 CloseGate;
	xatomic32 AbortGate;
	xatomic32 TerminalResult;
	xatomic64 Available;
	xatomic64 CipherPending;
	xatomicptr Transport;
	xtlssession* Session;
	xtlsstreamconfig Config;
	xtlsstreamevents Events;
	xatomicptr Data;
	xerror* Error;
	uint64 HandshakeTimer;
	uint64 CloseTimer;
	__xrt_net_engine_internal DriveCommand;
	__xrt_net_engine_internal CloseCommand;
	#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
		xrt_spinlock AsyncLock;
		__xrt_tls_stream_async* AsyncSendHead;
		__xrt_tls_stream_async* AsyncSendTail;
		__xrt_tls_stream_async* AsyncWaitHead;
		__xrt_tls_stream_async* AsyncWaitTail;
		xatomic64 AsyncBytes;
		xatomic32 AsyncCount;
		xatomic32 AsyncSends;
		xatomic32 AsyncReads;
		__xrt_net_engine_internal AsyncCommand;
		bool AsyncPosted;
		bool AsyncRunning;
		bool AsyncAgain;
		bool AsyncClosed;
	#endif
	bool Server;
	bool OpenEmitted;
	bool EndEmitted;
	bool CloseEmitted;
	bool ReadNotified;
	bool TransportPaused;
	bool Driving;
	bool Sending;
	bool DriveAgain;
	bool DrivePosted;
	bool WriteBlocked;
	bool DrainPending;
	bool Failing;
	bool CloseStarted;
	bool Closing;
	bool RuntimeHeld;
	bool ReadMore;
	size_t ReadMoreSize;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)
		bool (*ManagedOpen)(xtlsstream* pStream, ptr pData);
		void (*ManagedClose)(xtlsstream* pStream,
			xnetresult Result, const xerror* pError, ptr pData);
		ptr ManagedData;
	#endif
};



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER)

#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
typedef struct __xrt_tls_listener_wait __xrt_tls_listener_wait;
#endif

/* 每条受管 Stream 保留 Listener，直到组合流进入最终终态。 */
struct __xrt_tls_listener_stream {
	__xrt_tls_listener_stream* Next;
	__xrt_tls_listener_stream* HandshakePrevious;
	__xrt_tls_listener_stream* HandshakeNext;
	xtlslistener* Listener;
	xtlsstream* Stream;
	bool Queued;
	bool CallerHeld;
	bool HandshakePending;
	bool CloseMarked;
};



/* Listener 自身只管理握手入口和完成队列，不复制 TCP 传输状态机。 */
struct xtlslistener {
	volatile int32 References;
	xatomic32 State;
	xatomic64 Handshakes;
	xatomic64 Accepted;
	xatomic64 Rejected;
	xatomic64 HandshakeErrors;
	xatomic32 ActiveHandshakes;
	xatomic32 PeakHandshakes;
	xatomic32 QueuedAccepts;
	xatomic32 PeakQueuedAccepts;
	xatomic32 AcceptWaiters;
	xrt_spinlock Lock;
	xnetengine* Engine;
	xnetlistener* Native;
	xtlsserverconfig Tls;
	xtlsstreamconfig Stream;
	xtlslistenerevents Events;
	xtlsstreamevents StreamEvents;
	ptr Data;
	__xrt_tls_listener_stream* HandshakeHead;
	__xrt_tls_listener_stream* AcceptHead;
	__xrt_tls_listener_stream* AcceptTail;
	xerror* StartupError;
	uint32 AcceptQueueLimit;
	uint32 HandshakeLimit;
	#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
		__xrt_tls_listener_wait* WaitHead;
		__xrt_tls_listener_wait* WaitTail;
	#endif
	bool StartDone;
	bool Closed;
	bool Published;
	bool RuntimeHeld;
};



/* 为 Listener 安装不可被公开 SetEvents 替换的生命周期观察器。 */
void __xrtTlsStreamManage(
	xtlsstream* pStream,
	bool (*pOpen)(xtlsstream* pStream, ptr pData),
	void (*pClose)(xtlsstream* pStream,
		xnetresult Result, const xerror* pError, ptr pData),
	ptr pData
);



/* 调用方持有 Listener.Lock 时取走一个排队连接。 */
__xrt_tls_listener_stream* __xrtTlsListenerTakeQueued(
	xtlslistener* pListener
);



#if defined(XRT_FEATURE_TLS_STREAM_LISTENER_FUTURE)
/* 配对已完成握手的 Stream 与最早 Future，关闭后终结剩余等待。 */
void __xrtTlsListenerFutureNotify(xtlslistener* pListener);



/* 让最早 Future 直接接管一条已完成握手的 Stream。 */
bool __xrtTlsListenerFutureAccept(
	xtlslistener* pListener,
	__xrt_tls_listener_stream* pStream
);
#endif

#endif



/* 创建尚未绑定 TCP Stream 的组合对象，供客户端与服务端组合入口复用。 */
xtlsstream* __xrtTlsStreamCreate(
	xtlssession* pSession,
	bool bServer,
	const xtlsstreamconfig* pConfig,
	const xtlsstreamevents* pEvents,
	ptr pData
);



/* 释放尚未进入异步生命周期的两份初始引用。 */
void __xrtTlsStreamDiscard(xtlsstream* pStream);



/* 返回只读 TCP 事件表，供受管拨号复用同一适配状态机。 */
const xnetstreamevents* __xrtTlsStreamTransportEventTable(void);



/* 验证 TCP 队列硬上限能够容纳 TLS 会话的最大发送队列。 */
bool __xrtTlsStreamLimits(
	const xnetstreamconfig* pTransport,
	const xtlssession* pSession
);



/* 把名称解析或 TCP 拨号失败发布为未绑定传输的组合终态。 */
void __xrtTlsStreamTransportFailed(
	xtlsstream* pStream,
	xnetresult Result,
	const xerror* pError
);

#if defined(XRT_FEATURE_TLS_STREAM_FUTURE)
/* 在所属 Worker 上推进全部当前可完成的 TLS Stream Future。 */
void __xrtTlsStreamFutureNotify(xtlsstream* pStream);



/* 在异步发送 FIFO 清空后，于所属 Worker 上开始认证关闭。 */
void __xrtTlsStreamCloseReady(xtlsstream* pStream);
#endif

#endif

#endif
