#ifndef XMQTT_EXAMPLE_SERVER_LISTENER_H
#define XMQTT_EXAMPLE_SERVER_LISTENER_H
#include <xmqtt.h>
#include <xmqtt/server_tls.h>
#include <xrt/atomic.h>
#include <xrt/thread.h>
#include <xrt/time.h>
#include <stdio.h>

typedef struct serverexample {
	xmqttserver* Server;
	xnetengine* Engine;
	xnetlistener* Listener;
	xnetaddr Address;
	xatomic32 ListenerClosed;
	xnetportbackend Backend;
	bool (*Prepare)(xmqttserver* Server, ptr Data);
	ptr PrepareData;
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	xatomic32 Upgrades;
	bool WebSocket;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	const xtlsserverconfig* Tls;
#endif
#if defined(XMQTT_EXAMPLE_SERVER_AUTH)
	const xmqttserverauthconfig* Auth;
	bool RequireMethod;
#endif
} serverexample;

#if defined(XMQTT_EXAMPLE_SERVER_WS)
static bool exampleServerWsAccept(serverexample* Example, xnetstream* Stream);
#endif

static bool exampleServerAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
	(void)Listener;
	serverexample* Example = Data;
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	if (Example->WebSocket) return exampleServerWsAccept(Example, Stream);
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (Example->Tls) {
		if (xmqttServerAttachTls(Example->Server, Stream, Example->Tls, NULL)) return true;
		const xerror* Error = xrtGetError();
		if (Error) fprintf(stderr, "TLS accept: %s\n", xrtErrorMessage(Error));
		return false;
	}
#endif
	return xmqttServerAttach(Example->Server, Stream);
}

static void exampleServerListenerClosed(xnetlistener* Listener, ptr Data)
{
	(void)Listener;
	xrtAtomic32Store(&((serverexample*)Data)->ListenerClosed, 1, XMEMORY_RELEASE);
}

static bool exampleServerDrain(serverexample* Example)
{
	if (Example->Server && !xmqttServerClose(Example->Server)) return false;
	if (Example->Listener) {
		if (!xrtNetListenerClose(Example->Listener)) return false;
		double Until = xrtTimer() + 5.0;
		while (!xrtAtomic32Load(&Example->ListenerClosed, XMEMORY_ACQUIRE)) {
			if (xrtTimer() >= Until) return false;
			xrtSleep(1);
		}
		xrtNetListenerDestroy(Example->Listener);
		Example->Listener = NULL;
	}
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	if (Example->Engine) {
		double Until = xrtTimer() + 5.0;
		while (xrtAtomic32Load(&Example->Upgrades, XMEMORY_ACQUIRE)) {
			if (xrtTimer() >= Until) return false;
			xrtSleep(1);
		}
	}
#endif
	return !Example->Server || xmqttServerWaitFor(Example->Server, 5000) == XWAIT_OK;
}

static bool exampleServerStop(serverexample* Example)
{
	if (!exampleServerDrain(Example)) return false;
	if (Example->Server) {
		if (!xmqttServerDestroy(Example->Server)) return false;
		Example->Server = NULL;
	}
	if (Example->Engine) {
		if (!xrtNetEngineStop(Example->Engine) || !xrtNetEngineDestroy(Example->Engine)) return false;
		Example->Engine = NULL;
	}
	return true;
}

static bool exampleServerStart(serverexample* Example, const xmqttserverconfig* Config, uint32 Workers)
{
	xrtAtomic32Init(&Example->ListenerClosed, 0);
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	xrtAtomic32Init(&Example->Upgrades, 0);
#endif
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = Workers;
	EngineConfig.Backend = Example->Backend;
	Example->Engine = xrtNetEngineCreate(&EngineConfig);
	if (!Example->Engine || !xrtNetEngineStart(Example->Engine)) goto Failed;
#if defined(XMQTT_EXAMPLE_SERVER_AUTH)
	if (Example->Auth) {
		bool Created = Example->RequireMethod ?
			xmqttServerCreateAuthRequired(Example->Engine, Config, Example->Auth, &Example->Server) :
			xmqttServerCreateAuth(Example->Engine, Config, Example->Auth, &Example->Server);
		if (!Created) goto Failed;
	} else
#endif
	if (!xmqttServerCreate(Example->Engine, Config, &Example->Server)) goto Failed;
	if (Example->Prepare && !Example->Prepare(Example->Server, Example->PrepareData)) goto Failed;

	xnetlistenconfig Listen;
	xrtNetListenConfigInit(&Listen);
#if defined(XMQTT_EXAMPLE_SERVER_WS)
	if (Example->WebSocket) Listen.Distribution = XNET_ACCEPT_ROUND_ROBIN;
#endif
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	if (Example->Tls) Listen.Distribution = XNET_ACCEPT_ROUND_ROBIN;
#endif
	xrtNetAddrLoopback(&Listen.Address, XNET_FAMILY_IPV4, 0);
	static const xnetlistenerevents Events = {.Accept = exampleServerAccept, .Close = exampleServerListenerClosed};
	Example->Listener = xrtNetListen(Example->Engine, &Listen, &Events, NULL, Example);
	if (!Example->Listener || !xrtNetListenerLocal(Example->Listener, &Example->Address)) goto Failed;
	return true;

Failed:
	(void)exampleServerStop(Example);
	return false;
}
#endif
