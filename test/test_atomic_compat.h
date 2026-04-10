#ifndef XRT_TEST_ATOMIC_COMPAT_H
#define XRT_TEST_ATOMIC_COMPAT_H

#include "../xrt.h"

/* Route test-side atomics through xrt helpers so Linux TCC does not depend on GCC builtins. */
static inline long __xrtTestAtomicAddFetchLong(volatile long* pValue, long iDelta)
{
	return __xrtAtomicAddFetch32(pValue, iDelta);
}


// 内部函数：__xrtTestAtomicLoadLong
static inline long __xrtTestAtomicLoadLong(const volatile long* pValue)
{
	return __xrtAtomicCompareExchange32((volatile long*)pValue, 0, 0);
}


// 内部函数：__xrtTestAtomicCompareExchangeLong
static inline long __xrtTestAtomicCompareExchangeLong(volatile long* pValue, long iExchange, long iComparand)
{
	return __xrtAtomicCompareExchange32(pValue, iExchange, iComparand);
}


// 内部函数：__xrtTestAtomicStoreLong
static inline void __xrtTestAtomicStoreLong(volatile long* pValue, long iValue)
{
	(void)__xrtAtomicExchange32(pValue, iValue);
}

#endif
