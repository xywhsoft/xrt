#include "../../src/internal/xrt_mail_net.h"
#include "../test.h"
#include "../test_tls.h"
#include <xrt/memory_debug.h>
#include "../../../../tests/fixtures/mail_tls_partial.h"
#include "../../../../src/internal/xrt_tls_stream.h"

typedef struct testmailtlsclosepark {
	xatomic32 Entered;
	xatomic32 Release;
	xatomic32 Exited;
	xdeadline Deadline;
} testmailtlsclosepark;

typedef struct testmailtlscloseserver {
	xnetlistener* Listener;
	const xtlsserverconfig* Tls;
	xdeadline Deadline;
	xatomic32 Returned;
	bool Success;
} testmailtlscloseserver;

static xnetaddr TestMailTlsCloseAddress;

static xnetaddrlist* testMailTlsCloseResolve(cstr Host, xnetfamily Family, ptr Data)
{
	xnetaddr Address = TestMailTlsCloseAddress;
	(void)Family;
	(void)Data;
	if ( strcmp(Host, "close-fault.test") != 0 ) return NULL;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}

/* 占住客户端 Worker，使关闭命令尚未执行时的 OOM 窗口可复现。 */
static void testMailTlsCloseParkWorker(xnetworker* Worker, ptr Data)
{
	testmailtlsclosepark* Park = (testmailtlsclosepark*)Data;
	(void)Worker;
	xrtAtomic32Store(&Park->Entered, 1u, XMEMORY_RELEASE);
	while ( !xrtAtomic32Load(&Park->Release, XMEMORY_ACQUIRE) ) {
		testRequire(!xrtDeadlineExpired(Park->Deadline), "mail TLS close Worker park timed out");
		xrtThreadYield();
	}
	xrtAtomic32Store(&Park->Exited, 1u, XMEMORY_RELEASE);
}

static int32 testMailTlsCloseServer(ptr Data)
{
	testmailtlscloseserver* Server = (testmailtlscloseserver*)Data;
	xnetstream* Tcp = xrtNetListenerAcceptWait(Server->Listener, Server->Deadline, NULL);
	xtlsstream* Tls;
	bool Success;
	if ( Tcp == NULL ) return 1;
	Tls = testMailTlsUpgrade(&Tcp, Server->Tls, Server->Deadline);
	if ( Tls == NULL ) { xrtNetStreamDestroy(Tcp); return 2; }
	Success = testMailTlsSend(Tls, "ready\r\n", sizeof("ready\r\n") - 1u, Server->Deadline) &&
		testMailPartialWaitFlag(&Server->Returned, Server->Deadline);
	if ( Success ) {
		xfuture* Read = xrtTlsStreamRecvAsync(Tls, 1u);
		Success = Read != NULL && xrtFutureWaitUntil(Read, Server->Deadline) == XWAIT_OK &&
			xrtFutureState(Read) == XFUTURE_FAILED && xrtTlsStreamState(Tls) == XTLS_STREAM_FAILED;
		xrtFutureDestroy(Read);
	}
	(void)xrtTlsStreamAbort(Tls);
	while ( xrtTlsStreamState(Tls) != XTLS_STREAM_FAILED && xrtTlsStreamState(Tls) != XTLS_STREAM_CLOSED ) {
		if ( xrtDeadlineExpired(Server->Deadline) ) { Success = false; break; }
		xrtThreadYield();
	}
	xrtTlsStreamDestroy(Tls);
	Server->Success = Success;
	return Success ? 0 : 3;
}

int main(void)
{
	xtlscontext* Context = testTlsServerContext();
	xtlsidentity* Identity = testTlsServerIdentity();
	xtlsserverconfig TlsServer;
	xtlsverifierconfig VerifyConfig;
	xnetengineconfig EngineConfig;
	xnetlistenconfig ListenConfig;
	xnetresolverconfig ResolverConfig;
	xmailnetconfig Config;
	__xmailtransport Transport;
	testmailtlsclosepark Park;
	testmailtlscloseserver Server;
	xstrview Line;
	testRequire(Context != NULL && Identity != NULL, "mail TLS close identity creation failed");
	xrtTlsVerifierConfigInit(&VerifyConfig);
	VerifyConfig.Verify = testTlsServerAccept;
	xtlsverifier* Verifier = xrtTlsVerifierCreate(&VerifyConfig);
	testRequire(Verifier != NULL, "mail TLS close verifier creation failed");
	xrtTlsServerConfigInit(&TlsServer);
	TlsServer.Context = Context;
	TlsServer.Identity = Identity;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Backend = XNET_PORT_SELECT;
	EngineConfig.Workers = 2u;
	xnetengine* Engine = xrtNetEngineCreate(&EngineConfig);
	testRequire(Engine != NULL && xrtNetEngineStart(Engine), "mail TLS close engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(&ListenConfig.Address, XNET_FAMILY_IPV4, 0),
		"mail TLS close address creation failed");
	xnetlistener* Listener = xrtNetListen(Engine, &ListenConfig, NULL, NULL, NULL);
	testRequire(Listener != NULL && xrtNetListenerLocal(Listener, &TestMailTlsCloseAddress),
		"mail TLS close listener creation failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailTlsCloseResolve;
	ResolverConfig.CacheEntries = 0;
	xnetresolver* Resolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(Resolver != NULL, "mail TLS close resolver creation failed");
	xrtMailNetConfigInit(&Config);
	Config.Engine = Engine;
	Config.Resolver = Resolver;
	Config.Host = "close-fault.test";
	Config.Port = TestMailTlsCloseAddress.Port;
	Config.Security = XMAIL_SECURITY_TLS;
	Config.Tls.Context = Context;
	Config.Tls.Verifier = Verifier;
	memset(&Server, 0, sizeof(Server));
	Server.Listener = Listener;
	Server.Tls = &TlsServer;
	Server.Deadline = xrtDeadlineAfter(UINT64_C(10000000));
	xrtAtomic32Init(&Server.Returned, 0u);
	xthread* Thread = xrtThreadCreate(testMailTlsCloseServer, &Server, 0);
	testRequire(Thread != NULL, "mail TLS close server thread creation failed");
	testRequire(__xrtMailTransportOpen(&Transport, &Config, Server.Deadline, NULL) &&
		__xrtMailTransportLine(&Transport, &Line, Server.Deadline, NULL) &&
		testMailViewEqual(Line, XRT_STR_LITERAL("ready")), "mail TLS close transport open failed");
	xtlsstream* Observer = xrtTlsStreamRef(Transport.Tls);
	testRequire(Observer != NULL, "mail TLS close observer missing");
	xnetworker* Worker = xrtNetStreamWorker(xrtTlsStreamTransport(Observer));
	memset(&Park, 0, sizeof(Park));
	Park.Deadline = Server.Deadline;
	xrtAtomic32Init(&Park.Entered, 0u);
	xrtAtomic32Init(&Park.Release, 0u);
	xrtAtomic32Init(&Park.Exited, 0u);
	testRequire(Worker != NULL && xrtNetEnginePost(Engine, xrtNetWorkerIndex(Worker),
		testMailTlsCloseParkWorker, &Park) && testMailPartialWaitFlag(&Park.Entered, Server.Deadline),
		"mail TLS close Worker park failed");
	xrtClearError();
	testRequire(xrtMemDebugFailAfter(0), "mail TLS close allocation injection failed");
	bool Closed = __xrtMailTransportClose(&Transport, xrtDeadlineAfter(UINT64_C(1000000)));
	xerror* Failure = xrtTakeError();
	xrtMemDebugFailClear();
	bool Aborted = xrtAtomic32Load(&Observer->AbortGate, XMEMORY_ACQUIRE) != 0;
	xrtAtomic32Store(&Park.Release, 1u, XMEMORY_RELEASE);
	testRequire(testMailPartialWaitFlag(&Park.Exited, Server.Deadline), "mail TLS close Worker did not resume");
	testRequire(!Closed && Failure != NULL && xrtErrorKind(Failure) == XERR_MEMORY,
		"mail TLS close lost the wait-Future allocation error");
	testRequire(Aborted, "mail TLS close allocation failure did not request Abort");
	xrtErrorFree(Failure);
	xrtAtomic32Store(&Server.Returned, 1u, XMEMORY_RELEASE);
	testRequire(xrtThreadWaitUntil(Thread, Server.Deadline) == XWAIT_OK && Server.Success &&
		xrtThreadExitCode(Thread) == 0, "mail TLS close OOM did not abort before transport destruction");
	xrtThreadDestroy(Thread);
	__xrtMailTransportDestroy(&Transport);
	xrtTlsStreamDestroy(Observer);
	testRequire(xrtNetListenerClose(Listener), "mail TLS close listener close failed");
	xdeadline Retire = xrtDeadlineAfter(UINT64_C(3000000));
	while ( xrtNetListenerState(Listener) != XNET_LISTENER_CLOSED ) {
		testRequire(!xrtDeadlineExpired(Retire), "mail TLS close listener did not close");
		xrtThreadYield();
	}
	xrtNetListenerDestroy(Listener);
	testRequire(xrtNetResolverDestroy(Resolver), "mail TLS close resolver destroy failed");
	xrtTlsVerifierRelease(Verifier);
	xrtTlsIdentityRelease(Identity);
	xrtTlsContextRelease(Context);
	for ( ;; ) {
		xnetretireresult Result = xrtNetEngineTryDestroy(Engine);
		if ( Result == XNET_RETIRE_READY ) break;
		testRequire(Result != XNET_RETIRE_ERROR && !xrtDeadlineExpired(Retire),
			"mail TLS close engine did not retire");
		xrtThreadYield();
	}
	return 0;
}
