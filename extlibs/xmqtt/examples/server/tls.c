#include "app_listener.h"
#include "app_tls.h"
#include <string.h>

int main(int Argc, char** Argv)
{
	bool Compare = Argc == 4 && !strcmp(Argv[3], "--compare");
	if (Argc != 3 && !Compare) {
		puts("usage: examples_server_tls certificate.der rsa-private-key.der [--compare]");
		return Argc == 1 ? 0 : 1;
	}
	xtlsserverconfig Tls;
	if (!exampleServerLoadTls(&Tls, Argv[1], Argv[2])) return 1;

	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {.Tls = &Tls};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, Compare ? 1 : 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	exampleServerReleaseTls(&Tls);
	return Ready && Closed ? 0 : 1;
}
