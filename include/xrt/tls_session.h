#ifndef XRT_TLS_SESSION_H
#define XRT_TLS_SESSION_H

#include <xrt/tls.h>
#include <xrt/net.h>



#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_TLS_CONTEXT)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_TLS_CONTEXT"
#endif

#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_TLS_RECORD)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_TLS_RECORD"
#endif

#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_TLS_MESSAGES)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_TLS_MESSAGES"
#endif

#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_TLS_SCHEDULE)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_TLS_SCHEDULE"
#endif

#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_NET_BUFFER)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_NET_BUFFER"
#endif

#if defined(XRT_FEATURE_TLS_SESSION) && !defined(XRT_FEATURE_CRYPTO_CORE)
	#error "XRT_FEATURE_TLS_SESSION requires XRT_FEATURE_CRYPTO_CORE"
#endif



#if defined(XRT_FEATURE_TLS_SESSION)

/* 等待原因是可组合位；驱动器据此决定继续读、写或等待应用动作。 */
typedef enum xtlswait {
	XTLS_WAIT_NONE = 0,
	XTLS_WAIT_INPUT = (1u << 0),
	XTLS_WAIT_OUTPUT = (1u << 1),
	XTLS_WAIT_APPLICATION = (1u << 2),
	XTLS_WAIT_IDENTITY = (1u << 3),
	XTLS_WAIT_VERIFY = (1u << 4)
} xtlswait;



typedef struct xtlssession xtlssession;



XRT_EXTERN_C_BEGIN



/* 销毁会话、释放队列并归还上下文引用；空指针无操作。 */
XRT_API void xrtTlsSessionDestroy(xtlssession* pSession);



/* 返回会话的客户端或服务端角色；失败返回零并设置错误。 */
XRT_API xtlsrole xrtTlsSessionRole(const xtlssession* pSession);



/* 返回公开生命周期状态；空会话返回 FAILED 并设置错误。 */
XRT_API xtlsstate xrtTlsSessionState(const xtlssession* pSession);



/* 返回协商后的协议版本；握手尚未选定版本时返回零且不设置错误。 */
XRT_API xtlsversion xrtTlsSessionVersion(const xtlssession* pSession);



/* 返回协商后的密码套件；握手尚未选定套件时返回零且不设置错误。 */
XRT_API xtlscipher xrtTlsSessionCipher(const xtlssession* pSession);



/* 返回当前等待原因位；没有等待原因时返回 XTLS_WAIT_NONE。 */
XRT_API uint32 xrtTlsSessionWait(const xtlssession* pSession);



/* 借用会话持有的只读上下文；返回值不得超过会话生命周期使用。 */
XRT_API const xtlscontext* xrtTlsSessionContext(
	const xtlssession* pSession
);



/* 借用协商后的 ALPN 协议；尚未选择协议时返回 false 且不设置错误。 */
XRT_API bool xrtTlsSessionProtocol(
	const xtlssession* pSession,
	xbytesview* pProtocol
);



/* 查询最后收到的对端 Alert；尚未收到时返回 false 且不设置错误。 */
XRT_API bool xrtTlsSessionPeerAlert(
	const xtlssession* pSession,
	xtlsalertlevel* pLevel,
	xtlsalert* pAlert
);



/* 复制一段收到的 TLS 密文；达到输入硬上限时返回 XTLS_AGAIN。 */
XRT_API xtlsresult xrtTlsSessionFeed(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
);



/* 借用一段收到的密文，调用方须保持其存活到会话消费或销毁。 */
XRT_API xtlsresult xrtTlsSessionFeedBorrow(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
);



/* 接管一段由 xrtMalloc 家族分配的密文；失败时所有权仍归调用方。 */
XRT_API xtlsresult xrtTlsSessionFeedTake(
	xtlssession* pSession,
	ptr pData,
	size_t iSize
);



/* 接管带释放过程的密文引用；失败时不会调用释放过程。 */
XRT_API xtlsresult xrtTlsSessionFeedRef(
	xtlssession* pSession,
	const void* pData,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
);



/* 零复制接管一条密文缓冲链；AGAIN 或失败时源缓冲保持不变。 */
XRT_API xtlsresult xrtTlsSessionFeedBuffer(
	xtlssession* pSession,
	xnetbuf* pBuffer
);



/* 返回尚未由协议状态机消费的密文字节数。 */
XRT_API size_t xrtTlsSessionFeedSize(const xtlssession* pSession);



/* 返回等待底层传输发送的密文字节数。 */
XRT_API size_t xrtTlsSessionSendSize(const xtlssession* pSession);



/* 返回密文发送队列当前非空 Span 数。 */
XRT_API size_t xrtTlsSessionSendSpanCount(const xtlssession* pSession);



/* 借用密文发送队列的第一个连续 Span；空队列返回空 Span。 */
XRT_API bool xrtTlsSessionSendFront(
	const xtlssession* pSession,
	xnetspan* pSpan
);



/* 借用最多给定数量的密文发送 Span，供 scatter/gather 发送。 */
XRT_API size_t xrtTlsSessionSendSpans(
	const xtlssession* pSession,
	xnetspan* pSpans,
	size_t iCapacity
);



/* 精确消费已经由底层传输发送的密文字节，禁止静默过量消费。 */
XRT_API bool xrtTlsSessionSendConsume(
	xtlssession* pSession,
	size_t iSize
);



/* 返回等待应用读取的明文字节数。 */
XRT_API size_t xrtTlsSessionPlainSize(const xtlssession* pSession);



/* 返回明文读取队列当前非空 Span 数。 */
XRT_API size_t xrtTlsSessionPlainSpanCount(const xtlssession* pSession);



/* 借用明文读取队列的第一个连续 Span；空队列返回空 Span。 */
XRT_API bool xrtTlsSessionPlainFront(
	const xtlssession* pSession,
	xnetspan* pSpan
);



/* 借用最多给定数量的明文 Span，适合无复制协议解析。 */
XRT_API size_t xrtTlsSessionPlainSpans(
	const xtlssession* pSession,
	xnetspan* pSpans,
	size_t iCapacity
);



/* 精确消费应用已经处理的明文字节，禁止静默过量消费。 */
XRT_API bool xrtTlsSessionPlainConsume(
	xtlssession* pSession,
	size_t iSize
);



/* 复制并消费明文；无数据返回 AGAIN，认证关闭后返回 CLOSED。 */
XRT_API xtlsresult xrtTlsSessionRead(
	xtlssession* pSession,
	void* pOutput,
	size_t iCapacity,
	size_t* pRead
);



/* 把明文按记录边界加入有界发送队列；允许成功短写。 */
XRT_API xtlsresult xrtTlsSessionWrite(
	xtlssession* pSession,
	const void* pData,
	size_t iSize,
	size_t* pWritten
);



/* 排队一次 close_notify，并等待密文排空和对端认证关闭。 */
XRT_API xtlsresult xrtTlsSessionClose(xtlssession* pSession);



/* 通知底层传输已到 EOF；缺少 close_notify 时报告截断错误。 */
XRT_API xtlsresult xrtTlsSessionEof(xtlssession* pSession);



XRT_EXTERN_C_END

#endif

#endif
