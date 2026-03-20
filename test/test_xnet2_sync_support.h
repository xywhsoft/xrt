#ifndef XRT_TEST_XNET2_SYNC_SUPPORT_H
#define XRT_TEST_XNET2_SYNC_SUPPORT_H

#if defined(_WIN32) || defined(_WIN64)
	#include <windows.h>
#else
	#include <time.h>
	#include <unistd.h>
#endif

enum {
	__TEST_XNET2_SYNC_WAIT_TIMEOUT_MS = 30,
	__TEST_XNET2_SYNC_WAIT_DEADLINE_MS = 35,
	__TEST_XNET2_SYNC_RETRY_WINDOW_MS = 250,
};


static void __Test_XNet2_SyncSleepMs(uint32 iDelayMs)
{
	#if defined(_WIN32) || defined(_WIN64)
		Sleep(iDelayMs);
	#else
		usleep((useconds_t)iDelayMs * 1000u);
	#endif
}

static long __Test_XNet2_SyncAtomicInc(volatile long* pValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		return InterlockedIncrement((volatile LONG*)pValue);
	#else
		return __sync_add_and_fetch(pValue, 1);
	#endif
}

static void __Test_XNet2_SyncAtomicStore(volatile long* pValue, long iValue)
{
	#if defined(_WIN32) || defined(_WIN64)
		InterlockedExchange((volatile LONG*)pValue, iValue);
	#else
		__sync_lock_test_and_set(pValue, iValue);
	#endif
}

static int64_t __Test_XNet2_SyncNowMs(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		return (int64_t)GetTickCount64();
	#else
		struct timespec tNow;
		if ( clock_gettime(CLOCK_MONOTONIC, &tNow) != 0 ) return 0;
		return ((int64_t)tNow.tv_sec * 1000LL) + ((int64_t)tNow.tv_nsec / 1000000LL);
	#endif
}

static int64_t __Test_XNet2_SyncDeadlineFromTimer(uint32 iDelayMs)
{
	return (int64_t)(xrtTimer() * 1000.0) + (int64_t)iDelayMs;
}

static xnet_result __Test_XNet2_SyncWaitSourceUntilDelay(xnetwaitsrc* pSrc, uint32 iDelayMs)
{
	if ( !pSrc ) return XRT_NET_ERROR;
	return xrtNetWaitSourceWaitUntil(pSrc, __Test_XNet2_SyncDeadlineFromTimer(iDelayMs));
}

static xnet_result __Test_XNet2_SyncWaitSourceCoUntilDelay(xnetwaitsrc* pSrc, uint32 iDelayMs)
{
	if ( !pSrc ) return XRT_NET_ERROR;
	return xrtNetWaitSourceWaitCoUntil(pSrc, __Test_XNet2_SyncDeadlineFromTimer(iDelayMs));
}

static xnet_result __Test_XNet2_SyncRetryWaitSourceStreamSync(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs);

static bool __Test_XNet2_SyncPostStreamTaskDelayed(xnetengine* pEngine, xnetstream* pStream, uint32 iDelayMs, void (*Proc)(xnetworker*, ptr))
{
	if ( !pEngine || !pStream || !Proc ) return false;
	return xrtNetEnginePostDelayed(
		pEngine,
		pStream->pWorker ? pStream->pWorker->iId : 0,
		iDelayMs,
		Proc,
		pStream) == XRT_NET_OK;
}

static xnet_result __Test_XNet2_SyncPostStreamTaskAndRetryWaitSource(
	xnetengine* pEngine,
	xnetstream* pStream,
	uint32 iDelayMs,
	void (*Proc)(xnetworker*, ptr),
	uint32 iWaitKind,
	uint32 iTimeoutMs)
{
	if ( !__Test_XNet2_SyncPostStreamTaskDelayed(pEngine, pStream, iDelayMs, Proc) ) {
		return XRT_NET_ERROR;
	}
	return __Test_XNet2_SyncRetryWaitSourceStreamSync(pStream, iWaitKind, iTimeoutMs);
}

static bool __Test_XNet2_SyncDgramPacketTextEquals(const xnetdgrampkt* pPacket, const char* sText)
{
	char aBuf[128];
	size_t iWantLen;
	size_t iCopyLen;

	if ( !pPacket || !sText ) return false;
	memset(aBuf, 0, sizeof(aBuf));
	iWantLen = strlen(sText);
	iCopyLen = xrtNetDgramPacketPeek(pPacket, aBuf, sizeof(aBuf) - 1u);
	return iCopyLen == iWantLen && memcmp(aBuf, sText, iWantLen) == 0;
}

static bool __Test_XNet2_SyncWaitStreamOpen(xnetstream* pStream, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pStream ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0 ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(5);
	}

	return (pStream->iState & __XNET_STREAM_STATE_OPEN_EMITTED) != 0;
}

static void __Test_XNet2_SyncMarkStreamOpen(xnetstream* pStream)
{
	if ( !pStream ) return;
	pStream->iState |= __XNET_STREAM_STATE_OPEN_EMITTED;
}

static bool __Test_XNet2_SyncWaitStreamWaitCleared(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pStream || iWaitKind >= __XNET_STREAM_WAIT_COUNT ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( pStream->arrSyncWait[iWaitKind].pfnWait == NULL ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return pStream->arrSyncWait[iWaitKind].pfnWait == NULL;
}

static xnet_result __Test_XNet2_SyncRetryWaitSourceStreamSync(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_ERROR;

	if ( !pStream ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);

		iStatus = xrtNetWaitSourceWait(&tSrc);
		if ( iStatus != XRT_NET_ERROR ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static xnet_result __Test_XNet2_SyncRetryStreamTimeoutSync(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_ERROR;

	if ( !pStream ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		iStatus = xrtNetStreamWaitTimeoutEx(pStream, iWaitKind, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
		if ( iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_CLOSED ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static xnet_result __Test_XNet2_SyncRetryWaitSourceStreamValueTimeoutSync(xnetstream* pStream, uint32 iWaitKind, ptr* ppValue, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_ERROR;

	if ( ppValue ) *ppValue = NULL;
	if ( !pStream ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);

		if ( ppValue ) *ppValue = (ptr)1;
		iStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, ppValue);
		if ( iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_CLOSED ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static xnet_result __Test_XNet2_SyncRetryWaitSourceStreamTimeoutSync(xnetstream* pStream, uint32 iWaitKind, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_ERROR;

	if ( !pStream ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		xnetwaitsrc tSrc = xrtNetWaitSourceStream(pStream, iWaitKind);

		iStatus = xrtNetWaitSourceWaitTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS);
		if ( iStatus == XRT_NET_TIMEOUT || iStatus == XRT_NET_CLOSED ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static xnet_result __Test_XNet2_SyncRetryDgramRecvSync(xdgramsock* pSock, xnetdgrampkt** ppPacket, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_TIMEOUT;

	if ( ppPacket ) *ppPacket = NULL;
	if ( !pSock ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( ppPacket ) *ppPacket = NULL;
		iStatus = xrtNetDgramRecvUntil(pSock, __Test_XNet2_SyncNowMs() + __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, ppPacket);
		if ( iStatus == XRT_NET_OK || iStatus == XRT_NET_CLOSED ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static xnet_result __Test_XNet2_SyncRetryWaitSourceDgramSync(xdgramsock* pSock, xnetdgrampkt** ppPacket, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;
	xnet_result iStatus = XRT_NET_TIMEOUT;

	if ( ppPacket ) *ppPacket = NULL;
	if ( !pSock ) return XRT_NET_ERROR;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		xnetwaitsrc tSrc = xrtNetWaitSourceDgramRecv(pSock);

		if ( ppPacket ) *ppPacket = NULL;
		iStatus = xrtNetWaitSourceWaitValueTimeout(&tSrc, __TEST_XNET2_SYNC_WAIT_TIMEOUT_MS, (ptr*)ppPacket);
		if ( iStatus == XRT_NET_OK || iStatus == XRT_NET_CLOSED ) {
			return iStatus;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return iStatus;
}

static bool __Test_XNet2_SyncWaitDgramRecvWaitCleared(xdgramsock* pSock, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pSock ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( pSock->tRecvWait.pfnWait == NULL ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(1);
	}

	return pSock->tRecvWait.pfnWait == NULL;
}

static bool __Test_XNet2_SyncWaitListenerAcceptRegistered(xnetlistener* pListener, uint32 iTimeoutMs)
{
	int64_t iDeadlineMs = __Test_XNet2_SyncNowMs() + (int64_t)iTimeoutMs;

	if ( !pListener ) return false;

	while ( __Test_XNet2_SyncNowMs() < iDeadlineMs ) {
		if ( pListener->tAcceptWait.pfnWait != NULL ) {
			return true;
		}
		__Test_XNet2_SyncSleepMs(5);
	}

	return pListener->tAcceptWait.pfnWait != NULL;
}

#endif
