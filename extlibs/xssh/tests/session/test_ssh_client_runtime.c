#include "../test.h"
#include "../../src/session/ssh_client_future_internal.h"



#define TEST_SSH_CLIENT_CLOSE_WAITERS 16u
#define TEST_SSH_CLIENT_DIRECT_BYTES 96u
#define TEST_SSH_CLIENT_DIRECT_PACKET 16u
#define TEST_SSH_CLIENT_DIRECT_QUEUE 32u
#define TEST_SSH_CLIENT_DIRECT_PACKETS \
	(TEST_SSH_CLIENT_DIRECT_BYTES / TEST_SSH_CLIENT_DIRECT_PACKET)
#define TEST_SSH_CLIENT_DIRECT_ADJUSTS \
	(TEST_SSH_CLIENT_DIRECT_PACKETS - 1u)
#define TEST_SSH_CLIENT_WRITE_WAITERS TEST_SSH_CLIENT_DIRECT_ADJUSTS
#define TEST_SSH_CLIENT_FUTURE_OOM_MIN 5u
#define TEST_SSH_CLIENT_FUTURE_OOM_SCAN 32u
#define TEST_SSH_CLIENT_IGNORE_BYTES 32000u



typedef struct testsshclientruntime testsshclientruntime;



/* Mock 服务端只保存尚未发送的一条应用动作。 */
typedef enum testsshserveraction {
	TEST_SSH_SERVER_ACTION_NONE = 0,
	TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS = 1,
	TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE = 2,
	TEST_SSH_SERVER_ACTION_OPEN = 3,
	TEST_SSH_SERVER_ACTION_OPEN_FAILURE = 4,
	TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_ONLY = 5,
	TEST_SSH_SERVER_ACTION_REQUEST_FAILURE_ONLY = 6,
	TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_STREAM = 7,
	TEST_SSH_SERVER_ACTION_DATA_SESSION = 8,
	TEST_SSH_SERVER_ACTION_DATA_STDERR = 9,
	TEST_SSH_SERVER_ACTION_DATA_DIRECT = 10,
	TEST_SSH_SERVER_ACTION_WINDOW_ADJUST = 11,
	TEST_SSH_SERVER_ACTION_EXIT_STATUS = 12,
	TEST_SSH_SERVER_ACTION_EXIT_SIGNAL = 13,
	TEST_SSH_SERVER_ACTION_EOF = 14,
	TEST_SSH_SERVER_ACTION_CLOSE = 15,
	TEST_SSH_SERVER_ACTION_FORWARDED_OPEN = 16,
	TEST_SSH_SERVER_ACTION_DATA_FORWARDED = 17,
	TEST_SSH_SERVER_ACTION_ABORT = 18
} testsshserveraction;



/* 客户端依次验证 remote forward、exec、PTY shell 和 direct-tcpip。 */
typedef enum testsshclientphase {
	TEST_SSH_CLIENT_PHASE_FORWARD = 0,
	TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE = 1,
	TEST_SSH_CLIENT_PHASE_FORWARD_CANCEL = 2,
	TEST_SSH_CLIENT_PHASE_EXEC_OPEN = 3,
	TEST_SSH_CLIENT_PHASE_EXEC_ENV = 4,
	TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE = 5,
	TEST_SSH_CLIENT_PHASE_PTY_OPEN = 6,
	TEST_SSH_CLIENT_PHASE_PTY_REQUEST = 7,
	TEST_SSH_CLIENT_PHASE_SHELL_ACTIVE = 8,
	TEST_SSH_CLIENT_PHASE_DIRECT_REJECT_OPEN = 9,
	TEST_SSH_CLIENT_PHASE_DIRECT_OPEN = 10,
	TEST_SSH_CLIENT_PHASE_DIRECT_ACTIVE = 11,
	TEST_SSH_CLIENT_PHASE_DONE = 12
} testsshclientphase;



/* 同一真实 TCP fixture 覆盖成功、策略拒绝和对端断线。 */
typedef enum testsshclientmode {
	TEST_SSH_CLIENT_MODE_SUCCESS = 0,
	TEST_SSH_CLIENT_MODE_REJECT_HOST = 1,
	TEST_SSH_CLIENT_MODE_DISCONNECT = 2,
	TEST_SSH_CLIENT_MODE_TIMEOUT = 3
} testsshclientmode;



/* 服务端复用公开 SessionStream 与 Channels，不复制客户端或 packet 状态机。 */
typedef struct testsshserver {
	testsshclientruntime* Runtime;
	xsshsessionstream Session;
	xsshchannels Channels;
	xsshreplyqueue GlobalReplies;
	xnetstream* Stream;
	xsshchannel* Channel;
	unsigned char Seed[32];
	unsigned char HostKey[128];
	size_t HostKeySize;
	size_t OpenCount;
	uint32 RejectedChannelLocal;
	uint32 ForwardedChannelLocal;
	uint32 PendingConsume;
	testsshserveraction Application;
	bool AuthSuccess;
	bool RejectedChannelPending;
	bool ForwardedChannelPending;
	bool RetryRejectPending;
	bool ResourcesReady;
} testsshserver;



/* 主线程只读取原子结果，所有协议对象仍由唯一 Worker 串行访问。 */
struct testsshclientruntime {
	testsshserver Server;
	xsshclient Client;
	xnetengine* Engine;
	xnetresolver* Resolver;
	xnetlistener* Listener;
	xnetdial* ClientDial;
	xnetstream* ClientStream;
	xfuture* ReadyFuture;
	xfuture* ImmediateReadyFuture;
	xfuture* CloseFuture;
	xfuture* CancelFuture;
	xfuture* CloseStressFutures[TEST_SSH_CLIENT_CLOSE_WAITERS];
	xfuture* GlobalFutures[2];
	xfuture* OpenFutures[3];
	xfuture* ImmediateOpenFutures[3];
	xfuture* RejectedOpenFuture;
	xfuture* RejectedReplyFuture;
	xfuture* ForwardedOpenFuture;
	xfuture* ForwardedEofFuture;
	xfuture* ForwardedReadTerminalFuture;
	xfuture* ReplyFutures[3];
	xfuture* EofFutures[3];
	xfuture* ReadTerminalFutures[3];
	xfuture* StderrTerminalFuture;
	xfuture* WriteFutures[TEST_SSH_CLIENT_WRITE_WAITERS];
	unsigned char DirectPayload[TEST_SSH_CLIENT_DIRECT_BYTES];
	size_t DirectQueued;
	size_t WriteFutureCount;
	uint32 RejectedChannelLocal;
	bool RejectedChannelPending;
	xatomic32 DialDone;
	xatomic32 FuturesReady;
	xatomic32 CancellationDone;
	xatomic32 Ready;
	xatomic32 ChannelOpened;
	xatomic32 ChannelOpenFailed;
	xatomic32 RequestFailed;
	xatomic32 RequestSucceeded;
	xatomic32 DataReceived;
	xatomic32 ExitReceived;
	xatomic32 ExitSignalReceived;
	xatomic32 EofReceived;
	xatomic32 ChannelClosed;
	xatomic32 ClientClosed;
	xatomic32 ServerClosed;
	xatomic32 ListenerClosed;
	xatomic32 HostChecks;
	xatomic32 ClientAuthCalls;
	xatomic32 ServerNone;
	xatomic32 ServerPasswordRejected;
	xatomic32 ServerPassword;
	xatomic32 ServerEnvRejected;
	xatomic32 ServerExec;
	xatomic32 ServerForward;
	xatomic32 ServerForwardedOpen;
	xatomic32 ServerForwardedData;
	xatomic32 ServerForwardCancel;
	xatomic32 ServerPty;
	xatomic32 ServerResize;
	xatomic32 ServerShell;
	xatomic32 ServerDirectRejected;
	xatomic32 ServerDirect;
	xatomic32 ServerDirectData;
	xatomic32 ServerDirectBytes;
	xatomic32 ServerAdjusts;
	xatomic32 DirectFlushes;
	xatomic32 BackpressureRejected;
	xatomic32 FutureOomPoints;
	xatomic32 GlobalNotified;
	xatomic32 PtySucceeded;
	xatomic32 ShellSucceeded;
	xatomic32 ExecDataReceived;
	xatomic32 StderrDataReceived;
	xatomic32 PtyDataReceived;
	xatomic32 DirectDataReceived;
	xatomic32 ForwardedAccepted;
	xatomic32 ForwardedDataReceived;
	xatomic32 PacketHeld;
	xatomic32 PacketResumed;
	xatomic32 PacketRetryInjected;
	xatomic32 PacketRetryErrors;
	xatomic32 PacketRetried;
	xatomic32 ServerRetryRejected;
	xatomic32 IgnoreSent;
	xatomic32 ServerIgnores;
	xatomic32 TcpAgainObserved;
	xatomic32 TcpAgainResumed;
	xatomic32 HostRejectErrors;
	xatomic32 DisconnectErrors;
	xatomic32 TimeoutErrors;
	xatomic32 ServerDisconnects;
	xatomic32 ServerStalls;
	xatomic32 Errors;
	xnetresult ClientCloseResult;
	bool ClientCloseHasError;
	bool Backpressure;
	xnetportbackend Backend;
	testsshclientphase Phase;
	testsshclientmode Mode;
};



/* 等待 Worker 发布指定运行证据。 */
static void testSshClientRuntimeWait(
	const xatomic32* pValue,
	uint32 iExpected,
	const char* sMessage
)
{
	uint32 i;

	for ( i = 0u; i < 10000u; ++i ) {
		if ( xrtAtomic32Load(pValue, XMEMORY_ACQUIRE) >= iExpected ) {
			return;
		}
		xrtSleep(1u);
	}
	testRequire(false, sMessage);
}



/* Select 成功链不制造内核阈值；IOCP 链必须证明整包保留和 Drain 恢复。 */
static bool testSshClientRuntimeBackpressureEvidence(
	const testsshclientruntime* pRuntime
)
{
	uint32 iSent = xrtAtomic32Load(
		&pRuntime->IgnoreSent,
		XMEMORY_ACQUIRE
	);
	uint32 iReceived = xrtAtomic32Load(
		&pRuntime->ServerIgnores,
		XMEMORY_ACQUIRE
	);
	uint32 iObserved = xrtAtomic32Load(
		&pRuntime->TcpAgainObserved,
		XMEMORY_ACQUIRE
	);
	uint32 iResumed = xrtAtomic32Load(
		&pRuntime->TcpAgainResumed,
		XMEMORY_ACQUIRE
	);

	if ( !pRuntime->Backpressure ) {
		return (iSent == 0u) && (iReceived == 0u) &&
			(iObserved == 0u) && (iResumed == 0u);
	}
	return (pRuntime->Backend == XNET_PORT_IOCP) && (iSent >= 2u) &&
		(iReceived == iSent) && (iObserved == 1u) && (iResumed == 1u);
}



/* 返回当前 writer 的完整 payload。 */
static xbytesview testSshClientRuntimePayload(
	const xsshwriter* pWriter,
	const void* pData
)
{
	return (xbytesview){ (const unsigned char*)pData, pWriter->Size };
}



/* 把服务端 payload 交给 SessionStream 的唯一写事务。 */
static xsshcode testSshClientRuntimeServerSend(
	testsshserver* pServer,
	xbytesview Payload,
	xsshchannel* pChannel
)
{
	xsshsessiontcp* pSession = xrtSshSessionStreamSession(
		&pServer->Session
	);
	xsshsessionpacketkind Kind;

	if ( pSession == NULL ) {
		return XSSH_ERROR_STATE;
	}
	return xrtSshSessionTcpWritePrepare(
		pSession,
		Payload,
		pChannel != NULL ? &pChannel->Core : NULL,
		NULL,
		0u,
		xrtClock() / 1000u,
		&Kind
	);
}



/* 构建并发送当前服务端应用动作，提交后由下一次 CONNECTION 继续。 */
static xsshcode testSshClientRuntimeServerApplication(
	testsshserver* pServer
)
{
	unsigned char arrPayload[256];
	xbytesview Data;
	xsshchannel* pChannel;
	xsshwriter Writer;
	uint32 iLocal;
	uint32 iRemote;
	xsshcode Code;

	if ( pServer->Application == TEST_SSH_SERVER_ACTION_NONE ) {
		return XSSH_OK;
	}
	pChannel = ((pServer->Application ==
		TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS) ||
		(pServer->Application ==
		 TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE)) ? NULL : pServer->Channel;
	if ( !xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) ||
		((pChannel == NULL) &&
		 (pServer->Application != TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS) &&
		 (pServer->Application != TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE)) ) {
		 return XSSH_ERROR_STATE;
	}
	if ( pChannel != NULL ) {
		if ( pServer->Application == TEST_SSH_SERVER_ACTION_FORWARDED_OPEN ) {
			if ( xrtSshChannelCorePhase(&pChannel->Core) !=
				XSSH_CHANNEL_CORE_OPENING ) {
				return XSSH_ERROR_STATE;
			}
			iLocal = pChannel->Core.Local;
			iRemote = 0u;
		} else if ( !xrtSshChannelCoreIds(
			&pChannel->Core,
			&iLocal,
			&iRemote
		) ) {
			return XSSH_ERROR_STATE;
		}
	}
	if ( (pChannel != NULL) && (pServer->PendingConsume != 0u) ) {
		Code = xrtSshChannelCoreDataConsume(
			&pChannel->Core,
			pServer->PendingConsume
		);
		if ( Code != XSSH_OK ) {
			return Code;
		}
		pServer->PendingConsume = 0u;
	}
	if ( pServer->Application == TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS ) {
		Code = xrtSshGlobalSuccessWrite(
			&Writer,
			(xbytesview){ NULL, 0u }
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE ) {
		Code = xrtSshGlobalFailureWrite(&Writer);
	} else if ( pServer->Application == TEST_SSH_SERVER_ACTION_OPEN ) {
		Code = xrtSshChannelOpenConfirmationWrite(
			&Writer,
			iRemote,
			iLocal,
			pServer->Channel->Core.Window.ReceiveWindow,
			pServer->Channel->Core.Window.ReceiveMaxPacket,
			(xbytesview){ NULL, 0u }
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_OPEN_FAILURE ) {
		Code = xrtSshChannelOpenFailureWrite(
			&Writer,
			iRemote,
			XSSH_CHANNEL_OPEN_CONNECT_FAILED,
			XRT_STR_LITERAL("test endpoint rejected"),
			XRT_STR_LITERAL("en")
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_FORWARDED_OPEN ) {
		Code = xrtSshForwardedTcpipOpenWrite(
			&Writer,
			iLocal,
			pChannel->Core.Window.ReceiveWindow,
			pChannel->Core.Window.ReceiveMaxPacket,
			XRT_BYTES_LITERAL("127.0.0.1"),
			2222u,
			XRT_BYTES_LITERAL("198.51.100.7"),
			45000u
		);
	} else if ( (pServer->Application ==
		TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_ONLY) ||
		(pServer->Application ==
		 TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_STREAM) ) {
		Code = xrtSshChannelSuccessWrite(&Writer, iRemote);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_REQUEST_FAILURE_ONLY ) {
		Code = xrtSshChannelFailureWrite(&Writer, iRemote);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_SESSION ) {
		Data = pServer->OpenCount == 1u ?
			XRT_BYTES_LITERAL("ok\n") : XRT_BYTES_LITERAL("pty\n");
		Code = xrtSshChannelDataWrite(
			&Writer,
			iRemote,
			Data
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_STDERR ) {
		Code = xrtSshChannelExtendedDataWrite(
			&Writer,
			iRemote,
			XSSH_CHANNEL_EXTENDED_DATA_STDERR,
			XRT_BYTES_LITERAL("warn\n")
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_DIRECT ) {
		Code = xrtSshChannelDataWrite(
			&Writer,
			iRemote,
			XRT_BYTES_LITERAL("pong\n")
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_FORWARDED ) {
		Code = xrtSshChannelDataWrite(
			&Writer,
			iRemote,
			XRT_BYTES_LITERAL("forward\n")
		);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_WINDOW_ADJUST ) {
		uint32 iBytes = xrtSshChannelCoreAdjustLimit(&pChannel->Core);

		Code = iBytes == TEST_SSH_CLIENT_DIRECT_PACKET ?
			xrtSshChannelWindowAdjustWrite(
				&Writer,
				iRemote,
				iBytes
			) : XSSH_ERROR_STATE;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_EXIT_STATUS ) {
		Code = xrtSshChannelExitStatusWrite(&Writer, iRemote, 0u);
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_EXIT_SIGNAL ) {
		Code = xrtSshChannelExitSignalWrite(
			&Writer,
			iRemote,
			XRT_STR_LITERAL("TERM"),
			false,
			XRT_STR_LITERAL("session ended"),
			XRT_STR_LITERAL("en")
		);
	} else if ( pServer->Application == TEST_SSH_SERVER_ACTION_EOF ) {
		Code = xrtSshChannelEofWrite(&Writer, iRemote);
	} else if ( pServer->Application == TEST_SSH_SERVER_ACTION_CLOSE ) {
		Code = xrtSshChannelCloseWrite(&Writer, iRemote);
	} else {
		return XSSH_ERROR_STATE;
	}
	if ( Code != XSSH_OK ) {
		return Code;
	}
	Code = testSshClientRuntimeServerSend(
		pServer,
		testSshClientRuntimePayload(&Writer, arrPayload),
		pChannel
	);
	if ( Code != XSSH_OK ) {
		return Code;
	}
	if ( pServer->Application == TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS ) {
		pServer->Application = (pServer->Channel != NULL) &&
			(xrtSshChannelCorePhase(&pServer->Channel->Core) ==
			 XSSH_CHANNEL_CORE_OPENING) ?
			TEST_SSH_SERVER_ACTION_FORWARDED_OPEN :
			TEST_SSH_SERVER_ACTION_NONE;
	} else if ( (pServer->Application ==
		TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE) ||
		(pServer->Application == TEST_SSH_SERVER_ACTION_OPEN) ||
		(pServer->Application == TEST_SSH_SERVER_ACTION_OPEN_FAILURE) ||
		(pServer->Application == TEST_SSH_SERVER_ACTION_FORWARDED_OPEN) ||
		(pServer->Application ==
		 TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_ONLY) ||
		(pServer->Application ==
		 TEST_SSH_SERVER_ACTION_REQUEST_FAILURE_ONLY) ||
		(pServer->Application == TEST_SSH_SERVER_ACTION_WINDOW_ADJUST) ||
		(pServer->Application == TEST_SSH_SERVER_ACTION_CLOSE) ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_NONE;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_STREAM ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_DATA_SESSION;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_SESSION ) {
		pServer->Application = pServer->OpenCount == 1u ?
			TEST_SSH_SERVER_ACTION_DATA_STDERR :
			TEST_SSH_SERVER_ACTION_EXIT_SIGNAL;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_STDERR ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_EXIT_STATUS;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_DIRECT ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_EOF;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_DATA_FORWARDED ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_EOF;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_EXIT_STATUS ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_EOF;
	} else if ( pServer->Application ==
		TEST_SSH_SERVER_ACTION_EXIT_SIGNAL ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_EOF;
	} else if ( pServer->Application == TEST_SSH_SERVER_ACTION_EOF ) {
		pServer->Application = TEST_SSH_SERVER_ACTION_CLOSE;
	} else {
		return XSSH_ERROR_STATE;
	}
	return XSSH_OK;
}



/* 服务端建立 Worker 关联的动态 channel 集合。 */
static void testSshClientRuntimeServerOpen(
	xsshsessionstream* pSession,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;
	xsshchannelsconfig Config;
	xnetstream* pStream = xrtSshSessionStreamTcp(pSession);
	xnetworker* pWorker = pStream != NULL ?
		xrtNetStreamWorker(pStream) : NULL;
	xnetbufpool* pPool = pWorker != NULL ?
		xrtNetWorkerBufPool(pWorker) : NULL;

	testRequire((pSession == &pServer->Session) &&
		(pPool != NULL), "ssh client runtime server open failed");
	xrtSshChannelsConfigInit(&Config);
	Config.ReceiveWindow = TEST_SSH_CLIENT_DIRECT_QUEUE;
	Config.ReceiveMaxPacket = TEST_SSH_CLIENT_DIRECT_PACKET;
	Config.AdjustThreshold = TEST_SSH_CLIENT_DIRECT_PACKET;
	testRequire(xrtSshChannelsInit(&pServer->Channels, pPool, &Config),
		"ssh client runtime server channels failed");
	pServer->ResourcesReady = true;
}



/* 服务端按 SessionCore 动作构建 KEX、认证和应用输出。 */
static void testSshClientRuntimeServerAction(
	xsshsessionstream* pStream,
	xsshsessionaction Action,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;
	unsigned char arrPayload[1024];
	unsigned char arrSignature[128];
	unsigned char arrRawSignature[64];
	xsshsessiontcp* pSession = xrtSshSessionStreamSession(pStream);
	xsshsessioncore* pCore;
	xsshkexsession* pKex;
	xsshkexinitconfig KexConfig;
	xbytesview Hash;
	xsshwriter Writer;
	xsshwriter SignatureWriter;
	xsshcode Code = XSSH_OK;

	testRequire((pStream == &pServer->Session) && (pSession != NULL),
		"ssh client runtime server action ownership failed");
	if ( Action == XSSH_SESSION_ACTION_WRITE_IDENTIFICATION ) {
		if ( pServer->Runtime->Mode == TEST_SSH_CLIENT_MODE_TIMEOUT ) {
			(void)xrtAtomic32FetchAdd(
				&pServer->Runtime->ServerStalls,
				1u,
				XMEMORY_RELEASE
			);
		} else {
			Code = xrtSshSessionTcpIdentificationWritePrepare(
				pSession,
				XRT_STR_LITERAL("SSH-2.0-xssh_runtime_server")
			);
		}
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_KEXINIT ) {
		Code = (!xrtSshKexInitConfigInit(
			&KexConfig,
			XSSH_ROLE_SERVER,
			true
		) || !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		)) ? XSSH_ERROR_STATE : xrtSshKexInitWriteSecure(
			&Writer,
			&KexConfig
		);
		if ( Code == XSSH_OK ) {
			Code = testSshClientRuntimeServerSend(
				pServer,
				testSshClientRuntimePayload(&Writer, arrPayload),
				NULL
			);
		}
	} else if ( Action == XSSH_SESSION_ACTION_BEGIN_KEX ) {
		Code = xrtSshSessionTcpKexBegin(
			pSession,
			(xbytesview){ pServer->HostKey, pServer->HostKeySize }
		);
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_ECDH_REPLY ) {
		pCore = xrtSshSessionTcpCore(pSession);
		pKex = pCore != NULL ? xrtSshKexExchangeSession(
			xrtSshSessionCoreKex(pCore)
		) : NULL;
		Code = (pKex == NULL) ? XSSH_ERROR_STATE :
			xrtSshKexSessionExchangeHash(pKex, &Hash);
		if ( (Code == XSSH_OK) && !xrtEd25519Sign(
			pServer->Seed,
			Hash.Data,
			Hash.Size,
			arrRawSignature
		) ) {
			Code = XSSH_ERROR_STATE;
		}
		if ( (Code == XSSH_OK) && (!xrtSshWriterInit(
			&SignatureWriter,
			arrSignature,
			sizeof(arrSignature)
		) || (xrtSshEd25519SignatureWrite(
			&SignatureWriter,
			(xbytesview){ arrRawSignature, sizeof(arrRawSignature) }
		) != XSSH_OK) || !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		)) ) {
			Code = XSSH_ERROR_STATE;
		}
		if ( Code == XSSH_OK ) {
			Code = xrtSshKexSessionEcdhReplyPrepare(
				pKex,
				&Writer,
				testSshClientRuntimePayload(
					&SignatureWriter,
					arrSignature
				)
			);
		}
		if ( Code == XSSH_OK ) {
			Code = testSshClientRuntimeServerSend(
				pServer,
				testSshClientRuntimePayload(&Writer, arrPayload),
				NULL
			);
		}
		xrtSecureZero(arrRawSignature, sizeof(arrRawSignature));
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_NEWKEYS ) {
		pCore = xrtSshSessionTcpCore(pSession);
		pKex = pCore != NULL ? xrtSshKexExchangeSession(
			xrtSshSessionCoreKex(pCore)
		) : NULL;
		Code = ((pKex == NULL) || !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		)) ? XSSH_ERROR_STATE : xrtSshKexSessionNewKeysPrepare(
			pKex,
			&Writer
		);
		if ( Code == XSSH_OK ) {
			Code = testSshClientRuntimeServerSend(
				pServer,
				testSshClientRuntimePayload(&Writer, arrPayload),
				NULL
			);
		}
	} else if ( Action == XSSH_SESSION_ACTION_BEGIN_AUTH ) {
		Code = xrtSshSessionTcpAuthBegin(
			pSession,
			NULL,
			xrtClock() / 1000u
		);
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_SERVICE_ACCEPT ) {
		Code = !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		) ? XSSH_ERROR_STATE : xrtSshServiceAcceptWrite(
			&Writer,
			XRT_STR_LITERAL(XSSH_SERVICE_USERAUTH)
		);
		if ( Code == XSSH_OK ) {
			Code = testSshClientRuntimeServerSend(
				pServer,
				testSshClientRuntimePayload(&Writer, arrPayload),
				NULL
			);
		}
	} else if ( Action == XSSH_SESSION_ACTION_WRITE_AUTH_RESULT ) {
		Code = !xrtSshWriterInit(
			&Writer,
			arrPayload,
			sizeof(arrPayload)
		) ? XSSH_ERROR_STATE : (pServer->AuthSuccess ?
			xrtSshAuthSuccessWrite(&Writer) : xrtSshAuthFailureWrite(
				&Writer,
				XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD),
				false
			));
		if ( Code == XSSH_OK ) {
			Code = testSshClientRuntimeServerSend(
				pServer,
				testSshClientRuntimePayload(&Writer, arrPayload),
				NULL
			);
		}
	} else if ( Action == XSSH_SESSION_ACTION_CONNECTION ) {
		if ( pServer->RetryRejectPending ) {
			xsshchannel* pChannel = xrtSshChannelsGet(
				&pServer->Channels,
				pServer->ForwardedChannelLocal
			);

			Code = (pChannel != NULL) &&
				(xrtSshChannelCorePhase(&pChannel->Core) ==
				 XSSH_CHANNEL_CORE_FAILED) && xrtSshChannelsDiscard(
					&pServer->Channels,
					pServer->ForwardedChannelLocal
				) && (xrtSshChannelsOpen(
					&pServer->Channels,
					&pServer->Channel
				) == XSSH_OK) ? XSSH_OK : XSSH_ERROR_STATE;
			if ( Code == XSSH_OK ) {
				pServer->ForwardedChannelLocal =
					pServer->Channel->Core.Local;
				pServer->Application =
					TEST_SSH_SERVER_ACTION_FORWARDED_OPEN;
				pServer->RetryRejectPending = false;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerRetryRejected,
					1u,
					XMEMORY_RELEASE
				);
				Code = testSshClientRuntimeServerApplication(pServer);
			}
		} else if ( pServer->Application == TEST_SSH_SERVER_ACTION_ABORT ) {
			xnetstream* pTcp = xrtSshSessionStreamTcp(pStream);
			xnetsocket Socket = pTcp != NULL ?
				xrtNetStreamSocket(pTcp) : NULL;

			pServer->Application = TEST_SSH_SERVER_ACTION_NONE;
			Code = (Socket != NULL) && xrtNetSocketSet(
				Socket,
				XNET_OPTION_LINGER,
				0
			) && xrtSshSessionStreamAbort(pStream) ?
				XSSH_OK : XSSH_ERROR_STATE;
		} else {
			Code = testSshClientRuntimeServerApplication(pServer);
		}
	}
	if ( Code != XSSH_OK ) {
		fprintf(stderr, "[SERVER ACTION ERROR] action=%d code=%d\n",
			(int)Action, (int)Code);
		(void)xrtAtomic32FetchAdd(
			&pServer->Runtime->Errors,
			1u,
			XMEMORY_RELEASE
		);
		(void)xrtSshSessionStreamAbort(pStream);
	}
}



/* 服务端接受已经通过格式验证的 SSH-2.0 identification。 */
static xsshsessionstreamdecision testSshClientRuntimeServerIdentification(
	xsshsessionstream* pSession,
	xstrview Version,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;

	return (pSession == &pServer->Session) && (Version.Size >= 8u) &&
		(memcmp(Version.Data, "SSH-2.0-", 8u) == 0) ?
		XSSH_SESSION_STREAM_ACCEPT : XSSH_SESSION_STREAM_ABORT;
}



/* 服务端在提交前读取认证和应用字段，并暂存下一条响应。 */
static xsshsessionstreamdecision testSshClientRuntimeServerPacket(
	xsshsessionstream* pSession,
	const xsshsessiontcppacket* pPacket,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;
	const xsshconnectionpacket* pConnection;
	xsshauthsession* pAuth;
	xsshauthrequest Request;
	xsshauthpassword Password;
	xsshtcpipforward Forward;
	xsshtcpipopen Tcpip;
	xsshchannelenv Env;
	xsshchannelpty Pty;
	xsshchannelwindowchange Change;
	xbytesview Command;
	xsshcode Code = XSSH_OK;

	if ( pPacket->Session.Kind == XSSH_SESSION_PACKET_IGNORE ) {
		if ( (pPacket->Session.Message.Ignore.Data.Size !=
			TEST_SSH_CLIENT_IGNORE_BYTES) ||
			(pPacket->Session.Message.Ignore.Data.Data == NULL) ) {
			Code = XSSH_ERROR_PROTOCOL;
		} else {
			(void)xrtAtomic32FetchAdd(
				&pServer->Runtime->ServerIgnores,
				1u,
				XMEMORY_RELEASE
			);
		}
	} else if ( pPacket->Session.Kind == XSSH_SESSION_PACKET_AUTH ) {
		pAuth = xrtSshSessionCoreAuth(
			xrtSshSessionTcpCore(xrtSshSessionStreamSession(pSession))
		);
		if ( (pPacket->Session.Message.Auth ==
			XSSH_AUTH_SESSION_PACKET_REQUEST) && (pAuth != NULL) &&
			(xrtSshAuthSessionRequest(pAuth, &Request) == XSSH_OK) ) {
			if ( testSshTextEqual(
				Request.Method,
				XRT_STR_LITERAL(XSSH_AUTH_METHOD_NONE)
			) ) {
				pServer->AuthSuccess = false;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerNone,
					1u,
					XMEMORY_RELEASE
				);
			} else if ( testSshTextEqual(
				Request.Method,
				XRT_STR_LITERAL(XSSH_AUTH_METHOD_PASSWORD)
			) ) {
				Code = xrtSshAuthPasswordRead(
					pPacket->Session.Payload,
					&Password
				);
				if ( (Code != XSSH_OK) || !testSshTextEqual(
					Password.User,
					XRT_STR_LITERAL("alice")
				) ) {
					Code = XSSH_ERROR_PROTOCOL;
				} else if ( testSshTextEqual(
					Password.Password,
					XRT_STR_LITERAL("secret")
				) ) {
					pServer->AuthSuccess = true;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerPassword,
						1u,
						XMEMORY_RELEASE
					);
				} else {
					pServer->AuthSuccess = false;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerPasswordRejected,
						1u,
						XMEMORY_RELEASE
					);
				}
			} else {
				Code = XSSH_ERROR_PROTOCOL;
			}
		}
	} else if ( pPacket->Session.Kind == XSSH_SESSION_PACKET_CONNECTION ) {
		pConnection = &pPacket->Session.Message.Connection;
		if ( pConnection->Kind == XSSH_CONNECTION_PACKET_GLOBAL_REQUEST ) {
			if ( pServer->ForwardedChannelPending ) {
				xsshchannel* pForwarded = xrtSshChannelsGet(
					&pServer->Channels,
					pServer->ForwardedChannelLocal
				);

				Code = (pForwarded != NULL) &&
					(xrtSshChannelCorePhase(&pForwarded->Core) ==
					 XSSH_CHANNEL_CORE_CLOSED) && xrtSshChannelsDiscard(
						&pServer->Channels,
						pServer->ForwardedChannelLocal
					) ? XSSH_OK : XSSH_ERROR_STATE;
				pServer->Channel = NULL;
				pServer->ForwardedChannelPending = false;
			}
			if ( (Code != XSSH_OK) ||
				(pServer->Application != TEST_SSH_SERVER_ACTION_NONE) ||
				!pConnection->Message.GlobalRequest.WantReply ) {
				Code = XSSH_ERROR_PROTOCOL;
			} else if ( testSshTextEqual(
				pConnection->Message.GlobalRequest.Name,
				XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD)
			) ) {
				Code = xrtSshTcpipForwardRead(
					&pConnection->Message.GlobalRequest,
					&Forward
				);
				if ( (Code == XSSH_OK) && testSshBytesEqual(
					Forward.Address,
					XRT_BYTES_LITERAL("127.0.0.1")
				) && (Forward.Port == 2222u) ) {
					if ( pServer->Runtime->Mode ==
						TEST_SSH_CLIENT_MODE_DISCONNECT ) {
						(void)xrtAtomic32FetchAdd(
							&pServer->Runtime->ServerDisconnects,
							1u,
							XMEMORY_RELEASE
						);
					} else {
						Code = xrtSshChannelsOpen(
							&pServer->Channels,
							&pServer->Channel
						);
						if ( Code == XSSH_OK ) {
							pServer->ForwardedChannelLocal =
								pServer->Channel->Core.Local;
							(void)xrtAtomic32FetchAdd(
								&pServer->Runtime->ServerForward,
								1u,
								XMEMORY_RELEASE
							);
						}
					}
				} else {
					Code = XSSH_ERROR_PROTOCOL;
				}
			} else if ( testSshTextEqual(
				pConnection->Message.GlobalRequest.Name,
				XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD)
			) ) {
				Code = xrtSshTcpipForwardCancelRead(
					&pConnection->Message.GlobalRequest,
					&Forward
				);
				if ( (Code == XSSH_OK) && testSshBytesEqual(
					Forward.Address,
					XRT_BYTES_LITERAL("127.0.0.1")
				) && (Forward.Port == 2222u) ) {
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerForwardCancel,
						1u,
						XMEMORY_RELEASE
					);
				} else {
					Code = XSSH_ERROR_PROTOCOL;
				}
			} else {
				Code = XSSH_ERROR_PROTOCOL;
			}
			if ( Code == XSSH_OK ) {
				if ( (pServer->Runtime->Mode ==
					TEST_SSH_CLIENT_MODE_DISCONNECT) && testSshTextEqual(
						pConnection->Message.GlobalRequest.Name,
						XRT_STR_LITERAL(XSSH_GLOBAL_REQUEST_TCPIP_FORWARD)
					) ) {
					pServer->Application = TEST_SSH_SERVER_ACTION_ABORT;
				} else {
					pServer->Application = testSshTextEqual(
					pConnection->Message.GlobalRequest.Name,
					XRT_STR_LITERAL(
						XSSH_GLOBAL_REQUEST_CANCEL_TCPIP_FORWARD
					)
				) ? TEST_SSH_SERVER_ACTION_GLOBAL_FAILURE :
					TEST_SSH_SERVER_ACTION_GLOBAL_SUCCESS;
				}
			}
		} else if ( pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_CONFIRMATION ) {
			if ( (pServer->Application != TEST_SSH_SERVER_ACTION_NONE) ||
				(pServer->Channel == NULL) ||
				(pConnection->Message.ChannelConfirmation.Recipient !=
				 pServer->ForwardedChannelLocal) ) {
				Code = XSSH_ERROR_PROTOCOL;
			} else {
				pServer->Application = TEST_SSH_SERVER_ACTION_DATA_FORWARDED;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerForwardedOpen,
					1u,
					XMEMORY_RELEASE
				);
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerForwardedData,
					1u,
					XMEMORY_RELEASE
				);
			}
		} else if ( pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_OPEN_FAILURE ) {
			if ( (pServer->Runtime->Mode != TEST_SSH_CLIENT_MODE_SUCCESS) ||
				pServer->RetryRejectPending ||
				(pConnection->Message.ChannelOpenFailure.Recipient !=
				 pServer->ForwardedChannelLocal) ) {
				Code = XSSH_ERROR_PROTOCOL;
			} else {
				pServer->RetryRejectPending = true;
			}
		} else if ( pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_OPEN ) {
			size_t iNext = pServer->OpenCount + 1u;
			bool bSession = (iNext == 1u) || (iNext == 2u);
			bool bDirectCandidate = iNext == 3u;
			bool bDirect = false;
			bool bReject = false;

			if ( pServer->RejectedChannelPending ) {
				Code = (xrtSshChannelCorePhase(&pServer->Channel->Core) ==
					XSSH_CHANNEL_CORE_FAILED) && xrtSshChannelsDiscard(
						&pServer->Channels,
						pServer->RejectedChannelLocal
					) ? XSSH_OK : XSSH_ERROR_STATE;
				pServer->RejectedChannelPending = false;
			}
			Code = (pServer->Application != TEST_SSH_SERVER_ACTION_NONE) ?
				XSSH_ERROR_PROTOCOL : Code;
			if ( (Code == XSSH_OK) && bSession && !testSshTextEqual(
				pConnection->Message.ChannelOpen.Type,
				XRT_STR_LITERAL(XSSH_CHANNEL_TYPE_SESSION)
			) ) {
				Code = XSSH_ERROR_PROTOCOL;
			}
			if ( (Code == XSSH_OK) && bDirectCandidate ) {
				Code = xrtSshDirectTcpipOpenRead(
					&pConnection->Message.ChannelOpen,
					&Tcpip
				);
				if ( (Code != XSSH_OK) || !testSshBytesEqual(
					Tcpip.Originator,
					XRT_BYTES_LITERAL("127.0.0.1")
				) || (Tcpip.OriginatorPort != 50000u) ) {
					Code = XSSH_ERROR_PROTOCOL;
				} else {
					bDirect = testSshBytesEqual(
						Tcpip.Host,
						XRT_BYTES_LITERAL("service.internal")
					) && (Tcpip.Port == 8080u);
					bReject = testSshBytesEqual(
						Tcpip.Host,
						XRT_BYTES_LITERAL("reject.internal")
					) && (Tcpip.Port == 8081u);
					if ( !bDirect && !bReject ) {
						Code = XSSH_ERROR_PROTOCOL;
					}
				}
			}
			if ( (Code == XSSH_OK) && !bSession && !bDirectCandidate ) {
				Code = XSSH_ERROR_PROTOCOL;
			}
			if ( (Code == XSSH_OK) && bReject ) {
				Code = xrtSshChannelsAccept(
					&pServer->Channels,
					&pConnection->Message.ChannelOpen,
					&pServer->Channel
				);
				if ( Code == XSSH_OK ) {
					pServer->RejectedChannelLocal =
						pServer->Channel->Core.Local;
					pServer->RejectedChannelPending = true;
					pServer->Application =
						TEST_SSH_SERVER_ACTION_OPEN_FAILURE;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerDirectRejected,
						1u,
						XMEMORY_RELEASE
					);
				}
			} else if ( Code == XSSH_OK ) {
				Code = xrtSshChannelsAccept(
					&pServer->Channels,
					&pConnection->Message.ChannelOpen,
					&pServer->Channel
				);
			}
			if ( (Code == XSSH_OK) && !bReject ) {
				pServer->OpenCount = iNext;
				if ( bDirect ) {
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerDirect,
						1u,
						XMEMORY_RELEASE
					);
				}
				pServer->Application = TEST_SSH_SERVER_ACTION_OPEN;
			}
		} else if ( pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_REQUEST ) {
			if ( (pServer->OpenCount == 1u) &&
				testSshTextEqual(
					pConnection->Message.ChannelRequest.Type,
					XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_ENV)
				) ) {
				Code = xrtSshChannelEnvRead(
					&pConnection->Message.ChannelRequest,
					&Env
				);
				if ( (Code != XSSH_OK) || !testSshBytesEqual(
					Env.Name,
					XRT_BYTES_LITERAL("REJECT_ME")
				) || !testSshBytesEqual(
					Env.Value,
					XRT_BYTES_LITERAL("1")
				) || !pConnection->Message.ChannelRequest.WantReply ) {
					Code = XSSH_ERROR_PROTOCOL;
				}
				if ( Code == XSSH_OK ) {
					pServer->Application =
						TEST_SSH_SERVER_ACTION_REQUEST_FAILURE_ONLY;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerEnvRejected,
						1u,
						XMEMORY_RELEASE
					);
				}
			} else if ( (pServer->OpenCount == 1u) &&
				testSshTextEqual(
					pConnection->Message.ChannelRequest.Type,
					XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_EXEC)
				) ) {
				Code = xrtSshChannelExecRead(
					&pConnection->Message.ChannelRequest,
					&Command
				);
				if ( (Code != XSSH_OK) || !testSshBytesEqual(
					Command,
					XRT_BYTES_LITERAL("printf ok")
				) || !pConnection->Message.ChannelRequest.WantReply ) {
					Code = XSSH_ERROR_PROTOCOL;
				}
				pServer->Application =
					TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_STREAM;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerExec,
					1u,
					XMEMORY_RELEASE
				);
			} else if ( (pServer->OpenCount == 2u) &&
				testSshTextEqual(
					pConnection->Message.ChannelRequest.Type,
					XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_PTY)
				) ) {
				Code = xrtSshChannelPtyRead(
					&pConnection->Message.ChannelRequest,
					&Pty
				);
				if ( (Code != XSSH_OK) || !testSshBytesEqual(
					Pty.Terminal,
					XRT_BYTES_LITERAL("xterm-256color")
				) || (Pty.Columns != 80u) || (Pty.Rows != 24u) ||
					(Pty.PixelWidth != 0u) || (Pty.PixelHeight != 0u) ||
					!testSshBytesEqual(Pty.Modes, XRT_BYTES_LITERAL("\0")) ||
					!pConnection->Message.ChannelRequest.WantReply ) {
					Code = XSSH_ERROR_PROTOCOL;
				}
				pServer->Application =
					TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_ONLY;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerPty,
					1u,
					XMEMORY_RELEASE
				);
			} else if ( (pServer->OpenCount == 2u) &&
				testSshTextEqual(
					pConnection->Message.ChannelRequest.Type,
					XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_WINDOW_CHANGE)
				) ) {
				Code = xrtSshChannelWindowChangeRead(
					&pConnection->Message.ChannelRequest,
					&Change
				);
				if ( (Code != XSSH_OK) || (Change.Columns != 120u) ||
					(Change.Rows != 40u) || (Change.PixelWidth != 0u) ||
					(Change.PixelHeight != 0u) ||
					pConnection->Message.ChannelRequest.WantReply ) {
					Code = XSSH_ERROR_PROTOCOL;
				}
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerResize,
					1u,
					XMEMORY_RELEASE
				);
			} else if ( (pServer->OpenCount == 2u) &&
				testSshTextEqual(
					pConnection->Message.ChannelRequest.Type,
					XRT_STR_LITERAL(XSSH_CHANNEL_REQUEST_SHELL)
				) ) {
				Code = xrtSshChannelShellRead(
					&pConnection->Message.ChannelRequest
				);
				if ( (Code != XSSH_OK) ||
					!pConnection->Message.ChannelRequest.WantReply ) {
					Code = XSSH_ERROR_PROTOCOL;
				}
				pServer->Application =
					TEST_SSH_SERVER_ACTION_REQUEST_SUCCESS_STREAM;
				(void)xrtAtomic32FetchAdd(
					&pServer->Runtime->ServerShell,
					1u,
					XMEMORY_RELEASE
				);
			} else {
				Code = XSSH_ERROR_PROTOCOL;
			}
		} else if ( pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_DATA ) {
			xbytesview Data = pConnection->Message.ChannelData.Data;
			uint32 iReceived = xrtAtomic32Load(
				&pServer->Runtime->ServerDirectBytes,
				XMEMORY_RELAXED
			);

			if ( (pServer->OpenCount != 3u) ||
				(pServer->Application != TEST_SSH_SERVER_ACTION_NONE) ||
				(Data.Size != TEST_SSH_CLIENT_DIRECT_PACKET) ||
				(iReceived > (TEST_SSH_CLIENT_DIRECT_BYTES - Data.Size)) ||
				(memcmp(
					Data.Data,
					pServer->Runtime->DirectPayload + iReceived,
					Data.Size
				) != 0) ) {
				Code = XSSH_ERROR_PROTOCOL;
			} else {
				pServer->PendingConsume = (uint32)Data.Size;
				iReceived += (uint32)Data.Size;
				xrtAtomic32Store(
					&pServer->Runtime->ServerDirectBytes,
					iReceived,
					XMEMORY_RELEASE
				);
				if ( iReceived == TEST_SSH_CLIENT_DIRECT_BYTES ) {
					pServer->Application =
						TEST_SSH_SERVER_ACTION_DATA_DIRECT;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerDirectData,
						1u,
						XMEMORY_RELEASE
					);
				} else {
					pServer->Application =
						TEST_SSH_SERVER_ACTION_WINDOW_ADJUST;
					(void)xrtAtomic32FetchAdd(
						&pServer->Runtime->ServerAdjusts,
						1u,
						XMEMORY_RELEASE
					);
				}
			}
		} else if ( (pConnection->Kind ==
			XSSH_CONNECTION_PACKET_CHANNEL_CLOSE) &&
			(pServer->OpenCount == 0u) &&
			(pConnection->Message.Recipient ==
			 pServer->ForwardedChannelLocal) ) {
			pServer->ForwardedChannelPending = true;
		}
	}
	if ( Code != XSSH_OK ) {
		fprintf(stderr, "[SERVER PACKET ERROR] kind=%d code=%d\n",
			(int)pPacket->Session.Kind, (int)Code);
		(void)xrtAtomic32FetchAdd(
			&pServer->Runtime->Errors,
			1u,
			XMEMORY_RELEASE
		);
		return XSSH_SESSION_STREAM_ABORT;
	}
	return XSSH_SESSION_STREAM_ACCEPT;
}



/* 服务端协议错误必须进入唯一失败计数。 */
static void testSshClientRuntimeServerError(
	xsshsessionstream* pSession,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;

	(void)pSession;
	fprintf(stderr, "[SERVER ERROR] code=%d domain=%s operation=%s message=%s\n",
		(int)Code,
		pError != NULL ? xrtErrorDomain(pError) : "",
		pError != NULL ? xrtErrorOperation(pError) : "",
		pError != NULL ? xrtErrorMessage(pError) : "");
	(void)xrtAtomic32FetchAdd(
		&pServer->Runtime->Errors,
		1u,
		XMEMORY_RELEASE
	);
}



/* Peer FIN 只作为正常链路结束证据。 */
static void testSshClientRuntimeServerEnd(
	xsshsessionstream* pSession,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;

	(void)pSession;
	(void)pServer;
}



/* 服务端连接关闭后由主线程释放 Session 和 Channels。 */
static void testSshClientRuntimeServerClose(
	xsshsessionstream* pSession,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshserver* pServer = (testsshserver*)pData;

	(void)pSession;
	(void)Result;
	(void)pError;
	(void)xrtAtomic32FetchAdd(
		&pServer->Runtime->ServerClosed,
		1u,
		XMEMORY_RELEASE
	);
}



/* Listener 在 accepted Stream 所属 Worker 安装服务端驱动。 */
static bool testSshClientRuntimeAccept(
	xnetlistener* pListener,
	xnetstream* pStream,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	(void)pListener;
	pRuntime->Server.Stream = pStream;
	return xrtSshSessionStreamAttach(&pRuntime->Server.Session, pStream);
}



/* Listener 关闭只发布一次。 */
static void testSshClientRuntimeListenerClose(
	xnetlistener* pListener,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	(void)pListener;
	(void)xrtAtomic32FetchAdd(
		&pRuntime->ListenerClosed,
		1u,
		XMEMORY_RELEASE
	);
}



/* 客户端只接受本测试动态生成的服务端主机密钥。 */
static xsshclienthostdecision testSshClientRuntimeHostKey(
	xsshclientcore* pClient,
	const xsshclienthost* pHost,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	(void)pClient;
	(void)xrtAtomic32FetchAdd(
		&pRuntime->HostChecks,
		1u,
		XMEMORY_RELEASE
	);
	return (pRuntime->Mode != TEST_SSH_CLIENT_MODE_REJECT_HOST) &&
		testSshBytesEqual(
		pHost->Key,
		(xbytesview){
			pRuntime->Server.HostKey,
			pRuntime->Server.HostKeySize
		}
	) ? XSSH_CLIENT_HOST_ACCEPT : XSSH_CLIENT_HOST_REJECT;
}



/* 逐个击穿 Future 管理器、waiter、Promise、cancel 和 watch 分配点。 */
static void testSshClientRuntimeFutureOom(
	testsshclientruntime* pRuntime,
	xsshclient* pClient
)
{
	size_t i;

	testRequire(pClient->FutureState == NULL,
		"ssh client Future OOM fixture state mismatch");
	for ( i = 0u; i < TEST_SSH_CLIENT_FUTURE_OOM_SCAN; ++i ) {
		xfuture* pFuture;
		bool bHit;

		xrtClearError();
		testRequire(xrtMemDebugFailAfter((uint64)i),
			"ssh client Future OOM injection setup failed");
		pFuture = xrtSshClientWaitAsync(
			pClient,
			XSSH_CLIENT_WAIT_CLOSE
		);
		bHit = xrtMemDebugFailTriggered();
		xrtMemDebugFailClear();
		if ( pFuture == NULL ) {
			testRequire(bHit,
				"ssh client Future failed outside injected allocation");
		}
		if ( bHit ) {
			(void)xrtAtomic32FetchAdd(
				&pRuntime->FutureOomPoints,
				1u,
				XMEMORY_RELAXED
			);
		}
		__xrtSshClientFutureClear(pClient);
		xrtFutureDestroy(pFuture);
		xrtClearError();
		testRequire(pClient->FutureState == NULL,
			"ssh client Future OOM leaked a partial waiter");
		if ( !bHit ) {
			break;
		}
	}
	testRequire((i < TEST_SSH_CLIENT_FUTURE_OOM_SCAN) &&
		(xrtAtomic32Load(
			&pRuntime->FutureOomPoints,
			XMEMORY_ACQUIRE
		) >= TEST_SSH_CLIENT_FUTURE_OOM_MIN),
		"ssh client Future OOM allocation coverage mismatch");
}



/* 第一次提交错误口令，随后以同一 provider 验证认证失败后的重试预算。 */
static xsshcode testSshClientRuntimeAuth(
	xsshclientcore* pClient,
	xsshwriter* pWriter,
	const xsshclientauth* pAuth,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	xstrview Password;

	if ( (pRuntime == NULL) || (pAuth == NULL) ) {
		return XSSH_ERROR_ARGUMENT;
	}
	if ( pAuth->Attempts == 1u ) {
		Password = XRT_STR_LITERAL("wrong");
	} else if ( pAuth->Attempts == 2u ) {
		Password = XRT_STR_LITERAL("secret");
	} else {
		return XSSH_ERROR_AUTHENTICATION;
	}
	(void)xrtAtomic32FetchAdd(
		&pRuntime->ClientAuthCalls,
		1u,
		XMEMORY_RELEASE
	);
	return xrtSshClientPasswordAuth(
		pClient,
		pWriter,
		pAuth,
		&Password
	);
}



/* 认证完成后先验证 remote forwarding 的全局请求回复关联。 */
static void testSshClientRuntimeStartForward(
	testsshclientruntime* pRuntime,
	xsshclient* pClient
)
{
	pRuntime->Phase = TEST_SSH_CLIENT_PHASE_FORWARD;
	testRequire(xrtSshClientTcpipForward(
		pClient,
		XRT_BYTES_LITERAL("127.0.0.1"),
		2222u,
		UINT64_C(0xf001)
	) == XSSH_OK, "ssh client runtime forward request failed");
	pRuntime->GlobalFutures[0] = xrtSshClientGlobalReplyAsync(
		pClient,
		UINT64_C(0xf001)
	);
	testRequire(pRuntime->GlobalFutures[0] != NULL,
		"ssh client runtime forward Future failed");
}



/* 用合法 IGNORE 报文填满 TCP 硬队列，保留最后一个完整 SSH 写事务等待恢复。 */
static void testSshClientRuntimeTcpAgainStep(
	xnetworker* pWorker,
	ptr pData
);



/* 在同一 Worker 任务内连续提交，令 completion 未回收预算时命中硬上限。 */
static void testSshClientRuntimeTcpAgain(
	testsshclientruntime* pRuntime,
	xsshclient* pClient
)
{
	unsigned char arrPayload[32768];
	unsigned char arrIgnore[TEST_SSH_CLIENT_IGNORE_BYTES];
	xsshwriter Writer;
	xsshcode Code;
	uint32 iSent;

	memset(arrIgnore, 0x5a, sizeof(arrIgnore));
	testRequire(xrtSshWriterInit(&Writer, arrPayload, sizeof(arrPayload)) &&
		(xrtSshIgnoreWrite(
			&Writer,
			(xbytesview){ arrIgnore, sizeof(arrIgnore) }
		) == XSSH_OK), "ssh client runtime IGNORE build failed");
	Code = xrtSshClientSend(
		pClient,
		testSshClientRuntimePayload(&Writer, arrPayload),
		NULL,
		NULL,
		0u
	);
	iSent = xrtAtomic32Load(&pRuntime->IgnoreSent, XMEMORY_ACQUIRE);
	if ( Code != XSSH_OK ) {
		fprintf(stderr,
			"[TCP AGAIN EVIDENCE] sent=%u code=%d pending=%zu "
			"write-paused=%d transport=%zu\n",
			iSent,
			(int)Code,
			xrtNetStreamPending(xrtSshSessionStreamTcp(&pClient->Stream)),
			pClient->Stream.WritePaused ? 1 : 0,
			xrtSshTransportTcpWriteSize(
				&pClient->Stream.Session.Transport
			));
	}
	testRequire(Code == XSSH_OK,
		"ssh client runtime IGNORE send failed");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->IgnoreSent,
		1u,
		XMEMORY_RELEASE
	);
	if ( pClient->Stream.WritePaused ) {
		testRequire(xrtSshTransportTcpWriteSize(
			&pClient->Stream.Session.Transport
		) != 0u, "ssh client runtime TCP AGAIN lost pending packet");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->TcpAgainObserved,
			1u,
			XMEMORY_RELEASE
		);
	}
}



/* 从独立 Worker 任务继续填充 TCP 队列。 */
static void testSshClientRuntimeTcpAgainStep(
	xnetworker* pWorker,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	uint32 i;

	(void)pWorker;
	for ( i = 0u; i < 16u; ++i ) {
		testSshClientRuntimeTcpAgain(pRuntime, &pRuntime->Client);
		if ( xrtAtomic32Load(
			&pRuntime->TcpAgainObserved,
			XMEMORY_ACQUIRE
		) != 0u ) {
			break;
		}
	}
	testRequire(i < 16u, "ssh client runtime did not reach TCP AGAIN");
}



/* 认证完成后先验证 TCP AGAIN，排空回调再继续应用协议流程。 */
static void testSshClientRuntimeReady(xsshclient* pClient, ptr pData)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	testRequire(pClient->ReadyTimer == 0u,
		"ssh client runtime ready timer remained armed");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->Ready,
		1u,
		XMEMORY_RELEASE
	);
	pRuntime->ImmediateReadyFuture = xrtSshClientWaitAsync(
		pClient,
		XSSH_CLIENT_WAIT_READY
	);
	testRequire((pRuntime->ImmediateReadyFuture != NULL) &&
		(xrtFutureState(pRuntime->ImmediateReadyFuture) == XFUTURE_RESOLVED),
		"ssh client runtime immediate Ready Future failed");
	pRuntime->CloseFuture = xrtSshClientWaitAsync(
		pClient,
		XSSH_CLIENT_WAIT_CLOSE
	);
	testRequire(pRuntime->CloseFuture != NULL,
		"ssh client runtime close Future failed");
	if ( pRuntime->Backpressure ) {
		testRequire(xrtNetEnginePost(
			pRuntime->Engine,
			0u,
			testSshClientRuntimeTcpAgainStep,
			pRuntime
		), "ssh client runtime TCP AGAIN probe post failed");
	} else {
		testSshClientRuntimeStartForward(pRuntime, pClient);
	}
}



/* 内部保留包完成重试并真正排空后，继续 forwarding 工作流。 */
static void testSshClientRuntimeDrain(xsshclient* pClient, ptr pData)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	if ( (xrtAtomic32Load(&pRuntime->TcpAgainObserved, XMEMORY_ACQUIRE) !=
		1u) || (xrtAtomic32Load(
			&pRuntime->TcpAgainResumed,
			XMEMORY_ACQUIRE
		) != 0u) || pClient->Stream.WritePaused ) {
		return;
	}
	testRequire((pClient->TerminalError == NULL) &&
		(xrtSshTransportTcpWriteSize(
			&pClient->Stream.Session.Transport
		) == 0u), "ssh client runtime TCP AGAIN did not fully resume");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->TcpAgainResumed,
		1u,
		XMEMORY_RELEASE
	);
	testSshClientRuntimeStartForward(pRuntime, pClient);
}



/* 在同一 Worker 的下一轮显式提交被 Packet 回调保留的 channel open。 */
static void testSshClientRuntimePacketResume(
	xnetworker* pWorker,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	(void)pWorker;
	testRequire(xrtSshClientPacketAccept(&pRuntime->Client) == XSSH_OK,
		"ssh client runtime held packet resume failed");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->PacketResumed,
		1u,
		XMEMORY_RELEASE
	);
}



/* 在清除确定性分配故障后重跑内部 peer-open 默认拒绝事务。 */
static void testSshClientRuntimePacketRetry(
	xnetworker* pWorker,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	xsshclient* pClient = &pRuntime->Client;

	(void)pWorker;
	testRequire(pClient->ReceiveRetry && pClient->ReceiveRetryOpen &&
		(pClient->TerminalError == NULL) &&
		(xrtSshClientPacketRetry(pClient) == XSSH_OK) &&
		!pClient->ReceiveRetry && !pClient->ReceiveRetryOpen &&
		(pClient->TerminalError == NULL),
		"ssh client runtime packet OOM retry failed");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->PacketRetried,
		1u,
		XMEMORY_RELEASE
	);
}



/* 提交前解析两种进程退出消息，其他状态由提交后 Channel 事件推进。 */
static xsshsessionstreamdecision testSshClientRuntimePacket(
	xsshclient* pClient,
	const xsshsessiontcppacket* pPacket,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	const xsshconnectionpacket* pConnection;
	xsshchannelexitsignal Signal;
	xsshtcpipopen Tcpip;
	xsshchannel* pChannel = NULL;
	xsshcode Code;
	uint32 iStatus;

	if ( pPacket->Session.Kind != XSSH_SESSION_PACKET_CONNECTION ) {
		return XSSH_SESSION_STREAM_ACCEPT;
	}
	pConnection = &pPacket->Session.Message.Connection;
	if ( pConnection->Kind == XSSH_CONNECTION_PACKET_CHANNEL_OPEN ) {
		if ( xrtAtomic32Load(
			&pRuntime->PacketRetryInjected,
			XMEMORY_ACQUIRE
		) == 0u ) {
			testRequire(xrtMemDebugFailAfter(0u),
				"ssh client runtime packet OOM injection failed");
			(void)xrtAtomic32FetchAdd(
				&pRuntime->PacketRetryInjected,
				1u,
				XMEMORY_RELEASE
			);
			return XSSH_SESSION_STREAM_ACCEPT;
		}
		memset(&Tcpip, 0, sizeof(Tcpip));
		Code = xrtSshClientForwardedTcpipAccept(
			pClient,
			&pConnection->Message.ChannelOpen,
			&Tcpip,
			&pChannel
		);
		if ( (pRuntime->Phase != TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE) ||
			(Code != XSSH_OK) || (pChannel == NULL) || !testSshBytesEqual(
				Tcpip.Host,
				XRT_BYTES_LITERAL("127.0.0.1")
			) || (Tcpip.Port != 2222u) || !testSshBytesEqual(
				Tcpip.Originator,
				XRT_BYTES_LITERAL("198.51.100.7")
			) || (Tcpip.OriginatorPort != 45000u) ) {
			fprintf(stderr,
				"[FORWARDED EVIDENCE] phase=%d code=%d channel=%p "
				"host-size=%zu port=%u origin-size=%zu origin-port=%u\n",
				(int)pRuntime->Phase,
				(int)Code,
				(void*)pChannel,
				Tcpip.Host.Size,
				(unsigned int)Tcpip.Port,
				Tcpip.Originator.Size,
				(unsigned int)Tcpip.OriginatorPort);
		}
		testRequire((pRuntime->Phase ==
			TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE) && (Code == XSSH_OK) &&
			(pChannel != NULL) && testSshBytesEqual(
				Tcpip.Host,
				XRT_BYTES_LITERAL("127.0.0.1")
			) && (Tcpip.Port == 2222u) && testSshBytesEqual(
				Tcpip.Originator,
				XRT_BYTES_LITERAL("198.51.100.7")
			) && (Tcpip.OriginatorPort == 45000u),
			"ssh client runtime forwarded-tcpip accept mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ForwardedAccepted,
			1u,
			XMEMORY_RELEASE
		);
		testRequire(xrtNetEnginePost(
			pRuntime->Engine,
			0u,
			testSshClientRuntimePacketResume,
			pRuntime
		), "ssh client runtime packet resume post failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->PacketHeld,
			1u,
			XMEMORY_RELEASE
		);
		return XSSH_SESSION_STREAM_HOLD;
	}
	if ( pConnection->Kind != XSSH_CONNECTION_PACKET_CHANNEL_REQUEST ) {
		return XSSH_SESSION_STREAM_ACCEPT;
	}
	if ( xrtSshChannelExitStatusRead(
			&pConnection->Message.ChannelRequest,
			&iStatus
		) == XSSH_OK ) {
		testRequire(iStatus == 0u,
			"ssh client runtime exit status mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ExitReceived,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( xrtSshChannelExitSignalRead(
		&pConnection->Message.ChannelRequest,
		&Signal
	) == XSSH_OK ) {
		testRequire(testSshTextEqual(
			Signal.Signal,
			XRT_STR_LITERAL("TERM")
		) && !Signal.CoreDumped && testSshTextEqual(
			Signal.Message,
			XRT_STR_LITERAL("session ended")
		) && testSshTextEqual(
			Signal.Language,
			XRT_STR_LITERAL("en")
		), "ssh client runtime exit signal mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ExitSignalReceived,
			1u,
			XMEMORY_RELEASE
		);
	}
	return XSSH_SESSION_STREAM_ACCEPT;
}



/* DATA 已提交到动态 channel 缓冲后读取，不借用 transport 明文。 */
static void testSshClientRuntimeData(
	xsshclient* pClient,
	xsshchannel* pChannel,
	xsshchanneliostream Stream,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	unsigned char arrData[16];
	size_t iRead = 0u;
	xbytesview Expected;
	xatomic32* pEvidence;
	size_t iFuture;

	if ( Stream == XSSH_CHANNEL_IO_STDERR ) {
		Expected = XRT_BYTES_LITERAL("warn\n");
		testRequire((pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE) &&
			(xrtSshChannelIoRead(
				&pChannel->Io,
				Stream,
				arrData,
				sizeof(arrData),
				&iRead
			) == XSSH_OK) && (iRead == Expected.Size) &&
			(memcmp(arrData, Expected.Data, Expected.Size) == 0),
			"ssh client runtime STDERR mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->StderrDataReceived,
			1u,
			XMEMORY_RELEASE
		);
		(void)xrtAtomic32FetchAdd(
			&pRuntime->DataReceived,
			1u,
			XMEMORY_RELEASE
		);
		pRuntime->StderrTerminalFuture = xrtSshClientChannelReadAsync(
			pClient,
			pChannel,
			Stream
		);
		testRequire((pRuntime->StderrTerminalFuture != NULL) &&
			(xrtFutureState(pRuntime->StderrTerminalFuture) ==
			 XFUTURE_PENDING),
			"ssh client runtime STDERR Future did not remain pending");
		return;
	}
	if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE ) {
		Expected = XRT_BYTES_LITERAL("forward\n");
		testRequire((Stream == XSSH_CHANNEL_IO_DATA) &&
			(xrtSshChannelIoRead(
				&pChannel->Io,
				Stream,
				arrData,
				sizeof(arrData),
				&iRead
			) == XSSH_OK) && (iRead == Expected.Size) &&
			(memcmp(arrData, Expected.Data, Expected.Size) == 0),
			"ssh client runtime forwarded DATA mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ForwardedDataReceived,
			1u,
			XMEMORY_RELEASE
		);
		(void)xrtAtomic32FetchAdd(
			&pRuntime->DataReceived,
			1u,
			XMEMORY_RELEASE
		);
		pRuntime->ForwardedReadTerminalFuture =
			xrtSshClientChannelReadAsync(pClient, pChannel, Stream);
		testRequire((pRuntime->ForwardedReadTerminalFuture != NULL) &&
			(xrtFutureState(pRuntime->ForwardedReadTerminalFuture) ==
			 XFUTURE_PENDING),
			"ssh client runtime forwarded read Future did not remain pending");
		return;
	}
	if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE ) {
		Expected = XRT_BYTES_LITERAL("ok\n");
		pEvidence = &pRuntime->ExecDataReceived;
		iFuture = 0u;
	} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_SHELL_ACTIVE ) {
		Expected = XRT_BYTES_LITERAL("pty\n");
		pEvidence = &pRuntime->PtyDataReceived;
		iFuture = 1u;
	} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_DIRECT_ACTIVE ) {
		Expected = XRT_BYTES_LITERAL("pong\n");
		pEvidence = &pRuntime->DirectDataReceived;
		iFuture = 2u;
	} else {
		testRequire(false, "ssh client runtime DATA phase mismatch");
		return;
	}
	testRequire((Stream == XSSH_CHANNEL_IO_DATA) &&
		(xrtSshChannelIoRead(
			&pChannel->Io,
			Stream,
			arrData,
			sizeof(arrData),
			&iRead
		) == XSSH_OK) && (iRead == Expected.Size) &&
		(memcmp(arrData, Expected.Data, Expected.Size) == 0),
		"ssh client runtime DATA mismatch");
	(void)xrtAtomic32FetchAdd(pEvidence, 1u, XMEMORY_RELEASE);
	(void)xrtAtomic32FetchAdd(
		&pRuntime->DataReceived,
		1u,
		XMEMORY_RELEASE
	);
	pRuntime->ReadTerminalFutures[iFuture] = xrtSshClientChannelReadAsync(
		pClient,
		pChannel,
		Stream
	);
	testRequire((pRuntime->ReadTerminalFutures[iFuture] != NULL) &&
		(xrtFutureState(pRuntime->ReadTerminalFutures[iFuture]) ==
		 XFUTURE_PENDING),
		"ssh client runtime consumed read Future did not remain pending");
}



/* remote forward 建立后等待入站 channel，关闭后再撤销监听。 */
static void testSshClientRuntimeGlobal(
	xsshclient* pClient,
	const xsshclientglobalnotice* pNotice,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	xsshchannel* pChannel = NULL;

	if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_FORWARD ) {
		testRequire((pNotice->Event ==
			XSSH_CLIENT_GLOBAL_EVENT_REQUEST_SUCCESS) &&
			(pNotice->ReplyToken == UINT64_C(0xf001)),
			"ssh client runtime forward token mismatch");
		pRuntime->Phase = TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE;
	} else {
		testRequire((pRuntime->Phase ==
			TEST_SSH_CLIENT_PHASE_FORWARD_CANCEL) &&
			(pNotice->Event ==
			 XSSH_CLIENT_GLOBAL_EVENT_REQUEST_FAILURE) &&
			(pNotice->ReplyToken == UINT64_C(0xf002)),
			"ssh client runtime forward cancel token mismatch");
		pRuntime->Phase = TEST_SSH_CLIENT_PHASE_EXEC_OPEN;
		testRequire((xrtSshClientSessionOpen(pClient, &pChannel) == XSSH_OK) &&
			(pChannel != NULL), "ssh client runtime session open failed");
		pRuntime->OpenFutures[0] = xrtSshClientChannelWaitAsync(
			pClient,
			pChannel,
			XSSH_CLIENT_CHANNEL_WAIT_OPEN
		);
		testRequire(pRuntime->OpenFutures[0] != NULL,
			"ssh client runtime exec open Future failed");
	}
	(void)xrtAtomic32FetchAdd(
		&pRuntime->GlobalNotified,
		1u,
		XMEMORY_RELEASE
	);
}



/* 用 32 字节硬预算和 16 字节窗口分片推进一轮受背压 direct-tcpip 写入。 */
static void testSshClientRuntimeDirectFlush(
	testsshclientruntime* pRuntime,
	xsshclient* pClient,
	xsshchannel* pChannel
)
{
	size_t iWritable;
	size_t iAppend;
	size_t iRemain;
	xfuture* pFuture;

	if ( pRuntime->WriteFutureCount != 0u ) {
		testRequire(xrtFutureState(pRuntime->WriteFutures[
			pRuntime->WriteFutureCount - 1u
		]) == XFUTURE_RESOLVED,
			"ssh client backpressure Future did not resolve after send");
	}
	iWritable = xrtSshChannelIoWritable(&pChannel->Io);
	iRemain = TEST_SSH_CLIENT_DIRECT_BYTES - pRuntime->DirectQueued;
	iAppend = iRemain < iWritable ? iRemain : iWritable;
	if ( iAppend != 0u ) {
		testRequire(xrtSshChannelIoWrite(
			&pChannel->Io,
			XSSH_CHANNEL_IO_DATA,
			pRuntime->DirectPayload + pRuntime->DirectQueued,
			iAppend
		) == XSSH_OK, "ssh client backpressure queue append failed");
		pRuntime->DirectQueued += iAppend;
	}
	if ( xrtAtomic32Load(
		&pRuntime->BackpressureRejected,
		XMEMORY_RELAXED
	) == 0u ) {
		testRequire((xrtSshChannelIoWritable(&pChannel->Io) == 0u) &&
			(xrtSshChannelIoWrite(
				&pChannel->Io,
				XSSH_CHANNEL_IO_DATA,
				"x",
				1u
			) == XSSH_ERROR_SPACE),
			"ssh client send queue did not enforce its hard limit");
		xrtClearError();
		(void)xrtAtomic32FetchAdd(
			&pRuntime->BackpressureRejected,
			1u,
			XMEMORY_RELAXED
		);
	}
	if ( (xrtSshChannelIoWritable(&pChannel->Io) == 0u) &&
		(pRuntime->WriteFutureCount < TEST_SSH_CLIENT_WRITE_WAITERS) ) {
		pFuture = xrtSshClientChannelWaitAsync(
			pClient,
			pChannel,
			XSSH_CLIENT_CHANNEL_WAIT_WRITE
		);
		testRequire((pFuture != NULL) &&
			(xrtFutureState(pFuture) == XFUTURE_PENDING),
			"ssh client backpressure Future was not pending at hard limit");
		pRuntime->WriteFutures[pRuntime->WriteFutureCount++] = pFuture;
	}
	testRequire((xrtSshChannelIoQueued(
		&pChannel->Io,
		XSSH_CHANNEL_IO_DATA
	) != 0u) && (xrtSshClientChannelFlush(
		pClient,
		pChannel,
		XSSH_CHANNEL_IO_DATA
	) == XSSH_OK), "ssh client backpressure flush failed");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->DirectFlushes,
		1u,
		XMEMORY_RELAXED
	);
}



/* 提交后事件驱动 exec、PTY shell、direct-tcpip 和双向 close。 */
static void testSshClientRuntimeChannel(
	xsshclient* pClient,
	const xsshclientchannelnotice* pNotice,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	xsshchannel* pChannel = NULL;
	uint32 iLocal;
	size_t iFuture;

	if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_OPENED ) {
		if ( pNotice->Incoming ) {
			testRequire(pRuntime->Phase ==
				TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE,
				"ssh client runtime incoming channel phase mismatch");
			pRuntime->ForwardedOpenFuture = xrtSshClientChannelWaitAsync(
				pClient,
				pNotice->Channel,
				XSSH_CLIENT_CHANNEL_WAIT_OPEN
			);
			pRuntime->ForwardedEofFuture = xrtSshClientChannelWaitAsync(
				pClient,
				pNotice->Channel,
				XSSH_CLIENT_CHANNEL_WAIT_EOF
			);
			testRequire((pRuntime->ForwardedOpenFuture != NULL) &&
				(xrtFutureState(pRuntime->ForwardedOpenFuture) ==
				 XFUTURE_RESOLVED) &&
				(pRuntime->ForwardedEofFuture != NULL) &&
				(xrtFutureState(pRuntime->ForwardedEofFuture) ==
				 XFUTURE_PENDING),
				"ssh client runtime incoming channel Future mismatch");
			(void)xrtAtomic32FetchAdd(
				&pRuntime->ChannelOpened,
				1u,
				XMEMORY_RELEASE
			);
			return;
		}
		testRequire(!pNotice->Incoming,
			"ssh client runtime received unexpected incoming channel");
		iFuture = pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_OPEN ? 0u :
			pRuntime->Phase == TEST_SSH_CLIENT_PHASE_PTY_OPEN ? 1u : 2u;
		if ( (pRuntime->Phase == TEST_SSH_CLIENT_PHASE_DIRECT_OPEN) &&
			pRuntime->RejectedChannelPending ) {
			testRequire((xrtFutureState(pRuntime->RejectedOpenFuture) ==
				XFUTURE_FAILED) && xrtSshChannelsDiscard(
					xrtSshClientChannels(pClient),
					pRuntime->RejectedChannelLocal
				), "ssh client rejected channel cleanup failed");
			pRuntime->RejectedChannelPending = false;
		}
		pRuntime->ImmediateOpenFutures[iFuture] =
			xrtSshClientChannelWaitAsync(
				pClient,
				pNotice->Channel,
				XSSH_CLIENT_CHANNEL_WAIT_OPEN
			);
		testRequire((pRuntime->ImmediateOpenFutures[iFuture] != NULL) &&
			(xrtFutureState(pRuntime->ImmediateOpenFutures[iFuture]) ==
			 XFUTURE_RESOLVED),
			"ssh client runtime immediate Open Future failed");
		if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_OPEN ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_EXEC_ENV;
			testRequire(xrtSshClientSessionEnv(
				pClient,
				pNotice->Channel,
				XRT_BYTES_LITERAL("REJECT_ME"),
				XRT_BYTES_LITERAL("1"),
				true,
				UINT64_C(0xd001)
			) == XSSH_OK, "ssh client runtime rejected env send failed");
			pRuntime->RejectedReplyFuture = xrtSshClientChannelReplyAsync(
				pClient,
				pNotice->Channel,
				UINT64_C(0xd001)
			);
			testRequire(pRuntime->RejectedReplyFuture != NULL,
				"ssh client runtime rejected env Future failed");
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_PTY_OPEN ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_PTY_REQUEST;
			testRequire(xrtSshClientSessionPty(
				pClient,
				pNotice->Channel,
				XRT_BYTES_LITERAL("xterm-256color"),
				80u,
				24u,
				0u,
				0u,
				XRT_BYTES_LITERAL("\0"),
				true,
				UINT64_C(0x7001)
			) == XSSH_OK, "ssh client runtime PTY send failed");
			pRuntime->ReplyFutures[1] = xrtSshClientChannelReplyAsync(
				pClient,
				pNotice->Channel,
				UINT64_C(0x7001)
			);
			testRequire(pRuntime->ReplyFutures[1] != NULL,
				"ssh client runtime PTY reply Future failed");
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_DIRECT_OPEN ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_DIRECT_ACTIVE;
			testSshClientRuntimeDirectFlush(
				pRuntime,
				pClient,
				pNotice->Channel
			);
		} else {
			testRequire(false, "ssh client runtime open phase mismatch");
		}
		pRuntime->EofFutures[iFuture] = xrtSshClientChannelWaitAsync(
			pClient,
			pNotice->Channel,
			XSSH_CLIENT_CHANNEL_WAIT_EOF
		);
		testRequire(pRuntime->EofFutures[iFuture] != NULL,
			"ssh client runtime EOF Future failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ChannelOpened,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_OPEN_FAILED ) {
		testRequire((pRuntime->Phase ==
			TEST_SSH_CLIENT_PHASE_DIRECT_REJECT_OPEN) &&
			!pNotice->Incoming &&
			(pNotice->Reason == XSSH_CHANNEL_OPEN_CONNECT_FAILED),
			"ssh client runtime rejected open notice mismatch");
		pRuntime->RejectedChannelLocal = pNotice->Channel->Core.Local;
		pRuntime->RejectedChannelPending = true;
		pRuntime->Phase = TEST_SSH_CLIENT_PHASE_DIRECT_OPEN;
		testRequire((xrtSshClientDirectTcpipOpen(
			pClient,
			XRT_BYTES_LITERAL("service.internal"),
			8080u,
			XRT_BYTES_LITERAL("127.0.0.1"),
			50000u,
			&pChannel
		) == XSSH_OK) && (pChannel != NULL),
			"ssh client runtime direct channel failed after rejection");
		pRuntime->OpenFutures[2] = xrtSshClientChannelWaitAsync(
			pClient,
			pChannel,
			XSSH_CLIENT_CHANNEL_WAIT_OPEN
		);
		testRequire(pRuntime->OpenFutures[2] != NULL,
			"ssh client runtime direct open Future failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ChannelOpenFailed,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( pNotice->Event ==
		XSSH_CLIENT_CHANNEL_EVENT_REQUEST_SUCCESS ) {
		testRequire(pNotice->HasReplyToken,
			"ssh client runtime reply token missing");
		if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE ) {
			testRequire(pNotice->ReplyToken == UINT64_C(0xe001),
				"ssh client runtime exec token mismatch");
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_PTY_REQUEST ) {
			testRequire(pNotice->ReplyToken == UINT64_C(0x7001),
				"ssh client runtime PTY token mismatch");
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_SHELL_ACTIVE;
			testRequire(xrtSshClientSessionShell(
				pClient,
				pNotice->Channel,
				true,
				UINT64_C(0x7002)
			) == XSSH_OK, "ssh client runtime shell send failed");
			pRuntime->ReplyFutures[2] = xrtSshClientChannelReplyAsync(
				pClient,
				pNotice->Channel,
				UINT64_C(0x7002)
			);
			testRequire(pRuntime->ReplyFutures[2] != NULL,
				"ssh client runtime shell reply Future failed");
			(void)xrtAtomic32FetchAdd(
				&pRuntime->PtySucceeded,
				1u,
				XMEMORY_RELEASE
			);
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_SHELL_ACTIVE ) {
			testRequire((pNotice->ReplyToken == UINT64_C(0x7002)) &&
				(xrtSshClientSessionResize(
					pClient,
					pNotice->Channel,
					120u,
					40u,
					0u,
					0u
				) == XSSH_OK), "ssh client runtime shell/resize failed");
			(void)xrtAtomic32FetchAdd(
				&pRuntime->ShellSucceeded,
				1u,
				XMEMORY_RELEASE
			);
		} else {
			testRequire(false, "ssh client runtime reply phase mismatch");
		}
		(void)xrtAtomic32FetchAdd(
			&pRuntime->RequestSucceeded,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( pNotice->Event ==
		XSSH_CLIENT_CHANNEL_EVENT_REQUEST_FAILURE ) {
		testRequire((pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_ENV) &&
			pNotice->HasReplyToken &&
			(pNotice->ReplyToken == UINT64_C(0xd001)),
			"ssh client runtime rejected env token mismatch");
		pRuntime->Phase = TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE;
		testRequire(xrtSshClientSessionExec(
			pClient,
			pNotice->Channel,
			XRT_BYTES_LITERAL("printf ok"),
			true,
			UINT64_C(0xe001)
		) == XSSH_OK, "ssh client runtime exec send failed");
		pRuntime->ReplyFutures[0] = xrtSshClientChannelReplyAsync(
			pClient,
			pNotice->Channel,
			UINT64_C(0xe001)
		);
		testRequire(pRuntime->ReplyFutures[0] != NULL,
			"ssh client runtime exec reply Future failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->RequestFailed,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_WRITABLE ) {
		testRequire(pRuntime->Phase == TEST_SSH_CLIENT_PHASE_DIRECT_ACTIVE,
			"ssh client runtime writable event phase mismatch");
		testSshClientRuntimeDirectFlush(
			pRuntime,
			pClient,
			pNotice->Channel
		);
	} else if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_EOF ) {
		testRequire(xrtSshClientChannelClose(
			pClient,
			pNotice->Channel
		) == XSSH_OK, "ssh client runtime close response failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->EofReceived,
			1u,
			XMEMORY_RELEASE
		);
	} else if ( pNotice->Event == XSSH_CLIENT_CHANNEL_EVENT_CLOSED ) {
		iLocal = pNotice->Channel->Core.Local;
		(void)xrtAtomic32FetchAdd(
			&pRuntime->ChannelClosed,
			1u,
			XMEMORY_RELEASE
		);
		testRequire(xrtSshChannelsDiscard(
			xrtSshClientChannels(pClient),
			iLocal
		), "ssh client runtime channel discard failed");
		if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_FORWARDED_ACTIVE ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_FORWARD_CANCEL;
			testRequire(xrtSshClientTcpipForwardCancel(
				pClient,
				XRT_BYTES_LITERAL("127.0.0.1"),
				2222u,
				UINT64_C(0xf002)
			) == XSSH_OK, "ssh client runtime forward cancel failed");
			pRuntime->GlobalFutures[1] = xrtSshClientGlobalReplyAsync(
				pClient,
				UINT64_C(0xf002)
			);
			testRequire(pRuntime->GlobalFutures[1] != NULL,
				"ssh client runtime forward cancel Future failed");
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_EXEC_ACTIVE ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_PTY_OPEN;
			testRequire((xrtSshClientSessionOpen(pClient, &pChannel) == XSSH_OK) &&
				(pChannel != NULL), "ssh client runtime PTY channel failed");
			pRuntime->OpenFutures[1] = xrtSshClientChannelWaitAsync(
				pClient,
				pChannel,
				XSSH_CLIENT_CHANNEL_WAIT_OPEN
			);
			testRequire(pRuntime->OpenFutures[1] != NULL,
				"ssh client runtime PTY open Future failed");
		} else if ( pRuntime->Phase == TEST_SSH_CLIENT_PHASE_SHELL_ACTIVE ) {
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_DIRECT_REJECT_OPEN;
			testRequire((xrtSshClientDirectTcpipOpen(
				pClient,
				XRT_BYTES_LITERAL("reject.internal"),
				8081u,
				XRT_BYTES_LITERAL("127.0.0.1"),
				50000u,
				&pChannel
			) == XSSH_OK) && (pChannel != NULL),
				"ssh client runtime rejected direct channel failed");
			pRuntime->RejectedOpenFuture = xrtSshClientChannelWaitAsync(
				pClient,
				pChannel,
				XSSH_CLIENT_CHANNEL_WAIT_OPEN
			);
			testRequire(pRuntime->RejectedOpenFuture != NULL,
				"ssh client runtime rejected direct open Future failed");
		} else {
			testRequire(pRuntime->Phase == TEST_SSH_CLIENT_PHASE_DIRECT_ACTIVE,
				"ssh client runtime close phase mismatch");
			pRuntime->Phase = TEST_SSH_CLIENT_PHASE_DONE;
			testRequire(xrtNetStreamClose(
				xrtSshSessionStreamTcp(xrtSshClientStream(pClient))
			), "ssh client runtime TCP close failed");
		}
	}
}



/* 客户端任何协议或网络错误都使回环失败。 */
static void testSshClientRuntimeError(
	xsshclient* pClient,
	xsshcode Code,
	const xerror* pError,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	if ( (Code == XSSH_ERROR_SPACE) && pClient->ReceiveRetry &&
		pClient->ReceiveRetryOpen ) {
		testRequire((pRuntime->Mode == TEST_SSH_CLIENT_MODE_SUCCESS) &&
			(pClient->TerminalError == NULL) && xrtMemDebugFailTriggered() &&
			(pError != NULL) && (xrtErrorKind(pError) == XERR_MEMORY),
			"ssh client runtime packet retry error mismatch");
		xrtMemDebugFailClear();
		xrtClearError();
		testRequire(xrtNetEnginePost(
			pRuntime->Engine,
			0u,
			testSshClientRuntimePacketRetry,
			pRuntime
		), "ssh client runtime packet retry post failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->PacketRetryErrors,
			1u,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( pRuntime->Mode == TEST_SSH_CLIENT_MODE_REJECT_HOST ) {
		testRequire((Code == XSSH_ERROR_AUTHENTICATION) &&
			(pError != NULL) && (strcmp(
				xrtErrorDomain(pError),
				"xrt.ssh.client"
			) == 0), "ssh client runtime host rejection error mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->HostRejectErrors,
			1u,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( pRuntime->Mode == TEST_SSH_CLIENT_MODE_DISCONNECT ) {
		fprintf(stderr,
			"[CLIENT DISCONNECT ERROR] code=%d domain=%s operation=%s "
			"message=%s\n",
			(int)Code,
			pError != NULL ? xrtErrorDomain(pError) : "",
			pError != NULL ? xrtErrorOperation(pError) : "",
			pError != NULL ? xrtErrorMessage(pError) : "");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->DisconnectErrors,
			1u,
			XMEMORY_RELEASE
		);
		return;
	}
	if ( pRuntime->Mode == TEST_SSH_CLIENT_MODE_TIMEOUT ) {
		testRequire((Code == XSSH_ERROR_TIMEOUT) &&
			(pClient->ReadyTimer == 0u) &&
			(pError != NULL) &&
			(xrtErrorKind(pError) == XERR_TIMEOUT) && (strcmp(
				xrtErrorDomain(pError),
				"xrt.ssh.client"
			) == 0), "ssh client runtime ready timeout error mismatch");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->TimeoutErrors,
			1u,
			XMEMORY_RELEASE
		);
		return;
	}
	fprintf(stderr, "[CLIENT ERROR] code=%d domain=%s operation=%s message=%s\n",
		(int)Code,
		pError != NULL ? xrtErrorDomain(pError) : "",
		pError != NULL ? xrtErrorOperation(pError) : "",
		pError != NULL ? xrtErrorMessage(pError) : "");
	(void)xrtAtomic32FetchAdd(
		&pRuntime->Errors,
		1u,
		XMEMORY_RELEASE
	);
}



/* 客户端关闭终态由主线程统一清理。 */
static void testSshClientRuntimeClose(
	xsshclient* pClient,
	xnetresult Result,
	const xerror* pError,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;

	(void)pClient;
	pRuntime->ClientCloseResult = Result;
	pRuntime->ClientCloseHasError = pError != NULL;
	(void)xrtAtomic32FetchAdd(
		&pRuntime->ClientClosed,
		1u,
		XMEMORY_RELEASE
	);
}



/* TCP Dial 成功引用由主线程保留，SSH Ready 仍由独立协议事件证明。 */
static void testSshClientRuntimeDialDone(
	xnetdial* pDial,
	xnetresult Result,
	xnetstream* pStream,
	const xerror* pError,
	ptr pData
)
{
	testsshclientruntime* pRuntime = (testsshclientruntime*)pData;
	size_t i;

	(void)pDial;
	if ( (Result == XNET_RESULT_OK) && (pStream != NULL) ) {
		pRuntime->ClientStream = pStream;
		testSshClientRuntimeFutureOom(pRuntime, &pRuntime->Client);
		pRuntime->ReadyFuture = xrtSshClientWaitAsync(
			&pRuntime->Client,
			XSSH_CLIENT_WAIT_READY
		);
		pRuntime->CancelFuture = xrtSshClientWaitAsync(
			&pRuntime->Client,
			XSSH_CLIENT_WAIT_CLOSE
		);
		for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
			pRuntime->CloseStressFutures[i] = xrtSshClientWaitAsync(
				&pRuntime->Client,
				XSSH_CLIENT_WAIT_CLOSE
			);
			testRequire(pRuntime->CloseStressFutures[i] != NULL,
				"ssh client runtime close stress Future failed");
		}
		testRequire((pRuntime->ReadyFuture != NULL) &&
			(pRuntime->CancelFuture != NULL),
			"ssh client runtime initial Futures failed");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->FuturesReady,
			1u,
			XMEMORY_RELEASE
		);
		testSshClientRuntimeWait(
			&pRuntime->CancellationDone,
			1u,
			"ssh client runtime cancellation synchronization timeout"
		);
	} else {
		fprintf(stderr,
			"[CLIENT DIAL ERROR] result=%d domain=%s operation=%s message=%s\n",
			(int)Result,
			pError != NULL ? xrtErrorDomain(pError) : "",
			pError != NULL ? xrtErrorOperation(pError) : "",
			pError != NULL ? xrtErrorMessage(pError) : "");
		(void)xrtAtomic32FetchAdd(
			&pRuntime->Errors,
			1u,
			XMEMORY_RELEASE
		);
	}
	(void)xrtAtomic32FetchAdd(
		&pRuntime->DialDone,
		1u,
		XMEMORY_RELEASE
	);
}



/* 构建固定测试身份，但对动态 exchange hash 执行真实 Ed25519 签名。 */
static void testSshClientRuntimeHostInit(testsshserver* pServer)
{
	unsigned char arrPublic[32];
	xsshwriter Writer;
	size_t i;

	for ( i = 0u; i < sizeof(pServer->Seed); ++i ) {
		pServer->Seed[i] = (unsigned char)(0x40u + i);
	}
	testRequire(xrtEd25519Public(pServer->Seed, arrPublic) &&
		xrtSshWriterInit(
		&Writer,
		pServer->HostKey,
		sizeof(pServer->HostKey)
	) && (xrtSshEd25519PublicKeyWrite(
		&Writer,
		(xbytesview){ arrPublic, sizeof(arrPublic) }
	) == XSSH_OK), "ssh client runtime host key build failed");
	pServer->HostKeySize = Writer.Size;
}



/* 验证 callback、Future 与取消观察同用一份提交后状态。 */
static void testSshClientRuntimeFutures(testsshclientruntime* pRuntime)
{
	size_t i;

	testRequire((xrtFutureWaitFor(
		pRuntime->ReadyFuture,
		5000000u
	) == XWAIT_OK) &&
		(xrtFutureWaitFor(pRuntime->CloseFuture, 5000000u) == XWAIT_OK) &&
		(xrtFutureState(pRuntime->ReadyFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pRuntime->ImmediateReadyFuture) ==
		 XFUTURE_RESOLVED) &&
		(xrtFutureState(pRuntime->CloseFuture) == XFUTURE_RESOLVED) &&
		(xrtFutureState(pRuntime->CancelFuture) == XFUTURE_CANCELLED),
		"ssh client runtime client Future states mismatch");
	for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
		testRequire((xrtFutureWaitFor(
			pRuntime->CloseStressFutures[i],
			5000000u
		) == XWAIT_OK) &&
			(xrtFutureState(pRuntime->CloseStressFutures[i]) ==
			 ((i & 1u) != 0u ? XFUTURE_CANCELLED : XFUTURE_RESOLVED)),
			"ssh client runtime close stress Future mismatch");
	}
	for ( i = 0u; i < 2u; ++i ) {
		testRequire((xrtFutureWaitFor(
			pRuntime->GlobalFutures[i],
			5000000u
		) == XWAIT_OK), "ssh client runtime global Future timeout");
	}
	testRequire((xrtFutureState(pRuntime->GlobalFutures[0]) ==
		XFUTURE_RESOLVED) &&
		(xrtFutureState(pRuntime->GlobalFutures[1]) == XFUTURE_FAILED) &&
		(strcmp(
			xrtErrorDomain(xrtFutureError(pRuntime->GlobalFutures[1])),
			"xrt.ssh.global.request"
		) == 0), "ssh client runtime global Future results mismatch");
	testRequire((xrtFutureWaitFor(
		pRuntime->ForwardedOpenFuture,
		5000000u
	) == XWAIT_OK) && (xrtFutureState(pRuntime->ForwardedOpenFuture) ==
		XFUTURE_RESOLVED) && (xrtFutureWaitFor(
			pRuntime->ForwardedEofFuture,
			5000000u
		) == XWAIT_OK) && (xrtFutureState(pRuntime->ForwardedEofFuture) ==
		XFUTURE_RESOLVED) && (xrtFutureWaitFor(
			pRuntime->ForwardedReadTerminalFuture,
			5000000u
		) == XWAIT_OK) && (xrtFutureState(
			pRuntime->ForwardedReadTerminalFuture
		) == XFUTURE_CLOSED),
		"ssh client runtime forwarded channel Future mismatch");
	testRequire((xrtFutureWaitFor(
		pRuntime->RejectedReplyFuture,
		5000000u
	) == XWAIT_OK) &&
		(xrtFutureState(pRuntime->RejectedReplyFuture) == XFUTURE_FAILED) &&
		(strcmp(
			xrtErrorDomain(xrtFutureError(pRuntime->RejectedReplyFuture)),
			"xrt.ssh.channel.request"
		) == 0), "ssh client runtime rejected reply Future mismatch");
	testRequire((xrtFutureWaitFor(
		pRuntime->RejectedOpenFuture,
		5000000u
	) == XWAIT_OK) &&
		(xrtFutureState(pRuntime->RejectedOpenFuture) == XFUTURE_FAILED) &&
		(strcmp(
			xrtErrorDomain(xrtFutureError(pRuntime->RejectedOpenFuture)),
			"xrt.ssh.channel.open"
		) == 0), "ssh client runtime rejected open Future mismatch");
	for ( i = 0u; i < 3u; ++i ) {
		testRequire(xrtFutureWaitFor(
			pRuntime->OpenFutures[i],
			5000000u
		) == XWAIT_OK, "ssh client runtime channel Open Future timeout");
		testRequire(xrtFutureWaitFor(
				pRuntime->ReplyFutures[i],
				5000000u
			) == XWAIT_OK, "ssh client runtime channel Reply Future timeout");
		testRequire(xrtFutureWaitFor(
				pRuntime->EofFutures[i],
				5000000u
			) == XWAIT_OK, "ssh client runtime channel EOF Future timeout");
		if ( xrtFutureState(pRuntime->OpenFutures[i]) != XFUTURE_RESOLVED ) {
			fprintf(stderr,
				"[FUTURE EVIDENCE] channel=%zu open=%d immediate=%d "
				"reply=%d eof=%d read=%d\n",
				i,
				(int)xrtFutureState(pRuntime->OpenFutures[i]),
				(int)xrtFutureState(pRuntime->ImmediateOpenFutures[i]),
				(int)xrtFutureState(pRuntime->ReplyFutures[i]),
				(int)xrtFutureState(pRuntime->EofFutures[i]),
				(int)xrtFutureState(pRuntime->ReadTerminalFutures[i]));
		}
		testRequire(xrtFutureState(pRuntime->OpenFutures[i]) ==
			XFUTURE_RESOLVED,
			"ssh client runtime channel Open Future state mismatch");
		testRequire(
			(xrtFutureState(pRuntime->ImmediateOpenFutures[i]) ==
			 XFUTURE_RESOLVED),
			"ssh client runtime immediate Open Future state mismatch");
		testRequire(xrtFutureState(pRuntime->ReplyFutures[i]) ==
			XFUTURE_RESOLVED,
			"ssh client runtime channel Reply Future state mismatch");
		testRequire(xrtFutureState(pRuntime->EofFutures[i]) ==
			XFUTURE_RESOLVED,
			"ssh client runtime channel EOF Future state mismatch");
		testRequire(xrtFutureWaitFor(
			pRuntime->ReadTerminalFutures[i],
			5000000u
		) == XWAIT_OK, "ssh client runtime read terminal Future timeout");
		testRequire(xrtFutureState(pRuntime->ReadTerminalFutures[i]) ==
			XFUTURE_CLOSED,
			"ssh client runtime read terminal Future state mismatch");
	}
	testRequire((xrtFutureWaitFor(
		pRuntime->StderrTerminalFuture,
		5000000u
	) == XWAIT_OK) &&
		(xrtFutureState(pRuntime->StderrTerminalFuture) == XFUTURE_CLOSED),
		"ssh client runtime STDERR terminal Future mismatch");
	testRequire(pRuntime->WriteFutureCount ==
		TEST_SSH_CLIENT_WRITE_WAITERS,
		"ssh client runtime write Future count mismatch");
	for ( i = 0u; i < pRuntime->WriteFutureCount; ++i ) {
		testRequire((xrtFutureWaitFor(
			pRuntime->WriteFutures[i],
			5000000u
		) == XWAIT_OK) &&
			(xrtFutureState(pRuntime->WriteFutures[i]) == XFUTURE_RESOLVED),
			"ssh client runtime write Future state mismatch");
	}
}



/* 释放测试保留的全部 Future 消费端引用。 */
static void testSshClientRuntimeFuturesDestroy(
	testsshclientruntime* pRuntime
)
{
	size_t i;

	xrtFutureDestroy(pRuntime->ReadyFuture);
	xrtFutureDestroy(pRuntime->ImmediateReadyFuture);
	xrtFutureDestroy(pRuntime->CloseFuture);
	xrtFutureDestroy(pRuntime->CancelFuture);
	for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
		xrtFutureDestroy(pRuntime->CloseStressFutures[i]);
	}
	for ( i = 0u; i < 2u; ++i ) {
		xrtFutureDestroy(pRuntime->GlobalFutures[i]);
	}
	xrtFutureDestroy(pRuntime->RejectedOpenFuture);
	xrtFutureDestroy(pRuntime->RejectedReplyFuture);
	xrtFutureDestroy(pRuntime->ForwardedOpenFuture);
	xrtFutureDestroy(pRuntime->ForwardedEofFuture);
	xrtFutureDestroy(pRuntime->ForwardedReadTerminalFuture);
	for ( i = 0u; i < 3u; ++i ) {
		xrtFutureDestroy(pRuntime->OpenFutures[i]);
		xrtFutureDestroy(pRuntime->ImmediateOpenFutures[i]);
		xrtFutureDestroy(pRuntime->ReplyFutures[i]);
		xrtFutureDestroy(pRuntime->EofFutures[i]);
		xrtFutureDestroy(pRuntime->ReadTerminalFutures[i]);
	}
	xrtFutureDestroy(pRuntime->StderrTerminalFuture);
	for ( i = 0u; i < pRuntime->WriteFutureCount; ++i ) {
		xrtFutureDestroy(pRuntime->WriteFutures[i]);
	}
}



/* 运行一轮真实 TCP 客户端，并验证指定后端上的成功或故障终态。 */
static void testSshClientRuntimeRun(
	testsshclientmode Mode,
	xnetportbackend Backend,
	bool bBackpressure
)
{
	static const xsshsessionstreamevents ServerEvents = {
		testSshClientRuntimeServerOpen,
		testSshClientRuntimeServerAction,
		testSshClientRuntimeServerIdentification,
		testSshClientRuntimeServerPacket,
		NULL,
		testSshClientRuntimeServerError,
		testSshClientRuntimeServerEnd,
		NULL,
		NULL,
		NULL,
		testSshClientRuntimeServerClose
	};
	static const xnetlistenerevents ListenerEvents = {
		testSshClientRuntimeAccept,
		NULL,
		testSshClientRuntimeListenerClose
	};
	testsshclientruntime Runtime;
	xsshsessiontcpconfig ServerConfig;
	xsshclientconfig ClientConfig;
	xsshclientevents ClientEvents;
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xnetdialconfig DialConfig;
	xnetaddr Address;
	xmemdebugsnapshot Memory;
	size_t i;

	testRequire(xrtMemDebugEnable(true),
		"ssh client runtime memory debug enable failed");
	memset(&Runtime, 0, sizeof(Runtime));
	Runtime.Mode = Mode;
	Runtime.Backend = Backend;
	Runtime.Backpressure = bBackpressure;
	for ( i = 0u; i < sizeof(Runtime.DirectPayload); ++i ) {
		Runtime.DirectPayload[i] = (unsigned char)(11u + (i * 37u));
	}
	Runtime.Server.Runtime = &Runtime;
	testSshClientRuntimeHostInit(&Runtime.Server);
	testRequire(xrtSshReplyQueueInit(
		&Runtime.Server.GlobalReplies,
		NULL,
		0u
	) && xrtSshSessionTcpConfigInit(
		&ServerConfig,
		XSSH_ROLE_SERVER
	), "ssh client runtime server config failed");
	ServerConfig.ChannelResolve = xrtSshChannelsResolve;
	ServerConfig.ChannelUserData = &Runtime.Server.Channels;
	ServerConfig.GlobalReplies = &Runtime.Server.GlobalReplies;
	testRequire(xrtSshSessionStreamInit(
		&Runtime.Server.Session,
		&ServerConfig,
		&ServerEvents,
		&Runtime.Server
	), "ssh client runtime server init failed");

	memset(&ClientEvents, 0, sizeof(ClientEvents));
	ClientEvents.Ready = testSshClientRuntimeReady;
	ClientEvents.Packet = testSshClientRuntimePacket;
	ClientEvents.Data = testSshClientRuntimeData;
	ClientEvents.Error = testSshClientRuntimeError;
	ClientEvents.Drain = testSshClientRuntimeDrain;
	ClientEvents.Close = testSshClientRuntimeClose;
	ClientEvents.Channel = testSshClientRuntimeChannel;
	ClientEvents.Global = testSshClientRuntimeGlobal;
	testRequire(xrtSshClientConfigInit(&ClientConfig),
		"ssh client runtime client config failed");
	ClientConfig.Core.Version = XRT_STR_LITERAL(
		"SSH-2.0-xssh_runtime_client"
	);
	ClientConfig.Core.User = XRT_STR_LITERAL("alice");
	ClientConfig.Core.HostKey = testSshClientRuntimeHostKey;
	ClientConfig.Core.HostKeyData = &Runtime;
	ClientConfig.Core.Authenticate = testSshClientRuntimeAuth;
	ClientConfig.Core.AuthenticateData = &Runtime;
	ClientConfig.Channels.Io.SendLimit = TEST_SSH_CLIENT_DIRECT_QUEUE;
	if ( Runtime.Mode == TEST_SSH_CLIENT_MODE_TIMEOUT ) {
		ClientConfig.ReadyTimeout = UINT64_C(50000);
	}
	testRequire(xrtSshClientInit(
		&Runtime.Client,
		&ClientConfig,
		&ClientEvents,
		&Runtime
	), "ssh client runtime client init failed");

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = Runtime.Backend;
	EngineConfig.Workers = 1u;
	Runtime.Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire((Runtime.Engine != NULL) &&
		xrtNetEngineStart(Runtime.Engine),
		"ssh client runtime engine start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	Runtime.Resolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(Runtime.Resolver != NULL,
		"ssh client runtime resolver create failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0u
	), "ssh client runtime listener address failed");
	ListenConfig.Stream.ReadSize = Runtime.Backpressure ? 4096u : 17u;
	ListenConfig.Stream.ReadLimit = 1048576u;
	Runtime.Listener = xrtNetListen(
		Runtime.Engine,
		&ListenConfig,
		&ListenerEvents,
		NULL,
		&Runtime
	);
	testRequire((Runtime.Listener != NULL) && xrtNetListenerLocal(
		Runtime.Listener,
		&Address
	), "ssh client runtime listener failed");
	xrtNetDialConfigInit(&DialConfig);
	DialConfig.Stream.ReadSize = 19u;
	DialConfig.Stream.ReadLimit = 1048576u;
	if ( Runtime.Backpressure ) {
		DialConfig.Stream.WriteHighWater = 32768u;
		DialConfig.Stream.WriteLowWater = 16384u;
		DialConfig.Stream.WriteLimit = 49152u;
	}
	DialConfig.Timeout = 5000000u;
	Runtime.ClientDial = xrtSshClientDial(
		&Runtime.Client,
		Runtime.Engine,
		Runtime.Resolver,
		"127.0.0.1",
		Address.Port,
		&DialConfig,
		testSshClientRuntimeDialDone,
		&Runtime
	);
	testRequire(Runtime.ClientDial != NULL,
		"ssh client runtime dial submit failed");
	testSshClientRuntimeWait(&Runtime.FuturesReady, 1u,
		"ssh client runtime Future setup timeout");
	testRequire(xrtFutureCancel(Runtime.CancelFuture),
		"ssh client runtime cross-thread Future cancel failed");
	for ( i = 1u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; i += 2u ) {
		testRequire(xrtFutureCancel(Runtime.CloseStressFutures[i]),
			"ssh client runtime close stress cancellation failed");
	}
	(void)xrtAtomic32FetchAdd(
		&Runtime.CancellationDone,
		1u,
		XMEMORY_RELEASE
	);
	testSshClientRuntimeWait(&Runtime.DialDone, 1u,
		"ssh client runtime dial timeout");
	testRequire(Runtime.ClientStream != NULL,
		"ssh client runtime dial failed");

	testSshClientRuntimeWait(&Runtime.ClientClosed, 1u,
		"ssh client runtime client close timeout");
	testSshClientRuntimeWait(&Runtime.ServerClosed, 1u,
		"ssh client runtime server close timeout");
	if ( Runtime.Mode == TEST_SSH_CLIENT_MODE_REJECT_HOST ) {
		testRequire((xrtAtomic32Load(
			&Runtime.HostRejectErrors,
			XMEMORY_ACQUIRE
		) == 1u) &&
			(xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE) == 0u) &&
			(xrtFutureWaitFor(Runtime.ReadyFuture, 5000000u) == XWAIT_OK) &&
			(xrtFutureState(Runtime.ReadyFuture) == XFUTURE_FAILED) &&
			(xrtFutureError(Runtime.ReadyFuture) != NULL) &&
			(strcmp(
				xrtErrorDomain(xrtFutureError(Runtime.ReadyFuture)),
				"xrt.ssh.client"
			) == 0) && (xrtErrorCode(
				xrtFutureError(Runtime.ReadyFuture)
			) == XSSH_ERROR_AUTHENTICATION) &&
			(Runtime.ImmediateReadyFuture == NULL) &&
			(Runtime.CloseFuture == NULL) &&
			(xrtFutureState(Runtime.CancelFuture) == XFUTURE_CANCELLED),
			"ssh client runtime host rejection evidence incomplete");
		for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
			testRequire((xrtFutureWaitFor(
				Runtime.CloseStressFutures[i],
				5000000u
			) == XWAIT_OK) && (xrtFutureState(
				Runtime.CloseStressFutures[i]
			) == ((i & 1u) != 0u ? XFUTURE_CANCELLED : XFUTURE_RESOLVED)),
				"ssh client runtime rejected host close Future mismatch");
		}
		goto Cleanup;
	}
	if ( Runtime.Mode == TEST_SSH_CLIENT_MODE_TIMEOUT ) {
		testRequire((xrtAtomic32Load(
			&Runtime.TimeoutErrors,
			XMEMORY_ACQUIRE
		) == 1u) &&
			(xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.ServerStalls, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE) == 0u) &&
			(xrtFutureWaitFor(Runtime.ReadyFuture, 5000000u) == XWAIT_OK) &&
			(xrtFutureState(Runtime.ReadyFuture) == XFUTURE_FAILED) &&
			(xrtFutureError(Runtime.ReadyFuture) != NULL) &&
			(xrtErrorKind(xrtFutureError(Runtime.ReadyFuture)) ==
			 XERR_TIMEOUT) && (strcmp(
				xrtErrorDomain(xrtFutureError(Runtime.ReadyFuture)),
				"xrt.ssh.client"
			) == 0) && (xrtErrorCode(
				xrtFutureError(Runtime.ReadyFuture)
			) == XSSH_ERROR_TIMEOUT) &&
			(Runtime.ImmediateReadyFuture == NULL) &&
			(Runtime.CloseFuture == NULL) &&
			(xrtFutureState(Runtime.CancelFuture) == XFUTURE_CANCELLED) &&
			(Runtime.ClientCloseResult != XNET_RESULT_OK) &&
			Runtime.ClientCloseHasError,
			"ssh client runtime ready timeout evidence incomplete");
		for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
			testRequire((xrtFutureWaitFor(
				Runtime.CloseStressFutures[i],
				5000000u
			) == XWAIT_OK) && (xrtFutureState(
				Runtime.CloseStressFutures[i]
			) == ((i & 1u) != 0u ? XFUTURE_CANCELLED : XFUTURE_RESOLVED)),
				"ssh client runtime timeout close Future mismatch");
		}
		goto Cleanup;
	}
	if ( Runtime.Mode == TEST_SSH_CLIENT_MODE_DISCONNECT ) {
		testRequire((xrtAtomic32Load(
			&Runtime.DisconnectErrors,
			XMEMORY_ACQUIRE
		) == 0u) &&
			(xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.ServerDisconnects, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE) == 2u) &&
			(xrtAtomic32Load(&Runtime.ServerNone, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(
				&Runtime.ServerPasswordRejected,
				XMEMORY_ACQUIRE
			) == 1u) &&
			(xrtAtomic32Load(&Runtime.ServerPassword, XMEMORY_ACQUIRE) == 1u) &&
			(xrtAtomic32Load(&Runtime.ServerForward, XMEMORY_ACQUIRE) == 0u) &&
			(xrtAtomic32Load(&Runtime.GlobalNotified, XMEMORY_ACQUIRE) == 0u) &&
			(Runtime.ImmediateReadyFuture != NULL) &&
			(Runtime.CloseFuture != NULL) &&
			(Runtime.GlobalFutures[0] != NULL) &&
			(Runtime.GlobalFutures[1] == NULL) &&
			(xrtFutureWaitFor(Runtime.GlobalFutures[0], 5000000u) == XWAIT_OK) &&
			(xrtFutureState(Runtime.GlobalFutures[0]) == XFUTURE_FAILED) &&
			(xrtFutureError(Runtime.GlobalFutures[0]) != NULL) &&
			(strcmp(
				xrtErrorDomain(xrtFutureError(Runtime.GlobalFutures[0])),
				"xrt.net"
			) == 0) && (xrtErrorKind(
				xrtFutureError(Runtime.GlobalFutures[0])
			) == XERR_IO) &&
			(xrtFutureState(Runtime.ReadyFuture) == XFUTURE_RESOLVED) &&
			(xrtFutureState(Runtime.ImmediateReadyFuture) == XFUTURE_RESOLVED) &&
			(xrtFutureState(Runtime.CloseFuture) == XFUTURE_RESOLVED) &&
			(xrtFutureState(Runtime.CancelFuture) == XFUTURE_CANCELLED) &&
			(Runtime.ClientCloseResult == XNET_RESULT_ERROR) &&
			Runtime.ClientCloseHasError,
			"ssh client runtime disconnect evidence incomplete");
		for ( i = 0u; i < TEST_SSH_CLIENT_CLOSE_WAITERS; ++i ) {
			testRequire((xrtFutureWaitFor(
				Runtime.CloseStressFutures[i],
				5000000u
			) == XWAIT_OK) && (xrtFutureState(
				Runtime.CloseStressFutures[i]
			) == ((i & 1u) != 0u ? XFUTURE_CANCELLED : XFUTURE_RESOLVED)),
				"ssh client runtime disconnect close Future mismatch");
		}
		goto Cleanup;
	}
	if ( (xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE) != 0u) ||
		(xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE) != 2u) ||
		(xrtAtomic32Load(&Runtime.ServerNone, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(
			&Runtime.ServerPasswordRejected,
			XMEMORY_ACQUIRE
		) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerPassword, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerEnvRejected, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerForward, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerForwardedOpen, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerForwardedData, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerForwardCancel, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerExec, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerPty, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerResize, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerShell, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(
			&Runtime.ServerDirectRejected,
			XMEMORY_ACQUIRE
		) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerDirect, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerDirectData, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerDirectBytes, XMEMORY_ACQUIRE) !=
		 TEST_SSH_CLIENT_DIRECT_BYTES) ||
		(xrtAtomic32Load(&Runtime.ServerAdjusts, XMEMORY_ACQUIRE) !=
		 TEST_SSH_CLIENT_DIRECT_ADJUSTS) ||
		(xrtAtomic32Load(&Runtime.DirectFlushes, XMEMORY_ACQUIRE) !=
		 TEST_SSH_CLIENT_DIRECT_PACKETS) ||
		(xrtAtomic32Load(
			&Runtime.BackpressureRejected,
			XMEMORY_ACQUIRE
		) != 1u) ||
		(xrtAtomic32Load(&Runtime.FutureOomPoints, XMEMORY_ACQUIRE) <
		 TEST_SSH_CLIENT_FUTURE_OOM_MIN) ||
		(xrtAtomic32Load(&Runtime.GlobalNotified, XMEMORY_ACQUIRE) != 2u) ||
		(xrtAtomic32Load(&Runtime.ChannelOpenFailed, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ChannelOpened, XMEMORY_ACQUIRE) != 4u) ||
		(xrtAtomic32Load(&Runtime.RequestFailed, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.RequestSucceeded, XMEMORY_ACQUIRE) != 3u) ||
		(xrtAtomic32Load(&Runtime.DataReceived, XMEMORY_ACQUIRE) != 5u) ||
		(xrtAtomic32Load(&Runtime.ForwardedAccepted, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.PacketHeld, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.PacketResumed, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(
			&Runtime.PacketRetryInjected,
			XMEMORY_ACQUIRE
		) != 1u) ||
		(xrtAtomic32Load(&Runtime.PacketRetryErrors, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.PacketRetried, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ServerRetryRejected, XMEMORY_ACQUIRE) != 1u) ||
		!testSshClientRuntimeBackpressureEvidence(&Runtime) ||
		(xrtAtomic32Load(
			&Runtime.ForwardedDataReceived,
			XMEMORY_ACQUIRE
		) != 1u) ||
		(xrtAtomic32Load(&Runtime.StderrDataReceived, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ExitReceived, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.ExitSignalReceived, XMEMORY_ACQUIRE) != 1u) ||
		(xrtAtomic32Load(&Runtime.EofReceived, XMEMORY_ACQUIRE) != 4u) ||
		(xrtAtomic32Load(&Runtime.ChannelClosed, XMEMORY_ACQUIRE) != 4u) ||
		Runtime.Server.ForwardedChannelPending ||
		Runtime.RejectedChannelPending ||
		(Runtime.Phase != TEST_SSH_CLIENT_PHASE_DONE) ) {
		fprintf(stderr,
			"[EVIDENCE] error=%u ready=%u host=%u auth=%u none=%u "
			"auth-reject=%u password=%u env-reject=%u "
			"forward=%u forward-open=%u forward-data=%u "
			"forward-accept=%u forward-recv=%u cancel=%u "
			"exec=%u pty=%u resize=%u shell=%u "
			"direct-reject=%u direct=%u direct-data=%u bytes=%u "
			"adjust=%u flush=%u "
			"queue-reject=%u future-oom=%u global=%u open=%u "
			"open-reject=%u reply-reject=%u reply=%u "
			"data=%u stderr=%u exit=%u signal=%u eof=%u close=%u phase=%d\n",
			xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerNone, XMEMORY_ACQUIRE),
			xrtAtomic32Load(
				&Runtime.ServerPasswordRejected,
				XMEMORY_ACQUIRE
			),
			xrtAtomic32Load(&Runtime.ServerPassword, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerEnvRejected, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerForward, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerForwardedOpen, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerForwardedData, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ForwardedAccepted, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ForwardedDataReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerForwardCancel, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerExec, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerPty, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerResize, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerShell, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerDirectRejected, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerDirect, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerDirectData, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerDirectBytes, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ServerAdjusts, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.DirectFlushes, XMEMORY_ACQUIRE),
			xrtAtomic32Load(
				&Runtime.BackpressureRejected,
				XMEMORY_ACQUIRE
			),
			xrtAtomic32Load(&Runtime.FutureOomPoints, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.GlobalNotified, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ChannelOpened, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ChannelOpenFailed, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.RequestFailed, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.RequestSucceeded, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.DataReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.StderrDataReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ExitReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ExitSignalReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.EofReceived, XMEMORY_ACQUIRE),
			xrtAtomic32Load(&Runtime.ChannelClosed, XMEMORY_ACQUIRE),
			(int)Runtime.Phase);
	}
	testRequire((xrtAtomic32Load(&Runtime.Errors, XMEMORY_ACQUIRE) == 0u) &&
		(xrtAtomic32Load(&Runtime.Ready, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.HostChecks, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ClientAuthCalls, XMEMORY_ACQUIRE) == 2u) &&
		(xrtAtomic32Load(&Runtime.ServerNone, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(
			&Runtime.ServerPasswordRejected,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerPassword, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerEnvRejected, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerForward, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerForwardedOpen, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerForwardedData, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerForwardCancel, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerExec, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerPty, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerResize, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerShell, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(
			&Runtime.ServerDirectRejected,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerDirect, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerDirectData, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerDirectBytes, XMEMORY_ACQUIRE) ==
		 TEST_SSH_CLIENT_DIRECT_BYTES) &&
		(xrtAtomic32Load(&Runtime.ServerAdjusts, XMEMORY_ACQUIRE) ==
		 TEST_SSH_CLIENT_DIRECT_ADJUSTS) &&
		(xrtAtomic32Load(&Runtime.DirectFlushes, XMEMORY_ACQUIRE) ==
		 TEST_SSH_CLIENT_DIRECT_PACKETS) &&
		(xrtAtomic32Load(
			&Runtime.BackpressureRejected,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(&Runtime.FutureOomPoints, XMEMORY_ACQUIRE) >=
		 TEST_SSH_CLIENT_FUTURE_OOM_MIN) &&
		(xrtAtomic32Load(&Runtime.GlobalNotified, XMEMORY_ACQUIRE) == 2u) &&
		(xrtAtomic32Load(&Runtime.PtySucceeded, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ShellSucceeded, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ExecDataReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.StderrDataReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.PtyDataReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.DirectDataReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ForwardedAccepted, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.PacketHeld, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.PacketResumed, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(
			&Runtime.PacketRetryInjected,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(xrtAtomic32Load(&Runtime.PacketRetryErrors, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.PacketRetried, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ServerRetryRejected, XMEMORY_ACQUIRE) == 1u) &&
		testSshClientRuntimeBackpressureEvidence(&Runtime) &&
		(xrtAtomic32Load(
			&Runtime.ForwardedDataReceived,
			XMEMORY_ACQUIRE
		) == 1u) &&
		(Runtime.DirectQueued == TEST_SSH_CLIENT_DIRECT_BYTES) &&
		(Runtime.WriteFutureCount == TEST_SSH_CLIENT_WRITE_WAITERS) &&
		!Runtime.RejectedChannelPending &&
		(xrtAtomic32Load(&Runtime.ChannelOpenFailed, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ChannelOpened, XMEMORY_ACQUIRE) == 4u) &&
		(xrtAtomic32Load(&Runtime.RequestFailed, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.RequestSucceeded, XMEMORY_ACQUIRE) == 3u) &&
		(xrtAtomic32Load(&Runtime.DataReceived, XMEMORY_ACQUIRE) == 5u) &&
		(xrtAtomic32Load(&Runtime.ExitReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.ExitSignalReceived, XMEMORY_ACQUIRE) == 1u) &&
		(xrtAtomic32Load(&Runtime.EofReceived, XMEMORY_ACQUIRE) == 4u) &&
		(xrtAtomic32Load(&Runtime.ChannelClosed, XMEMORY_ACQUIRE) == 4u) &&
		!Runtime.Server.ForwardedChannelPending &&
		(Runtime.Phase == TEST_SSH_CLIENT_PHASE_DONE),
		"ssh client runtime transaction evidence incomplete");
	testSshClientRuntimeFutures(&Runtime);

Cleanup:
	testRequire(xrtNetListenerClose(Runtime.Listener),
		"ssh client runtime listener close failed");
	testSshClientRuntimeWait(&Runtime.ListenerClosed, 1u,
		"ssh client runtime listener close timeout");
	xrtNetStreamDestroy(Runtime.ClientStream);
	xrtNetStreamDestroy(Runtime.Server.Stream);
	xrtNetDialDestroy(Runtime.ClientDial);
	xrtNetListenerDestroy(Runtime.Listener);
	testRequire(xrtNetResolverDestroy(Runtime.Resolver) &&
		xrtNetEngineDestroy(Runtime.Engine) &&
		xrtSshClientClear(&Runtime.Client) &&
		xrtSshSessionStreamClear(&Runtime.Server.Session),
		"ssh client runtime object cleanup failed");
	testSshClientRuntimeFuturesDestroy(&Runtime);
	if ( Runtime.Server.ResourcesReady ) {
		xrtSshChannelsClear(&Runtime.Server.Channels);
	}
	xrtSecureZero(Runtime.Server.Seed, sizeof(Runtime.Server.Seed));
	xrtClearError();
	xrtMemDebugSnapshot(&Memory);
	testRequire(Memory.Enabled && (Memory.LiveCount == 0u) &&
		(Memory.LiveBytes == 0u) && (Memory.DoubleFreeCount == 0u) &&
		(Memory.InvalidFreeCount == 0u) && (Memory.OverflowCount == 0u) &&
		(Memory.UnderflowCount == 0u) &&
		(Memory.UseAfterFreeCount == 0u),
		"ssh client runtime memory debug did not return to zero");
	return;
}



/* Select 覆盖完整降级链；Windows IOCP 额外覆盖确定性 completion 背压。 */
int main(void)
{
	testSshClientRuntimeRun(
		TEST_SSH_CLIENT_MODE_REJECT_HOST,
		XNET_PORT_SELECT,
		false
	);
	testSshClientRuntimeRun(
		TEST_SSH_CLIENT_MODE_TIMEOUT,
		XNET_PORT_SELECT,
		false
	);
	testSshClientRuntimeRun(
		TEST_SSH_CLIENT_MODE_DISCONNECT,
		XNET_PORT_SELECT,
		false
	);
	testSshClientRuntimeRun(
		TEST_SSH_CLIENT_MODE_SUCCESS,
		XNET_PORT_SELECT,
		false
	);
	#if defined(_WIN32) || defined(_WIN64)
		testSshClientRuntimeRun(
			TEST_SSH_CLIENT_MODE_SUCCESS,
			XNET_PORT_IOCP,
			true
		);
	#endif
	return 0;
}
