#include "../internal/xrt_atomic.h"



#if defined(XRT_FEATURE_ATOMIC)

/* 检查原子对象地址满足操作宽度的自然对齐。 */
static bool __xrtAtomicAddressValid(const void* pAtomic, size_t iAlignment)
{
	if (
		(pAtomic == NULL) ||
		(((uintptr_t)pAtomic & (iAlignment - 1u)) != 0)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



#if defined(__TINYC__) && defined(_WIN32) && \
	(UINTPTR_MAX == UINT32_MAX)

#define XRT_ATOMIC64_FALLBACK_LOCKS 64u

/* TinyCC x86 不会把成员的 8 字节对齐传播给栈上外层结构，使用分片锁保护退化路径。 */
static volatile LONG __xrtAtomic64FallbackLocks[
	XRT_ATOMIC64_FALLBACK_LOCKS
];



/* 返回未对齐 64 位原子对象使用的稳定分片锁。 */
static volatile LONG* __xrtAtomic64FallbackLock(const xatomic64* pAtomic)
{
	uintptr_t iIndex = ((uintptr_t)pAtomic >> 2u) &
		(XRT_ATOMIC64_FALLBACK_LOCKS - 1u);

	return &__xrtAtomic64FallbackLocks[iIndex];
}



/* 获取 TinyCC x86 未对齐 64 位原子对象的分片锁。 */
static void __xrtAtomic64FallbackAcquire(volatile LONG* pLock)
{
	while ( InterlockedCompareExchange(pLock, 1, 0) != 0 ) {
		__xrtAtomicPause();
	}
}



/* 释放 TinyCC x86 未对齐 64 位原子对象的分片锁。 */
static void __xrtAtomic64FallbackRelease(volatile LONG* pLock)
{
	(void)InterlockedExchange(pLock, 0);
}



/* 在分片锁内读取可能仅有 4 字节对齐的 64 位值。 */
static uint64 __xrtAtomic64FallbackRead(const xatomic64* pAtomic)
{
	uint64 iValue;

	memcpy(&iValue, (const void*)&pAtomic->Value, sizeof(iValue));
	return iValue;
}



/* 在分片锁内写入可能仅有 4 字节对齐的 64 位值。 */
static void __xrtAtomic64FallbackWrite(xatomic64* pAtomic, uint64 iValue)
{
	memcpy((void*)&pAtomic->Value, &iValue, sizeof(iValue));
}



/* 使用强于请求顺序的分片锁读取未对齐 64 位原子值。 */
static uint64 __xrtAtomic64FallbackLoad(const xatomic64* pAtomic)
{
	volatile LONG* pLock = __xrtAtomic64FallbackLock(pAtomic);
	uint64 iValue;

	__xrtAtomic64FallbackAcquire(pLock);
	iValue = __xrtAtomic64FallbackRead(pAtomic);
	__xrtAtomic64FallbackRelease(pLock);
	return iValue;
}



/* 使用强于请求顺序的分片锁写入未对齐 64 位原子值。 */
static void __xrtAtomic64FallbackStore(xatomic64* pAtomic, uint64 iValue)
{
	volatile LONG* pLock = __xrtAtomic64FallbackLock(pAtomic);

	__xrtAtomic64FallbackAcquire(pLock);
	__xrtAtomic64FallbackWrite(pAtomic, iValue);
	__xrtAtomic64FallbackRelease(pLock);
}



/* 比较交换未对齐 64 位原子值并返回修改前的值。 */
static uint64 __xrtAtomic64FallbackCompare(
	xatomic64* pAtomic,
	uint64 iExpected,
	uint64 iDesired
)
{
	volatile LONG* pLock = __xrtAtomic64FallbackLock(pAtomic);
	uint64 iActual;

	__xrtAtomic64FallbackAcquire(pLock);
	iActual = __xrtAtomic64FallbackRead(pAtomic);
	if ( iActual == iExpected ) {
		__xrtAtomic64FallbackWrite(pAtomic, iDesired);
	}
	__xrtAtomic64FallbackRelease(pLock);
	return iActual;
}



/* 更新未对齐 64 位原子值并返回修改前的值。 */
static uint64 __xrtAtomic64FallbackUpdate(
	xatomic64* pAtomic,
	uint64 iValue,
	uint32 iOperation
)
{
	volatile LONG* pLock = __xrtAtomic64FallbackLock(pAtomic);
	uint64 iOld;
	uint64 iNew;

	__xrtAtomic64FallbackAcquire(pLock);
	iOld = __xrtAtomic64FallbackRead(pAtomic);
	switch ( iOperation ) {
		case 0u:
			iNew = iValue;
			break;
		case 1u:
			iNew = iOld + iValue;
			break;
		case 2u:
			iNew = iOld & iValue;
			break;
		case 3u:
			iNew = iOld | iValue;
			break;
		default:
			iNew = iOld ^ iValue;
			break;
	}
	__xrtAtomic64FallbackWrite(pAtomic, iNew);
	__xrtAtomic64FallbackRelease(pLock);
	return iOld;
}




/* 判断 64 位原子对象是否需要 TinyCC x86 分片锁退化路径。 */
static bool __xrtAtomic64NeedsFallback(const xatomic64* pAtomic)
{
	return (((uintptr_t)pAtomic & 7u) != 0u);
}

#endif



/* 检查 64 位原子对象满足当前编译器后端的最低安全对齐。 */
static bool __xrtAtomic64AddressValid(const xatomic64* pAtomic)
{
	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		return __xrtAtomicAddressValid(pAtomic, 4u);
	#else
		return __xrtAtomicAddressValid(pAtomic, 8u);
	#endif
}



/* 检查加载操作使用合法内存顺序。 */
static bool __xrtAtomicLoadOrderValid(xmemoryorder iOrder)
{
	if (
		(iOrder != XMEMORY_RELAXED) &&
		(iOrder != XMEMORY_ACQUIRE) &&
		(iOrder != XMEMORY_SEQ_CST)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 检查存储操作使用合法内存顺序。 */
static bool __xrtAtomicStoreOrderValid(xmemoryorder iOrder)
{
	if (
		(iOrder != XMEMORY_RELAXED) &&
		(iOrder != XMEMORY_RELEASE) &&
		(iOrder != XMEMORY_SEQ_CST)
	) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 检查读改写操作使用已定义内存顺序。 */
static bool __xrtAtomicRMWOrderValid(xmemoryorder iOrder)
{
	if ( (iOrder < XMEMORY_RELAXED) || (iOrder > XMEMORY_SEQ_CST) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}

	return true;
}



/* 检查比较交换成功和失败顺序满足 C11 约束。 */
static bool __xrtAtomicCASOrderValid(
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	bool bValid = false;

	if ( !__xrtAtomicRMWOrderValid(iSuccess) ) {
		return false;
	}
	switch ( iSuccess ) {
		case XMEMORY_RELAXED:
			bValid = iFailure == XMEMORY_RELAXED;
			break;
		case XMEMORY_ACQUIRE:
			bValid =
				(iFailure == XMEMORY_RELAXED) ||
				(iFailure == XMEMORY_ACQUIRE);
			break;
		case XMEMORY_RELEASE:
			bValid = iFailure == XMEMORY_RELAXED;
			break;
		case XMEMORY_ACQ_REL:
			bValid =
				(iFailure == XMEMORY_RELAXED) ||
				(iFailure == XMEMORY_ACQUIRE);
			break;
		case XMEMORY_SEQ_CST:
			bValid =
				(iFailure == XMEMORY_RELAXED) ||
				(iFailure == XMEMORY_ACQUIRE) ||
				(iFailure == XMEMORY_SEQ_CST);
			break;
		default:
			break;
	}
	if ( !bValid ) {
		__xrtErrorSetInvalidArgument();
	}

	return bValid;
}



/* 判断自然对齐的指定宽度是否由当前目标无锁实现。 */
XRT_API bool xrtAtomicIsLockFree(size_t iSize)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		if ( iSize == 4u ) {
			return __atomic_always_lock_free(4u, NULL);
		}
		if ( iSize == 8u ) {
			return __atomic_always_lock_free(8u, NULL);
		}
		return false;
	#elif defined(_WIN32) || defined(_WIN64)
		return (iSize == 4u) || (iSize == 8u);
	#elif defined(__x86_64__) || defined(_M_X64)
		return (iSize == 4u) || (iSize == 8u);
	#elif defined(__i386__) || defined(_M_IX86)
		return iSize == 4u;
	#else
		return false;
	#endif
}



/* 在对象发布给其他线程前初始化 32 位原子值。 */
XRT_API void xrtAtomic32Init(xatomic32* pAtomic, uint32 iValue)
{
	if ( !__xrtAtomicAddressValid(pAtomic, 4u) ) {
		return;
	}

	pAtomic->Value = iValue;
}



/* 按指定内存顺序读取 32 位原子值。 */
XRT_API uint32 xrtAtomic32Load(const xatomic32* pAtomic, xmemoryorder iOrder)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicLoadOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32LoadValue(&pAtomic->Value, iOrder);
}



/* 按指定内存顺序写入 32 位原子值。 */
XRT_API void xrtAtomic32Store(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicStoreOrderValid(iOrder)
	) {
		return;
	}

	__xrtAtomic32StoreValue(&pAtomic->Value, iValue, iOrder);
}



/* 原子交换 32 位值并返回旧值。 */
XRT_API uint32 xrtAtomic32Exchange(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32ExchangeValue(&pAtomic->Value, iValue, iOrder);
}



/* 强比较交换 32 位值，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomic32CompareExchange(
	xatomic32* pAtomic,
	uint32* pExpected,
	uint32 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	uint32 iActual;

	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		(pExpected == NULL) ||
		!__xrtAtomicCASOrderValid(iSuccess, iFailure)
	) {
		if ( pExpected == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	iActual = __xrtAtomic32CompareValue(
		&pAtomic->Value,
		*pExpected,
		iDesired,
		iSuccess,
		iFailure
	);
	if ( iActual == *pExpected ) {
		return true;
	}
	*pExpected = iActual;
	return false;
}



/* 原子加 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchAdd(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32FetchAddValue(&pAtomic->Value, iValue, iOrder);
}



/* 原子减 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchSub(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	return xrtAtomic32FetchAdd(pAtomic, 0u - iValue, iOrder);
}



/* 原子按位与 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchAnd(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32FetchBitsValue(&pAtomic->Value, iValue, 0u, iOrder);
}



/* 原子按位或 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchOr(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32FetchBitsValue(&pAtomic->Value, iValue, 1u, iOrder);
}



/* 原子按位异或 32 位值并返回修改前的值。 */
XRT_API uint32 xrtAtomic32FetchXor(
	xatomic32* pAtomic,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, 4u) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	return __xrtAtomic32FetchBitsValue(&pAtomic->Value, iValue, 2u, iOrder);
}



/* 在对象发布给其他线程前初始化 64 位原子值。 */
XRT_API void xrtAtomic64Init(xatomic64* pAtomic, uint64 iValue)
{
	if ( !__xrtAtomic64AddressValid(pAtomic) ) {
		return;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			__xrtAtomic64FallbackWrite(pAtomic, iValue);
			return;
		}
	#endif

	pAtomic->Value = iValue;
}



/* 按指定内存顺序读取 64 位原子值。 */
XRT_API uint64 xrtAtomic64Load(const xatomic64* pAtomic, xmemoryorder iOrder)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicLoadOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackLoad(pAtomic);
		}
	#endif

	return __xrtAtomic64LoadValue(&pAtomic->Value, iOrder);
}



/* 按指定内存顺序写入 64 位原子值。 */
XRT_API void xrtAtomic64Store(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicStoreOrderValid(iOrder)
	) {
		return;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			__xrtAtomic64FallbackStore(pAtomic, iValue);
			return;
		}
	#endif

	__xrtAtomic64StoreValue(&pAtomic->Value, iValue, iOrder);
}



/* 原子交换 64 位值并返回旧值。 */
XRT_API uint64 xrtAtomic64Exchange(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackUpdate(pAtomic, iValue, 0u);
		}
	#endif

	return __xrtAtomic64ExchangeValue(&pAtomic->Value, iValue, iOrder);
}



/* 强比较交换 64 位值，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomic64CompareExchange(
	xatomic64* pAtomic,
	uint64* pExpected,
	uint64 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	uint64 iActual;

	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		(pExpected == NULL) ||
		!__xrtAtomicCASOrderValid(iSuccess, iFailure)
	) {
		if ( pExpected == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			iActual = __xrtAtomic64FallbackCompare(
				pAtomic,
				*pExpected,
				iDesired
			);
		} else
	#endif
	{
		iActual = __xrtAtomic64CompareValue(
			&pAtomic->Value,
			*pExpected,
			iDesired,
			iSuccess,
			iFailure
		);
	}
	if ( iActual == *pExpected ) {
		return true;
	}
	*pExpected = iActual;
	return false;
}



/* 原子加 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchAdd(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackUpdate(pAtomic, iValue, 1u);
		}
	#endif

	return __xrtAtomic64FetchAddValue(&pAtomic->Value, iValue, iOrder);
}



/* 原子减 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchSub(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	return xrtAtomic64FetchAdd(pAtomic, 0u - iValue, iOrder);
}



/* 原子按位与 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchAnd(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackUpdate(pAtomic, iValue, 2u);
		}
	#endif

	return __xrtAtomic64FetchBitsValue(&pAtomic->Value, iValue, 0u, iOrder);
}



/* 原子按位或 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchOr(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackUpdate(pAtomic, iValue, 3u);
		}
	#endif

	return __xrtAtomic64FetchBitsValue(&pAtomic->Value, iValue, 1u, iOrder);
}



/* 原子按位异或 64 位值并返回修改前的值。 */
XRT_API uint64 xrtAtomic64FetchXor(
	xatomic64* pAtomic,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomic64AddressValid(pAtomic) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return 0u;
	}

	#if defined(__TINYC__) && defined(_WIN32) && \
		(UINTPTR_MAX == UINT32_MAX)
		if ( __xrtAtomic64NeedsFallback(pAtomic) ) {
			return __xrtAtomic64FallbackUpdate(pAtomic, iValue, 4u);
		}
	#endif

	return __xrtAtomic64FetchBitsValue(&pAtomic->Value, iValue, 2u, iOrder);
}



/* 在对象发布给其他线程前初始化原子指针。 */
XRT_API void xrtAtomicPtrInit(xatomicptr* pAtomic, ptr pValue)
{
	if ( !__xrtAtomicAddressValid(pAtomic, sizeof(ptr)) ) {
		return;
	}

	pAtomic->Value = pValue;
}



/* 按指定内存顺序读取原子指针。 */
XRT_API ptr xrtAtomicPtrLoad(const xatomicptr* pAtomic, xmemoryorder iOrder)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, sizeof(ptr)) ||
		!__xrtAtomicLoadOrderValid(iOrder)
	) {
		return NULL;
	}

	return __xrtAtomicPtrLoadValue(&pAtomic->Value, iOrder);
}



/* 按指定内存顺序写入原子指针。 */
XRT_API void xrtAtomicPtrStore(
	xatomicptr* pAtomic,
	ptr pValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, sizeof(ptr)) ||
		!__xrtAtomicStoreOrderValid(iOrder)
	) {
		return;
	}

	__xrtAtomicPtrStoreValue(&pAtomic->Value, pValue, iOrder);
}



/* 原子交换指针并返回旧值。 */
XRT_API ptr xrtAtomicPtrExchange(
	xatomicptr* pAtomic,
	ptr pValue,
	xmemoryorder iOrder
)
{
	if (
		!__xrtAtomicAddressValid(pAtomic, sizeof(ptr)) ||
		!__xrtAtomicRMWOrderValid(iOrder)
	) {
		return NULL;
	}

	return __xrtAtomicPtrExchangeValue(&pAtomic->Value, pValue, iOrder);
}



/* 强比较交换指针，失败时把实际值写回 Expected。 */
XRT_API bool xrtAtomicPtrCompareExchange(
	xatomicptr* pAtomic,
	ptr* pExpected,
	ptr pDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	ptr pActual;

	if (
		!__xrtAtomicAddressValid(pAtomic, sizeof(ptr)) ||
		(pExpected == NULL) ||
		!__xrtAtomicCASOrderValid(iSuccess, iFailure)
	) {
		if ( pExpected == NULL ) {
			__xrtErrorSetInvalidArgument();
		}
		return false;
	}

	pActual = __xrtAtomicPtrCompareValue(
		&pAtomic->Value,
		*pExpected,
		pDesired,
		iSuccess,
		iFailure
	);
	if ( pActual == *pExpected ) {
		return true;
	}
	*pExpected = pActual;
	return false;
}



/* 建立线程间内存栅栏。 */
XRT_API void xrtAtomicThreadFence(xmemoryorder iOrder)
{
	if ( !__xrtAtomicRMWOrderValid(iOrder) ) {
		return;
	}

	__xrtAtomicThreadFence(iOrder);
}



/* 建立当前线程与信号处理器之间的编译器栅栏。 */
XRT_API void xrtAtomicSignalFence(xmemoryorder iOrder)
{
	if ( !__xrtAtomicRMWOrderValid(iOrder) ) {
		return;
	}

	__xrtAtomicSignalFence(iOrder);
}



/* 向处理器提示当前线程处于短自旋等待。 */
XRT_API void xrtAtomicPause(void)
{
	__xrtAtomicPause();
}

#endif
