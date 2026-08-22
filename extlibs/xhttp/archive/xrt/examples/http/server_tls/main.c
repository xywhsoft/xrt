#include "../../tls/common.h"



/* 比较请求 target 与零结尾文本。 */
static bool exampleHttpsEqual(xstrview Value, cstr sText)
{
	size_t iSize = strlen(sText);

	return (Value.Size == iSize) &&
		(memcmp(Value.Data, sText, iSize) == 0);
}



/* 用直接响应入口处理常见 JSON 和文本结果。 */
static void exampleHttpsRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	xstrview Target = xrtHttpServerRequestTarget(pRequest);

	(void)pServer;
	(void)pData;
	if ( exampleHttpsEqual(Target, "/health") ) {
		(void)xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_OK,
			XRT_STR_LITERAL(
				"application/json; charset=utf-8"
			),
			XRT_BYTES_LITERAL(
				"{\"code\":200,\"msg\":\"OK\"}"
			)
		);
		return;
	}
	(void)xrtHttpConnReply(
		pConnection,
		XHTTP_STATUS_NOT_FOUND,
		XRT_STR_LITERAL("text/plain; charset=utf-8"),
		XRT_BYTES_LITERAL("Not Found")
	);
}



/* 输出 HTTPS 连接的稳定错误域、操作和根因消息。 */
static void exampleHttpsError(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xerror* pError,
	ptr pData
)
{
	(void)pServer;
	(void)pConnection;
	(void)pData;
	fprintf(
		stderr,
		"HTTPS error [%s/%s]: %s\n",
		xrtErrorDomain(pError),
		xrtErrorOperation(pError),
		xrtErrorMessage(pError)
	);
}



/* 在截止时间内等待 Server 完全关闭。 */
static bool exampleHttpsWaitClosed(xhttpserver* pServer)
{
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(5000000));

	while ( xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED ) {
		if ( xrtDeadlineExpired(iDeadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 加载 DER 身份并运行一个可由浏览器或 curl 访问的 HTTPS 服务。 */
int main(int argc, char** argv)
{
	uint8* pCertificateData = NULL;
	uint8* pPrivateData = NULL;
	size_t iCertificateSize = 0;
	size_t iPrivateSize = 0;
	xbytesview Certificate;
	xtlsidentity* pIdentity = NULL;
	xnetengineconfig EngineConfig;
	xhttpserverconfig ServerConfig;
	xhttpservertlsconfig TlsConfig;
	xhttpserverevents Events;
	xnetengine* pEngine = NULL;
	xhttpserver* pServer = NULL;
	xnetaddr Address;
	str sEndpoint = NULL;
	unsigned long iPort = 8443u;
	int iResult = 1;

	if ( argc < 4 ) {
		printf(
			"usage: server_tls <rsa|p256|p384|ed25519> "
			"<certificate.der> <private.der> [port]\n"
		);
		return 0;
	}

	/* 可选端口必须是非零 TCP 端口。 */
	if ( argc >= 5 ) {
		char* pEnd = NULL;

		iPort = strtoul(argv[4], &pEnd, 10);
		if ( (pEnd == argv[4]) || (*pEnd != '\0') ||
			(iPort == 0) || (iPort > 65535u) ) {
			fprintf(stderr, "invalid port\n");
			goto Cleanup;
		}
	}

	/* 身份输入只需保持到不可变身份对象创建完成。 */
	if ( !exampleTlsReadFile(
		argv[2],
		&pCertificateData,
		&iCertificateSize
	) || !exampleTlsReadFile(
		argv[3],
		&pPrivateData,
		&iPrivateSize
	) ) {
		fprintf(stderr, "failed to read DER identity\n");
		goto Cleanup;
	}
	Certificate = (xbytesview) {
		pCertificateData,
		iCertificateSize
	};
	pIdentity = exampleTlsIdentity(
		argv[1],
		&Certificate,
		1u,
		(xbytesview) {
			pPrivateData,
			iPrivateSize
		}
	);
	if ( pIdentity == NULL ) {
		fprintf(stderr, "failed to create TLS identity\n");
		goto Cleanup;
	}

	/* Engine 与 HTTP 配置沿用标准安全限额。 */
	xrtNetEngineConfigInit(&EngineConfig);
	pEngine = xrtNetEngineCreate(&EngineConfig);
	if ( (pEngine == NULL) || !xrtNetEngineStart(pEngine) ) {
		goto Cleanup;
	}
	xrtHttpServerConfigInit(&ServerConfig);
	if ( !xrtNetAddrLoopback(
		&ServerConfig.Network.Listen.Address,
		XNET_FAMILY_IPV4,
		(uint16)iPort
	) ) {
		goto Cleanup;
	}

	/* HTTPS 层保留身份并默认只协商 HTTP/1.1。 */
	xrtHttpServerTlsConfigInit(&TlsConfig);
	TlsConfig.Handshake.Identity = pIdentity;
	xrtHttpServerEventsInit(&Events);
	Events.Request = exampleHttpsRequest;
	Events.Error = exampleHttpsError;
	pServer = xrtHttpServerStartTls(
		pEngine,
		&ServerConfig,
		&TlsConfig,
		&Events
	);
	if ( (pServer == NULL) ||
		!xrtHttpServerLocal(pServer, 0, &Address) ) {
		goto Cleanup;
	}
	xrtTlsIdentityRelease(pIdentity);
	pIdentity = NULL;

	/* 打印实际监听地址并等待人工结束。 */
	sEndpoint = xrtNetAddrEndpointString(&Address);
	if ( sEndpoint == NULL ) {
		goto Cleanup;
	}
	printf(
		"listening on https://%s/health\n"
		"press Enter to drain\n",
		sEndpoint
	);
	xrtFree(sEndpoint);
	sEndpoint = NULL;
	(void)getchar();

	/* 排空现有请求后等待 Listener 和全部连接退出。 */
	if ( !xrtHttpServerDrain(pServer) ||
		!exampleHttpsWaitClosed(pServer) ) {
		goto Cleanup;
	}
	iResult = 0;

Cleanup:
	xrtFree(sEndpoint);
	if ( (pServer != NULL) &&
		(xrtHttpServerState(pServer) != XHTTP_SERVER_CLOSED) ) {
		(void)xrtHttpServerAbort(pServer);
		if ( !exampleHttpsWaitClosed(pServer) ) {
			iResult = 2;
		}
	}
	xrtHttpServerDestroy(pServer);
	if ( (pEngine != NULL) &&
		!xrtNetEngineDestroy(pEngine) &&
		(iResult == 0) ) {
		iResult = 2;
	}
	xrtTlsIdentityRelease(pIdentity);
	free(pPrivateData);
	free(pCertificateData);
	return iResult;
}
