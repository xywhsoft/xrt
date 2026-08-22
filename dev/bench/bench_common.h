#ifndef XRT_BENCH_COMMON_H
#define XRT_BENCH_COMMON_H

#if defined(__GNUC__) && !defined(__TINYC__)
	#pragma GCC diagnostic push
	#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if !defined(_WIN32) && !defined(_WIN64)
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif
#endif

#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <windows.h>
#else
	#include <sched.h>
	#include <time.h>
	#include <unistd.h>
#endif



/* 基准计时器保存单调纳秒起止刻度。 */
typedef struct xbenchtimer {
	uint64_t Start;
	uint64_t End;
} xbenchtimer;



/* 返回单调纳秒刻度，不把 Windows QPC 绝对刻度直接乘以十亿。 */
static uint64_t xbenchNowNs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		static LARGE_INTEGER Frequency;
		LARGE_INTEGER Current;
		uint64_t iTicks;
		uint64_t iFrequency;
		uint64_t iSeconds;
		uint64_t iRemainder;

		if ( Frequency.QuadPart == 0 ) {
			if ( QueryPerformanceFrequency(&Frequency) == 0 ) {
				return (uint64_t)GetTickCount() * UINT64_C(1000000);
			}
		}
		QueryPerformanceCounter(&Current);
		iTicks = (uint64_t)Current.QuadPart;
		iFrequency = (uint64_t)Frequency.QuadPart;
		iSeconds = iTicks / iFrequency;
		iRemainder = iTicks % iFrequency;
		return
			(iSeconds * UINT64_C(1000000000)) +
			((iRemainder * UINT64_C(1000000000)) / iFrequency);
	#else
		struct timespec Current;

		clock_gettime(CLOCK_MONOTONIC, &Current);
		return
			((uint64_t)Current.tv_sec * UINT64_C(1000000000)) +
			(uint64_t)Current.tv_nsec;
	#endif
}



/* 启动一次基准计时。 */
static void xbenchTimerStart(xbenchtimer* pTimer)
{
	if ( pTimer == NULL ) {
		return;
	}
	pTimer->Start = xbenchNowNs();
	pTimer->End = 0u;
}



/* 停止一次基准计时。 */
static void xbenchTimerStop(xbenchtimer* pTimer)
{
	if ( pTimer == NULL ) {
		return;
	}
	pTimer->End = xbenchNowNs();
}



/* 返回计时器已经经过的纳秒数。 */
static uint64_t xbenchTimerElapsedNs(const xbenchtimer* pTimer)
{
	if ( (pTimer == NULL) || (pTimer->Start == 0u) ) {
		return 0u;
	}
	if ( pTimer->End > pTimer->Start ) {
		return pTimer->End - pTimer->Start;
	}
	return xbenchNowNs() - pTimer->Start;
}



/* 把纳秒转换为秒。 */
static double xbenchNsToSec(uint64_t iNanoseconds)
{
	return (double)iNanoseconds / 1000000000.0;
}



/* 计算总量除以耗时得到的每秒速率。 */
static double xbenchSafeRate(uint64_t iCount, uint64_t iNanoseconds)
{
	if ( iNanoseconds == 0u ) {
		return 0.0;
	}
	return (double)iCount / xbenchNsToSec(iNanoseconds);
}



/* 比较纳秒样本，供 qsort 生成稳定的延迟分位数。 */
static int xbenchCompareU64(const void* pLeft, const void* pRight)
{
	uint64_t iLeft = *(const uint64_t*)pLeft;
	uint64_t iRight = *(const uint64_t*)pRight;

	if ( iLeft < iRight ) {
		return -1;
	}
	if ( iLeft > iRight ) {
		return 1;
	}
	return 0;
}



/* 按向下取整的 nearest-rank 索引读取已排序纳秒样本。 */
static double xbenchPercentileUs(
	const uint64_t* pSamples,
	size_t iCount,
	double fPercentile
)
{
	size_t iIndex;

	if (
		(pSamples == NULL) ||
		(iCount == 0) ||
		(fPercentile < 0.0) ||
		(fPercentile > 1.0)
	) {
		return 0.0;
	}
	iIndex = (size_t)(((double)(iCount - 1u)) * fPercentile);
	return ((double)pSamples[iIndex]) / 1000.0;
}



/* 让当前线程至少休眠指定毫秒数。 */
static void xbenchSleepMs(uint32_t iDelay)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelay);
	#else
		struct timespec Delay;

		Delay.tv_sec = (time_t)(iDelay / 1000u);
		Delay.tv_nsec = (long)((iDelay % 1000u) * 1000000u);
		while ( nanosleep(&Delay, &Delay) != 0 ) {
			if ( errno != EINTR ) {
				break;
			}
		}
	#endif
}



/* 原子增加指定增量并返回新值。 */
static long xbenchAtomicAdd(volatile long* pValue, long iDelta)
{
	#if defined(_WIN32) || defined(_WIN64)
		LONG iOld = InterlockedCompareExchange(
			(volatile LONG*)pValue,
			0,
			0
		);
		LONG iActual;

		for ( ;; ) {
			iActual = InterlockedCompareExchange(
				(volatile LONG*)pValue,
				iOld + (LONG)iDelta,
				iOld
			);
			if ( iActual == iOld ) {
				return (long)(iOld + (LONG)iDelta);
			}
			iOld = iActual;
		}
	#else
		return __atomic_add_fetch(pValue, iDelta, __ATOMIC_SEQ_CST);
	#endif
}



/* 原子增加一个基准 long 计数器并返回新值。 */
static long xbenchAtomicInc(volatile long* pValue)
{
	return xbenchAtomicAdd(pValue, 1);
}



/* 原子读取一个基准 long 计数器。 */
static long xbenchAtomicLoad(const volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
	#endif
}



/* 原子写入一个基准 long 计数器。 */
static void xbenchAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__atomic_store_n(pValue, iValue, __ATOMIC_SEQ_CST);
	#endif
}



/* 判断指定纳秒期限是否已经到达。 */
static bool xbenchDeadlineReached(uint64_t iDeadline)
{
	return (iDeadline > 0u) && (xbenchNowNs() >= iDeadline);
}



/* 从当前单调时钟构造毫秒期限。 */
static uint64_t xbenchDeadlineAfterMs(uint32_t iTimeout)
{
	uint64_t iNow = xbenchNowNs();
	uint64_t iDelta = (uint64_t)iTimeout * UINT64_C(1000000);

	if ( iNow > (UINT64_MAX - iDelta) ) {
		return UINT64_MAX;
	}
	return iNow + iDelta;
}



/* 原子读取一个 64 位无符号计数器。 */
static uint64_t xbenchAtomicLoadU64(const volatile uint64_t* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64_t)InterlockedCompareExchange64(
			(volatile LONG64*)pValue,
			0,
			0
		);
	#else
		return __atomic_load_n(pValue, __ATOMIC_SEQ_CST);
	#endif
}



/* 仅在目标仍为零时设置一次 64 位值。 */
static bool xbenchAtomicSetOnceU64(
	volatile uint64_t* pValue,
	uint64_t iValue
)
{
	if ( (pValue == NULL) || (iValue == 0u) ) {
		return false;
	}
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64_t)InterlockedCompareExchange64(
			(volatile LONG64*)pValue,
			(LONG64)iValue,
			0
		) == 0u;
	#else
		{
			uint64_t iExpected = 0u;

			return __atomic_compare_exchange_n(
				pValue,
				&iExpected,
				iValue,
				false,
				__ATOMIC_SEQ_CST,
				__ATOMIC_SEQ_CST
			);
		}
	#endif
}



/* 原子地把 long 计数器提升到不小于候选值。 */
static long xbenchAtomicMax(volatile long* pValue, long iCandidate)
{
	long iPrevious = xbenchAtomicLoad(pValue);

	while ( iCandidate > iPrevious ) {
		#if defined(_WIN32) || defined(_WIN64)
			long iSeen = InterlockedCompareExchange(
				(volatile LONG*)pValue,
				iCandidate,
				iPrevious
			);
		#else
			long iSeen = iPrevious;

			__atomic_compare_exchange_n(
				pValue,
				&iSeen,
				iCandidate,
				false,
				__ATOMIC_SEQ_CST,
				__ATOMIC_SEQ_CST
			);
		#endif

		if ( iSeen == iPrevious ) {
			return iCandidate;
		}
		iPrevious = iSeen;
	}
	return iPrevious;
}



/* 在毫秒期限内等待计数器达到最小值。 */
static bool xbenchWaitMin(
	volatile long* pValue,
	long iMinimum,
	uint32_t iTimeout
)
{
	uint64_t iDeadline = xbenchDeadlineAfterMs(iTimeout);

	while ( !xbenchDeadlineReached(iDeadline) ) {
		if ( xbenchAtomicLoad(pValue) >= iMinimum ) {
			return true;
		}
		xbenchSleepMs(10u);
	}
	return xbenchAtomicLoad(pValue) >= iMinimum;
}



/* 安全计算需要进入 long 计数器的两个 32 位数量之积。 */
static bool xbenchCountProductU32(
	uint32_t iLeft,
	uint32_t iRight,
	uint32_t* pProduct
)
{
	uint64_t iProduct;

	if (
		(pProduct == NULL) ||
		(iLeft == 0u) ||
		(iRight == 0u)
	) {
		return false;
	}
	iProduct = (uint64_t)iLeft * (uint64_t)iRight;
	if (
		(iProduct > UINT32_MAX) ||
		(iProduct > (uint64_t)LONG_MAX)
	) {
		return false;
	}

	*pProduct = (uint32_t)iProduct;
	return true;
}



/* 从命令行读取一个严格的 32 位无符号整数。 */
static uint32_t xbenchArgU32(
	int iArgumentCount,
	char** pArguments,
	int iIndex,
	uint32_t iDefault
)
{
	char* pEnd;
	unsigned long iValue;

	if (
		(iArgumentCount <= iIndex) ||
		(pArguments == NULL) ||
		(pArguments[iIndex] == NULL) ||
		(pArguments[iIndex][0] == '\0')
	) {
		return iDefault;
	}
	errno = 0;
	pEnd = NULL;
	iValue = strtoul(pArguments[iIndex], &pEnd, 10);
	if (
		(errno != 0) ||
		(pEnd == pArguments[iIndex]) ||
		(*pEnd != '\0') ||
		(iValue > UINT32_MAX)
	) {
		return iDefault;
	}
	return (uint32_t)iValue;
}



/* 从命令行读取一个严格的 64 位无符号整数。 */
static uint64_t xbenchArgU64(
	int iArgumentCount,
	char** pArguments,
	int iIndex,
	uint64_t iDefault
)
{
	char* pEnd;
	uint64_t iValue;

	if (
		(iArgumentCount <= iIndex) ||
		(pArguments == NULL) ||
		(pArguments[iIndex] == NULL) ||
		(pArguments[iIndex][0] == '\0')
	) {
		return iDefault;
	}
	errno = 0;
	pEnd = NULL;
	#if defined(_WIN32) || defined(_WIN64)
		iValue = (uint64_t)_strtoui64(pArguments[iIndex], &pEnd, 10);
	#else
		iValue = (uint64_t)strtoull(pArguments[iIndex], &pEnd, 10);
	#endif
	if (
		(errno != 0) ||
		(pEnd == pArguments[iIndex]) ||
		(*pEnd != '\0')
	) {
		return iDefault;
	}
	return iValue;
}



/* 判断给定路径是否指向可打开的普通文件入口。 */
static bool xbenchFileExists(const char* sPath)
{
	FILE* pFile;

	if ( (sPath == NULL) || (sPath[0] == '\0') ) {
		return false;
	}
	pFile = fopen(sPath, "rb");
	if ( pFile == NULL ) {
		return false;
	}
	fclose(pFile);
	return true;
}



/* 按 XBENCH_PIN_CPU 环境变量固定当前进程的可运行 CPU。 */
static void xbenchApplyCpuPinFromEnv(void)
{
	const char* sCpu = getenv("XBENCH_PIN_CPU");
	char* pEnd;
	unsigned long iCpu;

	if ( (sCpu == NULL) || (sCpu[0] == '\0') ) {
		return;
	}
	errno = 0;
	pEnd = NULL;
	iCpu = strtoul(sCpu, &pEnd, 10);
	if (
		(errno != 0) ||
		(pEnd == sCpu) ||
		(*pEnd != '\0')
	) {
		fprintf(stderr, "bench_cpu_pin_error: invalid '%s'\n", sCpu);
		return;
	}

	#if defined(_WIN32) || defined(_WIN64)
		if ( iCpu >= (sizeof(DWORD_PTR) * 8u) ) {
			fprintf(
				stderr,
				"bench_cpu_pin_error: cpu %lu unsupported\n",
				iCpu
			);
			return;
		}
		if (
			!SetProcessAffinityMask(
				GetCurrentProcess(),
				((DWORD_PTR)1u) << iCpu
			)
		) {
			fprintf(
				stderr,
				"bench_cpu_pin_error: SetProcessAffinityMask failed (%lu)\n",
				(unsigned long)GetLastError()
			);
			return;
		}
	#else
		{
			cpu_set_t Set;

			CPU_ZERO(&Set);
			CPU_SET((int)iCpu, &Set);
			if ( sched_setaffinity(0, sizeof(Set), &Set) != 0 ) {
				fprintf(
					stderr,
					"bench_cpu_pin_error: sched_setaffinity(%lu) failed (%d)\n",
					iCpu,
					errno
				);
				return;
			}
		}
	#endif

	printf("bench_cpu_pin: %lu\n", iCpu);
}



/* 从环境变量读取严格的 32 位无符号整数。 */
static uint32_t xbenchEnvU32(const char* sName, uint32_t iDefault)
{
	char* pEnd;
	const char* sValue;
	unsigned long iValue;

	if ( (sName == NULL) || (sName[0] == '\0') ) {
		return iDefault;
	}
	sValue = getenv(sName);
	if ( (sValue == NULL) || (sValue[0] == '\0') ) {
		return iDefault;
	}
	errno = 0;
	pEnd = NULL;
	iValue = strtoul(sValue, &pEnd, 10);
	if (
		(errno != 0) ||
		(pEnd == sValue) ||
		(*pEnd != '\0') ||
		(iValue > UINT32_MAX)
	) {
		return iDefault;
	}
	return (uint32_t)iValue;
}



/* 输出一个可由汇总脚本读取的 64 位整数指标。 */
static void xbenchPrintMetricU64(const char* sLabel, uint64_t iValue)
{
	printf("%s: %" PRIu64 "\n", sLabel, iValue);
}



/* 输出一个可由汇总脚本读取的浮点指标。 */
static void xbenchPrintMetricDouble(const char* sLabel, double fValue)
{
	printf("%s: %.3f\n", sLabel, fValue);
}

#if defined(__GNUC__) && !defined(__TINYC__)
	#pragma GCC diagnostic pop
#endif

#endif
