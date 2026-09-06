/*
 * 范例：tls/session_tour —— 会话喂入五形态与明文/发送双队列
 * ----------------------------------------------------------------
 * 演示 API：
 *   【喂入】      xrtTlsSessionFeedBorrow（借用）/ FeedTake（接管）/
 *                 FeedRef（自定义释放，恰好一次）/
 *                 FeedBuffer（零复制缓冲链）/ FeedSize（未消费计数）
 *   【发送队列】  xrtTlsSessionSendSpanCount / SendSpans（聚集借用）
 *   【明文队列】  xrtTlsSessionPlainSize / PlainSpanCount /
 *                 PlainFront / PlainSpans / PlainConsume（零复制消费）
 *   【会话属性】  xrtTlsSessionRole / Version / Cipher / Context /
 *                 Wait（事件位）
 *   【关闭】      xrtTlsSessionClose / Eof / PeerAlert
 * 模块宏：XRT_MODULE_TLS（+ CLIENT/SERVER/IDENTITY_P256/VERIFIER）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -I examples/tls -include xrt.h
 *       impl.c examples/tls/session_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   session: handshake via feed-borrow -> both ready ok
 *   session: role/version/cipher/wait/context ok
 *   session: feed-take + feed-ref (release once) ok
 *   session: feed-buffer zero-copy chain ok
 *   session: send spans gathered + plain spans consumed ok
 *   session: close_notify -> peer eof ok
 *
 * 双会话内存桥：Client/Server Drive 交替推进，
 *   密文经四种喂入形态搬运；明文用 Span 族零复制检查。
 *   嵌入证书见 embedded_identity.h（CN=localhost）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>
#include "embedded_identity.h"

/* FeedRef 的释放回调：记录次数。 */
static void exampleRelease(ptr pContext, cbytes pData, size_t iSize)
{
	int* pCount = (int*)pContext;

	(void)pData;
	(void)iSize;
	++(*pCount);
}

/* 验证器回调：只接管信任决策，协议签名仍由 XRT 验证。 */
static xtlsverifydecision exampleAccept(
	const xtlspeer* pPeer,
	ptr pContext
)
{
	(void)pContext;
	return ((pPeer != NULL) && (pPeer->CertificateCount != 0u)) ?
		XTLS_VERIFY_ACCEPT : XTLS_VERIFY_REJECT;
}

/* 用 SendSpans 聚集全部待发密文，按指定形态喂给对端。 */
typedef enum examplefeed {
	EXAMPLE_FEED_BORROW,
	EXAMPLE_FEED_TAKE,
	EXAMPLE_FEED_REF,
	EXAMPLE_FEED_BUFFER
} examplefeed;

/* 按角色驱动会话推进。 */
static xtlsresult exampleDrive(xtlssession* pSession)
{
	return (xrtTlsSessionRole(pSession) == XTLS_SERVER) ?
		xrtTlsServerDrive(pSession) :
		xrtTlsClientDrive(pSession);
}

static bool exampleMove(
	xtlssession* pSource,
	xtlssession* pTarget,
	examplefeed Kind,
	int* pReleased
)
{
	xnetspan Spans[8];
	xnetbuf Chain;

	/* 借用/引用形态的数据必须存活到对端消费——
	 * 顺序固定为：喂入 → 驱动对端 → 消费源 Span。 */
	while ( xrtTlsSessionSendSize(pSource) != 0u ) {
		xtlsresult Result;

		if ( (xrtTlsSessionSendSpans(pSource, Spans, 8u) == 0u) ||
			(Spans[0].Size == 0u) ) {
			return false;
		}
		switch ( Kind ) {
			case EXAMPLE_FEED_BORROW:
				Result = xrtTlsSessionFeedBorrow(pTarget,
					Spans[0].Data, Spans[0].Size);
				break;
			case EXAMPLE_FEED_TAKE: {
				ptr pCopy = xrtMalloc(Spans[0].Size);

				if ( pCopy == NULL ) {
					return false;
				}
				memcpy(pCopy, Spans[0].Data, Spans[0].Size);
				Result = xrtTlsSessionFeedTake(pTarget,
					pCopy, Spans[0].Size);
				break;
			}
			case EXAMPLE_FEED_REF:
				Result = xrtTlsSessionFeedRef(pTarget,
					Spans[0].Data, Spans[0].Size,
					exampleRelease, (ptr)pReleased);
				break;
			case EXAMPLE_FEED_BUFFER:
			default:
				if ( !xrtNetBufInit(&Chain, NULL) ||
					!xrtNetBufAppend(&Chain, Spans[0].Data,
						Spans[0].Size) ) {
					return false;
				}
				Result = xrtTlsSessionFeedBuffer(pTarget,
					&Chain);
				break;
		}
		if ( Result != XTLS_OK ) {
			return false;
		}
		Result = exampleDrive(pTarget);
		if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
			return false;
		}
		if ( !xrtTlsSessionSendConsume(pSource,
				Spans[0].Size) ) {
			return false;
		}
	}
	return true;
}

/* 双向驱动 + 搬运，直到双方 READY。 */
static bool exampleHandshake(
	xtlssession* pClient,
	xtlssession* pServer,
	int* pReleased
)
{
	for ( size_t i = 0; i < 4096u; i++ ) {
		xtlsresult C = xrtTlsClientDrive(pClient);
		xtlsresult S = xrtTlsServerDrive(pServer);

		if ( ((C != XTLS_OK) && (C != XTLS_AGAIN)) ||
			((S != XTLS_OK) && (S != XTLS_AGAIN)) ||
			!exampleMove(pClient, pServer, EXAMPLE_FEED_BORROW,
				pReleased) ||
			!exampleMove(pServer, pClient, EXAMPLE_FEED_BORROW,
				pReleased) ) {
			return false;
		}
		/* exampleMove 内部已驱动对端；此处再补一轮源侧驱动。 */
		if ( (xrtTlsSessionState(pClient) == XTLS_STATE_READY) &&
			(xrtTlsSessionState(pServer) == XTLS_STATE_READY) ) {
			return true;
		}
	}
	return false;
}

/* 单向：写应用数据 → 搬运 → 驱动对端。 */
static bool exampleSend(
	xtlssession* pSource,
	xtlssession* pTarget,
	bool bTargetServer,
	cstr sText,
	examplefeed Kind,
	int* pReleased
)
{
	size_t iWritten = 0;
	xtlsresult Result;

	(void)bTargetServer;
	(void)Result;
	if ( (xrtTlsSessionWrite(pSource, sText, strlen(sText),
			&iWritten) != XTLS_OK) ||
		(iWritten != strlen(sText)) ||
		!exampleMove(pSource, pTarget, Kind, pReleased) ) {
		return false;
	}
	return true;
}

int main(void)
{
	xtlsidentity* pIdentity = NULL;
	xtlsverifier* pVerifier = NULL;
	xtlsclientconfig ClientConfig;
	xtlsserverconfig ServerConfig;
	xtlssession* pClient = NULL;
	xtlssession* pServer = NULL;
	xnetspan Spans[4];
	int iReleased = 0;
	int iResult = 1;

	/* ---- 建立双会话：嵌入证书 + 接受式验证器。 ---- */
	pIdentity = exampleEmbeddedIdentity();
	{
		xtlsverifierconfig VerifierConfig;

		xrtTlsVerifierConfigInit(&VerifierConfig);
		VerifierConfig.Verify = exampleAccept;
		pVerifier = xrtTlsVerifierCreate(&VerifierConfig);
	}
	if ( (pIdentity == NULL) || (pVerifier == NULL) ) {
		goto Cleanup;
	}
	xrtTlsClientConfigInit(&ClientConfig);
	ClientConfig.ServerName = XRT_STR_LITERAL("localhost");
	ClientConfig.VerifyName = XRT_STR_LITERAL("localhost");
	ClientConfig.Verifier = pVerifier;
	xrtTlsServerConfigInit(&ServerConfig);
	ServerConfig.Identity = pIdentity;
	pClient = xrtTlsClientCreate(&ClientConfig, NULL);
	pServer = xrtTlsServerCreate(&ServerConfig, NULL);
	if ( (pClient == NULL) || (pServer == NULL) ||
		!exampleHandshake(pClient, pServer, &iReleased) ||
		(xrtTlsSessionFeedSize(pClient) != 0u) ) {
		goto Cleanup;
	}
	printf("session: handshake via feed-borrow -> both ready ok\n");

	/* ---- 会话属性：角色/版本/套件/事件位/上下文。 ---- */
	if ( (xrtTlsSessionRole(pClient) != XTLS_CLIENT) ||
		(xrtTlsSessionRole(pServer) != XTLS_SERVER) ||
		(xrtTlsSessionVersion(pClient) != XTLS_VERSION_13) ||
		(xrtTlsSessionCipher(pClient) == 0u) ||
		(xrtTlsSessionWait(pClient) == 0u) ||
		/* 未显式配置 Context 时会话仍挂在共享默认上下文上。 */
		(xrtTlsSessionContext(pClient) == NULL) ||
		(xrtTlsSessionContext(pServer) == NULL) ) {
		goto Cleanup;
	}
	printf("session: role/version/cipher/wait/context ok\n");

	/* ---- Take/Ref 喂入：接管与释放回调恰好一次。 ---- */
	if ( !exampleSend(pClient, pServer, true, "take",
			EXAMPLE_FEED_TAKE, &iReleased) ||
		(xrtTlsSessionPlainSize(pServer) != 4u) ||
		!exampleSend(pClient, pServer, true, "ref!",
			EXAMPLE_FEED_REF, &iReleased) ) {
		goto Cleanup;
	}
	{
		/* 驱动消费 Ref 喂入的数据后释放回调恰好一次。 */
		for ( int i = 0; (i < 100) && (iReleased == 0); i++ ) {
			(void)xrtTlsServerDrive(pServer);
			xrtSleep(1u);
		}
		if ( iReleased != 1 ) {
			goto Cleanup;
		}
	}
	printf("session: feed-take + feed-ref (release once) ok\n");

	/* ---- Buffer 零复制：密文缓冲链整体移交。 ---- */
	if ( !exampleSend(pServer, pClient, false, "chain",
			EXAMPLE_FEED_BUFFER, &iReleased) ||
		(xrtTlsSessionPlainSize(pClient) != 5u) ) {
		goto Cleanup;
	}
	printf("session: feed-buffer zero-copy chain ok\n");

	/* ---- 明文队列零复制检查 + 消费。 ---- */
	{
		char arrText[16];
		size_t iCount;
		size_t iSize;

		/* 发送侧 Span 计数与聚集同族——上一轮 Move 已清空。 */
		if ( (xrtTlsSessionSendSpanCount(pClient) != 0u) ) {
			goto Cleanup;
		}
		iSize = xrtTlsSessionPlainSize(pClient);
		iCount = xrtTlsSessionPlainSpanCount(pClient);
		if ( (iSize != 5u) || (iCount == 0u) ||
			!xrtTlsSessionPlainFront(pClient, &Spans[0]) ||
			(Spans[0].Size == 0u) ||
			(xrtTlsSessionPlainSpans(pClient, Spans, 4u) !=
				iCount) ) {
			goto Cleanup;
		}
		memcpy(arrText, Spans[0].Data,
			Spans[0].Size < 5u ? Spans[0].Size : 5u);
		/* 前缀可能分散在多个 Span——按总长复制拼合。 */
		{
			size_t iOffset = 0;

			for ( size_t i = 0; (i < iCount) && (iOffset < 5u);
				i++ ) {
				size_t n = Spans[i].Size;

				if ( n > 5u - iOffset ) {
					n = 5u - iOffset;
				}
				memcpy(arrText + iOffset, Spans[i].Data, n);
				iOffset += n;
			}
			if ( (iOffset != 5u) ||
				(memcmp(arrText, "chain", 5u) != 0) ||
				!xrtTlsSessionPlainConsume(pClient, 5u) ||
				(xrtTlsSessionPlainSize(pClient) != 0u) ) {
				goto Cleanup;
			}
		}
	}
	printf("session: send spans gathered + plain spans consumed ok\n");

	/* ---- 关闭：Close 排队 close_notify，对端驱动后 Eof。 ---- */
	if ( xrtTlsSessionClose(pClient) != XTLS_OK ) {
		goto Cleanup;
	}
	if ( !exampleMove(pClient, pServer, EXAMPLE_FEED_BORROW,
			&iReleased) ) {
		goto Cleanup;
	}
	{
		xtlsresult Result = xrtTlsServerDrive(pServer);

		if ( (Result != XTLS_OK) && (Result != XTLS_AGAIN) ) {
			goto Cleanup;
		}
	}
	{
		xtlsalertlevel Level;
		xtlsalert Alert;

		/* close_notify 本身就是一条 warning 级 Alert——
		 * 干净关闭后 PeerAlert 为真且代码恰为 close_notify。 */
		if ( (xrtTlsSessionEof(pServer) != XTLS_OK) ||
			!xrtTlsSessionPeerAlert(pServer, &Level, &Alert) ||
			(Level != XTLS_ALERT_WARNING) ||
			(Alert != XTLS_ALERT_CLOSE_NOTIFY) ) {
			goto Cleanup;
		}
	}
	printf("session: close_notify -> peer eof ok\n");
	iResult = 0;

Cleanup:
	xrtTlsSessionDestroy(pServer);
	xrtTlsSessionDestroy(pClient);
	xrtTlsVerifierRelease(pVerifier);
	xrtTlsIdentityRelease(pIdentity);
	return iResult;
}
