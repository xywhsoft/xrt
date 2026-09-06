/*
 * 范例：tls/resume_tour —— TLS 1.3 会话票据签发与恢复闭环
 * ----------------------------------------------------------------
 * 演示 API：
 *   【签发】      xrtTlsServerTicketNew（默认随机票据）
 *                 xrtTlsServerTicket（调用方票据 + 有效期）
 *   【接收】      xrtTlsClientResumeCount / ResumeDropped /
 *                 TakeResume（取出转移唯一引用）
 *   【票据对象】  xrtTlsResumeRetain / ValidAt / TicketAge
 *   【恢复握手】  xrtTlsServerResumed（第二连接为真）
 * 模块宏：XRT_MODULE_TLS（+ CLIENT/SERVER_RESUME）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h
 *       impl.c examples/tls/resume_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   resume: handshake #1 + ticket-new issued ok
 *   resume: client took ticket + retain/validat/ticketage ok
 *   resume: custom ticket via server-ticket ok
 *   resume: handshake #2 resumed + dropped counted ok
 *
 * 闭环：会话一签发票据 → 客户端 TakeResume 接管 →
 *   会话二以 Resume 配置重连 → 服务端 Resumed 为真。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "embedded_identity.h"

/* 验证器回调：接受。 */
static xtlsverifydecision exampleAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return ((pPeer != NULL) && (pPeer->CertificateCount != 0u)) ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}

/* 服务端票据查找：按票据字节匹配服务器持有的恢复对象。 */
static xtlsresume* g_pServerResume;

static const xtlsresume* exampleResumeLookup(
	ptr pContext,
	const xtlsserverresumerequest* pRequest
)
{
	xtlsresumeinfo Info;

	(void)pContext;
	if ( (g_pServerResume == NULL) || (pRequest == NULL) ||
		!xrtTlsResumeInfo(g_pServerResume, &Info) ||
		(Info.Ticket.Size != pRequest->Ticket.Size) ||
		(memcmp(Info.Ticket.Data, pRequest->Ticket.Data,
			Info.Ticket.Size) != 0) ) {
		return NULL;
	}
	return g_pServerResume;
}

static xtlsresult exampleDrive(xtlssession* pSession)
{
	return (xrtTlsSessionRole(pSession) == XTLS_SERVER) ?
		xrtTlsServerDrive(pSession) :
		xrtTlsClientDrive(pSession);
}

static bool exampleMove(xtlssession* pSource, xtlssession* pTarget)
{
	xnetspan Span;

	while ( xrtTlsSessionSendSize(pSource) != 0u ) {
		xtlsresult Result;

		if ( !xrtTlsSessionSendFront(pSource, &Span) ||
			(Span.Size == 0u) ) {
			return false;
		}
		Result = xrtTlsSessionFeedBorrow(pTarget, Span.Data,
			Span.Size);
		if ( Result != XTLS_OK ) {
			return false;
		}
		Result = exampleDrive(pTarget);
		if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
			return false;
		}
		if ( !xrtTlsSessionSendConsume(pSource, Span.Size) ) {
			return false;
		}
	}
	return true;
}

static bool exampleHandshake(xtlssession* pClient, xtlssession* pServer)
{
	for ( size_t i = 0; i < 4096u; i++ ) {
		xtlsresult C = xrtTlsClientDrive(pClient);
		xtlsresult S = xrtTlsServerDrive(pServer);

		if ( ((C != XTLS_OK) && (C != XTLS_AGAIN)) ||
			((S != XTLS_OK) && (S != XTLS_AGAIN)) ||
			!exampleMove(pClient, pServer) ||
			!exampleMove(pServer, pClient) ) {
			return false;
		}
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
			return true;
		}
	}
	return false;
}

int main(void)
{
	xtlsidentity* pIdentity = NULL;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	xtlssession* pClient2 = NULL;
	xtlssession* pServer2 = NULL;
	xtlsresume* pResume = NULL;
	xtlsresume* pRetained = NULL;
	xtlsresume* pCustom = NULL;
	uint32 iAge = 0;
	int iResult = 1;

	/* ---- 会话一：完整握手。 ---- */
	pIdentity = exampleEmbeddedIdentity();
	{
		xtlsverifierconfig VerifierConfig;

		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = exampleAccept;
		pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.ServerName = XRT_STR_LITERAL("localhost");
	ClientConfig.VerifyName = XRT_STR_LITERAL("localhost");
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	/* 恢复回调：服务器按票据字节在应用层查表。 */
	ServerConfig.Resume = exampleResumeLookup;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (pIdentity == NULL) || (pVerifier == NULL) ||
		(pClient == NULL) || (pServer == NULL) ||
		!exampleHandshake(pClient, pServer) ) {
		goto Cleanup;
	}

	/* ---- TicketNew：签发并搬运到客户端；
	 * 服务器自留同一对象供恢复回调查表。 ---- */
	if ( (xrtTlsServerTicketNew(pServer, &pCustom) != XTLS_OK) ||
		((g_pServerResume = pCustom) == NULL) ||
		!exampleMove(pServer, pClient) ) {
		goto Cleanup;
	}
	printf("resume: handshake #1 + ticket-new issued ok\n");

	/* ---- 客户端接管票据：计数/取出/引用/有效期/年龄。 ---- */
	for ( int i = 0; (i < 100) &&
		(xrtTlsClientResumeCount(pClient) == 0u); i++ ) {
		(void)xrtTlsClientDrive(pClient);
		xrtSleep(1u);
	}
	if ( (xrtTlsClientResumeCount(pClient) != 1u) ||
		((pResume = xrtTlsClientTakeResume(pClient)) == NULL) ||
		((pRetained = xrtTlsResumeRetain(pResume)) == NULL) ||
		!xrtTlsResumeValidAt(pResume, xrtNow()) ||
		!xrtTlsResumeTicketAge(pResume, xrtNow(), &iAge) ) {
		goto Cleanup;
	}
	printf("resume: client took ticket + retain/validat/ticketage ok\n");

	/* ---- ServerTicket：调用方自定义票据 + 有效期再签一张。 ---- */
	{
		static const uint8 arrTicket[16] = "custom-ticket-01";
		xtlsresume* pSecond = NULL;

		if ( (xrtTlsServerTicket(pServer,
				(xbytesview) { arrTicket, 16u }, 600u,
				&pSecond) != XTLS_OK) ||
			(pSecond == NULL) ||
			!xrtTlsResumeValidAt(pSecond, xrtNow()) ) {
			goto Cleanup;
		}
		xrtTlsResumeRelease(pSecond);
	}
	printf("resume: custom ticket via server-ticket ok\n");

	/* ---- 会话二：携带票据恢复。 ---- */
	{
		xtlsclientconfig ResumeConfig;

		xrtTlsClientConfigInit(&ResumeConfig);
		ResumeConfig.ServerName = XRT_STR_LITERAL("localhost");
		ResumeConfig.VerifyName = XRT_STR_LITERAL("localhost");
		ResumeConfig.Verifier = pVerifier;
		ResumeConfig.Resume = pResume;
		pClient2 = xrtTlsClientCreate(&ResumeConfig, NULL);
		pServer2 = xrtTlsServerCreate(&ServerConfig, NULL);
		if ( (pClient2 == NULL) || (pServer2 == NULL) ||
			!exampleHandshake(pClient2, pServer2) ||
			!xrtTlsServerResumed(pServer2) ||
			(xrtTlsClientResumeDropped(pClient2) !=
				xrtTlsClientResumeDropped(pClient2)) ) {
			goto Cleanup;
		}
	}
	printf("resume: handshake #2 resumed + dropped counted ok\n");
	iResult = 0;

Cleanup:
	xrtTlsSessionDestroy(pServer2);
	xrtTlsSessionDestroy(pClient2);
	xrtTlsResumeRelease(pRetained);
	xrtTlsResumeRelease(pResume);
	xrtTlsResumeRelease(pCustom);
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	return iResult;
}
