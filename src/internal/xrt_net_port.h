#ifndef XRT_INTERNAL_NET_PORT_H
#define XRT_INTERNAL_NET_PORT_H

#include "xrt_net_socket.h"



#if defined(XRT_FEATURE_NET_PORT)

#define XRT_NET_PORT_BUCKET_MIN 16u
#define XRT_NET_PORT_BUCKET_MAX 65536u
#define XRT_NET_PORT_CACHE_CLASS_MIN 256u
#define XRT_NET_PORT_CACHE_CLASS_COUNT 4u



typedef struct __xrt_net_port_driver __xrt_net_port_driver;
typedef struct __xrt_net_port_post __xrt_net_port_post;
typedef struct __xrt_net_port_submit __xrt_net_port_submit;



/* 完成后端共用的 owner 线程尺寸类缓存；空闲节点用首个指针槽连接。 */
typedef struct __xrt_net_port_cache {
	ptr Head[XRT_NET_PORT_CACHE_CLASS_COUNT];
	size_t Count[XRT_NET_PORT_CACHE_CLASS_COUNT];
	size_t Limit;
} __xrt_net_port_cache;



/* 返回可容纳指定字节数的缓存尺寸类，过大对象返回尺寸类总数。 */
static inline size_t __xrtNetPortCacheClass(size_t iRequired)
{
	size_t iCapacity = XRT_NET_PORT_CACHE_CLASS_MIN;

	for ( size_t i = 0; i < XRT_NET_PORT_CACHE_CLASS_COUNT; i++ ) {
		if ( iRequired <= iCapacity ) {
			return i;
		}
		iCapacity <<= 1;
	}
	return XRT_NET_PORT_CACHE_CLASS_COUNT;
}



/* 从匹配的尺寸类复用对象；返回容量供对象终态时精确归还。 */
static inline ptr __xrtNetPortCacheAlloc(
	__xrt_net_port_cache* pCache,
	size_t iRequired,
	size_t* pCapacity
)
{
	size_t iClass = __xrtNetPortCacheClass(iRequired);
	size_t iCapacity = iRequired;
	ptr pMemory = NULL;

	if ( iClass < XRT_NET_PORT_CACHE_CLASS_COUNT ) {
		iCapacity = XRT_NET_PORT_CACHE_CLASS_MIN << iClass;
		pMemory = pCache->Head[iClass];
		if ( pMemory != NULL ) {
			pCache->Head[iClass] = *(ptr*)pMemory;
			pCache->Count[iClass]--;
		}
	}
	if ( pMemory == NULL ) {
		pMemory = xrtMalloc(iCapacity);
	}
	if ( pMemory != NULL ) {
		memset(pMemory, 0, iCapacity);
	}
	*pCapacity = iCapacity;
	return pMemory;
}



/* 把终态对象放回匹配尺寸类；缓存关闭或已满时立即归还堆。 */
static inline void __xrtNetPortCacheFree(
	__xrt_net_port_cache* pCache,
	ptr pMemory,
	size_t iCapacity
)
{
	size_t iClass;
	size_t iClassCapacity;

	if ( pMemory == NULL ) {
		return;
	}
	iClass = __xrtNetPortCacheClass(iCapacity);
	iClassCapacity = (iClass < XRT_NET_PORT_CACHE_CLASS_COUNT) ?
		(XRT_NET_PORT_CACHE_CLASS_MIN << iClass) : 0;
	if ( (iClass >= XRT_NET_PORT_CACHE_CLASS_COUNT) ||
		 (iClassCapacity != iCapacity) || (pCache->Limit == 0) ||
		 (pCache->Count[iClass] >= pCache->Limit) ) {
		xrtFree(pMemory);
		return;
	}

	*(ptr*)pMemory = pCache->Head[iClass];
	pCache->Head[iClass] = pMemory;
	pCache->Count[iClass]++;
}



/* 释放全部缓存对象；只能在完成后端已经排空之后调用。 */
static inline void __xrtNetPortCacheUnit(__xrt_net_port_cache* pCache)
{
	for ( size_t i = 0; i < XRT_NET_PORT_CACHE_CLASS_COUNT; i++ ) {
		ptr pMemory = pCache->Head[i];

		while ( pMemory != NULL ) {
			ptr pNext = *(ptr*)pMemory;

			xrtFree(pMemory);
			pMemory = pNext;
		}
	}
	memset(pCache, 0, sizeof(*pCache));
}



/* 为 readiness 观察上限选择有界的 2 次幂哈希桶数量。 */
static inline size_t __xrtNetPortBucketCount(size_t iWatchLimit)
{
	size_t iTarget = iWatchLimit;
	size_t iCount = XRT_NET_PORT_BUCKET_MIN;

	if ( iTarget > XRT_NET_PORT_BUCKET_MAX ) {
		iTarget = XRT_NET_PORT_BUCKET_MAX;
	}
	while ( iCount < iTarget ) {
		iCount <<= 1;
	}
	return iCount;
}



/* 为活动操作 ID 混合全部 64 位，32 位构建也保留高位熵。 */
static inline size_t __xrtNetPortHashId(uint64 Id, size_t iBucketCount)
{
	Id ^= Id >> 30;
	Id *= UINT64_C(0xBF58476D1CE4E5B9);
	Id ^= Id >> 27;
	Id *= UINT64_C(0x94D049BB133111EB);
	Id ^= Id >> 31;
	return (size_t)Id & (iBucketCount - 1u);
}



/* 活动节点超过两倍桶数时返回下一档索引大小。 */
static inline size_t __xrtNetPortBucketNext(
	size_t iCount,
	size_t iBucketCount,
	size_t iBucketLimit
)
{
	if ( (iCount <= (iBucketCount * 2u)) ||
		 (iBucketCount >= iBucketLimit) ) {
		return iBucketCount;
	}
	return (iBucketCount * 2u) < iBucketLimit ?
		iBucketCount * 2u : iBucketLimit;
}



/* 按平台指针字宽混合描述符或内部令牌，避免顺序值集中到少数桶。 */
static inline size_t __xrtNetPortHash(
	uintptr_t iKey,
	size_t iBucketCount
)
{
	#if UINTPTR_MAX > UINT32_MAX
		uint64 iValue = (uint64)iKey;

		iValue ^= iValue >> 33;
		iValue *= UINT64_C(0xFF51AFD7ED558CCD);
		iValue ^= iValue >> 33;
		iValue *= UINT64_C(0xC4CEB9FE1A85EC53);
		iValue ^= iValue >> 33;
		return (size_t)iValue & (iBucketCount - 1u);
	#else
		uint32 iValue = (uint32)iKey;

		iValue ^= iValue >> 16;
		iValue *= UINT32_C(0x7FEB352D);
		iValue ^= iValue >> 15;
		iValue *= UINT32_C(0x846CA68B);
		iValue ^= iValue >> 16;
		return (size_t)iValue & (iBucketCount - 1u);
	#endif
}



/* 完成式提交只借用 Socket 与缓冲，后端必须复制 Span 描述符和地址。 */
struct __xrt_net_port_submit {
	xnetporteventtype Type;
	xnetsocket Socket;
	uint64 Id;
	ptr User;
	const xnetwspan* ReadSpans;
	const xnetspan* WriteSpans;
	size_t SpanCount;
	const xnetaddr* Address;
	const xnetdgramcontrol* Control;
	intptr_t File;
	uint64 FileOffset;
	size_t FileSize;
	bool* FileAssociated;
};



/* 后端只负责平台观察与唤醒，共同事件队列留在 port 核心。 */
struct __xrt_net_port_driver {
	cstr Name;
	xnetportbackend Backend;
	uint32 Capabilities;
	bool (*Init)(xnetport* pPort);
	bool (*Unit)(xnetport* pPort);
	bool (*Watch)(xnetport* pPort, xnetsocket Socket,
		uint64 Id, uint32 iEvents, ptr pUser);
	bool (*Unwatch)(xnetport* pPort, xnetsocket Socket);
	bool (*Submit)(xnetport* pPort,
		const __xrt_net_port_submit* pSubmit);
	bool (*Cancel)(xnetport* pPort, uint64 Id);
	xnetresult (*Wait)(xnetport* pPort, xnetportevent* pEvents,
		size_t iCapacity, uint64 iTimeout, size_t* pCount);
	bool (*Wake)(xnetport* pPort);
};



/* 用户事件节点只复制标识和上下文，不拥有用户数据。 */
struct __xrt_net_port_post {
	__xrt_net_port_post* Next;
	uint64 Id;
	ptr User;
};



/* 端口核心拥有后端、跨线程队列和可合并唤醒状态。 */
struct xnetport_impl {
	const __xrt_net_port_driver* Driver;
	ptr Context;
	xnetportconfig Config;
	xmutex Lock;
	__xrt_net_port_post* PostHead;
	__xrt_net_port_post* PostTail;
	size_t PostCount;
	bool NotifyPending;
	bool WakePending;
	uint32 Capabilities;
	uint64 Owner;
	/* Post 队列仍有积压时，下一轮先给原生后端一次非阻塞提取机会。 */
	bool BackendTurn;
	bool Closing;
};



/* 提交一个借用原生文件句柄的完成式文件发送。 */
bool __xrtNetPortSendFile(
	xnetport* pPort,
	xnetsocket Socket,
	intptr_t iFile,
	uint64 iOffset,
	size_t iSize,
	uint64 Id,
	ptr pUser
);



/* 返回每次端口创建都唯一的文件归属标识。 */
uint64 __xrtNetPortOwner(const xnetport* pPort);



/* 提交一次借用调用方缓冲的原生定位文件读取。 */
bool __xrtNetPortFileRead(
	xnetport* pPort,
	intptr_t iFile,
	uint64 iOffset,
	void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser,
	bool* pAssociated
);



/* 提交一次借用调用方缓冲的原生定位文件写入。 */
bool __xrtNetPortFileWrite(
	xnetport* pPort,
	intptr_t iFile,
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	uint64 Id,
	ptr pUser,
	bool* pAssociated
);



#if defined(XRT_FEATURE_NET_PORT_SELECT)
const __xrt_net_port_driver* __xrtNetPortSelectDriver(void);
#endif

#if defined(XRT_FEATURE_NET_PORT_EPOLL)
const __xrt_net_port_driver* __xrtNetPortEpollDriver(void);
#endif

#if defined(XRT_FEATURE_NET_PORT_URING)
const __xrt_net_port_driver* __xrtNetPortUringDriver(void);
#endif

#if defined(XRT_FEATURE_NET_PORT_KQUEUE)
const __xrt_net_port_driver* __xrtNetPortKqueueDriver(void);
#endif

#if defined(XRT_FEATURE_NET_PORT_IOCP)
const __xrt_net_port_driver* __xrtNetPortIOCPDriver(void);
#endif

#endif

#endif
