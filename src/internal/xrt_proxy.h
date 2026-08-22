#ifndef XRT_INTERNAL_PROXY_H
#define XRT_INTERNAL_PROXY_H

#include "xrt_net_buffer.h"

#if defined(XRT_FEATURE_NET_PROXY_DIAL)
	#include "xrt_tcp.h"
#endif



#if defined(XRT_FEATURE_NET_PROXY)

#define XRT_NET_PROXY_HOST_LIMIT 1024u
#define XRT_NET_PROXY_RECEIVE_LIMIT 65536u



/* 代理配置与全部变长字段共用一块分配，最后释放时可以一次完整清零。 */
struct xnetproxy {
	volatile int32 References;
	xnetproxyinfo Info;
	size_t AllocationSize;
};



/* 验证网络主机视图的共同空值、长度和零字节契约。 */
bool __xrtNetProxyHostValid(
	xstrview Host,
	cstr sOperation,
	cstr sName
);

#endif



#if defined(XRT_FEATURE_NET_PROXY_HANDSHAKE)

/* 协议阶段由具体代理后端解释，通用层只维护传输状态和所有权。 */
struct xnetproxyhandshake {
	xnetproxy* Proxy;
	xnetbuf Output;
	xerror* Error;
	xnetproxyendpoint Bound;
	str TargetHost;
	str BoundHost;
	size_t AllocationSize;
	size_t TargetSize;
	size_t ReceiveLimit;
	size_t ScanOffset;
	uint32 Code;
	uint16 TargetPort;
	uint8 Stage;
	uint8 InterimCount;
	bool HasCode;
	bool HasBound;
	xnetproxyhandshakestate State;
};



/* 设置并捕获代理握手错误，使异步传输可以跨执行上下文读取原因。 */
xnetproxyhandshakestate __xrtNetProxyHandshakeFail(
	xnetproxyhandshake* pHandshake,
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage
);



/* 在代理层包装底层协议、内存或传输错误并保留完整原因链。 */
xnetproxyhandshakestate __xrtNetProxyHandshakeFailCause(
	xnetproxyhandshake* pHandshake,
	xerrkind Kind,
	xneterror Code,
	cstr sOperation,
	cstr sMessage,
	const xerror* pCause
);



/* 原子追加一个协议输出报文，并把握手切换到 WRITE。 */
bool __xrtNetProxyHandshakeQueue(
	xnetproxyhandshake* pHandshake,
	const void* pData,
	size_t iSize
);



#if defined(XRT_FEATURE_NET_PROXY_SOCKS5)

/* 生成 SOCKS5 问候报文并进入方法选择阶段。 */
bool __xrtNetProxySocks5Start(xnetproxyhandshake* pHandshake);



/* 增量消费一个完整 SOCKS5 回复前缀。 */
xnetproxyhandshakestate __xrtNetProxySocks5Step(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
);

#endif



#if defined(XRT_FEATURE_NET_PROXY_HTTP_CONNECT)

/* 生成标准 HTTP/1.1 CONNECT 请求并进入响应阶段。 */
bool __xrtNetProxyHttpStart(xnetproxyhandshake* pHandshake);



/* 使用共享 HTTP/1 解析器增量消费代理响应。 */
xnetproxyhandshakestate __xrtNetProxyHttpStep(
	xnetproxyhandshake* pHandshake,
	xnetbuf* pInput
);

#endif

#endif

#endif
