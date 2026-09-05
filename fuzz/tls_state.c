/* 有状态 TLS fuzz：真实 PSK+DHE 握手后变异操作序列和已认证的后握手消息。 */
#include "../tests/fixtures/tls_server.h"
#include "../src/internal/xrt_tls_session.h"

static xtlscontext* __xrtTlsStateContext;
static xtlsidentity* __xrtTlsStateIdentity;
void xrtTlsStateFuzzerCleanup(void);

static const xtlsresume* __xrtTlsStateResume(ptr pContext,
	const xtlsserverresumerequest* pRequest)
{
	(void)pRequest;
	return (const xtlsresume*)pContext;
}

/* 共享夹具不可变；每个输入拥有全新的会话、随机 key share 和恢复对象。 */
static void __xrtTlsStateInit(void)
{
	if ( __xrtTlsStateContext == NULL ) {
		xtlscontextconfig Config;
		xtlslimits Limits;

		xrtTlsContextConfigInit(&Config);
		Limits = Config.Limits;
		Limits.FeedLimit = Limits.SendLimit = 32768u;
		Limits.HandshakeLimit = 8192u;
		Limits.PlainLimit = 16384u;
		Limits.RecordBudget = Limits.HandshakeBudget = 4u;
		__xrtTlsStateContext = testTlsServerContextWithLimits(&Limits);
		__xrtTlsStateIdentity = testTlsServerIdentity();
		testRequire((__xrtTlsStateContext != NULL) && (__xrtTlsStateIdentity != NULL),
			"state fuzz fixture initialization failed");
		#if defined(XRT_TLS_STATE_LIBFUZZER)
			testRequire(atexit(xrtTlsStateFuzzerCleanup) == 0,
				"state fuzz cleanup registration failed");
		#endif
	}
}

static void __xrtTlsStateMove(xtlssession* pSource, xtlssession* pTarget,
	size_t iChunk, uint8 iMutation)
{
	xnetspan Span;
	uint8 Bytes[256];

	if ( (xrtTlsSessionSendSize(pSource) == 0) ||
		!xrtTlsSessionSendFront(pSource, &Span) ) {
		return;
	}
	if ( iChunk > Span.Size ) {
		iChunk = Span.Size;
	}
	memcpy(Bytes, Span.Data, iChunk);
	if ( iMutation != 0 ) {
		Bytes[iChunk - 1u] ^= iMutation;
	}
	if ( xrtTlsSessionFeed(pTarget, Bytes, iChunk) == XTLS_OK ) {
		testRequire(xrtTlsSessionSendConsume(pSource, iChunk),
			"state fuzz send consumption failed");
	}
}

int xrtTlsStateFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	static const uint8 Secret[32] = { 7u, 19u, 31u };
	xstrview Protocol = XRT_STR_LITERAL("http/1.1");
	xtlsresumeconfig ResumeConfig;
	xtlsclientconfig Client;
	xtlsserverconfig Server;
	xtlsresume* pResume;
	xtlssession* Sessions[2];
	xmemdebugsnapshot Before, After;
	bool Terminal[2] = { false, false };
	uint8 Bytes[256];

	if ( (iSize > 4096u) || ((pData == NULL) && (iSize != 0)) ) {
		return 0;
	}
	__xrtTlsStateInit();
	xrtClearError();
	xrtMemDebugSnapshot(&Before);
	xrtTlsResumeConfigInit(&ResumeConfig);
	ResumeConfig.Cipher = XTLS_AES_128_GCM_SHA256;
	ResumeConfig.Ticket = (xbytesview) { (const uint8*)"state-fuzz", 10u };
	ResumeConfig.Secret = (xbytesview) { Secret, sizeof(Secret) };
	ResumeConfig.ServerName = XRT_STR_LITERAL("example.com");
	ResumeConfig.Protocol = (xbytesview) { (const uint8*)Protocol.Data, Protocol.Size };
	ResumeConfig.Lifetime = 3600u;
	pResume = xrtTlsResumeCreate(&ResumeConfig);
	testRequire(pResume != NULL, "state fuzz PSK creation failed");
	xrtTlsClientConfigInit(&Client);
	Client.Context = __xrtTlsStateContext;
	Client.ServerName = ResumeConfig.ServerName;
	Client.Protocols = &Protocol;
	Client.ProtocolCount = 1;
	Client.Resume = pResume;
	Client.ResumeOnly = true;
	xrtTlsServerConfigInit(&Server);
	Server.Context = __xrtTlsStateContext;
	Server.Identity = __xrtTlsStateIdentity;
	Server.Protocols = &Protocol;
	Server.ProtocolCount = 1;
	Server.Resume = __xrtTlsStateResume;
	Server.ResumeContext = pResume;
	Sessions[0] = xrtTlsClientCreate(&Client, NULL);
	Sessions[1] = xrtTlsServerCreate(&Server, NULL);
	testRequire((Sessions[0] != NULL) && (Sessions[1] != NULL),
		"state fuzz session creation failed");
	if ( (iSize == 0) || ((pData[0] & 1u) == 0) ) {
		test_tls_server_rng Rng = { UINT32_C(0x13371337) };

		testRequire(testTlsServerHandshake(Sessions[0], Sessions[1], &Rng) &&
			xrtTlsClientResumed(Sessions[0]) && xrtTlsServerResumed(Sessions[1]),
			"state fuzz authenticated bootstrap failed");
	}
	/* 操作数、每次输入/输出量以及内部 drive 预算都与变异长度无关地有界。 */
	for ( size_t i = 1u, iOps = 0; (i + 1u < iSize) && (iOps < 256u); i += 2u, iOps++ ) {
		uint8 Op = pData[i];
		size_t iSide = (Op >> 7u) & 1u;
		xtlssession* pSource = Sessions[iSide];
		xtlssession* pTarget = Sessions[iSide ^ 1u];
		size_t iChunk = (size_t)pData[i + 1u] + 1u;
		size_t iDone;

		memset(Bytes, pData[i + 1u], sizeof(Bytes));
		switch ( (Op & 0x7Fu) % 13u ) {
			case 0:
				__xrtTlsStateMove(pSource, pTarget, iChunk, 0);
				break;
			case 1:
				(void)(iSide ? xrtTlsServerDrive(pSource) : xrtTlsClientDrive(pSource));
				break;
			case 2:
				(void)xrtTlsSessionWrite(pSource, Bytes, iChunk, &iDone);
				break;
			case 3:
				(void)xrtTlsSessionRead(pSource, Bytes, iChunk, &iDone);
				break;
			case 4:
				(void)(iSide ? xrtTlsServerKeyUpdate(pSource, (xtlskeyupdate)(iChunk % 3u)) :
					xrtTlsClientKeyUpdate(pSource, (xtlskeyupdate)(iChunk % 3u)));
				break;
			case 5: {
				xtlsresume* pTicket = NULL;

				(void)xrtTlsServerTicket(Sessions[1], (xbytesview) { Bytes, iChunk }, 60u, &pTicket);
				xrtTlsResumeRelease(pTicket);
				pTicket = xrtTlsClientTakeResume(Sessions[0]);
				xrtTlsResumeRelease(pTicket);
				break;
			}
			case 6:
				(void)xrtTlsSessionClose(pSource);
				break;
			case 7:
				(void)xrtTlsSessionFeed(pSource, pData + i, iSize - i < iChunk ? iSize - i : iChunk);
				break;
			case 8:
				/* AEAD 正确的畸形消息能进入 READY 的后握手解析与事务提交路径。 */
				if ( xrtTlsSessionState(pSource) == XTLS_STATE_READY ) {
					Bytes[0] = (pData[i + 1u] & 1u) ? XTLS_HANDSHAKE_KEY_UPDATE : XTLS_HANDSHAKE_NEW_SESSION_TICKET;
					Bytes[1] = Bytes[2] = 0;
					Bytes[3] = (uint8)(iChunk > 4u ? iChunk - 4u : 0);
					(void)__xrtTlsSessionRecordProtect(pSource, XTLS_RECORD_HANDSHAKE,
						(xbytesview) { Bytes, iChunk }, 0);
				}
				break;
			case 9:
				(void)xrtTlsSessionEof(pSource);
				break;
			case 10:
				__xrtTlsStateMove(pSource, pTarget, iChunk, pData[i + 1u]);
				break;
			case 11:
				if ( (xrtTlsSessionState(pSource) == XTLS_STATE_READY) &&
					(xrtTlsSessionState(pTarget) == XTLS_STATE_READY) &&
					(xrtTlsSessionSendSize(pSource) == 0) && (xrtTlsSessionFeedSize(pTarget) == 0) ) {
					pSource->WriteKey.Sequence = __xrtTlsRecordKeyLimit(&pSource->WriteKey) - 1u;
					pTarget->ReadKey.Sequence = pSource->WriteKey.Sequence;
				}
				break;
			case 12:
				for ( size_t j = 0; j < 32u; j++ ) {
					__xrtTlsStateMove(Sessions[0], Sessions[1], 256u, 0);
					__xrtTlsStateMove(Sessions[1], Sessions[0], 256u, 0);
					(void)xrtTlsClientDrive(Sessions[0]);
					(void)xrtTlsServerDrive(Sessions[1]);
				}
				break;
		}
		for ( size_t j = 0; j < 2u; j++ ) {
			xtlsstate State = xrtTlsSessionState(Sessions[j]);
			bool bTerminal = (State == XTLS_STATE_FAILED) || (State == XTLS_STATE_CLOSED);

			testRequire(!Terminal[j] || bTerminal, "TLS terminal state resurrected");
			Terminal[j] = bTerminal;
			testRequire((xrtTlsSessionFeedSize(Sessions[j]) <= 32768u) &&
				(xrtTlsSessionSendSize(Sessions[j]) <= 32768u) &&
				(xrtTlsSessionPlainSize(Sessions[j]) <= 16384u), "TLS state fuzz exceeded resource budget");
		}
		xrtClearError();
	}
	xrtTlsSessionDestroy(Sessions[0]);
	xrtTlsSessionDestroy(Sessions[1]);
	xrtTlsResumeRelease(pResume);
	xrtClearError();
	xrtMemDebugSnapshot(&After);
	testRequire((Before.LiveCount == After.LiveCount) && (Before.LiveBytes == After.LiveBytes),
		"TLS state fuzz leaked a logical allocation");
	return 0;
}

/* 确定性测试在进程结束前释放共享夹具；libFuzzer 在进程退出时释放。 */
void xrtTlsStateFuzzerCleanup(void)
{
	xrtTlsIdentityRelease(__xrtTlsStateIdentity);
	xrtTlsContextRelease(__xrtTlsStateContext);
	__xrtTlsStateIdentity = NULL;
	__xrtTlsStateContext = NULL;
}

#if defined(XRT_TLS_STATE_LIBFUZZER)
int LLVMFuzzerTestOneInput(const uint8* pData, size_t iSize)
{
	return xrtTlsStateFuzzerTestOneInput(pData, iSize);
}
#endif
