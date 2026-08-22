#include "../common.h"



/* 输出当前线程借用的结构化错误。 */
static void exampleTlsDialFutureError(cstr sPrefix, const xerror* pError)
{
	fprintf(
		stderr,
		"%s: %s\n",
		sPrefix,
		pError != NULL ? xrtErrorMessage(pError) : "unknown error"
	);
}



/* 等待主动中止的 TLS Stream 释放全部后台网络资源。 */
static bool exampleTlsDialFutureClosed(xtlsstream* pStream)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( (xrtTlsStreamState(pStream) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pStream) != XTLS_STREAM_FAILED) ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 使用系统信任库以 Future 连接一个 TLS 主机名。 */
int main(int argc, char** argv)
{
	cstr sHost;
	unsigned long iPort = 443u;
	xx509store* pStore = NULL;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig TlsConfig;
	xtlsdialconfig DialConfig;
	xnetengineconfig EngineConfig;
	xnetengine* pEngine = NULL;
	xnetresolver* pResolver = NULL;
	xfuture* pFuture = NULL;
	xtlsstream* pStream = NULL;
	int iResult = 1;

	if ( argc < 2 ) {
		printf("usage: dial_future <host> [port]\n");
		return 0;
	}
	sHost = argv[1];
	if ( argc >= 3 ) {
		char* pEnd = NULL;

		iPort = strtoul(argv[2], &pEnd, 10);
		if ( (pEnd == argv[2]) || (*pEnd != 0) ||
			(iPort == 0) || (iPort > 65535u) ) {
			fprintf(stderr, "invalid port\n");
			goto Cleanup;
		}
	}

	pStore = xrtX509StoreSystem();
	if ( pStore == NULL ) {
		exampleTlsDialFutureError(
			"failed to load system trust store",
			xrtGetError()
		);
		goto Cleanup;
	}
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Store = pStore;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if ( pVerifier == NULL ) {
		exampleTlsDialFutureError(
			"failed to create TLS verifier",
			xrtGetError()
		);
		goto Cleanup;
	}
	xrtX509StoreFree(pStore);
	pStore = NULL;

	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		exampleTlsDialFutureError(
			"failed to start network engine",
			xrtGetError()
		);
		goto Cleanup;
	}
	pResolver = xrtNetResolverCreate(NULL);
	if ( pResolver == NULL ) {
		exampleTlsDialFutureError(
			"failed to create resolver",
			xrtGetError()
		);
		goto Cleanup;
	}

	xrtTlsClientConfigInit(&TlsConfig);
	TlsConfig.Verifier = pVerifier;
	xrtTlsDialConfigInit(&DialConfig);
	DialConfig.Timeout = UINT64_C(15000000);
	pFuture = xrtTlsDialAsync(
		pEngine,
		pResolver,
		sHost,
		(uint16)iPort,
		&TlsConfig,
		&DialConfig,
		NULL,
		NULL
	);
	if ( pFuture == NULL ) {
		exampleTlsDialFutureError(
			"failed to submit TLS dial",
			xrtGetError()
		);
		goto Cleanup;
	}
	if ( xrtFutureWaitFor(
		pFuture,
		UINT64_C(16000000)
	) != XWAIT_OK ) {
		(void)xrtFutureCancel(pFuture);
		exampleTlsDialFutureError(
			"TLS dial wait failed",
			xrtGetError()
		);
		goto Cleanup;
	}
	if ( xrtFutureState(pFuture) != XFUTURE_RESOLVED ) {
		exampleTlsDialFutureError(
			"TLS dial failed",
			xrtFutureError(pFuture)
		);
		goto Cleanup;
	}
	pStream = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
	if ( pStream == NULL ) {
		exampleTlsDialFutureError(
			"failed to retain TLS stream",
			xrtGetError()
		);
		goto Cleanup;
	}
	xrtFutureDestroy(pFuture);
	pFuture = NULL;
	printf("TLS stream opened for %s:%lu\n", sHost, iPort);
	iResult = 0;

Cleanup:
	if ( pStream != NULL ) {
		(void)xrtTlsStreamAbort(pStream);
		if ( !exampleTlsDialFutureClosed(pStream) ) {
			fprintf(stderr, "TLS stream did not close\n");
			iResult = 1;
		}
		xrtTlsStreamDestroy(pStream);
	}
	if ( pFuture != NULL ) {
		(void)xrtFutureCancel(pFuture);
		xrtFutureDestroy(pFuture);
	}
	if ( (pResolver != NULL) && !xrtNetResolverDestroy(pResolver) ) {
		exampleTlsDialFutureError(
			"failed to destroy resolver",
			xrtGetError()
		);
		iResult = 1;
	}
	if ( (pEngine != NULL) && !xrtNetEngineDestroy(pEngine) ) {
		exampleTlsDialFutureError(
			"failed to destroy network engine",
			xrtGetError()
		);
		iResult = 1;
	}
	xrtTlsVerifierRelease(pVerifier);
	xrtX509StoreFree(pStore);
	return iResult;
}
