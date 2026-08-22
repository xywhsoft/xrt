#include "../internal/xrt_memory.h"



/* 旧版全局堆使用的尺寸类参数。 */
#define XRT_HEAP_CLASS_STEP		16u
#define XRT_HEAP_CLASS_CUTOFF	1024u
#define XRT_HEAP_CLASS_COUNT		(XRT_HEAP_CLASS_CUTOFF / XRT_HEAP_CLASS_STEP)
#define XRT_HEAP_SPAN_BYTES		4096u
#define XRT_HEAP_SPAN_MIN_BLOCKS	8u
#define XRT_HEAP_SPAN_MAX_BLOCKS	128u
#define XRT_HEAP_CLASS_BACKING	UINT16_MAX
#define XRT_HEAP_CACHE_LIMIT		32u
#define XRT_HEAP_CACHE_REFILL		16u



/* 空闲块直接把用户区作为单链表节点。 */
typedef struct xrt_heap_free {
	struct xrt_heap_free* Next;
} xrt_heap_free;



/* 一个 span 批量承载同一尺寸类的内存块。 */
typedef struct xrt_heap_span {
	struct xrt_heap_span* Next;
	ptr Allocation;
	uint16 Class;
	uint16 Reserved;
	uint32 BlockCount;
} xrt_heap_span;



/* 每个尺寸类维护独立中央空闲链，降低锁竞争。 */
typedef struct xrt_heap_class {
	xrt_spinlock Lock;
	xrt_heap_free* Free;
	uint32 FreeCount;
	uint32 SpanCount;
} xrt_heap_class;



/* 每个原生线程缓存少量小块，避免每次操作都竞争中央锁。 */
typedef struct xrt_heap_cache {
	xrt_heap_free* Free[XRT_HEAP_CLASS_COUNT];
	uint16 Count[XRT_HEAP_CLASS_COUNT];
	uint16 Reserved;
} xrt_heap_cache;



/* 全局堆状态不再暴露到公共运行时对象。 */
typedef struct xrt_heap_state {
	volatile int32 InitState;
	xrt_spinlock SpanLock;
	xrt_heap_span* Spans;
	xrt_heap_class Classes[XRT_HEAP_CLASS_COUNT];
	uint8 SizeClass[XRT_HEAP_CLASS_CUTOFF + 1];
} xrt_heap_state;



static xrt_heap_state __xrtHeap;

#if defined(_WIN32) || defined(_WIN64)
static DWORD __xrtHeapCacheTls = FLS_OUT_OF_INDEXES;
static DWORD __xrtHeapCacheGuardTls = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtHeapCacheTlsState = 0;
#else
static pthread_key_t __xrtHeapCacheTls;
static pthread_key_t __xrtHeapCacheGuardTls;
static pthread_once_t __xrtHeapCacheTlsOnce = PTHREAD_ONCE_INIT;
static bool __xrtHeapCacheTlsReady = false;
#endif



/* 前置声明供调试所有权解析在初始化实现之前使用。 */
static void __xrtHeapEnsure(void);

#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static pthread_once_t __xrtHeapOnce = PTHREAD_ONCE_INIT;
#endif



/* 将大小向堆对齐单位上取整。 */
static size_t __xrtHeapAlign(size_t iSize)
{
	return (iSize + (XRT_HEAP_ALIGNMENT - 1)) & ~((size_t)XRT_HEAP_ALIGNMENT - 1);
}



/* 在原始分配中计算满足全局堆对齐的地址。 */
static ptr __xrtHeapAlignPointer(ptr pMemory)
{
	uintptr_t iAddress = (uintptr_t)pMemory;

	return (ptr)((iAddress + (XRT_HEAP_ALIGNMENT - 1u)) &
		~((uintptr_t)XRT_HEAP_ALIGNMENT - 1u));
}



/* 返回一个尺寸类包含调试尾边界后的实际用户区容量。 */
static size_t __xrtHeapClassCapacity(uint16 iClass)
{
	return (((size_t)iClass + 1) * XRT_HEAP_CLASS_STEP) + __xrtMemDebugTailSize();
}



/* 返回块头对应的用户地址。 */
static ptr __xrtHeapUser(xrt_heap_header* pHeader)
{
	return (ptr)((unsigned char*)pHeader + __xrtHeapHeaderSize());
}



/* 返回用户地址前的块头。 */
static xrt_heap_header* __xrtHeapHeader(ptr pMemory)
{
	return (xrt_heap_header*)((unsigned char*)pMemory - __xrtHeapHeaderSize());
}



#if defined(XRT_FEATURE_MEMORY_DEBUG)
/* 在已分配 span 中安全解析一个精确的池化块地址。 */
static bool __xrtHeapFindPooledHeader(ptr pMemory, xrt_heap_header** ppHeader)
{
	uintptr_t iAddress = (uintptr_t)pMemory;
	xrt_heap_span* pSpan;
	bool bFound = false;

	__xrtHeapEnsure();
	__xrtSpinLock(&__xrtHeap.SpanLock);
	pSpan = __xrtHeap.Spans;
	while ( pSpan != NULL ) {
		size_t iPayload = __xrtHeapClassCapacity(pSpan->Class);
		size_t iStride = __xrtHeapAlign(__xrtHeapHeaderSize() + iPayload);
		uintptr_t iFirst = (uintptr_t)((unsigned char*)pSpan + __xrtHeapAlign(sizeof(xrt_heap_span)) +
			__xrtHeapHeaderSize());
		uintptr_t iEnd = iFirst + ((uintptr_t)iStride * pSpan->BlockCount);

		if ( (iAddress >= iFirst) && (iAddress < iEnd) && (((iAddress - iFirst) % iStride) == 0) ) {
			*ppHeader = (xrt_heap_header*)(iAddress - __xrtHeapHeaderSize());
			bFound = true;
			break;
		}
		pSpan = pSpan->Next;
	}
	__xrtSpinUnlock(&__xrtHeap.SpanLock);
	return bFound;
}



/* 不读取未知地址，先从调试登记和 span 边界解析块头。 */
static xrt_heap_header* __xrtHeapResolveHeader(ptr pMemory)
{
	xrt_heap_header* pHeader = NULL;

	if ( __xrtMemDebugFindHeader(pMemory, &pHeader) ) {
		return pHeader;
	}
	if ( __xrtHeapFindPooledHeader(pMemory, &pHeader) ) {
		return pHeader;
	}

	return NULL;
}
#endif



/* 验证块头归属和尺寸类边界。 */
static bool __xrtHeapHeaderValid(const xrt_heap_header* pHeader)
{
	if ( (pHeader == NULL) || (pHeader->Magic != XRT_HEAP_MAGIC) ) {
		return false;
	}
	if ( pHeader->Flags == XRT_HEAP_FLAG_POOLED ) {
		return pHeader->Class < XRT_HEAP_CLASS_COUNT;
	}
	if ( pHeader->Flags == XRT_HEAP_FLAG_BACKING ) {
		return pHeader->Class == XRT_HEAP_CLASS_BACKING;
	}

	return false;
}



/* 写入一个正在使用的块头。 */
static void __xrtHeapWriteHeader(xrt_heap_header* pHeader, uint16 iClass, uint16 iFlags, size_t iSize)
{
	pHeader->Magic = XRT_HEAP_MAGIC;
	pHeader->Class = iClass;
	pHeader->Flags = iFlags;
	pHeader->Size = iSize;
	pHeader->Allocation = NULL;
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		pHeader->DebugPrev = NULL;
		pHeader->DebugNext = NULL;
		pHeader->AllocFile = NULL;
		pHeader->FreeFile = NULL;
		pHeader->AllocLine = 0;
		pHeader->FreeLine = 0;
		pHeader->DebugState = 0;
	#endif
}



/* 初始化尺寸类和查找表。 */
static void __xrtHeapInit(void)
{
	for ( uint32 i = 0; i < XRT_HEAP_CLASS_COUNT; i++ ) {
		__xrtSpinInit(&__xrtHeap.Classes[i].Lock);
	}
	__xrtSpinInit(&__xrtHeap.SpanLock);
	for ( uint32 i = 1; i <= XRT_HEAP_CLASS_CUTOFF; i++ ) {
		__xrtHeap.SizeClass[i] = (uint8)(((i + (XRT_HEAP_CLASS_STEP - 1)) / XRT_HEAP_CLASS_STEP) - 1);
	}
}



/* 线程安全地完成全局堆惰性初始化。 */
static void __xrtHeapEnsure(void)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_once(&__xrtHeapOnce, __xrtHeapInit);
	#else
		int32 iState;

		#if defined(_MSC_VER)
			iState = (int32)_InterlockedCompareExchange((volatile long*)&__xrtHeap.InitState, 1, 0);
		#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
			iState = (int32)InterlockedCompareExchange((volatile LONG*)&__xrtHeap.InitState, 1, 0);
		#else
			iState = (int32)__sync_val_compare_and_swap(&__xrtHeap.InitState, 0, 1);
		#endif

		if ( iState == 0 ) {
			__xrtHeapInit();
			#if defined(_MSC_VER)
				(void)_InterlockedExchange((volatile long*)&__xrtHeap.InitState, 2);
			#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
				(void)InterlockedExchange((volatile LONG*)&__xrtHeap.InitState, 2);
			#else
				(void)__sync_lock_test_and_set(&__xrtHeap.InitState, 2);
			#endif
			return;
		}

		while ( __xrtAtomicRefLoad(&__xrtHeap.InitState) != 2 ) {
			#if defined(_WIN32) || defined(_WIN64)
				(void)SwitchToThread();
			#else
				(void)sched_yield();
			#endif
		}
	#endif
}



/* 返回请求大小对应的尺寸类。 */
static uint16 __xrtHeapClass(size_t iSize)
{
	if ( iSize == 0 ) {
		iSize = 1;
	}
	if ( iSize > XRT_HEAP_CLASS_CUTOFF ) {
		return XRT_HEAP_CLASS_BACKING;
	}

	return __xrtHeap.SizeClass[iSize];
}



/* 将一组空闲块压入尺寸类中央链表。 */
static void __xrtHeapPush(uint16 iClass, xrt_heap_free* pHead, xrt_heap_free* pTail, uint32 iCount)
{
	xrt_heap_class* pClass = &__xrtHeap.Classes[iClass];

	__xrtSpinLock(&pClass->Lock);
	pTail->Next = pClass->Free;
	pClass->Free = pHead;
	pClass->FreeCount += iCount;
	__xrtSpinUnlock(&pClass->Lock);
}



/* 从尺寸类中央链表弹出一个空闲块。 */
static xrt_heap_free* __xrtHeapPop(uint16 iClass)
{
	xrt_heap_class* pClass = &__xrtHeap.Classes[iClass];
	xrt_heap_free* pNode;

	__xrtSpinLock(&pClass->Lock);
	pNode = pClass->Free;
	if ( pNode != NULL ) {
		pClass->Free = pNode->Next;
		pClass->FreeCount--;
	}
	__xrtSpinUnlock(&pClass->Lock);

	return pNode;
}



/* 从一个尺寸类批量取得空闲块，减少中央锁获取次数。 */
static xrt_heap_free* __xrtHeapPopBatch(uint16 iClass, uint32 iLimit, uint32* pCount)
{
	xrt_heap_class* pClass = &__xrtHeap.Classes[iClass];
	xrt_heap_free* pHead;
	xrt_heap_free* pTail;
	xrt_heap_free* pNext;
	uint32 iCount = 0;

	__xrtSpinLock(&pClass->Lock);
	pHead = pClass->Free;
	pTail = NULL;
	pNext = pHead;
	while ( (pNext != NULL) && (iCount < iLimit) ) {
		pTail = pNext;
		pNext = pNext->Next;
		iCount++;
	}
	if ( pTail != NULL ) {
		pTail->Next = NULL;
		pClass->Free = pNext;
		pClass->FreeCount -= iCount;
	}
	__xrtSpinUnlock(&pClass->Lock);

	*pCount = iCount;
	return pHead;
}



/* 将一个线程缓存中的指定尺寸类批量归还中央链。 */
static void __xrtHeapCacheDrain(
	xrt_heap_cache* pCache,
	uint16 iClass,
	uint16 iKeep
)
{
	xrt_heap_free* pHead;
	xrt_heap_free* pTail;
	uint32 iCount;

	if ( pCache->Count[iClass] <= iKeep ) {
		return;
	}
	pHead = pCache->Free[iClass];
	pTail = pHead;
	iCount = (uint32)pCache->Count[iClass] - iKeep;
	for ( uint32 i = 1; i < iCount; i++ ) {
		pTail = pTail->Next;
	}
	pCache->Free[iClass] = pTail->Next;
	pTail->Next = NULL;
	pCache->Count[iClass] = iKeep;
	__xrtHeapPush(iClass, pHead, pTail, iCount);
}



/* 在线程退出时归还全部小块并释放缓存元数据。 */
static void __xrtHeapCacheRelease(xrt_heap_cache* pCache)
{
	if ( pCache == NULL ) {
		return;
	}
	for ( uint16 i = 0; i < XRT_HEAP_CLASS_COUNT; i++ ) {
		__xrtHeapCacheDrain(pCache, i, 0);
	}
	__xrtBackingFree(pCache);
}



#if defined(_WIN32) || defined(_WIN64)

/* Windows 在线程退出时禁用缓存重建并归还全部小块。 */
static VOID WINAPI __xrtHeapCacheDestroy(PVOID pValue)
{
	(void)FlsSetValue(__xrtHeapCacheGuardTls, (ptr)(uintptr_t)1);
	__xrtHeapCacheRelease((xrt_heap_cache*)pValue);
}



/* 初始化缓存槽与无析构的退出保护槽。 */
static bool __xrtHeapCacheTlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtHeapCacheTlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtHeapCacheTls = FlsAlloc(__xrtHeapCacheDestroy);
		__xrtHeapCacheGuardTls = FlsAlloc(NULL);
		if ( (__xrtHeapCacheTls == FLS_OUT_OF_INDEXES) ||
			 (__xrtHeapCacheGuardTls == FLS_OUT_OF_INDEXES) ) {
			if ( __xrtHeapCacheTls != FLS_OUT_OF_INDEXES ) {
				(void)FlsFree(__xrtHeapCacheTls);
			}
			if ( __xrtHeapCacheGuardTls != FLS_OUT_OF_INDEXES ) {
				(void)FlsFree(__xrtHeapCacheGuardTls);
			}
			__xrtHeapCacheTls = FLS_OUT_OF_INDEXES;
			__xrtHeapCacheGuardTls = FLS_OUT_OF_INDEXES;
			(void)InterlockedExchange(&__xrtHeapCacheTlsState, 3);
			return false;
		}
		(void)InterlockedExchange(&__xrtHeapCacheTlsState, 2);
		return true;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtHeapCacheTlsState, 0, 0)) == 1 ) {
		(void)SwitchToThread();
	}

	return iState == 2;
}

#else

/* POSIX 在线程退出时禁用缓存重建并归还全部小块。 */
static void __xrtHeapCacheDestroy(ptr pValue)
{
	(void)pthread_setspecific(__xrtHeapCacheGuardTls, (ptr)(uintptr_t)1);
	__xrtHeapCacheRelease((xrt_heap_cache*)pValue);
}



/* 初始化缓存槽与无析构的退出保护槽。 */
static void __xrtHeapCacheTlsInit(void)
{
	if ( pthread_key_create(&__xrtHeapCacheTls, __xrtHeapCacheDestroy) != 0 ) {
		return;
	}
	if ( pthread_key_create(&__xrtHeapCacheGuardTls, NULL) != 0 ) {
		(void)pthread_key_delete(__xrtHeapCacheTls);
		return;
	}
	__xrtHeapCacheTlsReady = true;
}



/* 确保 POSIX 缓存槽只初始化一次。 */
static bool __xrtHeapCacheTlsEnsure(void)
{
	(void)pthread_once(&__xrtHeapCacheTlsOnce, __xrtHeapCacheTlsInit);
	return __xrtHeapCacheTlsReady;
}

#endif



/* 返回当前线程的惰性缓存，系统资源不足时安静退回中央链。 */
static xrt_heap_cache* __xrtHeapCacheGet(void)
{
	xrt_heap_cache* pCache;

	if ( !__xrtHeapCacheTlsEnsure() ) {
		return NULL;
	}
	#if defined(_WIN32) || defined(_WIN64)
		if ( FlsGetValue(__xrtHeapCacheGuardTls) != NULL ) {
			return NULL;
		}
		pCache = (xrt_heap_cache*)FlsGetValue(__xrtHeapCacheTls);
	#else
		if ( pthread_getspecific(__xrtHeapCacheGuardTls) != NULL ) {
			return NULL;
		}
		pCache = (xrt_heap_cache*)pthread_getspecific(__xrtHeapCacheTls);
	#endif
	if ( pCache != NULL ) {
		return pCache;
	}

	pCache = (xrt_heap_cache*)__xrtBackingAlloc(sizeof(xrt_heap_cache));
	if ( pCache == NULL ) {
		return NULL;
	}
	memset(pCache, 0, sizeof(xrt_heap_cache));
	#if defined(_WIN32) || defined(_WIN64)
		if ( !FlsSetValue(__xrtHeapCacheTls, pCache) ) {
			__xrtBackingFree(pCache);
			return NULL;
		}
	#else
		if ( pthread_setspecific(__xrtHeapCacheTls, pCache) != 0 ) {
			__xrtBackingFree(pCache);
			return NULL;
		}
	#endif

	return pCache;
}



/* 优先从线程缓存取得小块，空缓存按批从中央链补给。 */
static xrt_heap_free* __xrtHeapTake(uint16 iClass)
{
	xrt_heap_cache* pCache = __xrtHeapCacheGet();
	xrt_heap_free* pNode;

	if ( pCache == NULL ) {
		return __xrtHeapPop(iClass);
	}
	if ( pCache->Free[iClass] == NULL ) {
		uint32 iCount = 0;

		pCache->Free[iClass] = __xrtHeapPopBatch(
			iClass,
			XRT_HEAP_CACHE_REFILL,
			&iCount
		);
		pCache->Count[iClass] = (uint16)iCount;
	}
	pNode = pCache->Free[iClass];
	if ( pNode != NULL ) {
		pCache->Free[iClass] = pNode->Next;
		pCache->Count[iClass]--;
		pNode->Next = NULL;
	}

	return pNode;
}



/* 将小块放回线程缓存，超过上限时批量归还一半。 */
static void __xrtHeapReturn(uint16 iClass, xrt_heap_free* pNode)
{
	xrt_heap_cache* pCache = __xrtHeapCacheGet();

	if ( pCache == NULL ) {
		pNode->Next = NULL;
		__xrtHeapPush(iClass, pNode, pNode, 1);
		return;
	}
	pNode->Next = pCache->Free[iClass];
	pCache->Free[iClass] = pNode;
	pCache->Count[iClass]++;
	if ( pCache->Count[iClass] > XRT_HEAP_CACHE_LIMIT ) {
		__xrtHeapCacheDrain(
			pCache,
			iClass,
			XRT_HEAP_CACHE_LIMIT / 2u
		);
	}
}



/* 为一个尺寸类分配并切分新的 span，为创建者保留一个块。 */
static xrt_heap_free* __xrtHeapAllocSpan(uint16 iClass)
{
	size_t iPayload = __xrtHeapClassCapacity(iClass);
	size_t iStride = __xrtHeapAlign(__xrtHeapHeaderSize() + iPayload);
	uint32 iBlockCount = (uint32)(XRT_HEAP_SPAN_BYTES / iStride);
	size_t iSpanHeader = __xrtHeapAlign(sizeof(xrt_heap_span));
	size_t iBytes;
	ptr pAllocation;
	xrt_heap_span* pSpan;
	xrt_heap_free* pHead = NULL;
	xrt_heap_free* pTail = NULL;
	unsigned char* pCursor;

	if ( iBlockCount < XRT_HEAP_SPAN_MIN_BLOCKS ) {
		iBlockCount = XRT_HEAP_SPAN_MIN_BLOCKS;
	}
	if ( iBlockCount > XRT_HEAP_SPAN_MAX_BLOCKS ) {
		iBlockCount = XRT_HEAP_SPAN_MAX_BLOCKS;
	}
	if ( iStride > ((SIZE_MAX - iSpanHeader) / iBlockCount) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = iSpanHeader + (iStride * iBlockCount);
	if ( iBytes > (SIZE_MAX - (XRT_HEAP_ALIGNMENT - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pAllocation = __xrtBackingAlloc(iBytes + (XRT_HEAP_ALIGNMENT - 1u));
	if ( pAllocation == NULL ) {
		__xrtErrorSetOutOfMemory();
		return NULL;
	}
	if ( (uintptr_t)pAllocation > (UINTPTR_MAX - (XRT_HEAP_ALIGNMENT - 1u)) ) {
		__xrtBackingFree(pAllocation);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pSpan = (xrt_heap_span*)__xrtHeapAlignPointer(pAllocation);

	pSpan->Allocation = pAllocation;
	pSpan->Class = iClass;
	pSpan->Reserved = 0;
	pSpan->BlockCount = iBlockCount;
	pCursor = (unsigned char*)pSpan + iSpanHeader;
	for ( uint32 i = 0; i < iBlockCount; i++ ) {
		xrt_heap_header* pHeader = (xrt_heap_header*)pCursor;
		xrt_heap_free* pNode;

		__xrtHeapWriteHeader(pHeader, iClass, XRT_HEAP_FLAG_POOLED, iPayload);
		pNode = (xrt_heap_free*)__xrtHeapUser(pHeader);
		pNode->Next = NULL;
		if ( pHead == NULL ) {
			pHead = pNode;
		} else {
			pTail->Next = pNode;
		}
		pTail = pNode;
		pCursor += iStride;
	}

	__xrtSpinLock(&__xrtHeap.SpanLock);
	pSpan->Next = __xrtHeap.Spans;
	__xrtHeap.Spans = pSpan;
	__xrtSpinUnlock(&__xrtHeap.SpanLock);

	/* 创建者直接取得第一个块，避免发布后被其他线程整批抢空。 */
	{
		xrt_heap_free* pReserved = pHead;

		pHead = pReserved->Next;
		pReserved->Next = NULL;
		__xrtHeapPush(iClass, pHead, pTail, iBlockCount - 1u);
		__xrtSpinLock(&__xrtHeap.Classes[iClass].Lock);
		__xrtHeap.Classes[iClass].SpanCount++;
		__xrtSpinUnlock(&__xrtHeap.Classes[iClass].Lock);
		return pReserved;
	}
}



/* 从池化尺寸类分配内存。 */
static ptr __xrtHeapAllocPooled(uint16 iClass, size_t iSize, bool bZero, cstr sFile, uint32 iLine)
{
	xrt_heap_free* pNode = __xrtHeapTake(iClass);
	xrt_heap_header* pHeader;
	size_t iCapacity = __xrtHeapClassCapacity(iClass);

	if ( pNode == NULL ) {
		pNode = __xrtHeapAllocSpan(iClass);
		if ( pNode == NULL ) {
			return NULL;
		}
	}

	pHeader = __xrtHeapHeader(pNode);
	__xrtMemDebugReuse(pHeader, pNode, iCapacity, sFile, iLine);
	__xrtHeapWriteHeader(pHeader, iClass, XRT_HEAP_FLAG_POOLED, iSize);
	if ( bZero ) {
		memset(pNode, 0, iCapacity);
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	} else {
		memset(pNode, 0xCD, iCapacity);
	#endif
	}
	__xrtMemDebugAlloc(pHeader, pNode, iCapacity, sFile, iLine);
	__xrtMemStatsBlockAlloc(iSize, iClass, true);

	return pNode;
}



/* 直接通过底层分配器申请大块内存。 */
static ptr __xrtHeapAllocBacking(size_t iSize, bool bZero, cstr sFile, uint32 iLine)
{
	size_t iPayload = iSize != 0 ? iSize : 1;
	size_t iHeaderSize = __xrtHeapHeaderSize();
	size_t iBytes;
	xrt_heap_header* pHeader;
	ptr pAllocation;
	ptr pMemory;

	if ( iPayload > (SIZE_MAX - __xrtMemDebugTailSize()) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iPayload += __xrtMemDebugTailSize();
	if ( iPayload > (SIZE_MAX - iHeaderSize) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	iBytes = iHeaderSize + iPayload;
	if ( iBytes > (SIZE_MAX - (XRT_HEAP_ALIGNMENT - 1u)) ) {
		__xrtErrorSetSizeOverflow();
		return NULL;
	}
	pAllocation = __xrtBackingAlloc(iBytes + (XRT_HEAP_ALIGNMENT - 1u));
	if ( pAllocation == NULL ) {
		__xrtErrorSetOutOfMemory();
		return NULL;
	}
	if (
		(uintptr_t)pAllocation >
		(UINTPTR_MAX - iHeaderSize - (XRT_HEAP_ALIGNMENT - 1u))
	) {
		__xrtBackingFree(pAllocation);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	pMemory = __xrtHeapAlignPointer((unsigned char*)pAllocation + iHeaderSize);
	pHeader = __xrtHeapHeader(pMemory);
	__xrtHeapWriteHeader(pHeader, XRT_HEAP_CLASS_BACKING, XRT_HEAP_FLAG_BACKING, iSize);
	pHeader->Allocation = pAllocation;
	if ( bZero ) {
		memset(pMemory, 0, iPayload);
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
	} else {
		memset(pMemory, 0xCD, iPayload);
	#endif
	}
	__xrtMemDebugAlloc(pHeader, pMemory, iPayload, sFile, iLine);
	__xrtMemStatsBlockAlloc(iSize, XRT_HEAP_CLASS_BACKING, false);

	return pMemory;
}



/* 执行公共分配和清零的共同路径。 */
static ptr __xrtHeapAlloc(size_t iSize, bool bZero, cstr sFile, uint32 iLine)
{
	uint16 iClass;

	__xrtHeapEnsure();
	iClass = __xrtHeapClass(iSize);
	if ( iClass == XRT_HEAP_CLASS_BACKING ) {
		return __xrtHeapAllocBacking(iSize, bZero, sFile, iLine);
	}

	return __xrtHeapAllocPooled(iClass, iSize, bZero, sFile, iLine);
}



/* 分配至少一个字节的内存。 */
XRT_API ptr (xrtMalloc)(size_t iSize)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_MALLOC, iSize);
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	return __xrtHeapAlloc(iSize, false, NULL, 0);
}



/* 分配并清零内存，乘法溢出时失败。 */
XRT_API ptr (xrtCalloc)(size_t iCount, size_t iSize)
{
	if ( (iCount != 0) && (iSize > (SIZE_MAX / iCount)) ) {
		__xrtMemStatsRecord(XRT_MEM_STATS_OP_CALLOC, SIZE_MAX);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	__xrtMemStatsRecord(XRT_MEM_STATS_OP_CALLOC, iCount * iSize);
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	return __xrtHeapAlloc(iCount * iSize, true, NULL, 0);
}



/* 释放内存的共同实现。 */
static bool __xrtHeapFree(ptr pMemory, cstr sFile, uint32 iLine)
{
	xrt_heap_header* pHeader;

	if ( pMemory == NULL ) {
		return true;
	}
	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		pHeader = __xrtHeapResolveHeader(pMemory);
	#else
		pHeader = __xrtHeapHeader(pMemory);
	#endif
	if ( !__xrtHeapHeaderValid(pHeader) ) {
		__xrtMemDebugInvalidFree(pMemory, sFile, iLine);
		__xrtErrorSetInvalidArgument();
		return false;
	}

	if ( (pHeader->Flags & XRT_HEAP_FLAG_POOLED) != 0 ) {
		xrt_heap_free* pNode = (xrt_heap_free*)pMemory;
		size_t iCapacity = __xrtHeapClassCapacity(pHeader->Class);
		int iDisposition = __xrtMemDebugFree(pHeader, pMemory, iCapacity, sFile, iLine);

		if ( iDisposition == XRT_MEMDEBUG_FREE_INVALID ) {
			return false;
		}
		__xrtMemStatsBlockFree(pHeader->Size, pHeader->Class, true);
		if ( iDisposition == XRT_MEMDEBUG_FREE_CONSUMED ) {
			return true;
		}

		__xrtHeapReturn(pHeader->Class, pNode);
		return true;
	}
	if ( (pHeader->Flags & XRT_HEAP_FLAG_BACKING) != 0 ) {
		size_t iCapacity = (pHeader->Size != 0 ? pHeader->Size : 1) + __xrtMemDebugTailSize();
		int iDisposition = __xrtMemDebugFree(pHeader, pMemory, iCapacity, sFile, iLine);
		ptr pAllocation;

		if ( iDisposition == XRT_MEMDEBUG_FREE_INVALID ) {
			return false;
		}
		__xrtMemStatsBlockFree(pHeader->Size, pHeader->Class, false);
		if ( iDisposition == XRT_MEMDEBUG_FREE_CONSUMED ) {
			return true;
		}
		pAllocation = pHeader->Allocation;
		pHeader->Magic = 0;
		__xrtBackingFree(pAllocation);
		return true;
	}

	__xrtErrorSetInvalidArgument();
	return false;
}



/* 释放内存，允许传入空指针。 */
XRT_API void (xrtFree)(ptr pMemory)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_FREE, 0);
	(void)__xrtHeapFree(pMemory, NULL, 0);
}



/* 调整内存大小的共同实现。 */
static ptr __xrtHeapRealloc(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine)
{
	xrt_heap_header* pHeader;
	uint16 iNewClass;
	ptr pNewMemory;
	size_t iCopySize;

	if ( pMemory == NULL ) {
		if ( __xrtMemDebugShouldFailAlloc() ) {
			return NULL;
		}
		return __xrtHeapAlloc(iSize, false, sFile, iLine);
	}
	if ( iSize == 0 ) {
		__xrtHeapFree(pMemory, sFile, iLine);
		return NULL;
	}

	#if defined(XRT_FEATURE_MEMORY_DEBUG)
		pHeader = __xrtHeapResolveHeader(pMemory);
	#else
		pHeader = __xrtHeapHeader(pMemory);
	#endif
	if ( !__xrtHeapHeaderValid(pHeader) ) {
		__xrtMemDebugInvalidFree(pMemory, sFile, iLine);
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtMemDebugCheck(pHeader, pMemory, sFile, iLine) ) {
		return NULL;
	}
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	iNewClass = __xrtHeapClass(iSize);
	if ( ((pHeader->Flags & XRT_HEAP_FLAG_POOLED) != 0) && (pHeader->Class == iNewClass) ) {
		size_t iOldSize = pHeader->Size;

		pHeader->Size = iSize;
		__xrtMemDebugResize(pHeader, pMemory, iOldSize, sFile, iLine);
		return pMemory;
	}
	#if !defined(XRT_FEATURE_MEMORY_DEBUG)
	if ( ((pHeader->Flags & XRT_HEAP_FLAG_BACKING) != 0) &&
		 (iNewClass == XRT_HEAP_CLASS_BACKING) ) {
		size_t iHeaderSize = __xrtHeapHeaderSize();
		size_t iBytes;
		size_t iOldSize = pHeader->Size;
		size_t iOldOffset;
		size_t iMoveSize;
		ptr pAllocation = pHeader->Allocation;
		ptr pNewAllocation;
		ptr pNewUser;

		if ( iSize > (SIZE_MAX - iHeaderSize) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iBytes = iHeaderSize + iSize;
		if ( iBytes > (SIZE_MAX - (XRT_HEAP_ALIGNMENT - 1u)) ) {
			__xrtErrorSetSizeOverflow();
			return NULL;
		}
		iOldOffset = (size_t)((unsigned char*)pMemory - (unsigned char*)pAllocation);
		pNewAllocation = __xrtBackingRealloc(
			pAllocation,
			iBytes + (XRT_HEAP_ALIGNMENT - 1u)
		);
		if ( pNewAllocation == NULL ) {
			__xrtErrorSetOutOfMemory();
			return NULL;
		}

		/* 原始地址变化时，对齐偏移可能变化，先校正有效数据。 */
		pNewUser = __xrtHeapAlignPointer((unsigned char*)pNewAllocation + iHeaderSize);
		iMoveSize = iOldSize < iSize ? iOldSize : iSize;
		if ( iMoveSize != 0 ) {
			memmove(
				pNewUser,
				(unsigned char*)pNewAllocation + iOldOffset,
				iMoveSize
			);
		}
		pHeader = __xrtHeapHeader(pNewUser);
		__xrtHeapWriteHeader(pHeader, XRT_HEAP_CLASS_BACKING, XRT_HEAP_FLAG_BACKING, iSize);
		pHeader->Allocation = pNewAllocation;
		return pNewUser;
	}
	#endif

	pNewMemory = __xrtHeapAlloc(iSize, false, sFile, iLine);
	if ( pNewMemory == NULL ) {
		return NULL;
	}
	iCopySize = pHeader->Size < iSize ? pHeader->Size : iSize;
	if ( iCopySize != 0 ) {
		memcpy(pNewMemory, pMemory, iCopySize);
	}
	__xrtHeapFree(pMemory, sFile, iLine);
	__xrtMemDebugRealloc(pNewMemory, iSize, sFile, iLine);
	return pNewMemory;
}



/* 调整内存大小，大小为零时释放并返回空指针。 */
XRT_API ptr (xrtRealloc)(ptr pMemory, size_t iSize)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_REALLOC, iSize);
	return __xrtHeapRealloc(pMemory, iSize, NULL, 0);
}



/* 复制一段二进制内存的共同实现。 */
static ptr __xrtHeapMemDup(const void* pData, size_t iSize, cstr sFile, uint32 iLine)
{
	ptr pCopy;

	if ( (pData == NULL) && (iSize != 0) ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	pCopy = __xrtHeapAlloc(iSize, false, sFile, iLine);
	if ( (pCopy != NULL) && (iSize != 0) ) {
		memcpy(pCopy, pData, iSize);
	}

	return pCopy;
}



/* 复制一段二进制内存。 */
XRT_API ptr (xrtMemDup)(const void* pData, size_t iSize)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_MEMDUP, iSize);
	return __xrtHeapMemDup(pData, iSize, NULL, 0);
}



/* 通过 volatile 写入清除敏感内存，避免普通 memset 被死存储优化删除。 */
XRT_API void xrtSecureZero(ptr pData, size_t iSize)
{
	volatile uint8* pWrite = (volatile uint8*)pData;

	if ( iSize == 0 ) {
		return;
	}
	if ( pWrite == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}
	while ( iSize != 0 ) {
		*pWrite++ = 0;
		iSize--;
	}
}



#if defined(XRT_FEATURE_MEMORY_DEBUG)
/* 记录分配调用位置。 */
XRT_API ptr xrtMallocAt(size_t iSize, cstr sFile, uint32 iLine)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_MALLOC, iSize);
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	return __xrtHeapAlloc(iSize, false, sFile, iLine);
}



/* 记录清零分配调用位置。 */
XRT_API ptr xrtCallocAt(size_t iCount, size_t iSize, cstr sFile, uint32 iLine)
{
	if ( (iCount != 0) && (iSize > (SIZE_MAX / iCount)) ) {
		__xrtMemStatsRecord(XRT_MEM_STATS_OP_CALLOC, SIZE_MAX);
		__xrtErrorSetSizeOverflow();
		return NULL;
	}

	__xrtMemStatsRecord(XRT_MEM_STATS_OP_CALLOC, iCount * iSize);
	if ( __xrtMemDebugShouldFailAlloc() ) {
		return NULL;
	}
	return __xrtHeapAlloc(iCount * iSize, true, sFile, iLine);
}



/* 记录重分配调用位置。 */
XRT_API ptr xrtReallocAt(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_REALLOC, iSize);
	return __xrtHeapRealloc(pMemory, iSize, sFile, iLine);
}



/* 记录释放调用位置。 */
XRT_API void xrtFreeAt(ptr pMemory, cstr sFile, uint32 iLine)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_FREE, 0);
	(void)__xrtHeapFree(pMemory, sFile, iLine);
}



/* 记录内存复制分配调用位置。 */
XRT_API ptr xrtMemDupAt(const void* pData, size_t iSize, cstr sFile, uint32 iLine)
{
	__xrtMemStatsRecord(XRT_MEM_STATS_OP_MEMDUP, iSize);
	return __xrtHeapMemDup(pData, iSize, sFile, iLine);
}
#endif
