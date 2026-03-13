#ifndef XNET2_BENCH_COMMON_H
#define XNET2_BENCH_COMMON_H

#include <inttypes.h>
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
		return ((uint64)tNow.tv_sec * 1000000000ULL) + (uint64)tNow.tv_nsec;
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
	uint32_t iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32_t i = 0; i < iLoops; ++i ) {
		if ( xbenchAtomicLoad(pValue) >= iExpectMin ) return true;
		xbenchSleepMs(10u);
	}
	return xbenchAtomicLoad(pValue) >= iExpectMin;
}

static bool xbenchWaitEquals(volatile long* pValue, long iExpect, uint32_t iTimeoutMs)
{
	uint32_t iLoops = (iTimeoutMs / 10u) + 1u;
	for ( uint32_t i = 0; i < iLoops; ++i ) {
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
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA tWSA;
		return WSAStartup(MAKEWORD(2, 2), &tWSA) == 0;
	#else
		return true;
	#endif
}

static void xbenchNetUnit(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif
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
