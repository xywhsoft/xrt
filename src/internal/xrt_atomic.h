#ifndef XRT_INTERNAL_ATOMIC_H
#define XRT_INTERNAL_ATOMIC_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_ATOMIC)

#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
/* 内建 64 位原子类型在 32 位目标上也必须保持 8 字节静态对齐。 */
typedef uint64 __xrt_atomic64_value __attribute__((aligned(8)));



/* 把公开内存顺序转换为 GCC/Clang 内建常量。 */
static inline int __xrtAtomicOrder(xmemoryorder iOrder)
{
	switch ( iOrder ) {
		case XMEMORY_RELAXED:
			return __ATOMIC_RELAXED;
		case XMEMORY_ACQUIRE:
			return __ATOMIC_ACQUIRE;
		case XMEMORY_RELEASE:
			return __ATOMIC_RELEASE;
		case XMEMORY_ACQ_REL:
			return __ATOMIC_ACQ_REL;
		case XMEMORY_SEQ_CST:
		default:
			return __ATOMIC_SEQ_CST;
	}
}



/* 公共入口已验证 8 字节对齐；把这一事实传递给 32 位目标的内建原子。 */
static inline volatile __xrt_atomic64_value* __xrtAtomic64Aligned(
	volatile uint64* pValue
)
{
	return (volatile __xrt_atomic64_value*)__builtin_assume_aligned(
		(void*)pValue,
		8u
	);
}



/* 只读路径保留限定符并传递相同的 8 字节对齐前置条件。 */
static inline const volatile __xrt_atomic64_value* __xrtAtomic64AlignedConst(
	const volatile uint64* pValue
)
{
	return (const volatile __xrt_atomic64_value*)__builtin_assume_aligned(
		(const void*)pValue,
		8u
	);
}
#endif



/* 比较交换 32 位原始值并返回修改前的值。 */
static inline uint32 __xrtAtomic32CompareValue(
	volatile uint32* pValue,
	uint32 iExpected,
	uint32 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		uint32 iActual = iExpected;

		(void)__atomic_compare_exchange_n(
			pValue,
			&iActual,
			iDesired,
			false,
			__xrtAtomicOrder(iSuccess),
			__xrtAtomicOrder(iFailure)
		);
		return iActual;
	#elif defined(_MSC_VER)
		(void)iSuccess;
		(void)iFailure;
		return (uint32)_InterlockedCompareExchange(
			(volatile long*)pValue,
			(long)iDesired,
			(long)iExpected
		);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)iSuccess;
		(void)iFailure;
		return (uint32)InterlockedCompareExchange(
			(volatile LONG*)pValue,
			(LONG)iDesired,
			(LONG)iExpected
		);
	#elif defined(__TINYC__) && (defined(__x86_64__) || defined(_M_X64))
		uint32 iActual;

		(void)iSuccess;
		(void)iFailure;
		__asm__ volatile (
			"lock; cmpxchgl %2, %1"
			: "=a"(iActual), "+m"(*pValue)
			: "r"(iDesired), "0"(iExpected)
			: "cc", "memory"
		);
		return iActual;
	#else
		(void)iSuccess;
		(void)iFailure;
		return __sync_val_compare_and_swap(pValue, iExpected, iDesired);
	#endif
}



/* 读取 32 位原始原子值。 */
static inline uint32 __xrtAtomic32LoadValue(
	const volatile uint32* pValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_load_n(pValue, __xrtAtomicOrder(iOrder));
	#else
		return __xrtAtomic32CompareValue(
			(volatile uint32*)pValue,
			0u,
			0u,
			iOrder,
			iOrder
		);
	#endif
}



/* 交换 32 位原始原子值并返回旧值。 */
static inline uint32 __xrtAtomic32ExchangeValue(
	volatile uint32* pValue,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_exchange_n(pValue, iValue, __xrtAtomicOrder(iOrder));
	#elif defined(_MSC_VER)
		(void)iOrder;
		return (uint32)_InterlockedExchange((volatile long*)pValue, (long)iValue);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)iOrder;
		return (uint32)InterlockedExchange((volatile LONG*)pValue, (LONG)iValue);
	#else
		uint32 iOld = __xrtAtomic32LoadValue(pValue, XMEMORY_RELAXED);
		uint32 iActual;

		for ( ;; ) {
			iActual = __xrtAtomic32CompareValue(
				pValue,
				iOld,
				iValue,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#endif
}



/* 写入 32 位原始原子值。 */
static inline void __xrtAtomic32StoreValue(
	volatile uint32* pValue,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		__atomic_store_n(pValue, iValue, __xrtAtomicOrder(iOrder));
	#else
		(void)__xrtAtomic32ExchangeValue(pValue, iValue, iOrder);
	#endif
}



/* 原子加 32 位原始值并返回旧值。 */
static inline uint32 __xrtAtomic32FetchAddValue(
	volatile uint32* pValue,
	uint32 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_fetch_add(pValue, iValue, __xrtAtomicOrder(iOrder));
	#elif defined(_MSC_VER)
		(void)iOrder;
		return (uint32)_InterlockedExchangeAdd((volatile long*)pValue, (long)iValue);
	#elif defined(__TINYC__)
		uint32 iOld = __xrtAtomic32LoadValue(pValue, XMEMORY_RELAXED);
		uint32 iActual;

		for ( ;; ) {
			iActual = __xrtAtomic32CompareValue(
				pValue,
				iOld,
				iOld + iValue,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#else
		(void)iOrder;
		return __sync_fetch_and_add(pValue, iValue);
	#endif
}



/* 用比较交换循环更新 32 位原始值并返回旧值。 */
static inline uint32 __xrtAtomic32FetchBitsValue(
	volatile uint32* pValue,
	uint32 iValue,
	uint32 iOperation,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		switch ( iOperation ) {
			case 0u:
				return __atomic_fetch_and(
					pValue,
					iValue,
					__xrtAtomicOrder(iOrder)
				);
			case 1u:
				return __atomic_fetch_or(
					pValue,
					iValue,
					__xrtAtomicOrder(iOrder)
				);
			default:
				return __atomic_fetch_xor(
					pValue,
					iValue,
					__xrtAtomicOrder(iOrder)
				);
		}
	#else
		uint32 iOld = __xrtAtomic32LoadValue(pValue, XMEMORY_RELAXED);
		uint32 iNew;
		uint32 iActual;

		for ( ;; ) {
			switch ( iOperation ) {
				case 0u:
					iNew = iOld & iValue;
					break;
				case 1u:
					iNew = iOld | iValue;
					break;
				default:
					iNew = iOld ^ iValue;
					break;
			}
			iActual = __xrtAtomic32CompareValue(
				pValue,
				iOld,
				iNew,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#endif
}



/* 比较交换 64 位原始值并返回修改前的值。 */
static inline uint64 __xrtAtomic64CompareValue(
	volatile uint64* pValue,
	uint64 iExpected,
	uint64 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		uint64 iActual = iExpected;

		(void)__atomic_compare_exchange_n(
			__xrtAtomic64Aligned(pValue),
			&iActual,
			iDesired,
			false,
			__xrtAtomicOrder(iSuccess),
			__xrtAtomicOrder(iFailure)
		);
		return iActual;
	#elif defined(_MSC_VER)
		(void)iSuccess;
		(void)iFailure;
		return (uint64)_InterlockedCompareExchange64(
			(volatile __int64*)pValue,
			(__int64)iDesired,
			(__int64)iExpected
		);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)iSuccess;
		(void)iFailure;
		return (uint64)InterlockedCompareExchange64(
			(volatile LONG64*)pValue,
			(LONG64)iDesired,
			(LONG64)iExpected
		);
	#elif defined(__TINYC__) && (defined(__x86_64__) || defined(_M_X64))
		uint64 iActual;

		(void)iSuccess;
		(void)iFailure;
		__asm__ volatile (
			"lock; cmpxchgq %2, %1"
			: "=a"(iActual), "+m"(*pValue)
			: "r"(iDesired), "0"(iExpected)
			: "cc", "memory"
		);
		return iActual;
	#else
		(void)iSuccess;
		(void)iFailure;
		return __sync_val_compare_and_swap(pValue, iExpected, iDesired);
	#endif
}



/* 读取 64 位原始原子值。 */
static inline uint64 __xrtAtomic64LoadValue(
	const volatile uint64* pValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_load_n(
			__xrtAtomic64AlignedConst(pValue),
			__xrtAtomicOrder(iOrder)
		);
	#else
		return __xrtAtomic64CompareValue(
			(volatile uint64*)pValue,
			0u,
			0u,
			iOrder,
			iOrder
		);
	#endif
}



/* 交换 64 位原始原子值并返回旧值。 */
static inline uint64 __xrtAtomic64ExchangeValue(
	volatile uint64* pValue,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_exchange_n(
			__xrtAtomic64Aligned(pValue),
			iValue,
			__xrtAtomicOrder(iOrder)
		);
	#else
		uint64 iOld = __xrtAtomic64LoadValue(pValue, XMEMORY_RELAXED);
		uint64 iActual;

		for ( ;; ) {
			iActual = __xrtAtomic64CompareValue(
				pValue,
				iOld,
				iValue,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#endif
}



/* 写入 64 位原始原子值。 */
static inline void __xrtAtomic64StoreValue(
	volatile uint64* pValue,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		__atomic_store_n(
			__xrtAtomic64Aligned(pValue),
			iValue,
			__xrtAtomicOrder(iOrder)
		);
	#else
		(void)__xrtAtomic64ExchangeValue(pValue, iValue, iOrder);
	#endif
}



/* 原子加 64 位原始值并返回旧值。 */
static inline uint64 __xrtAtomic64FetchAddValue(
	volatile uint64* pValue,
	uint64 iValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_fetch_add(
			__xrtAtomic64Aligned(pValue),
			iValue,
			__xrtAtomicOrder(iOrder)
		);
	#elif defined(_MSC_VER) || defined(__TINYC__)
		uint64 iOld = __xrtAtomic64LoadValue(pValue, XMEMORY_RELAXED);
		uint64 iActual;

		for ( ;; ) {
			iActual = __xrtAtomic64CompareValue(
				pValue,
				iOld,
				iOld + iValue,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#else
		(void)iOrder;
		return __sync_fetch_and_add(pValue, iValue);
	#endif
}



/* 用比较交换循环更新 64 位原始值并返回旧值。 */
static inline uint64 __xrtAtomic64FetchBitsValue(
	volatile uint64* pValue,
	uint64 iValue,
	uint32 iOperation,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		switch ( iOperation ) {
			case 0u:
				return __atomic_fetch_and(
					__xrtAtomic64Aligned(pValue),
					iValue,
					__xrtAtomicOrder(iOrder)
				);
			case 1u:
				return __atomic_fetch_or(
					__xrtAtomic64Aligned(pValue),
					iValue,
					__xrtAtomicOrder(iOrder)
				);
			default:
				return __atomic_fetch_xor(
					__xrtAtomic64Aligned(pValue),
					iValue,
					__xrtAtomicOrder(iOrder)
				);
		}
	#else
		uint64 iOld = __xrtAtomic64LoadValue(pValue, XMEMORY_RELAXED);
		uint64 iNew;
		uint64 iActual;

		for ( ;; ) {
			switch ( iOperation ) {
				case 0u:
					iNew = iOld & iValue;
					break;
				case 1u:
					iNew = iOld | iValue;
					break;
				default:
					iNew = iOld ^ iValue;
					break;
			}
			iActual = __xrtAtomic64CompareValue(
				pValue,
				iOld,
				iNew,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( iActual == iOld ) {
				return iOld;
			}
			iOld = iActual;
		}
	#endif
}



/* 比较交换原始指针并返回修改前的值。 */
static inline ptr __xrtAtomicPtrCompareValue(
	ptr volatile* pValue,
	ptr pExpected,
	ptr pDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		ptr pActual = pExpected;

		(void)__atomic_compare_exchange_n(
			pValue,
			&pActual,
			pDesired,
			false,
			__xrtAtomicOrder(iSuccess),
			__xrtAtomicOrder(iFailure)
		);
		return pActual;
	#elif defined(_MSC_VER)
		(void)iSuccess;
		(void)iFailure;
		return _InterlockedCompareExchangePointer(
			(void* volatile*)pValue,
			pDesired,
			pExpected
		);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)iSuccess;
		(void)iFailure;
		return InterlockedCompareExchangePointer(
			(PVOID volatile*)pValue,
			pDesired,
			pExpected
		);
	#elif UINTPTR_MAX == UINT64_MAX
		return (ptr)(uintptr_t)__xrtAtomic64CompareValue(
			(volatile uint64*)pValue,
			(uint64)(uintptr_t)pExpected,
			(uint64)(uintptr_t)pDesired,
			iSuccess,
			iFailure
		);
	#else
		return (ptr)(uintptr_t)__xrtAtomic32CompareValue(
			(volatile uint32*)pValue,
			(uint32)(uintptr_t)pExpected,
			(uint32)(uintptr_t)pDesired,
			iSuccess,
			iFailure
		);
	#endif
}



/* 读取原始原子指针。 */
static inline ptr __xrtAtomicPtrLoadValue(
	ptr const volatile* pValue,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_load_n(pValue, __xrtAtomicOrder(iOrder));
	#else
		return __xrtAtomicPtrCompareValue(
			(ptr volatile*)pValue,
			NULL,
			NULL,
			iOrder,
			iOrder
		);
	#endif
}



/* 交换原始原子指针并返回旧值。 */
static inline ptr __xrtAtomicPtrExchangeValue(
	ptr volatile* pValue,
	ptr pNew,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		return __atomic_exchange_n(pValue, pNew, __xrtAtomicOrder(iOrder));
	#elif defined(_MSC_VER)
		(void)iOrder;
		return _InterlockedExchangePointer((void* volatile*)pValue, pNew);
	#elif defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))
		(void)iOrder;
		return InterlockedExchangePointer((PVOID volatile*)pValue, pNew);
	#else
		ptr pOld = __xrtAtomicPtrLoadValue(pValue, XMEMORY_RELAXED);
		ptr pActual;

		for ( ;; ) {
			pActual = __xrtAtomicPtrCompareValue(
				pValue,
				pOld,
				pNew,
				iOrder,
				XMEMORY_RELAXED
			);
			if ( pActual == pOld ) {
				return pOld;
			}
			pOld = pActual;
		}
	#endif
}



/* 写入原始原子指针。 */
static inline void __xrtAtomicPtrStoreValue(
	ptr volatile* pValue,
	ptr pNew,
	xmemoryorder iOrder
)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		__atomic_store_n(pValue, pNew, __xrtAtomicOrder(iOrder));
	#else
		(void)__xrtAtomicPtrExchangeValue(pValue, pNew, iOrder);
	#endif
}



/* 建立线程间内存栅栏。 */
static inline void __xrtAtomicThreadFence(xmemoryorder iOrder)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		__atomic_thread_fence(__xrtAtomicOrder(iOrder));
	#elif defined(_WIN32) || defined(_WIN64)
		(void)iOrder;
		MemoryBarrier();
	#elif defined(__TINYC__) && \
		(defined(__i386__) || defined(__x86_64__) || \
		 defined(_M_IX86) || defined(_M_X64))
		(void)iOrder;
		__asm__ volatile ("mfence" ::: "memory");
	#elif defined(__TINYC__) && (defined(__aarch64__) || defined(__arm__))
		(void)iOrder;
		__asm__ volatile ("dmb ish" ::: "memory");
	#else
		(void)iOrder;
		__sync_synchronize();
	#endif
}



/* 建立当前线程与信号处理器之间的编译器栅栏。 */
static inline void __xrtAtomicSignalFence(xmemoryorder iOrder)
{
	#if (defined(__GNUC__) || defined(__clang__)) && !defined(__TINYC__)
		__atomic_signal_fence(__xrtAtomicOrder(iOrder));
	#elif defined(_MSC_VER)
		(void)iOrder;
		_ReadWriteBarrier();
	#else
		(void)iOrder;
		__asm__ volatile ("" ::: "memory");
	#endif
}



/* 向处理器提示当前执行流处于短自旋等待。 */
static inline void __xrtAtomicPause(void)
{
	#if defined(_MSC_VER) && (defined(_M_IX86) || defined(_M_X64))
		_mm_pause();
	#elif defined(__i386__) || defined(__x86_64__) || defined(_M_IX86) || defined(_M_X64)
		__asm__ volatile ("pause");
	#elif defined(__aarch64__) || defined(__arm__)
		__asm__ volatile ("yield");
	#else
		__xrtAtomicSignalFence(XMEMORY_SEQ_CST);
	#endif
}

#endif

#endif
