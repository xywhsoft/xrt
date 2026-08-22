#include "../../src/internal/xrt_mail_net.h"
#include "../test.h"



static xnetaddr TestMailNetAddress;



/* 把测试主机映射到本地监听地址。 */
static xnetaddrlist* testMailNetResolve(
	cstr sHost,
	xnetfamily Family,
	ptr pData
)
{
	xnetaddr Address;

	(void)Family;
	(void)pData;
	if ( strcmp(sHost, "mail.test") != 0 ) {
		return NULL;
	}
	Address = TestMailNetAddress;
	Address.Port = 0;
	return xrtNetAddrListCreate(&Address, 1u);
}



/* 等待 Stream 进入最终关闭状态。 */
static bool testMailNetClosed(xnetstream* pStream)
{
	xdeadline Deadline = xrtDeadlineAfter(UINT64_C(3000000));

	while ( xrtNetStreamState(pStream) != XNET_STREAM_CLOSED ) {
		if ( xrtDeadlineExpired(Deadline) ) {
			return false;
		}
		xrtThreadYield();
	}
	return true;
}



/* 验证动态多行缓存、完整发送和外部 Engine/Resolver 所有权。 */
int main(void)
{
	xnetengineconfig EngineConfig;
	xnetresolverconfig ResolverConfig;
	xnetlistenconfig ListenConfig;
	xmailnetconfig Config;
	xnetengine* pEngine;
	xnetresolver* pResolver;
	xnetlistener* pListener;
	xnetstream* pServer;
	xnetbytes* pCommand;
	xbytesview Command;
	__xmailtransport Transport;
	xstrview Line;
	xdeadline Deadline;
	bool bClosed;

	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = 1u;
	pEngine = xrtNetEngineCreate(&EngineConfig);
	testRequire((pEngine != NULL) && xrtNetEngineStart(pEngine),
		"mail network engine start failed");
	xrtNetListenConfigInit(&ListenConfig);
	testRequire(xrtNetAddrLoopback(
		&ListenConfig.Address,
		XNET_FAMILY_IPV4,
		0
	), "mail network loopback address failed");
	pListener = xrtNetListen(pEngine, &ListenConfig, NULL, NULL, NULL);
	testRequire((pListener != NULL) && xrtNetListenerLocal(
		pListener,
		&TestMailNetAddress
	), "mail network listener start failed");
	xrtNetResolverConfigInit(&ResolverConfig);
	ResolverConfig.Workers = 1u;
	ResolverConfig.Lookup = testMailNetResolve;
	ResolverConfig.CacheEntries = 0;
	pResolver = xrtNetResolverCreate(&ResolverConfig);
	testRequire(pResolver != NULL, "mail network resolver creation failed");

	xrtMailNetConfigInit(&Config);
	Config.Engine = pEngine;
	Config.Resolver = pResolver;
	Config.Host = "mail.test";
	Config.Port = TestMailNetAddress.Port;
	testRequire(xrtMailNetConfigValid(&Config),
		"mail network plain config validation failed");
	Deadline = xrtDeadlineAfter(UINT64_C(3000000));
	testRequire(__xrtMailTransportOpen(
		&Transport,
		&Config,
		Deadline,
		NULL
	), "mail network plain transport open failed");
	pServer = xrtNetListenerAcceptWait(pListener, Deadline, NULL);
	testRequire(pServer != NULL, "mail network server accept failed");

	testRequire(xrtNetStreamSend(
		pServer,
		"220 mail.test ready\r\n250 queued\r\n",
		33u
	) == XNET_RESULT_OK, "mail network server response send failed");
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && testMailViewEqual(Line, XRT_STR_LITERAL("220 mail.test ready")),
		"mail network first buffered line mismatch");
	testRequire(__xrtMailTransportLine(
		&Transport,
		&Line,
		Deadline,
		NULL
	) && testMailViewEqual(Line, XRT_STR_LITERAL("250 queued")),
		"mail network second buffered line mismatch");
	testRequire(__xrtMailTransportSend(
		&Transport,
		"EHLO client.test\r\n",
		18u,
		Deadline,
		NULL
	), "mail network client command send failed");
	pCommand = xrtNetStreamRecv(pServer, 0, Deadline, NULL);
	testRequire(pCommand != NULL, "mail network server command receive failed");
	Command = xrtNetBytesView(pCommand);
	testRequire((Command.Size == 18u) &&
		(memcmp(Command.Data, "EHLO client.test\r\n", 18u) == 0),
		"mail network command bytes mismatch");
	xrtNetBytesDestroy(pCommand);

	testRequire(xrtNetStreamClose(pServer),
		"mail network server close request failed");
	bClosed = __xrtMailTransportClose(&Transport, Deadline);
	testRequire(bClosed && testMailNetClosed(pServer),
		"mail network graceful close failed");
	__xrtMailTransportDestroy(&Transport);
	xrtNetStreamDestroy(pServer);
	testRequire(xrtNetListenerClose(pListener),
		"mail network listener close request failed");
	while ( xrtNetListenerState(pListener) != XNET_LISTENER_CLOSED ) {
		xrtThreadYield();
	}
	xrtNetListenerDestroy(pListener);
	testRequire(xrtNetResolverDestroy(pResolver),
		"mail network resolver destroy failed");
	testRequire(xrtNetEngineDestroy(pEngine),
		"mail network engine destroy failed");
	return 0;
}
