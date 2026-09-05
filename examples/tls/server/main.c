#include "../common.h"



/* 范例验证器只接管信任决策，协议签名和 Finished 仍由 XRT 验证。 */
static xtlsverifydecision exampleTlsAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return ((pPeer != NULL) && (pPeer->Role == XTLS_SERVER) &&
		(pPeer->CertificateCount != 0)) ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}



/* 把一个会话的全部待发密文复制到对端并精确消费。 */
static bool exampleTlsMove(xtlssession* pSource, xtlssession* pTarget)
{
	xnetspan Span;

	while ( xrtTlsSessionSendSize(pSource) != 0 ) {
		if ( !xrtTlsSessionSendFront(pSource, &Span) ||
			(Span.Size == 0) || (xrtTlsSessionFeed(
				pTarget, Span.Data, Span.Size
			) != XTLS_OK) || !xrtTlsSessionSendConsume(
				pSource, Span.Size
			) ) {
			return false;
		}
	}
	return true;
}



/* 驱动双方并搬运密文，直到会话同时进入 READY。 */
static bool exampleTlsHandshake(
	xtlssession* pClient,
	xtlssession* pServer
)
{
	for ( size_t i = 0; i < 4096u; i++ ) {
		xtlsresult ClientResult = xrtTlsClientDrive(pClient);
		xtlsresult ServerResult = xrtTlsServerDrive(pServer);

		if ( ((ClientResult != XTLS_OK) &&
			 (ClientResult != XTLS_AGAIN)) ||
			((ServerResult != XTLS_OK) &&
			 (ServerResult != XTLS_AGAIN)) ||
			!exampleTlsMove(pClient, pServer) ||
			!exampleTlsMove(pServer, pClient) ) {
			return false;
		}
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
			return true;
		}
	}
	return false;
}



/* 发送一段应用数据，并在对端驱动后完整读取。 */
static bool exampleTlsTransfer(
	xtlssession* pSource,
	xtlssession* pTarget,
	bool bTargetServer,
	cstr sText
)
{
	char Output[64];
	size_t iSize = strlen(sText);
	size_t iWritten = 0;
	size_t iRead = 0;
	xtlsresult Result;

	if ( (iSize > sizeof(Output)) || (xrtTlsSessionWrite(
		pSource, sText, iSize, &iWritten
	) != XTLS_OK) || (iWritten != iSize) ||
		!exampleTlsMove(pSource, pTarget) ) {
		return false;
	}
	Result = bTargetServer ?
		xrtTlsServerDrive(pTarget) : xrtTlsClientDrive(pTarget);
	if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
		return false;
	}
	return (xrtTlsSessionRead(
		pTarget, Output, sizeof(Output), &iRead
	) == XTLS_OK) && (iRead == iSize) &&
		(memcmp(Output, sText, iSize) == 0);
}



/* 打印当前线程保存的结构化根错误。 */
static void exampleTlsError(void)
{
	const xerror* pError = xrtGetError();

	if ( pError != NULL ) {
		fprintf(
			stderr, "%s: %s\n",
			xrtErrorOperation(pError), xrtErrorMessage(pError)
		);
	}
}



/*
 * 范例：tls/server —— 传输无关 TLS 1.3 服务端（会话层对打）
 * ----------------------------------------------------------------
 * 演示 API：
 *   会话级 API：ServerContext / Client 会话驱动
 *   exampleTlsMove             双会话密文搬运（内存回环传输）
 *   服务端握手 + 双向数据      与真实客户端互验
 * 模块宏：XRT_MODULE_TLS（依赖 CRYPTO/X509）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/tls/server/main.c -lws2_32 -liphlpapi
 * 用法：
 *   server <rsa|p256|p384|ed25519> <certificate.der> <private.der>
 * 预期输出（无参数时）：
 *   usage: server <rsa|p256|p384|ed25519> ...
 *
 * 会话层（xtlssession）是 Stream 层之下的"裸协议机"：
 *   输入密文块、输出密文块，传输完全由调用方决定——
 *   exampleTlsMove 用内存搬运演示"传输无关"；
 *   接真实网络就是 stream 范例的形态。协议测试、
 *   自定义传输（IoT/串口）用这一层。
 */


/* 用真实客户端演示传输无关 TLS 1.3 服务端握手与双向数据。 */
int main(int argc, char** argv)
{
	static const xstrview Protocols[] = {
		XRT_STR_INIT("h2"),
		XRT_STR_INIT("http/1.1")
	};
	uint8* pCertificateData = NULL;
	uint8* pPrivateData = NULL;
	size_t iCertificateSize = 0;
	size_t iPrivateSize = 0;
	xbytesview Certificate;
	xtlsidentity* pIdentity = NULL;
	xtlsverifierconfig VerifierConfig;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	xbytesview Protocol;
	int iResult = 1;

	if ( argc != 4 ) {
		printf(
			"usage: server <rsa|p256|p384|ed25519> "
			"<certificate.der> <private.der>\n"
		);
		return 0;
	}
	if ( !exampleTlsReadFile(
		argv[2], &pCertificateData, &iCertificateSize
	) || !exampleTlsReadFile(
		argv[3], &pPrivateData, &iPrivateSize
	) ) {
		fprintf(stderr, "failed to read DER identity\n");
		goto Cleanup;
	}
	Certificate = (xbytesview) {
		pCertificateData, iCertificateSize
	};
	pIdentity = exampleTlsIdentity(
		argv[1], &Certificate, 1u,
		(xbytesview) { pPrivateData, iPrivateSize }
	);
	if ( pIdentity == NULL ) {
		exampleTlsError();
		goto Cleanup;
	}
	xrtTlsVerifierConfigInit(&VerifierConfig);
	VerifierConfig.Verify = exampleTlsAccept;
	pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	if ( pVerifier == NULL ) {
		exampleTlsError();
		goto Cleanup;
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.ServerName = XRT_STR_LITERAL("example.com");
	ClientConfig.Protocols = Protocols;
	ClientConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	ServerConfig.Protocols = Protocols;
	ServerConfig.ProtocolCount = sizeof(Protocols) / sizeof(Protocols[0]);
	ServerConfig.RequireProtocol = true;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (pClient == NULL) || (pServer == NULL) ||
		!exampleTlsHandshake(pClient, pServer) ||
		!exampleTlsTransfer(pClient, pServer, true, "ping") ||
		!exampleTlsTransfer(pServer, pClient, false, "pong") ||
		!xrtTlsSessionProtocol(pServer, &Protocol) ) {
		exampleTlsError();
		goto Cleanup;
	}
	printf(
		"ready, protocol=%.*s, client=ping, server=pong\n",
		(int)Protocol.Size, (const char*)Protocol.Data
	);
	iResult = 0;

Cleanup:
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	free(pPrivateData);
	free(pCertificateData);
	return iResult;
}
