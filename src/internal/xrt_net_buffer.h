#ifndef XRT_INTERNAL_NET_BUFFER_H
#define XRT_INTERNAL_NET_BUFFER_H

#include "xrt_net.h"



#if defined(XRT_FEATURE_NET_BUFFER)

/* 块分类编号四个缓存类，后跟动态块和外部引用块。 */
#define XRT_NET_BLOCK_DYNAMIC XNET_BUFFER_CLASS_COUNT
#define XRT_NET_BLOCK_REF (XNET_BUFFER_CLASS_COUNT + 1u)
#define XRT_NET_BLOCK_FILE (XNET_BUFFER_CLASS_COUNT + 2u)



/* 块头同时承担链节点；拥有块的数据紧随结构，引用块只保存外部视图。 */
struct xnetblock {
	xnetblock* Next;
	xnetbufpool* Pool;
	cbytes External;
	xnetreleaseproc Release;
	ptr ReleaseContext;
	size_t Begin;
	size_t End;
	size_t Capacity;
	size_t ExternalSize;
	uint32 Class;
	uint32 Flags;
	uint8 Data[1];
};



/* 缓冲池不加锁，由一个 worker 或一个调用线程独占。 */
struct xnetbufpool {
	xnetbufpoolconfig Config;
	xnetblock* Free[XNET_BUFFER_CLASS_COUNT];
	size_t Cached[XNET_BUFFER_CLASS_COUNT];
	xnetbufpoolinfo Info;
};



/* 拥有型网络字节把引用、只读视图和载荷放在同一块内存中。 */
struct xnetbytes {
	xbytesview View;
	volatile int32 References;
	uint8 Data[];
};



/* 分配指定长度的拥有型网络字节，并返回仅供初始化阶段使用的可写视图。 */
xnetbytes* __xrtNetBytesAlloc(size_t iSize, xnetwspan* pSpan);



/* 返回拥有块或外部引用块的只读数据起点。 */
static inline cbytes __xrtNetBlockData(const xnetblock* pBlock)
{
	return ((pBlock->Class == XRT_NET_BLOCK_REF) ||
		(pBlock->Class == XRT_NET_BLOCK_FILE)) ?
		pBlock->External : pBlock->Data;
}



/* 返回块当前可读字节数。 */
static inline size_t __xrtNetBlockReadable(const xnetblock* pBlock)
{
	return (pBlock != NULL) && (pBlock->End >= pBlock->Begin) ?
		pBlock->End - pBlock->Begin : 0;
}



/* 验证缓冲池尺寸类和缓存预算，供拥有缓冲池的上层模块复用。 */
bool __xrtNetBufPoolConfigValid(const xnetbufpoolconfig* pConfig);



/* 追加一个只由 TCP 文件发送路径解释的文件区间节点。 */
bool __xrtNetBufAppendFile(
	xnetbuf* pBuffer,
	ptr pDescriptor,
	size_t iSize,
	xnetreleaseproc pRelease,
	ptr pContext
);



/* 返回队首文件节点的描述符和已发送偏移；队首不是文件时返回 false。 */
bool __xrtNetBufFileFront(
	const xnetbuf* pBuffer,
	ptr* pDescriptor,
	size_t* pOffset,
	size_t* pSize
);



/* 在调用方已经验证两端可变且容量可相加后，无失败地移动全部块。 */
void __xrtNetBufMoveTrusted(xnetbuf* pTarget, xnetbuf* pSource);



/* 清零拥有块中即将消费的前缀，再按普通消费语义释放或推进块。 */
size_t __xrtNetBufConsumeSecure(xnetbuf* pBuffer, size_t iSize);



/* 清零全部拥有块后释放缓冲；外部借用和引用数据不会被修改。 */
void __xrtNetBufClearSecure(xnetbuf* pBuffer);

#endif

#endif
