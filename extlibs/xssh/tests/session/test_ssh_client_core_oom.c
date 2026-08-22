#include "../test.h"
#include "../../../../tests/test_fault_allocator.h"



/* 为 KEXINIT 动态输出故障建立稳定 identification 状态。 */
static void testSshClientCoreOomVersions(xsshsessiontcp* pSession)
{
	xsshsessioncore* pCore = &pSession->Session;
	xsshtransportcore* pTransport = &pSession->Transport.Core;

	testRequire((xrtSshSessionCoreVersionPrepare(
		pCore,
		pTransport,
		XSSH_TRANSPORT_LOCAL,
		XRT_STR_LITERAL("SSH-2.0-client_oom")
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pTransport,
		XSSH_TRANSPORT_LOCAL
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		pCore,
		pTransport
	) == XSSH_OK) && (xrtSshSessionCoreVersionPrepare(
		pCore,
		pTransport,
		XSSH_TRANSPORT_PEER,
		XRT_STR_LITERAL("SSH-2.0-server_oom")
	) == XSSH_OK) && (xrtSshTransportCoreIdentificationCommit(
		pTransport,
		XSSH_TRANSPORT_PEER
	) == XSSH_OK) && (xrtSshSessionCoreVersionCommit(
		pCore,
		pTransport
	) == XSSH_OK), "ssh client OOM version setup failed");
}



/* 初始化和扩容失败都必须保持核心可清理、可重试且不推进会话。 */
int main(void)
{
	static testfaultallocator State = { 0u, SIZE_MAX, 0u, false };
	xallocator Allocator = testFaultAllocator(&State);
	xsshclientcoreconfig ClientConfig;
	xsshsessiontcpconfig SessionConfig;
	xsshsessionreader Reader;
	xsshclientnext Next;
	xsshclientcore Client;
	xsshsessiontcp Session;
	xnetbufpool* pPool;
	size_t iCalls;

	testRequire(xrtSetAllocator(&Allocator) &&
		xrtSshClientCoreConfigInit(&ClientConfig),
		"ssh client OOM setup failed");
	ClientConfig.Version = XRT_STR_LITERAL("SSH-2.0-client_oom");
	ClientConfig.User = XRT_STR_LITERAL("alice");
	ClientConfig.OutputInitial = 32u;
	ClientConfig.OutputLimit = 4096u;

	State.FailAt = SIZE_MAX;
	pPool = xrtNetBufPoolCreate(NULL);
	testRequire((pPool != NULL) && xrtSshSessionTcpConfigInit(
		&SessionConfig,
		XSSH_ROLE_CLIENT
	) && xrtSshSessionTcpInit(
		&Session,
		pPool,
		&SessionConfig,
		0u
	) && xrtSshSessionReaderInit(
		&Reader,
		pPool,
		&Session
	), "ssh client OOM session initialization failed");
	iCalls = State.Calls;
	testRequire(xrtSshClientCoreInit(
		&Client,
		&ClientConfig
	) && (Client.Output == NULL) && (Client.OutputCapacity == 0u) &&
		(State.Calls == iCalls),
		"ssh client empty initialization allocated output");
	testSshClientCoreOomVersions(&Session);

	State.FailAt = State.Calls + 1u;
	State.Hit = false;
	testRequire((xrtSshClientCoreNext(
		&Client,
		&Session,
		&Reader,
		0u,
		&Next
	) == XSSH_ERROR_SPACE) && State.Hit &&
		(Client.Output == NULL) && (Client.OutputCapacity == 0u) &&
		(xrtSshSessionTcpAction(&Session) ==
		 XSSH_SESSION_ACTION_WRITE_KEXINIT),
		"ssh client output OOM changed protocol state");
	xrtClearError();

	State.FailAt = SIZE_MAX;
	testRequire((xrtSshClientCoreNext(
		&Client,
		&Session,
		&Reader,
		0u,
		&Next
	) == XSSH_OK) && (Next.Kind == XSSH_CLIENT_NEXT_PAYLOAD) &&
		(Next.Data.Size > ClientConfig.OutputInitial) &&
		(Client.OutputCapacity > ClientConfig.OutputInitial),
		"ssh client output did not recover after OOM");

	xrtSshClientCoreClear(&Client);
	xrtSshSessionReaderClear(&Reader);
	xrtSshSessionTcpClear(&Session);
	xrtNetBufPoolDestroy(pPool);
	return 0;
}
