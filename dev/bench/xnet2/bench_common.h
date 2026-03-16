#ifndef XNET2_BENCH_COMMON_H
#define XNET2_BENCH_COMMON_H

#if !defined(_WIN32) && !defined(_WIN64)
	#ifndef _GNU_SOURCE
		#define _GNU_SOURCE
	#endif
#endif

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
	#include <windows.h>
#else
	#include <errno.h>
	#include <pthread.h>
	#include <sched.h>
	#include <time.h>
	#include <unistd.h>
#endif

typedef struct {
	uint64_t iStartNs;
	uint64_t iEndNs;
} xbenchtimer;

static uint64_t xbenchNowNs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		static LARGE_INTEGER tFreq;
		LARGE_INTEGER tNow;
		if ( tFreq.QuadPart == 0 ) QueryPerformanceFrequency(&tFreq);
		QueryPerformanceCounter(&tNow);
		return (uint64_t)((tNow.QuadPart * 1000000000ULL) / tFreq.QuadPart);
	#else
		struct timespec tNow;
		clock_gettime(CLOCK_MONOTONIC, &tNow);
		return ((uint64_t)tNow.tv_sec * 1000000000ULL) + (uint64_t)tNow.tv_nsec;
	#endif
}

static void xbenchTimerStart(xbenchtimer* pTimer)
{
	if ( !pTimer ) return;
	pTimer->iStartNs = xbenchNowNs();
	pTimer->iEndNs = 0u;
}

static void xbenchTimerStop(xbenchtimer* pTimer)
{
	if ( !pTimer ) return;
	pTimer->iEndNs = xbenchNowNs();
}

static uint64_t xbenchTimerElapsedNs(const xbenchtimer* pTimer)
{
	if ( !pTimer || pTimer->iStartNs == 0u ) return 0u;
	if ( pTimer->iEndNs > pTimer->iStartNs ) return pTimer->iEndNs - pTimer->iStartNs;
	return xbenchNowNs() - pTimer->iStartNs;
}

static double xbenchNsToSec(uint64_t iNs)
{
	return (double)iNs / 1000000000.0;
}

static double xbenchSafeRate(uint64_t iCount, uint64_t iNs)
{
	if ( iNs == 0u ) return 0.0;
	return (double)iCount / xbenchNsToSec(iNs);
}

static void xbenchSleepMs(uint32_t iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long xbenchAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static long xbenchAtomicAdd(volatile long* pValue, long iDelta)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedExchangeAdd((volatile LONG*)pValue, iDelta) + iDelta;
	#else
		return __sync_add_and_fetch(pValue, iDelta);
	#endif
}

static long xbenchAtomicLoad(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedCompareExchange((volatile LONG*)pValue, 0, 0);
	#else
		return __sync_add_and_fetch(pValue, 0);
	#endif
}

static bool xbenchDeadlineReached(uint64_t iDeadlineNs)
{
	return iDeadlineNs > 0u && xbenchNowNs() >= iDeadlineNs;
}

static uint64_t xbenchDeadlineAfterMs(uint32_t iTimeoutMs)
{
	if ( iTimeoutMs == 0u ) return xbenchNowNs();
	return xbenchNowNs() + ((uint64_t)iTimeoutMs * 1000000ULL);
}

static uint64_t xbenchAtomicLoadU64(volatile uint64_t* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)pValue, 0, 0);
	#else
		return (uint64_t)__sync_val_compare_and_swap(pValue, 0u, 0u);
	#endif
}

static bool xbenchAtomicSetOnceU64(volatile uint64_t* pValue, uint64_t iValue)
{
	if ( !pValue || iValue == 0u ) return false;
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64_t)InterlockedCompareExchange64((volatile LONG64*)pValue, (LONG64)iValue, 0) == 0u;
	#else
		return (uint64_t)__sync_val_compare_and_swap(pValue, 0u, iValue) == 0u;
	#endif
}

static long xbenchAtomicMax(volatile long* pValue, long iCandidate)
{
	long iPrev = xbenchAtomicLoad(pValue);
	while ( iCandidate > iPrev ) {
		#if defined(_WIN32) || defined(_WIN64)
			long iSeen = InterlockedCompareExchange((volatile LONG*)pValue, iCandidate, iPrev);
		#else
			long iSeen = __sync_val_compare_and_swap(pValue, iPrev, iCandidate);
		#endif
		if ( iSeen == iPrev ) return iCandidate;
		iPrev = iSeen;
	}
	return iPrev;
}

static bool xbenchWaitMin(volatile long* pValue, long iExpectMin, uint32_t iTimeoutMs)
{
	uint64_t iDeadlineNs = xbenchDeadlineAfterMs(iTimeoutMs);
	while ( !xbenchDeadlineReached(iDeadlineNs) ) {
		if ( xbenchAtomicLoad(pValue) >= iExpectMin ) return true;
		xbenchSleepMs(10u);
	}
	return xbenchAtomicLoad(pValue) >= iExpectMin;
}

static bool xbenchWaitEquals(volatile long* pValue, long iExpect, uint32_t iTimeoutMs)
{
	uint64_t iDeadlineNs = xbenchDeadlineAfterMs(iTimeoutMs);
	while ( !xbenchDeadlineReached(iDeadlineNs) ) {
		if ( xbenchAtomicLoad(pValue) == iExpect ) return true;
		xbenchSleepMs(10u);
	}
	return xbenchAtomicLoad(pValue) == iExpect;
}

static uint32_t xbenchArgU32(int argc, char** argv, int iIndex, uint32_t iDefault)
{
	if ( argc <= iIndex || !argv || !argv[iIndex] || argv[iIndex][0] == '\0' ) return iDefault;
	return (uint32_t)strtoul(argv[iIndex], NULL, 10);
}

static uint64_t xbenchArgU64(int argc, char** argv, int iIndex, uint64_t iDefault)
{
	if ( argc <= iIndex || !argv || !argv[iIndex] || argv[iIndex][0] == '\0' ) return iDefault;
	#if defined(_WIN32) || defined(_WIN64)
		return (uint64_t)_strtoui64(argv[iIndex], NULL, 10);
	#else
		return (uint64_t)strtoull(argv[iIndex], NULL, 10);
	#endif
}

static bool xbenchFileExists(const char* sPath)
{
	FILE* pFile;
	if ( !sPath || sPath[0] == '\0' ) return false;
	pFile = fopen(sPath, "rb");
	if ( !pFile ) return false;
	fclose(pFile);
	return true;
}

static bool xbenchNetInit(void)
{
	const char* sCpuPin = getenv("XBENCH_PIN_CPU");

	#if defined(_WIN32) || defined(_WIN64)
		WSADATA tWSA;
		if ( WSAStartup(MAKEWORD(2, 2), &tWSA) != 0 ) return false;
	#else
		/* no-op */
	#endif

	if ( sCpuPin && sCpuPin[0] != '\0' ) {
		char* pEnd = NULL;
		unsigned long iCpu = strtoul(sCpuPin, &pEnd, 10);
		if ( pEnd == sCpuPin || *pEnd != '\0' ) {
			fprintf(stderr, "bench_cpu_pin_error: invalid '%s'\n", sCpuPin);
		} else {
			#if defined(_WIN32) || defined(_WIN64)
				if ( iCpu >= (sizeof(DWORD_PTR) * 8u) ) {
					fprintf(stderr, "bench_cpu_pin_error: cpu %lu unsupported on this Windows build\n", iCpu);
				} else {
					DWORD_PTR iMask = ((DWORD_PTR)1u) << iCpu;
					if ( !SetProcessAffinityMask(GetCurrentProcess(), iMask) ) {
						fprintf(stderr, "bench_cpu_pin_error: SetProcessAffinityMask failed (%lu)\n", (unsigned long)GetLastError());
					} else {
						printf("bench_cpu_pin: %lu\n", iCpu);
					}
				}
			#else
				cpu_set_t tSet;
				CPU_ZERO(&tSet);
				CPU_SET((int)iCpu, &tSet);
				if ( sched_setaffinity(0, sizeof(tSet), &tSet) != 0 ) {
					fprintf(stderr, "bench_cpu_pin_error: sched_setaffinity(%lu) failed (%d)\n", iCpu, errno);
				} else {
					printf("bench_cpu_pin: %lu\n", iCpu);
				}
			#endif
		}
	}

	return true;
}

static void xbenchNetUnit(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif
}

static uint32_t xbenchEnvU32(const char* sName, uint32_t iDefault)
{
	const char* sValue;
	if ( !sName || sName[0] == '\0' ) return iDefault;
	sValue = getenv(sName);
	if ( !sValue || sValue[0] == '\0' ) return iDefault;
	return (uint32_t)strtoul(sValue, NULL, 10);
}

static void xbenchPrintMetricU64(const char* sLabel, uint64_t iValue)
{
	printf("%s: %" PRIu64 "\n", sLabel, iValue);
}

static void xbenchPrintMetricDouble(const char* sLabel, double fValue)
{
	printf("%s: %.3f\n", sLabel, fValue);
}

#endif
