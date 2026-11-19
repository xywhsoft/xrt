#define XMQTT_EXAMPLE_SERVER_WS
#include "app_listener.h"
#include "app_tls.h"
#include "app_ws.h"

int main(int Argc, char** Argv)
{
	if (Argc != 2 && Argc != 4) {
		puts("usage: examples_server_ws wait [certificate.der rsa-private-key.der]");
		return Argc == 1 ? 0 : 1;
	}
	xtlsserverconfig Tls;
	if (Argc == 4 && !exampleServerLoadTls(&Tls, Argv[2], Argv[3])) return 1;
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {.WebSocket = true, .Tls = Argc == 4 ? &Tls : NULL};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	if (Argc == 4) exampleServerReleaseTls(&Tls);
	return Ready && Closed ? 0 : 1;
}
