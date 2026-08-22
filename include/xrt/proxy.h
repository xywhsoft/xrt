#ifndef XRT_PROXY_H
#define XRT_PROXY_H

#include <xrt/net.h>

#if defined(XRT_FEATURE_NET_PROXY_DIAL)
	#include <xrt/tcp.h>
#endif



#if defined(XRT_FEATURE_NET_PROXY) && !defined(XRT_FEATURE_NET)
	#error "XRT proxy support requires XRT_FEATURE_NET"
#endif

#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE) && \
	(!defined(XRT_FEATURE_NET_PROXY) || !defined(XRT_FEATURE_NET_BUFFER))
	#error "XRT proxy handshake support requires proxy and network buffer support"
#endif

#if defined(XRT_FEATURE_NET_PROXY_SOCKS5) && \
	!defined(XRT_FEATURE_NET_PROXY_HANDSHAKE)
	#error "XRT SOCKS5 support requires proxy handshake support"
#endif

#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT) && \
	(!defined(XRT_FEATURE_NET_PROXY_HANDSHAKE) || \
	 !defined(XRT_FEATURE_HTTP1_HEAD) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XRT HTTP CONNECT requires proxy handshake, HTTP/1 head and Base64 support"
#endif

#if defined(XRT_FEATURE_NET_PROXY_DIAL) && \
	(!defined(XRT_FEATURE_NET_PROXY_HANDSHAKE) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL))
	#error "XRT proxy Dial support requires proxy handshake and TCP Dial support"
#endif



#if defined(XRT_FEATURE_NET_PROXY)

/* 代理类型只描述协议；TCP、TLS 和上层客户端决定如何承载协议。 */
typedef enum xnetproxytype {
	XNET_PROXY_SOCKS5 = 1,
	XNET_PROXY_HTTP_CONNECT
} xnetproxytype;



/* AUTO 在存在凭据时要求认证，否则只允许匿名；OPTIONAL 显式允许降级为匿名。 */
typedef enum xnetproxyauth {
	XNET_PROXY_AUTH_AUTO = 0,
	XNET_PROXY_AUTH_NONE,
	XNET_PROXY_AUTH_REQUIRED,
	XNET_PROXY_AUTH_OPTIONAL
} xnetproxyauth;



/* 代理对象持有配置深拷贝；主机不要求零结尾，凭据允许任意字节。 */
typedef struct xnetproxyconfig {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyconfig;



/* 信息视图由代理对象持有，只能在至少一个对象引用存活时借用。 */
typedef struct xnetproxyinfo {
	xnetproxytype Type;
	xstrview Host;
	uint16 Port;
	xnetproxyauth Auth;
	xbytesview Username;
	xbytesview Password;
} xnetproxyinfo;



/* 不可变代理端点可以跨请求和线程共享。 */
typedef struct xnetproxy xnetproxy;

#endif



#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE)

/* 握手状态同时告诉传输层下一步应发送、接收还是发布隧道。 */
typedef enum xnetproxyhandshakestate {
	XNET_PROXY_HANDSHAKE_WRITE = 1,
	XNET_PROXY_HANDSHAKE_READ,
	XNET_PROXY_HANDSHAKE_READY,
	XNET_PROXY_HANDSHAKE_ERROR
} xnetproxyhandshakestate;



/* 域名端点使用 Host；数字端点使用 Address，端口始终保存在 Address.Port。 */
typedef struct xnetproxyendpoint {
	xnetaddr Address;
	xstrview Host;
} xnetproxyendpoint;



/* 输入缓冲池由调用方借用，并且必须比握手对象存活更久。 */
typedef struct xnetproxyhandshakeconfig {
	const xnetproxy* Proxy;
	xstrview TargetHost;
	uint16 TargetPort;
	size_t ReceiveLimit;
	xnetbufpool* Pool;
} xnetproxyhandshakeconfig;



/* 单个握手由一个传输执行上下文独占驱动。 */
typedef struct xnetproxyhandshake xnetproxyhandshake;

#endif



#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)

/* SOCKS5 CONNECT 回复码保留 RFC 1928 的线路值，便于日志和策略判断。 */
typedef enum xnetsocks5reply {
	XNET_SOCKS5_SUCCEEDED = 0,
	XNET_SOCKS5_GENERAL_FAILURE = 1,
	XNET_SOCKS5_RULESET_DENIED = 2,
	XNET_SOCKS5_NETWORK_UNREACHABLE = 3,
	XNET_SOCKS5_HOST_UNREACHABLE = 4,
	XNET_SOCKS5_CONNECTION_REFUSED = 5,
	XNET_SOCKS5_TTL_EXPIRED = 6,
	XNET_SOCKS5_COMMAND_UNSUPPORTED = 7,
	XNET_SOCKS5_ADDRESS_UNSUPPORTED = 8
} xnetsocks5reply;

#endif



#if defined(XRT_FEATURE_NET_PROXY_DIAL)

/* Proxy Dial 状态区分代理端点解析、TCP 连接和协议握手。 */
typedef enum xnetproxydialstate {
	XNET_PROXY_DIAL_RESOLVING = 0,
	XNET_PROXY_DIAL_CONNECTING,
	XNET_PROXY_DIAL_HANDSHAKE,
	XNET_PROXY_DIAL_CONNECTED,
	XNET_PROXY_DIAL_FAILED,
	XNET_PROXY_DIAL_CANCELLED
} xnetproxydialstate;



/* Timeout 覆盖 DNS、TCP 和代理握手全过程；零值保留各内层超时。 */
typedef struct xnetproxydialconfig {
	xnetdialconfig Transport;
	uint64 Timeout;
	size_t ReceiveLimit;
} xnetproxydialconfig;



/* Proxy Dial 保持底层 TCP Dial 统计，并补充当前协议阶段。 */
typedef struct xnetproxydialstats {
	xnetproxydialstate State;
	xnetdialstats Transport;
} xnetproxydialstats;



typedef struct xnetproxydial xnetproxydial;



/*
	完成回调在代理传输 Worker 上至多执行一次，不会从提交调用栈重入。
	pDial 和 Error 只在回调期间借用；成功回调接管隧道 Stream 引用。
*/
typedef void (*xnetproxydialproc)(
	xnetproxydial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
);

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_NET_PROXY)

/* 初始化 SOCKS5、自动认证且没有固定容量字段的代理配置。 */
XRT_API void xrtNetProxyConfigInit(xnetproxyconfig* pConfig);



/* 深拷贝代理端点和凭据，创建可跨线程共享的不可变对象。 */
XRT_API xnetproxy* xrtNetProxyCreate(const xnetproxyconfig* pConfig);



/* 增加代理对象引用并返回原指针。 */
XRT_API xnetproxy* xrtNetProxyRetain(const xnetproxy* pProxy);



/* 释放代理对象引用；最后一个引用会清零整块配置存储。 */
XRT_API void xrtNetProxyRelease(xnetproxy* pProxy);



/* 复制代理对象的只读信息视图。 */
XRT_API bool xrtNetProxyInfo(
	const xnetproxy* pProxy,
	xnetproxyinfo* pInfo
);

#endif



#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE)

/* 初始化握手配置；64 KiB 上限主要约束后续 HTTP CONNECT Header。 */
XRT_API void xrtNetProxyHandshakeConfigInit(
	xnetproxyhandshakeconfig* pConfig
);



/* 创建握手并立即生成首个协议报文；目标主机会被深拷贝。 */
XRT_API xnetproxyhandshake* xrtNetProxyHandshakeCreate(
	const xnetproxyhandshakeconfig* pConfig
);



/* 销毁握手，并清零尚未发送的认证报文和内部目标信息。 */
XRT_API void xrtNetProxyHandshakeDestroy(xnetproxyhandshake* pHandshake);



/* 返回当前握手状态；空指针返回 ERROR。 */
XRT_API xnetproxyhandshakestate xrtNetProxyHandshakeState(
	const xnetproxyhandshake* pHandshake
);



/*
	处理输入链中的完整协议前缀；只消费代理回复，成功后的应用数据保持原位。
	WRITE 状态必须先发送并确认全部输出，READ 状态才会继续解析输入。
*/
XRT_API xnetproxyhandshakestate xrtNetProxyHandshakeStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
);



/* 借用当前待发送的首段连续输出；失败时把非空输出规范化为空 Span。 */
XRT_API bool xrtNetProxyHandshakeOutput(
	const xnetproxyhandshake* pHandshake,
	xnetspan* pOutput
);



/* 确认已经发送的输出前缀；支持 Socket 部分写入。 */
XRT_API size_t xrtNetProxyHandshakeSent(
	xnetproxyhandshake* pHandshake,
	size_t iSize
);



/* READY 后复制可用的绑定端点；HTTP CONNECT 没有该信息并返回 NOT_FOUND。 */
XRT_API bool xrtNetProxyHandshakeBound(
	const xnetproxyhandshake* pHandshake,
	xnetproxyendpoint* pEndpoint
);



/* 返回协议失败时捕获的不可变错误；对象所有权仍属于握手。 */
XRT_API const xerror* xrtNetProxyHandshakeError(
	const xnetproxyhandshake* pHandshake
);



/* 复制 SOCKS5 线路回复码或 HTTP 状态码；尚未收到回复时返回 false。 */
XRT_API bool xrtNetProxyHandshakeCode(
	const xnetproxyhandshake* pHandshake,
	uint32* pCode
);

#endif



#if defined(XRT_FEATURE_NET_PROXY_DIAL)

/* 初始化 TCP 拨号、64 KiB 协议上限和 30 秒全过程超时。 */
XRT_API void xrtNetProxyDialConfigInit(xnetproxydialconfig* pConfig);



/*
	连接代理端点并完成目标 CONNECT；成功 Stream 引用转移给完成回调。
	非 Worker 提交者可能与完成回调并发，不能依赖返回值已经完成赋值。
*/
XRT_API xnetproxydial* xrtNetProxyDial(
	xnetengine* pEngine,
	xnetresolver* pResolver,
	const xnetproxy* pProxy,
	cstr sTargetHost,
	uint16 iTargetPort,
	const xnetproxydialconfig* pConfig,
	const xnetstreamevents* pStreamEvents,
	ptr pStreamData,
	xnetproxydialproc pDone,
	ptr pDoneData
);



/* 增加 Proxy Dial 引用并返回原指针。 */
XRT_API xnetproxydial* xrtNetProxyDialRef(xnetproxydial* pDial);



/* 释放 Proxy Dial 引用；空指针视为空操作。 */
XRT_API void xrtNetProxyDialDestroy(xnetproxydial* pDial);



/* 协作取消名称解析、TCP 连接或代理握手。 */
XRT_API bool xrtNetProxyDialCancel(xnetproxydial* pDial);



/* 返回当前拨号阶段或不可变终态。 */
XRT_API xnetproxydialstate xrtNetProxyDialState(
	const xnetproxydial* pDial
);



/* 失败或取消后借用完整错误原因链。 */
XRT_API const xerror* xrtNetProxyDialError(
	const xnetproxydial* pDial
);



/* 复制代理阶段和底层 TCP 地址竞速统计。 */
XRT_API bool xrtNetProxyDialStats(
	const xnetproxydial* pDial,
	xnetproxydialstats* pStats
);

#endif



XRT_EXTERN_C_END

#endif
