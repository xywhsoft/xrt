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
#if defined(XMQTT_FEATURE_XMQTT_SERVER_TLS)
	const xtlsserverconfig* Tls;
#endif
} serverexample;

static bool exampleServerAccept(xnetlistener* Listener, xnetstream* Stream, ptr Data)
{
	(void)Listener;
	serverexample* Example = Data;
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

static bool exampleServerStop(serverexample* Example)
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
	if (Example->Server) {
		if (xmqttServerWaitFor(Example->Server, 5000) != XWAIT_OK ||
			!xmqttServerDestroy(Example->Server)) return false;
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
	xnetengineconfig EngineConfig;
	xrtNetEngineConfigInit(&EngineConfig);
	EngineConfig.Workers = Workers;
	Example->Engine = xrtNetEngineCreate(&EngineConfig);
	if (!Example->Engine || !xrtNetEngineStart(Example->Engine) ||
		!xmqttServerCreate(Example->Engine, Config, &Example->Server)) goto Failed;

	xnetlistenconfig Listen;
	xrtNetListenConfigInit(&Listen);
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
