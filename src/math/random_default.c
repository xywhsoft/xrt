#include "../internal/xrt_random.h"

#include <time.h>

#if !defined(_WIN32) && !defined(_WIN64)
	#include <unistd.h>
#endif



#if defined(XRT_FEATURE_RANDOM_DEFAULT)

/* SplitMix64 只用于扩散自动种子材料，不承担对外随机序列。 */
static uint64 __xrtRandMix(uint64 iValue)
{
	iValue += UINT64_C(0x9E3779B97F4A7C15);
	iValue = (iValue ^ (iValue >> 30u)) * UINT64_C(0xBF58476D1CE4E5B9);
	iValue = (iValue ^ (iValue >> 27u)) * UINT64_C(0x94D049BB133111EB);
	return iValue ^ (iValue >> 31u);
}



/* 使用时间、进程和线程存储地址生成非密码学自动种子。 */
static void __xrtRandAutoSeed(xrng* pRng)
{
	uint64 iMaterial = (uint64)(uintptr_t)pRng;

	iMaterial ^= (uint64)time(NULL);
	iMaterial ^= (uint64)clock() << 32u;
	#if defined(_WIN32) || defined(_WIN64)
		iMaterial ^= (uint64)GetCurrentProcessId() << 16u;
		iMaterial ^= (uint64)GetCurrentThreadId();
	#else
		iMaterial ^= (uint64)getpid() << 16u;
	#endif
	xrtRngSeed(pRng, __xrtRandMix(iMaterial),
		__xrtRandMix(iMaterial ^ UINT64_C(0xD1B54A32D192ED03)));
}



#if defined(__TINYC__) && (defined(_WIN32) || defined(_WIN64))

static DWORD __xrtRandFls = FLS_OUT_OF_INDEXES;
static volatile LONG __xrtRandFlsState = 0;



/* 在线程或 fiber 退出时释放 TinyCC 的动态状态。 */
static void NTAPI __xrtRandLocalFree(PVOID pData)
{
	xrtFree(pData);
}



/* 线程安全地创建 TinyCC 使用的 FLS 槽。 */
static bool __xrtRandFlsEnsure(void)
{
	LONG iState = InterlockedCompareExchange(&__xrtRandFlsState, 1, 0);

	if ( iState == 0 ) {
		__xrtRandFls = FlsAlloc(__xrtRandLocalFree);
		InterlockedExchange(&__xrtRandFlsState,
			__xrtRandFls == FLS_OUT_OF_INDEXES ? 3 : 2);
		return __xrtRandFls != FLS_OUT_OF_INDEXES;
	}
	while ( (iState = InterlockedCompareExchange(&__xrtRandFlsState, 0, 0)) == 1 ) {
		Sleep(0);
	}
	return iState == 2;
}



/* 取得或创建 TinyCC Windows 当前执行线程的状态。 */
xrng* __xrtRandCurrent(void)
{
	xrng* pRng;

	if ( !__xrtRandFlsEnsure() ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pRng = (xrng*)FlsGetValue(__xrtRandFls);
	if ( pRng != NULL ) {
		return pRng;
	}

	pRng = (xrng*)xrtMalloc(sizeof(xrng));
	if ( pRng == NULL ) {
		return NULL;
	}
	__xrtRandAutoSeed(pRng);
	if ( !FlsSetValue(__xrtRandFls, pRng) ) {
		xrtFree(pRng);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pRng;
}

#elif defined(__TINYC__)

static pthread_key_t __xrtRandKey;
static pthread_once_t __xrtRandKeyOnce = PTHREAD_ONCE_INIT;
static int __xrtRandKeyError = 0;



/* 在线程退出时释放 TinyCC POSIX 的动态状态。 */
static void __xrtRandLocalFree(void* pData)
{
	xrtFree(pData);
}



/* 创建 TinyCC POSIX 使用的 pthread key。 */
static void __xrtRandKeyInit(void)
{
	__xrtRandKeyError = pthread_key_create(&__xrtRandKey, __xrtRandLocalFree);
}



/* 取得或创建 TinyCC POSIX 当前线程的状态。 */
xrng* __xrtRandCurrent(void)
{
	xrng* pRng;

	(void)pthread_once(&__xrtRandKeyOnce, __xrtRandKeyInit);
	if ( __xrtRandKeyError != 0 ) {
		__xrtErrorSetInvalidState();
		return NULL;
	}
	pRng = (xrng*)pthread_getspecific(__xrtRandKey);
	if ( pRng != NULL ) {
		return pRng;
	}

	pRng = (xrng*)xrtMalloc(sizeof(xrng));
	if ( pRng == NULL ) {
		return NULL;
	}
	__xrtRandAutoSeed(pRng);
	if ( pthread_setspecific(__xrtRandKey, pRng) != 0 ) {
		xrtFree(pRng);
		__xrtErrorSetInvalidState();
		return NULL;
	}
	return pRng;
}

#else

static XRT_THREAD_LOCAL xrng __xrtRandState;
static XRT_THREAD_LOCAL bool __xrtRandReady = false;



/* 直接返回编译器 TLS 中的当前线程状态。 */
xrng* __xrtRandCurrent(void)
{
	if ( !__xrtRandReady ) {
		__xrtRandAutoSeed(&__xrtRandState);
		__xrtRandReady = true;
	}
	return &__xrtRandState;
}

#endif



/* 显式重置当前线程的便捷状态。 */
XRT_API void xrtRandSeed(uint64 iSeed, uint64 iStream)
{
	xrng* pRng = __xrtRandCurrent();

	if ( pRng != NULL ) {
		xrtRngSeed(pRng, iSeed, iStream);
	}
}



/* 从当前线程状态生成 32 位值。 */
XRT_API uint32 xrtRand32(void)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRng32(pRng) : 0;
}



/* 从当前线程状态生成 64 位值。 */
XRT_API uint64 xrtRand64(void)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRng64(pRng) : 0;
}



/* 使用当前线程状态按稳定字节序填充缓冲区。 */
XRT_API bool xrtRandBytes(ptr pData, size_t iSize)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL && xrtRngBytes(pRng, pData, iSize);
}



/* 从当前线程状态生成无偏有界值。 */
XRT_API uint64 xrtRandBelow(uint64 iBound)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngBelow64(pRng, iBound) : 0;
}



/* 从当前线程状态生成半开区间值。 */
XRT_API int64 xrtRandRange(int64 iMin, int64 iMax)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngRange(pRng, iMin, iMax) : 0;
}



/* 从当前线程状态生成闭区间值。 */
XRT_API int64 xrtRandRangeClosed(int64 iMin, int64 iMax)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngRangeClosed(pRng, iMin, iMax) : 0;
}



/* 从当前线程状态生成半开单位实数。 */
XRT_API double xrtRandReal(void)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL ? xrtRngReal(pRng) : 0.0;
}



/* 使用当前线程状态原地打乱数组。 */
XRT_API bool xrtRandShuffle(ptr pData, size_t iCount, size_t iItemSize)
{
	xrng* pRng = __xrtRandCurrent();

	return pRng != NULL && xrtRngShuffle(pRng, pData, iCount, iItemSize);
}

#endif
