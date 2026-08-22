#include "../internal/xrt_memory.h"



#if defined(XRT_FEATURE_MEMORY_DEBUG)

/* 调试块状态同时记录生命周期和是否进入统计体系。 */
#define XRT_MEMDEBUG_STATE_MASK			0x00FFu
#define XRT_MEMDEBUG_STATE_LIVE			0x0001u
#define XRT_MEMDEBUG_STATE_FREED		0x0002u
#define XRT_MEMDEBUG_STATE_QUARANTINE	0x0003u
#define XRT_MEMDEBUG_STATE_TRACKED		0x0100u



/* 有界历史避免调试功能自身无限占用内存。 */
#define XRT_MEMDEBUG_QUARANTINE_LIMIT	256u



/* 活动分配按地址散列，避免批量释放退化为平方复杂度。 */
#define XRT_MEMDEBUG_LIVE_BUCKET_COUNT	4096u



/* 边界值混入地址和大小，降低固定字节被误判为有效的概率。 */
#define XRT_MEMDEBUG_FRONT_CANARY	0xC35A91E7u
#define XRT_MEMDEBUG_TAIL_CANARY		0x7E19A53Cu
#define XRT_MEMDEBUG_ALLOC_FILL		0xCD
#define XRT_MEMDEBUG_FREE_FILL		0xDD



/* 内存调试状态只依赖底层分配器和内部短锁。 */
typedef struct xrt_memdebug_state {
	volatile int32 InitState;
	xrt_spinlock Lock;
	bool Enabled;
	size_t ActiveCount;
	xrt_heap_header* LiveBuckets[XRT_MEMDEBUG_LIVE_BUCKET_COUNT];
	xrt_heap_header* QuarantineHead;
	xrt_heap_header* QuarantineTail;
	size_t LiveCount;
	size_t LiveBytes;
	size_t PeakCount;
	size_t PeakBytes;
	size_t QuarantineCount;
	size_t QuarantineBytes;
	uint64 AllocCount;
	uint64 FreeCount;
	uint64 ReallocCount;
	uint64 DoubleFreeCount;
	uint64 InvalidFreeCount;
	uint64 OverflowCount;
	uint64 UnderflowCount;
	uint64 UseAfterFreeCount;
	size_t TempActiveBytes;
	size_t TempCurrentBytes;
	size_t TempPeakBytes;
	uint64 TempResetCount;
	uint64 NextSequence;
	xmemdebugevent Events[XRT_MEMDEBUG_EVENT_LIMIT];
	size_t EventStart;
	size_t EventCount;
} xrt_memdebug_state;



static xrt_memdebug_state __xrtMemDebug;



/* 故障注入只影响当前线程，避免并发测试互相污染。 */
typedef struct xrt_memdebug_fail_state {
	uint64 Remaining;
	bool Armed;
	bool Triggered;
} xrt_memdebug_fail_state;



#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))

static DWORD __xrtMemDebugFailFls = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtMemDebugFailFlsState;



/* 在线程或 fiber 退出时释放 TinyCC 故障状态。 */
static void NTAPI __xrtMemDebugFailLocalFree(PVOID pData)
{
	free(pData);
}



/* 线程安全地创建 TinyCC Windows 故障状态槽。 */
static bool __xrtMemDebugFailFlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(
		&__xrtMemDebugFailFlsState,
		1,
		0
	);

	if ( iState == 0 ) {
		__xrtMemDebugFailFls = FlsAlloc(
			__xrtMemDebugFailLocalFree
		);
		(void)InterlockedExchange(
			&__xrtMemDebugFailFlsState,
			__xrtMemDebugFailFls == FLS_OUT_OF_INDEXES ? 3 : 2
		);
		return __xrtMemDebugFailFls != FLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(
		&__xrtMemDebugFailFlsState,
		0,
		0
	)) == 1 ) {
		(void)SwitchToThread();
	}
	return iState == 2;
}



/* 取得或按需创建 TinyCC Windows 当前线程的故障状态。 */
static xrt_memdebug_fail_state* __xrtMemDebugFailStateGet(
	bool bCreate
)
{
	xrt_memdebug_fail_state* pState;

	if ( !__xrtMemDebugFailFlsEnsure() ) {
		if ( bCreate ) {
			__xrtErrorSetInvalidState();
		}
		return NULL;
	}
	pState = (xrt_memdebug_fail_state*)FlsGetValue(
		__xrtMemDebugFailFls
	);
	if ( (pState != NULL) || !bCreate ) {
		return pState;
	}
	pState = (xrt_memdebug_fail_state*)calloc(
		1,
		sizeof(*pState)
	);
	if ( pState == NULL ) {
		__xrtErrorSetOutOfMemory();
		return NULL;
	}
	if ( !FlsSetValue(__xrtMemDebugFailFls, pState) ) {
		free(pState);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pState;
}

#elif defined(__TINYC__)

static pthread_key_t __xrtMemDebugFailKey;
static pthread_once_t __xrtMemDebugFailKeyOnce =
	PTHREAD_ONCE_INIT;
static int __xrtMemDebugFailKeyError;



/* 在线程退出时释放 TinyCC POSIX 故障状态。 */
static void __xrtMemDebugFailLocalFree(void* pData)
{
	free(pData);
}



/* 创建 TinyCC POSIX 故障状态 key。 */
static void __xrtMemDebugFailKeyInit(void)
{
	__xrtMemDebugFailKeyError = pthread_key_create(
		&__xrtMemDebugFailKey,
		__xrtMemDebugFailLocalFree
	);
}



/* 取得或按需创建 TinyCC POSIX 当前线程的故障状态。 */
static xrt_memdebug_fail_state* __xrtMemDebugFailStateGet(
	bool bCreate
)
{
	xrt_memdebug_fail_state* pState;

	(void)pthread_once(
		&__xrtMemDebugFailKeyOnce,
		__xrtMemDebugFailKeyInit
	);
	if ( __xrtMemDebugFailKeyError != 0 ) {
		if ( bCreate ) {
			__xrtErrorSetInvalidState();
		}
		return NULL;
	}
	pState = (xrt_memdebug_fail_state*)pthread_getspecific(
		__xrtMemDebugFailKey
	);
	if ( (pState != NULL) || !bCreate ) {
		return pState;
	}
	pState = (xrt_memdebug_fail_state*)calloc(
		1,
		sizeof(*pState)
	);
	if ( pState == NULL ) {
		__xrtErrorSetOutOfMemory();
		return NULL;
	}
	if ( pthread_setspecific(
		__xrtMemDebugFailKey,
		pState
	) != 0 ) {
		free(pState);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pState;
}

#else

static XRT_THREAD_LOCAL xrt_memdebug_fail_state
	__xrtMemDebugFailState;



/* 返回编译器 TLS 中的当前线程故障状态。 */
static xrt_memdebug_fail_state* __xrtMemDebugFailStateGet(
	bool bCreate
)
{
	(void)bCreate;
	return &__xrtMemDebugFailState;
}

#endif

#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
static pthread_once_t __xrtMemDebugOnce = PTHREAD_ONCE_INIT;
#endif



/* 初始化调试锁并默认开启调试记录。 */
static void __xrtMemDebugInit(void)
{
	__xrtSpinInit(&__xrtMemDebug.Lock);
	__xrtMemDebug.Enabled = true;
}



/* 线程安全地完成内存调试状态初始化。 */
static void __xrtMemDebugEnsure(void)
{
	#if defined(__TINYC__) && !defined(_WIN32) && !defined(_WIN64)
		(void)pthread_once(&__xrtMemDebugOnce, __xrtMemDebugInit);
	#else
		int32 iState;

		#if defined(_MSC_VER)
			iState = (int32)_InterlockedCompareExchange((volatile long*)&__xrtMemDebug.InitState, 1, 0);
		#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
			iState = (int32)InterlockedCompareExchange((volatile LONG*)&__xrtMemDebug.InitState, 1, 0);
		#else
			iState = (int32)__sync_val_compare_and_swap(&__xrtMemDebug.InitState, 0, 1);
		#endif

		if ( iState == 0 ) {
			__xrtMemDebugInit();
			#if defined(_MSC_VER)
				(void)_InterlockedExchange((volatile long*)&__xrtMemDebug.InitState, 2);
			#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
				(void)InterlockedExchange((volatile LONG*)&__xrtMemDebug.InitState, 2);
			#else
				(void)__sync_lock_test_and_set(&__xrtMemDebug.InitState, 2);
			#endif
			return;
		}

		while ( __xrtAtomicRefLoad(&__xrtMemDebug.InitState) != 2 ) {
			#if defined(_WIN32) || defined(_WIN64)
				(void)SwitchToThread();
			#else
				(void)sched_yield();
			#endif
		}
	#endif
}



/* 在当前线程的逻辑分配边界消费一次故障计数。 */
bool __xrtMemDebugShouldFailAlloc(void)
{
	xrt_memdebug_fail_state* pState =
		__xrtMemDebugFailStateGet(false);

	if ( (pState == NULL) || !pState->Armed ) {
		return false;
	}
	if ( pState->Remaining != 0 ) {
		pState->Remaining--;
		return false;
	}
	pState->Armed = false;
	pState->Triggered = true;
	__xrtErrorSetOutOfMemory();
	return true;
}



/* 返回紧邻用户内存之前的前边界值位置。 */
static uint32* __xrtMemDebugFrontPtr(ptr pMemory)
{
	return (uint32*)((unsigned char*)pMemory - sizeof(uint32));
}



/* 计算与当前地址关联的前边界值。 */
static uint32 __xrtMemDebugFrontValue(ptr pMemory)
{
	uint64 iAddress = (uint64)(uintptr_t)pMemory;

	return XRT_MEMDEBUG_FRONT_CANARY ^ (uint32)iAddress ^ (uint32)(iAddress >> 32);
}



/* 计算与当前地址和请求大小关联的尾边界值。 */
static uint32 __xrtMemDebugTailValue(ptr pMemory, size_t iSize)
{
	uint64 iAddress = (uint64)(uintptr_t)pMemory;
	uint64 iLength = (uint64)iSize;

	return XRT_MEMDEBUG_TAIL_CANARY ^ (uint32)iAddress ^ (uint32)(iAddress >> 32) ^
		(uint32)iLength ^ (uint32)(iLength >> 32);
}



/* 写入可能未对齐的前后边界值。 */
static void __xrtMemDebugWriteCanary(xrt_heap_header* pHeader, ptr pMemory)
{
	uint32 iFront = __xrtMemDebugFrontValue(pMemory);
	uint32 iTail = __xrtMemDebugTailValue(pMemory, pHeader->Size);

	memcpy(__xrtMemDebugFrontPtr(pMemory), &iFront, sizeof(iFront));
	memcpy((unsigned char*)pMemory + pHeader->Size, &iTail, sizeof(iTail));
}



/* 检查前边界值是否仍然完整。 */
static bool __xrtMemDebugFrontValid(ptr pMemory)
{
	uint32 iStored;

	memcpy(&iStored, __xrtMemDebugFrontPtr(pMemory), sizeof(iStored));
	return iStored == __xrtMemDebugFrontValue(pMemory);
}



/* 检查尾边界值是否仍然完整。 */
static bool __xrtMemDebugTailValid(const xrt_heap_header* pHeader, ptr pMemory)
{
	uint32 iStored;

	memcpy(&iStored, (unsigned char*)pMemory + pHeader->Size, sizeof(iStored));
	return iStored == __xrtMemDebugTailValue(pMemory, pHeader->Size);
}



/* 在持锁状态下追加一个有界事件。 */
static void __xrtMemDebugRecord(xmemdebugeventkind Kind, ptr pAddress, size_t iSize, cstr sFile, uint32 iLine)
{
	size_t iIndex;
	xmemdebugevent* pEvent;

	if ( __xrtMemDebug.EventCount < XRT_MEMDEBUG_EVENT_LIMIT ) {
		iIndex = (__xrtMemDebug.EventStart + __xrtMemDebug.EventCount) % XRT_MEMDEBUG_EVENT_LIMIT;
		__xrtMemDebug.EventCount++;
	} else {
		iIndex = __xrtMemDebug.EventStart;
		__xrtMemDebug.EventStart = (__xrtMemDebug.EventStart + 1) % XRT_MEMDEBUG_EVENT_LIMIT;
	}

	pEvent = &__xrtMemDebug.Events[iIndex];
	pEvent->Kind = Kind;
	pEvent->Sequence = ++__xrtMemDebug.NextSequence;
	pEvent->Address = pAddress;
	pEvent->Size = iSize;
	pEvent->File = sFile;
	pEvent->Line = iLine;
}



/* 返回公开事件类型的稳定诊断名称。 */
XRT_API cstr xrtMemDebugEventName(xmemdebugeventkind Kind)
{
	switch ( Kind ) {
		case XMEMDEBUG_ALLOC:
			return "alloc";
		case XMEMDEBUG_FREE:
			return "free";
		case XMEMDEBUG_REALLOC:
			return "realloc";
		case XMEMDEBUG_DOUBLE_FREE:
			return "double_free";
		case XMEMDEBUG_INVALID_FREE:
			return "invalid_free";
		case XMEMDEBUG_OVERFLOW:
			return "overflow";
		case XMEMDEBUG_UNDERFLOW:
			return "underflow";
		case XMEMDEBUG_USE_AFTER_FREE:
			return "use_after_free";
		case XMEMDEBUG_TEMP_ALLOC:
			return "temp_alloc";
		case XMEMDEBUG_TEMP_REWIND:
			return "temp_rewind";
		case XMEMDEBUG_TEMP_RESET:
			return "temp_reset";
		default:
			return "unknown";
	}
}



/* 返回活动分配所在的固定哈希桶。 */
static size_t __xrtMemDebugLiveBucket(ptr pMemory)
{
	uintptr_t iValue = (uintptr_t)pMemory;

	iValue >>= 4;
	iValue ^= iValue >> 11;
	iValue *= (uintptr_t)0x9E3779B1u;
	iValue ^= iValue >> 16;
	return (size_t)iValue & (XRT_MEMDEBUG_LIVE_BUCKET_COUNT - 1u);
}



/* 在持锁状态下将分配块接入活动哈希桶。 */
static void __xrtMemDebugAttachLive(xrt_heap_header* pHeader)
{
	ptr pMemory = (unsigned char*)pHeader + __xrtHeapHeaderSize();
	size_t iBucket = __xrtMemDebugLiveBucket(pMemory);

	pHeader->DebugPrev = NULL;
	pHeader->DebugNext = __xrtMemDebug.LiveBuckets[iBucket];
	if ( __xrtMemDebug.LiveBuckets[iBucket] != NULL ) {
		__xrtMemDebug.LiveBuckets[iBucket]->DebugPrev = pHeader;
	}
	__xrtMemDebug.LiveBuckets[iBucket] = pHeader;
}



/* 在持锁状态下将分配块移出活动哈希桶。 */
static void __xrtMemDebugDetachLive(xrt_heap_header* pHeader)
{
	ptr pMemory = (unsigned char*)pHeader + __xrtHeapHeaderSize();
	size_t iBucket = __xrtMemDebugLiveBucket(pMemory);

	if ( pHeader->DebugPrev != NULL ) {
		pHeader->DebugPrev->DebugNext = pHeader->DebugNext;
	} else {
		__xrtMemDebug.LiveBuckets[iBucket] = pHeader->DebugNext;
	}
	if ( pHeader->DebugNext != NULL ) {
		pHeader->DebugNext->DebugPrev = pHeader->DebugPrev;
	}
	pHeader->DebugPrev = NULL;
	pHeader->DebugNext = NULL;
}



/* 在持锁状态下记录边界损坏。 */
static bool __xrtMemDebugCheckCanary(xrt_heap_header* pHeader, ptr pMemory, cstr sFile, uint32 iLine)
{
	bool bValid = true;

	if ( !__xrtMemDebugFrontValid(pMemory) ) {
		__xrtMemDebug.UnderflowCount++;
		__xrtMemDebugRecord(XMEMDEBUG_UNDERFLOW, pMemory, pHeader->Size, sFile, iLine);
		bValid = false;
	}
	if ( !__xrtMemDebugTailValid(pHeader, pMemory) ) {
		__xrtMemDebug.OverflowCount++;
		__xrtMemDebugRecord(XMEMDEBUG_OVERFLOW, pMemory, pHeader->Size, sFile, iLine);
		bValid = false;
	}

	return bValid;
}



/* 清空统计字段，调用者必须持有调试锁。 */
static void __xrtMemDebugClearStats(void)
{
	__xrtMemDebug.LiveCount = 0;
	__xrtMemDebug.LiveBytes = 0;
	__xrtMemDebug.PeakCount = 0;
	__xrtMemDebug.PeakBytes = 0;
	__xrtMemDebug.QuarantineCount = 0;
	__xrtMemDebug.QuarantineBytes = 0;
	__xrtMemDebug.AllocCount = 0;
	__xrtMemDebug.FreeCount = 0;
	__xrtMemDebug.ReallocCount = 0;
	__xrtMemDebug.DoubleFreeCount = 0;
	__xrtMemDebug.InvalidFreeCount = 0;
	__xrtMemDebug.OverflowCount = 0;
	__xrtMemDebug.UnderflowCount = 0;
	__xrtMemDebug.UseAfterFreeCount = 0;
	__xrtMemDebug.TempCurrentBytes = 0;
	__xrtMemDebug.TempPeakBytes = 0;
	__xrtMemDebug.TempResetCount = 0;
	__xrtMemDebug.NextSequence = 0;
	__xrtMemDebug.EventStart = 0;
	__xrtMemDebug.EventCount = 0;
	memset(__xrtMemDebug.Events, 0, sizeof(__xrtMemDebug.Events));
}



/* 返回调试尾部边界需要的额外字节。 */
size_t __xrtMemDebugTailSize(void)
{
	return sizeof(uint32);
}



/* 写入边界并登记一个新分配。 */
void __xrtMemDebugAlloc(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine)
{
	bool bTracked;

	(void)iCapacity;
	__xrtMemDebugEnsure();
	__xrtMemDebugWriteCanary(pHeader, pMemory);

	__xrtSpinLock(&__xrtMemDebug.Lock);
	bTracked = __xrtMemDebug.Enabled;
	pHeader->DebugState = XRT_MEMDEBUG_STATE_LIVE | (bTracked ? XRT_MEMDEBUG_STATE_TRACKED : 0);
	pHeader->AllocFile = sFile;
	pHeader->AllocLine = iLine;
	__xrtMemDebug.ActiveCount++;
	__xrtMemDebugAttachLive(pHeader);
	if ( bTracked ) {
		__xrtMemDebug.LiveCount++;
		__xrtMemDebug.LiveBytes += pHeader->Size;
		__xrtMemDebug.AllocCount++;
		if ( __xrtMemDebug.LiveCount > __xrtMemDebug.PeakCount ) {
			__xrtMemDebug.PeakCount = __xrtMemDebug.LiveCount;
		}
		if ( __xrtMemDebug.LiveBytes > __xrtMemDebug.PeakBytes ) {
			__xrtMemDebug.PeakBytes = __xrtMemDebug.LiveBytes;
		}
		__xrtMemDebugRecord(XMEMDEBUG_ALLOC, pMemory, pHeader->Size, sFile, iLine);
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 在池化块复用前检查释放后的填充值。 */
void __xrtMemDebugReuse(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine)
{
	unsigned char* pBytes = (unsigned char*)pMemory;
	bool bChanged = false;

	__xrtMemDebugEnsure();
	if ( ((pHeader->DebugState & XRT_MEMDEBUG_STATE_MASK) != XRT_MEMDEBUG_STATE_FREED) ||
		 ((pHeader->DebugState & XRT_MEMDEBUG_STATE_TRACKED) == 0) ) {
		return;
	}
	for ( size_t i = sizeof(ptr); i < iCapacity; i++ ) {
		if ( pBytes[i] != XRT_MEMDEBUG_FREE_FILL ) {
			bChanged = true;
			break;
		}
	}
	if ( !bChanged ) {
		return;
	}

	__xrtSpinLock(&__xrtMemDebug.Lock);
	__xrtMemDebug.UseAfterFreeCount++;
	__xrtMemDebugRecord(XMEMDEBUG_USE_AFTER_FREE, pMemory, pHeader->Size, sFile, iLine);
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 验证释放并按块来源选择池化回收或隔离。 */
int __xrtMemDebugFree(xrt_heap_header* pHeader, ptr pMemory, size_t iCapacity, cstr sFile, uint32 iLine)
{
	uint32 iState;
	bool bTracked;
	bool bCanaryValid = true;
	xrt_heap_header* pRelease = NULL;

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	iState = pHeader->DebugState & XRT_MEMDEBUG_STATE_MASK;
	bTracked = (pHeader->DebugState & XRT_MEMDEBUG_STATE_TRACKED) != 0;
	if ( iState != XRT_MEMDEBUG_STATE_LIVE ) {
		if ( bTracked || __xrtMemDebug.Enabled ) {
			__xrtMemDebug.DoubleFreeCount++;
			__xrtMemDebugRecord(XMEMDEBUG_DOUBLE_FREE, pMemory, pHeader->Size, sFile, iLine);
		}
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		__xrtErrorSetInvalidState();
		return XRT_MEMDEBUG_FREE_INVALID;
	}

	if ( bTracked ) {
		bCanaryValid = __xrtMemDebugCheckCanary(pHeader, pMemory, sFile, iLine);
		if ( __xrtMemDebug.LiveCount != 0 ) {
			__xrtMemDebug.LiveCount--;
		}
		if ( __xrtMemDebug.LiveBytes >= pHeader->Size ) {
			__xrtMemDebug.LiveBytes -= pHeader->Size;
		} else {
			__xrtMemDebug.LiveBytes = 0;
		}
		__xrtMemDebug.FreeCount++;
		__xrtMemDebugRecord(XMEMDEBUG_FREE, pMemory, pHeader->Size, sFile, iLine);
	}
	__xrtMemDebugDetachLive(pHeader);
	if ( __xrtMemDebug.ActiveCount != 0 ) {
		__xrtMemDebug.ActiveCount--;
	}
	pHeader->FreeFile = sFile;
	pHeader->FreeLine = iLine;
	memset(pMemory, XRT_MEMDEBUG_FREE_FILL, iCapacity);

	if ( (pHeader->Flags == XRT_HEAP_FLAG_BACKING) && bTracked ) {
		pHeader->DebugState = XRT_MEMDEBUG_STATE_QUARANTINE | XRT_MEMDEBUG_STATE_TRACKED;
		pHeader->DebugPrev = __xrtMemDebug.QuarantineTail;
		pHeader->DebugNext = NULL;
		if ( __xrtMemDebug.QuarantineTail != NULL ) {
			__xrtMemDebug.QuarantineTail->DebugNext = pHeader;
		} else {
			__xrtMemDebug.QuarantineHead = pHeader;
		}
		__xrtMemDebug.QuarantineTail = pHeader;
		__xrtMemDebug.QuarantineCount++;
		__xrtMemDebug.QuarantineBytes += pHeader->Size;

		if ( __xrtMemDebug.QuarantineCount > XRT_MEMDEBUG_QUARANTINE_LIMIT ) {
			pRelease = __xrtMemDebug.QuarantineHead;
			__xrtMemDebug.QuarantineHead = pRelease->DebugNext;
			if ( __xrtMemDebug.QuarantineHead != NULL ) {
				__xrtMemDebug.QuarantineHead->DebugPrev = NULL;
			} else {
				__xrtMemDebug.QuarantineTail = NULL;
			}
			__xrtMemDebug.QuarantineCount--;
			__xrtMemDebug.QuarantineBytes -= pRelease->Size;
			pRelease->DebugPrev = NULL;
			pRelease->DebugNext = NULL;
		}
		__xrtSpinUnlock(&__xrtMemDebug.Lock);

		if ( pRelease != NULL ) {
			ptr pAllocation = pRelease->Allocation;

			pRelease->Magic = 0;
			__xrtBackingFree(pAllocation);
		}
		if ( !bCanaryValid ) {
			__xrtErrorSetInvalidState();
		}
		return XRT_MEMDEBUG_FREE_CONSUMED;
	}

	pHeader->DebugState = XRT_MEMDEBUG_STATE_FREED | (bTracked ? XRT_MEMDEBUG_STATE_TRACKED : 0);
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
	if ( !bCanaryValid ) {
		__xrtErrorSetInvalidState();
	}
	return XRT_MEMDEBUG_FREE_RECLAIM;
}



/* 验证重分配前的生命周期和边界。 */
bool __xrtMemDebugCheck(xrt_heap_header* pHeader, ptr pMemory, cstr sFile, uint32 iLine)
{
	bool bTracked;
	bool bValid;

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	bTracked = (pHeader->DebugState & XRT_MEMDEBUG_STATE_TRACKED) != 0;
	if ( (pHeader->DebugState & XRT_MEMDEBUG_STATE_MASK) != XRT_MEMDEBUG_STATE_LIVE ) {
		if ( bTracked || __xrtMemDebug.Enabled ) {
			__xrtMemDebug.UseAfterFreeCount++;
			__xrtMemDebugRecord(XMEMDEBUG_USE_AFTER_FREE, pMemory, pHeader->Size, sFile, iLine);
		}
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	bValid = !bTracked || __xrtMemDebugCheckCanary(pHeader, pMemory, sFile, iLine);
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
	if ( !bValid ) {
		__xrtErrorSetInvalidState();
	}

	return bValid;
}



/* 记录同一块内存上的重分配并重写尾边界。 */
void __xrtMemDebugResize(xrt_heap_header* pHeader, ptr pMemory, size_t iOldSize, cstr sFile, uint32 iLine)
{
	bool bTracked;

	__xrtMemDebugEnsure();
	__xrtMemDebugWriteCanary(pHeader, pMemory);
	__xrtSpinLock(&__xrtMemDebug.Lock);
	bTracked = (pHeader->DebugState & XRT_MEMDEBUG_STATE_TRACKED) != 0;
	if ( bTracked ) {
		if ( __xrtMemDebug.LiveBytes >= iOldSize ) {
			__xrtMemDebug.LiveBytes -= iOldSize;
		} else {
			__xrtMemDebug.LiveBytes = 0;
		}
		__xrtMemDebug.LiveBytes += pHeader->Size;
		if ( __xrtMemDebug.LiveBytes > __xrtMemDebug.PeakBytes ) {
			__xrtMemDebug.PeakBytes = __xrtMemDebug.LiveBytes;
		}
		__xrtMemDebug.ReallocCount++;
		__xrtMemDebugRecord(XMEMDEBUG_REALLOC, pMemory, pHeader->Size, sFile, iLine);
		pHeader->AllocFile = sFile;
		pHeader->AllocLine = iLine;
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 记录已经完成的跨块重分配。 */
void __xrtMemDebugRealloc(ptr pMemory, size_t iSize, cstr sFile, uint32 iLine)
{
	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( __xrtMemDebug.Enabled ) {
		__xrtMemDebug.ReallocCount++;
		__xrtMemDebugRecord(XMEMDEBUG_REALLOC, pMemory, iSize, sFile, iLine);
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 记录无法识别的释放请求。 */
void __xrtMemDebugInvalidFree(ptr pMemory, cstr sFile, uint32 iLine)
{
	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( __xrtMemDebug.Enabled ) {
		__xrtMemDebug.InvalidFreeCount++;
		__xrtMemDebugRecord(XMEMDEBUG_INVALID_FREE, pMemory, 0, sFile, iLine);
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 从活动哈希桶和大块隔离队列中安全查找块头。 */
bool __xrtMemDebugFindHeader(ptr pMemory, xrt_heap_header** ppHeader)
{
	xrt_heap_header* pHeader;
	size_t iBucket;
	bool bFound = false;

	if ( (pMemory == NULL) || (ppHeader == NULL) ) {
		return false;
	}

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	iBucket = __xrtMemDebugLiveBucket(pMemory);
	pHeader = __xrtMemDebug.LiveBuckets[iBucket];
	while ( pHeader != NULL ) {
		ptr pUser = (unsigned char*)pHeader + __xrtHeapHeaderSize();

		if ( pUser == pMemory ) {
			*ppHeader = pHeader;
			bFound = true;
			break;
		}
		pHeader = pHeader->DebugNext;
	}
	if ( !bFound ) {
		pHeader = __xrtMemDebug.QuarantineHead;
		while ( pHeader != NULL ) {
			ptr pUser = (unsigned char*)pHeader + __xrtHeapHeaderSize();

			if ( pUser == pMemory ) {
				*ppHeader = pHeader;
				bFound = true;
				break;
			}
			pHeader = pHeader->DebugNext;
		}
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
	return bFound;
}



/* 记录临时 arena 的一次成功分配。 */
void __xrtMemDebugTempAlloc(size_t iSize, cstr sFile, uint32 iLine)
{
	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( iSize <= (SIZE_MAX - __xrtMemDebug.TempActiveBytes) ) {
		__xrtMemDebug.TempActiveBytes += iSize;
	} else {
		__xrtMemDebug.TempActiveBytes = SIZE_MAX;
	}
	if ( __xrtMemDebug.Enabled ) {
		if ( iSize <= (SIZE_MAX - __xrtMemDebug.TempCurrentBytes) ) {
			__xrtMemDebug.TempCurrentBytes += iSize;
		} else {
			__xrtMemDebug.TempCurrentBytes = SIZE_MAX;
		}
		if ( __xrtMemDebug.TempCurrentBytes > __xrtMemDebug.TempPeakBytes ) {
			__xrtMemDebug.TempPeakBytes = __xrtMemDebug.TempCurrentBytes;
		}
		__xrtMemDebugRecord(XMEMDEBUG_TEMP_ALLOC, NULL, iSize, sFile, iLine);
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 记录临时 arena 的作用域回退或整体重置。 */
void __xrtMemDebugTempRelease(size_t iSize, xmemdebugeventkind Kind, cstr sFile, uint32 iLine)
{
	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( __xrtMemDebug.TempActiveBytes >= iSize ) {
		__xrtMemDebug.TempActiveBytes -= iSize;
	} else {
		__xrtMemDebug.TempActiveBytes = 0;
	}
	if ( __xrtMemDebug.Enabled ) {
		if ( __xrtMemDebug.TempCurrentBytes >= iSize ) {
			__xrtMemDebug.TempCurrentBytes -= iSize;
		} else {
			__xrtMemDebug.TempCurrentBytes = 0;
		}
		if ( Kind == XMEMDEBUG_TEMP_RESET ) {
			__xrtMemDebug.TempResetCount++;
		}
		__xrtMemDebugRecord(Kind, NULL, iSize, sFile, iLine);
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 在没有活动分配时切换运行时调试记录。 */
XRT_API bool xrtMemDebugEnable(bool bEnable)
{
	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( (__xrtMemDebug.ActiveCount != 0) || (__xrtMemDebug.TempActiveBytes != 0) ) {
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	__xrtMemDebug.Enabled = bEnable;
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
	return true;
}



/* 返回运行时调试记录开关。 */
XRT_API bool xrtMemDebugEnabled(void)
{
	bool bEnabled;

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	bEnabled = __xrtMemDebug.Enabled;
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
	return bEnabled;
}



/* 配置当前线程的一次性逻辑分配故障。 */
XRT_API bool xrtMemDebugFailAfter(uint64 iSuccessfulAllocations)
{
	xrt_memdebug_fail_state* pState =
		__xrtMemDebugFailStateGet(true);

	if ( pState == NULL ) {
		return false;
	}
	pState->Remaining = iSuccessfulAllocations;
	pState->Armed = true;
	pState->Triggered = false;
	return true;
}



/* 清除当前线程的一次性逻辑分配故障。 */
XRT_API void xrtMemDebugFailClear(void)
{
	xrt_memdebug_fail_state* pState =
		__xrtMemDebugFailStateGet(false);

	if ( pState != NULL ) {
		memset(pState, 0, sizeof(*pState));
	}
}



/* 查询当前线程最近一次故障配置是否已经触发。 */
XRT_API bool xrtMemDebugFailTriggered(void)
{
	xrt_memdebug_fail_state* pState =
		__xrtMemDebugFailStateGet(false);

	return (pState != NULL) && pState->Triggered;
}



/* 清空统计并释放大块隔离队列。 */
XRT_API bool xrtMemDebugReset(void)
{
	xrt_heap_header* pRelease;

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	if ( (__xrtMemDebug.ActiveCount != 0) || (__xrtMemDebug.TempActiveBytes != 0) ) {
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		__xrtErrorSetInvalidState();
		return false;
	}
	pRelease = __xrtMemDebug.QuarantineHead;
	memset(
		__xrtMemDebug.LiveBuckets,
		0,
		sizeof(__xrtMemDebug.LiveBuckets)
	);
	__xrtMemDebug.QuarantineHead = NULL;
	__xrtMemDebug.QuarantineTail = NULL;
	__xrtMemDebugClearStats();
	__xrtSpinUnlock(&__xrtMemDebug.Lock);

	while ( pRelease != NULL ) {
		xrt_heap_header* pNext = pRelease->DebugNext;
		ptr pAllocation = pRelease->Allocation;

		pRelease->Magic = 0;
		__xrtBackingFree(pAllocation);
		pRelease = pNext;
	}
	return true;
}



/* 复制一份一致的统计快照。 */
XRT_API void xrtMemDebugSnapshot(xmemdebugsnapshot* pSnapshot)
{
	if ( pSnapshot == NULL ) {
		__xrtErrorSetInvalidArgument();
		return;
	}

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	pSnapshot->Enabled = __xrtMemDebug.Enabled;
	pSnapshot->LiveCount = __xrtMemDebug.LiveCount;
	pSnapshot->LiveBytes = __xrtMemDebug.LiveBytes;
	pSnapshot->PeakCount = __xrtMemDebug.PeakCount;
	pSnapshot->PeakBytes = __xrtMemDebug.PeakBytes;
	pSnapshot->QuarantineCount = __xrtMemDebug.QuarantineCount;
	pSnapshot->QuarantineBytes = __xrtMemDebug.QuarantineBytes;
	pSnapshot->AllocCount = __xrtMemDebug.AllocCount;
	pSnapshot->FreeCount = __xrtMemDebug.FreeCount;
	pSnapshot->ReallocCount = __xrtMemDebug.ReallocCount;
	pSnapshot->DoubleFreeCount = __xrtMemDebug.DoubleFreeCount;
	pSnapshot->InvalidFreeCount = __xrtMemDebug.InvalidFreeCount;
	pSnapshot->OverflowCount = __xrtMemDebug.OverflowCount;
	pSnapshot->UnderflowCount = __xrtMemDebug.UnderflowCount;
	pSnapshot->UseAfterFreeCount = __xrtMemDebug.UseAfterFreeCount;
	pSnapshot->TempCurrentBytes = __xrtMemDebug.TempCurrentBytes;
	pSnapshot->TempPeakBytes = __xrtMemDebug.TempPeakBytes;
	pSnapshot->TempResetCount = __xrtMemDebug.TempResetCount;
	pSnapshot->EventCount = __xrtMemDebug.EventCount;
	__xrtSpinUnlock(&__xrtMemDebug.Lock);
}



/* 复制有界事件后在锁外调用用户访问器。 */
XRT_API size_t xrtMemDebugVisit(xmemdebugvisitor pVisitor, ptr pUserData)
{
	xmemdebugevent arrEvents[XRT_MEMDEBUG_EVENT_LIMIT];
	size_t iCount;
	size_t iVisited = 0;

	if ( pVisitor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}

	__xrtMemDebugEnsure();
	__xrtSpinLock(&__xrtMemDebug.Lock);
	iCount = __xrtMemDebug.EventCount;
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iIndex = (__xrtMemDebug.EventStart + i) % XRT_MEMDEBUG_EVENT_LIMIT;

		arrEvents[i] = __xrtMemDebug.Events[iIndex];
	}
	__xrtSpinUnlock(&__xrtMemDebug.Lock);

	for ( size_t i = 0; i < iCount; i++ ) {
		iVisited++;
		if ( !pVisitor(&arrEvents[i], pUserData) ) {
			break;
		}
	}
	return iVisited;
}



/* 捕获一份完整活动分配快照。 */
bool __xrtMemDebugCaptureLive(xmemdebugallocation** ppAllocations, size_t* pCount)
{
	xmemdebugallocation* pAllocations = NULL;
	xrt_heap_header* pHeader;
	size_t iCapacity = 0;
	size_t iCount = 0;
	size_t iBucket;

	if ( (ppAllocations == NULL) || (pCount == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*ppAllocations = NULL;
	*pCount = 0;

	__xrtMemDebugEnsure();
	for ( ;; ) {
		size_t iRequired;

		__xrtSpinLock(&__xrtMemDebug.Lock);
		iRequired = __xrtMemDebug.LiveCount;
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		if ( iRequired == 0 ) {
			__xrtBackingFree(pAllocations);
			return true;
		}
		if ( iRequired > iCapacity ) {
			if ( iRequired > (SIZE_MAX / sizeof(xmemdebugallocation)) ) {
				__xrtBackingFree(pAllocations);
				__xrtErrorSetSizeOverflow();
				return false;
			}
			__xrtBackingFree(pAllocations);
			pAllocations = (xmemdebugallocation*)__xrtBackingAlloc(
				iRequired * sizeof(xmemdebugallocation)
			);
			if ( pAllocations == NULL ) {
				__xrtErrorSetOutOfMemory();
				return false;
			}
			iCapacity = iRequired;
		}

		__xrtSpinLock(&__xrtMemDebug.Lock);
		if ( __xrtMemDebug.LiveCount > iCapacity ) {
			__xrtSpinUnlock(&__xrtMemDebug.Lock);
			continue;
		}
		iCount = 0;
		for ( iBucket = 0; iBucket < XRT_MEMDEBUG_LIVE_BUCKET_COUNT; iBucket++ ) {
			pHeader = __xrtMemDebug.LiveBuckets[iBucket];
			while ( pHeader != NULL ) {
				if ( (pHeader->DebugState & XRT_MEMDEBUG_STATE_TRACKED) != 0 ) {
					if ( iCount == iCapacity ) {
						__xrtSpinUnlock(&__xrtMemDebug.Lock);
						__xrtBackingFree(pAllocations);
						__xrtErrorSetInvalidState();
						return false;
					}
					pAllocations[iCount].Address =
						(unsigned char*)pHeader + __xrtHeapHeaderSize();
					pAllocations[iCount].Size = pHeader->Size;
					pAllocations[iCount].File = pHeader->AllocFile;
					pAllocations[iCount].Line = pHeader->AllocLine;
					iCount++;
				}
				pHeader = pHeader->DebugNext;
			}
		}
		__xrtSpinUnlock(&__xrtMemDebug.Lock);
		break;
	}

	*ppAllocations = pAllocations;
	*pCount = iCount;
	return true;
}



/* 复制活动分配后在锁外调用用户访问器。 */
XRT_API size_t xrtMemDebugVisitLive(xmemdebugallocationvisitor pVisitor, ptr pUserData)
{
	xmemdebugallocation* pAllocations;
	size_t iCount;
	size_t iVisited = 0;

	if ( pVisitor == NULL ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	if ( !__xrtMemDebugCaptureLive(&pAllocations, &iCount) ) {
		return 0;
	}

	for ( size_t i = 0; i < iCount; i++ ) {
		iVisited++;
		if ( !pVisitor(&pAllocations[i], pUserData) ) {
			break;
		}
	}
	__xrtBackingFree(pAllocations);
	return iVisited;
}

#endif
