#ifndef XRT_INTERNAL_NET_SOCKET_H
#define XRT_INTERNAL_NET_SOCKET_H

#include "xrt_net.h"



#if defined(XRT_FEATURE_NET_SOCKET)

/* 向量 IO 使用固定栈描述符，避免为每次系统调用分配内存。 */
#define XRT_NET_SOCKET_VECTOR_LIMIT 64u
#define XRT_NET_SOCKET_DGRAM_CONTROL_SIZE 256u



/* 对象只包装句柄及其稳定属性，不在原语层增加隐藏锁或缓冲。 */
struct xnetsocket_impl {
	uintptr_t Native;
	xnetfamily Family;
	xnetsockettype Type;
	uint32 Flags;
	uint32 DgramMeta;
	bool Connecting;
	/* Windows Socket 只能永久关联一个完成端口；唯一标识不受上下文地址复用影响。 */
	uint64 CompletionOwner;
	/* WSARecvMsg 在显式启用元数据时加载；其他平台恒为零。 */
	uintptr_t ReceiveMessage;
	/* WSASendMsg 只在逐数据报控制路径使用；其他平台恒为零。 */
	uintptr_t SendMessage;
};



/* 包装并接管一个新建平台句柄；失败时句柄也由本函数关闭。 */
xnetsocket __xrtNetSocketAdopt(uintptr_t iNative,
	xnetfamily Family, xnetsockettype Type, uint32 iFlags);



/* 把平台 Socket 错误映射为稳定错误类别，供原语层和端口后端共用。 */
xerrkind __xrtNetSocketErrorKind(int iCode);



/* 设置保留平台错误码的结构化网络错误。 */
void __xrtNetSocketSetSystemError(xneterror Code,
	cstr sOperation, cstr sMessage, int iSystemCode);



/* 查询平台接收队列，不修改调用线程的 XRT 错误对象。 */
bool __xrtNetSocketAvailableNative(xnetsocket Socket,
	size_t* pSize, int* pSystemCode);



/* 控制 Windows 是否把 UDP ICMP Port Unreachable 映射为接收重置。 */
bool __xrtNetSocketUdpConnReset(xnetsocket Socket, bool bEnabled);



/* 清零并解析平台控制消息，只发布 Socket 已启用的字段。 */
void __xrtNetSocketDgramMetaParse(
	xnetsocket Socket,
	xnetdgrammeta* pMeta,
	const void* pControl,
	size_t iControl,
	uint32 iMessageFlags
);



/* 返回已缓存的 WSARecvMsg 地址；非 Windows 平台恒为零。 */
uintptr_t __xrtNetSocketReceiveMessage(xnetsocket Socket);



/* 返回已缓存的 WSASendMsg 地址；非 Windows 平台恒为零。 */
uintptr_t __xrtNetSocketSendMessage(xnetsocket Socket);



/* 校验并构建发送控制消息；零 Flags 不产生控制缓冲。 */
bool __xrtNetSocketDgramControlBuild(
	xnetsocket Socket,
	const xnetdgramcontrol* pControl,
	size_t iPayload,
	void* pBuffer,
	size_t iCapacity,
	size_t* pSize,
	xneterror Code,
	cstr sOperation
);

#endif

#endif
