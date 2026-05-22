#include "bench_v1_common.h"

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iEchoBytes;
} __bench_v1_echo_tls_server_ctx;

typedef struct {
	volatile long iOpenCount;
	volatile long iCloseCount;
	volatile long iErrorCount;
	volatile long iConnectOk;
} __bench_v1_echo_tls_client_ctx;

static int __benchV1EchoTlsLatencyCmp(const void* pA, const void* pB)
{
	uint64_t a = *(const uint64_t*)pA;
	uint64_t b = *(const uint64_t*)pB;
	return (a > b) - (a < b);
}

static void __benchV1EchoTlsServerOnAccept(ptr pOwner, xnetconn* pConn)
{
	__bench_v1_echo_tls_server_ctx* pCtx;
	(void)pConn;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_server_ctx*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iOpenCount);
}

static void __benchV1EchoTlsServerOnRecv(ptr pOwner, xnetconn* pConn, const char* pData, size_t iLen)
{
	__bench_v1_echo_tls_server_ctx* pCtx;
	if ( !pOwner || !pConn || !pData || iLen == 0u ) return;
	pCtx = (__bench_v1_echo_tls_server_ctx*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( xrtTcpServerSend((xtcpserver*)pOwner, pConn->iId, pData, iLen) == XRT_NET_OK && pCtx ) {
		xbenchAtomicAdd(&pCtx->iEchoBytes, (long)iLen);
	}
}

static void __benchV1EchoTlsServerOnClose(ptr pOwner, xnetconn* pConn)
{
	__bench_v1_echo_tls_server_ctx* pCtx;
	(void)pConn;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_server_ctx*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchV1EchoTlsServerOnError(ptr pOwner, xnetconn* pConn, int iErrorCode)
{
	__bench_v1_echo_tls_server_ctx* pCtx;
	(void)pConn;
	(void)iErrorCode;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_server_ctx*)xrtTcpServerGetUserData((xtcpserver*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchV1EchoTlsClientOnConnect(ptr pOwner, xnetconn* pConn, bool bSuccess)
{
	__bench_v1_echo_tls_client_ctx* pCtx;
	(void)pConn;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_client_ctx*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( !pCtx ) return;
	xbenchAtomicInc(&pCtx->iOpenCount);
	if ( bSuccess ) xbenchAtomicInc(&pCtx->iConnectOk);
	else xbenchAtomicInc(&pCtx->iErrorCount);
}

static void __benchV1EchoTlsClientOnClose(ptr pOwner, xnetconn* pConn)
{
	__bench_v1_echo_tls_client_ctx* pCtx;
	(void)pConn;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_client_ctx*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iCloseCount);
}

static void __benchV1EchoTlsClientOnError(ptr pOwner, xnetconn* pConn, int iErrorCode)
{
	__bench_v1_echo_tls_client_ctx* pCtx;
	(void)pConn;
	(void)iErrorCode;
	if ( !pOwner ) return;
	pCtx = (__bench_v1_echo_tls_client_ctx*)xrtTcpClientGetUserData((xtcpclient*)pOwner);
	if ( pCtx ) xbenchAtomicInc(&pCtx->iErrorCount);
}

int main(int argc, char** argv)
{
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 2000u);
	uint32_t iMessageSize = xbenchArgU32(argc, argv, 2, 64u);
	const char* sCertFile = argc > 3 ? argv[3] : "dev/xnet2_tls_test_cert.pem";
	const char* sKeyFile = argc > 4 ? argv[4] : "dev/xnet2_tls_test_key.pem";
	uint16_t iPort = (uint16_t)xbenchArgU32(argc, argv, 5, 19182u);
	xnetconfig tCfg;
	xnetevents tSrvEvents;
	xnetevents tCliEvents;
	xtlsconfig tServerTls;
	xtlsconfig tClientTls;
	__bench_v1_echo_tls_server_ctx tSrvCtx;
	__bench_v1_echo_tls_client_ctx tCliCtx;
	xtcpserver* pServer = NULL;
	xtcpclient* pClient = NULL;
	char* pPayload = NULL;
	char* pRecvBuf = NULL;
	uint64_t* arrLatencyNs = NULL;
	xbenchtimer tRunTimer;
	uint64_t iElapsedNs = 0u;
	double fP50 = 0.0;
	double fP95 = 0.0;
	double fP99 = 0.0;
	int iExitCode = 0;

	setvbuf(stdout, NULL, _IONBF, 0);
	setvbuf(stderr, NULL, _IONBF, 0);

	printf("xnet-v1 bench_echo_tls\n");
	printf("iterations=%u message_size=%u cert=%s key=%s port=%u\n",
		(unsigned)iIterations, (unsigned)iMessageSize, sCertFile, sKeyFile, (unsigned)iPort);

	if ( !xbenchFileExists(sCertFile) || !xbenchFileExists(sKeyFile) ) {
		printf("tls fixture files missing\n");
		return 1;
	}
	if ( !xbenchNetInit() ) return 1;
	if ( !xrtInit() ) {
		xbenchNetUnit();
		return 1;
	}
	memset(&tSrvCtx, 0, sizeof(tSrvCtx));
	memset(&tCliCtx, 0, sizeof(tCliCtx));
	memset(&tSrvEvents, 0, sizeof(tSrvEvents));
	memset(&tCliEvents, 0, sizeof(tCliEvents));
	xbenchV1ConfigInit(&tCfg);
	xbenchV1TlsInit(&tServerTls, sCertFile, sKeyFile, NULL);
	xbenchV1TlsInit(&tClientTls, NULL, NULL, "127.0.0.1");
	tSrvEvents.OnAccept = __benchV1EchoTlsServerOnAccept;
	tSrvEvents.OnRecv = __benchV1EchoTlsServerOnRecv;
	tSrvEvents.OnClose = __benchV1EchoTlsServerOnClose;
	tSrvEvents.OnError = __benchV1EchoTlsServerOnError;
	tCliEvents.OnConnect = __benchV1EchoTlsClientOnConnect;
	tCliEvents.OnClose = __benchV1EchoTlsClientOnClose;
	tCliEvents.OnError = __benchV1EchoTlsClientOnError;

	pPayload = (char*)malloc(iMessageSize);
	pRecvBuf = (char*)malloc(iMessageSize);
	arrLatencyNs = (uint64_t*)calloc(iIterations, sizeof(uint64_t));
	if ( !pPayload || !pRecvBuf || !arrLatencyNs ) goto cleanup;
	memset(pPayload, 'T', iMessageSize);

	pServer = xrtTcpServerCreate("127.0.0.1", iPort, &tCfg, &tSrvEvents);
	if ( !pServer ) {
		printf("setup_failed: server_create\n");
		iExitCode = 2;
		goto cleanup;
	}
	xrtTcpServerSetUserData(pServer, &tSrvCtx);
	if ( xrtTcpServerEnableTLS(pServer, &tServerTls) != XRT_NET_OK ) {
		printf("setup_failed: server_enable_tls\n");
		iExitCode = 2;
		goto cleanup;
	}
	if ( xrtTcpServerStart(pServer) != XRT_NET_OK ) {
		printf("setup_failed: server_start\n");
		iExitCode = 2;
		goto cleanup;
	}

	pClient = xrtTcpClientCreate("127.0.0.1", iPort, &tCfg, &tCliEvents);
	if ( !pClient ) {
		printf("setup_failed: client_create\n");
		iExitCode = 2;
		goto cleanup;
	}
	xrtTcpClientSetUserData(pClient, &tCliCtx);
	xrtTcpClientEnableSyncRecv(pClient);
	if ( xrtTcpClientEnableTLS(pClient, &tClientTls) != XRT_NET_OK ) {
		printf("setup_failed: client_enable_tls\n");
		iExitCode = 2;
		goto cleanup;
	}
	if ( xrtTcpClientConnect(pClient) != XRT_NET_OK ) {
		printf("setup_failed: client_connect\n");
		iExitCode = 2;
		goto cleanup;
	}
	if ( !xbenchV1WaitTcpConnected(pClient, 10000u) ) {
		printf("setup_failed: client_connected_timeout\n");
		iExitCode = 2;
		goto cleanup;
	}

	xbenchTimerStart(&tRunTimer);
	for ( uint32_t i = 0; i < iIterations; ++i ) {
		size_t iRecvLen = 0u;
		uint64_t iStartNs;
		if ( xrtTcpClientSend(pClient, pPayload, iMessageSize) != XRT_NET_OK ) {
			iExitCode = 2;
			break;
		}
		iStartNs = xbenchNowNs();
		if ( xrtTcpClientRecvSync(pClient, pRecvBuf, iMessageSize, &iRecvLen, 10000) != XRT_NET_OK || iRecvLen != iMessageSize ) {
			iExitCode = 2;
			break;
		}
		arrLatencyNs[i] = xbenchNowNs() - iStartNs;
	}
	xbenchTimerStop(&tRunTimer);
	iElapsedNs = xbenchTimerElapsedNs(&tRunTimer);

	if ( iExitCode == 0 ) {
		qsort(arrLatencyNs, iIterations, sizeof(uint64_t), __benchV1EchoTlsLatencyCmp);
		if ( iIterations > 0u ) {
			fP50 = (double)arrLatencyNs[(iIterations - 1u) / 2u] / 1000.0;
			fP95 = (double)arrLatencyNs[(uint32_t)((double)(iIterations - 1u) * 0.95)] / 1000.0;
			fP99 = (double)arrLatencyNs[(uint32_t)((double)(iIterations - 1u) * 0.99)] / 1000.0;
		}
		xbenchPrintMetricU64("echo_elapsed_ns", iElapsedNs);
		xbenchPrintMetricDouble("messages_per_sec", xbenchSafeRate(iIterations, iElapsedNs));
		xbenchPrintMetricDouble("bytes_per_sec", xbenchSafeRate((uint64_t)iIterations * iMessageSize, iElapsedNs));
		xbenchPrintMetricDouble("latency_p50_us", fP50);
		xbenchPrintMetricDouble("latency_p95_us", fP95);
		xbenchPrintMetricDouble("latency_p99_us", fP99);
		printf("client_open=%ld server_open=%ld recv=%u server_echo_bytes=%ld\n",
			xbenchAtomicLoad(&tCliCtx.iOpenCount),
			xbenchAtomicLoad(&tSrvCtx.iOpenCount),
			(unsigned)iIterations,
			xbenchAtomicLoad(&tSrvCtx.iEchoBytes));
		printf("client_errors=%ld server_errors=%ld\n",
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
	} else {
		printf("incomplete_run: expected=%u client_errors=%ld server_errors=%ld\n",
			(unsigned)iIterations,
			xbenchAtomicLoad(&tCliCtx.iErrorCount),
			xbenchAtomicLoad(&tSrvCtx.iErrorCount));
	}

cleanup:
	if ( pClient ) {
		xrtTcpClientDisconnect(pClient);
		xrtTcpClientDestroy(pClient);
	}
	if ( pServer ) {
		xrtTcpServerStop(pServer);
		xrtTcpServerDestroy(pServer);
	}
	free(arrLatencyNs);
	free(pRecvBuf);
	free(pPayload);
	xrtUnit();
	xbenchNetUnit();
	return iExitCode;
}
