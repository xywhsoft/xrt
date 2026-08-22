#include "xrt.h"

#include <stdio.h>
#include <string.h>

static int wait_point(xfuture* pFuture, void** ppValue, const char* sStage, int iRound)
{
	void** ppBox;

	*ppValue = NULL;
	if ( pFuture == NULL ) {
		fprintf(stderr, "round %d: %s future is null\n", iRound, sStage);
		return 0;
	}
	if ( !xFutureWait(pFuture) ) {
		fprintf(stderr, "round %d: %s wait failed, status=%d, error=%s\n",
			iRound, sStage, xFutureStatus(pFuture), xFutureError(pFuture));
		xFutureRelease(pFuture);
		return 0;
	}
	ppBox = (void**)xFutureValue(pFuture);
	if ( ppBox == NULL || *ppBox == NULL ) {
		fprintf(stderr, "round %d: %s value is null, status=%d, error=%s\n",
			iRound, sStage, xFutureStatus(pFuture), xFutureError(pFuture));
		xFutureRelease(pFuture);
		return 0;
	}
	*ppValue = *ppBox;
	xFutureRelease(pFuture);
	return 1;
}

static int wait_text(xfuture* pFuture, const char* sExpected, const char* sStage, int iRound)
{
	char** ppText;
	int bOk;

	if ( pFuture == NULL ) {
		fprintf(stderr, "round %d: %s future is null\n", iRound, sStage);
		return 0;
	}
	if ( !xFutureWait(pFuture) ) {
		fprintf(stderr, "round %d: %s wait failed, status=%d, error=%s\n",
			iRound, sStage, xFutureStatus(pFuture), xFutureError(pFuture));
		xFutureRelease(pFuture);
		return 0;
	}
	ppText = (char**)xFutureValue(pFuture);
	bOk = ppText != NULL && *ppText != NULL && strcmp(*ppText, sExpected) == 0;
	if ( !bOk ) {
		fprintf(stderr, "round %d: %s text mismatch, status=%d, value=%s, error=%s\n",
			iRound,
			sStage,
			xFutureStatus(pFuture),
			(ppText && *ppText) ? *ppText : "<null>",
			xFutureError(pFuture));
	}
	xFutureRelease(pFuture);
	return bOk;
}

static int run_round(int iRound)
{
	xnetlistener* pListener = NULL;
	xnetstream* pClient = NULL;
	xnetstream* pPeer = NULL;
	xfuture* pAccept = NULL;
	xfuture* pRecv = NULL;
	int bOk = 0;

	pListener = xrtNetTcpListen("127.0.0.1", 0, 128);
	if ( pListener == NULL || xrtNetTcpListenerPort(pListener) == 0 ) {
		fprintf(stderr, "round %d: listen failed\n", iRound);
		goto cleanup;
	}
	pAccept = xrtNetTcpAcceptAsync(pListener, 8000);
	pClient = xrtNetTcpConnect("127.0.0.1", xrtNetTcpListenerPort(pListener), 8000);
	if ( pClient == NULL ) {
		fprintf(stderr, "round %d: connect failed: %s\n", iRound, xrtGetError());
		goto cleanup;
	}
	if ( !wait_point(pAccept, (void**)&pPeer, "accept", iRound) ) {
		pAccept = NULL;
		goto cleanup;
	}
	pAccept = NULL;
	pRecv = xrtNetStreamRecvTextAsync(pPeer, 0, 8000);
	if ( !xrtNetStreamSendText(pClient, "ping") || !xrtNetStreamDrain(pClient, 8000) ) {
		fprintf(stderr, "round %d: send or drain failed: %s\n", iRound, xrtGetError());
		goto cleanup;
	}
	if ( !wait_text(pRecv, "ping", "recv", iRound) ) {
		pRecv = NULL;
		goto cleanup;
	}
	pRecv = NULL;
	bOk = 1;

cleanup:
	if ( pAccept ) { xFutureRelease(pAccept); }
	if ( pRecv ) { xFutureRelease(pRecv); }
	if ( pClient ) { xrtNetTcpStreamDestroy(pClient); }
	if ( pPeer ) { xrtNetTcpStreamDestroy(pPeer); }
	if ( pListener ) { xrtNetTcpListenerDestroy(pListener); }
	return bOk;
}

int main(int argc, char** argv)
{
	int iRounds = argc > 1 ? atoi(argv[1]) : 1000;
	int i;

	xrtInit();
	for ( i = 1; i <= iRounds; ++i ) {
		if ( !run_round(i) ) {
			xrtUnit();
			return 1;
		}
	}
	xrtUnit();
	printf("xnet async stress ok: %d rounds\n", iRounds);
	return 0;
}
