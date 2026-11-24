#include "app_listener.h"
#include "app_tls.h"
#include <stdlib.h>
#include <string.h>

int main(int Argc, char** Argv)
{
	bool Compare = Argc >= 4 && !strcmp(Argv[3], "--compare");
	uint32 Workers = Compare ? 1 : 2;
	if (Compare && Argc == 6 && !strcmp(Argv[4], "--workers")) {
		const char* Text = Argv[5];
		char* End;
		unsigned long Count = strtoul(Text, &End, 10);
		if (*Text < '0' || *Text > '9' || *End || !Count || Count > 64) {
			fputs("--workers requires a count from 1 to 64\n", stderr);
			return 2;
		}
		Workers = (uint32)Count;
		Argc -= 2;
	}
	if (Argc != 3 && !(Compare && Argc == 4)) {
		puts("usage: examples_server_tls certificate.der rsa-private-key.der [--compare [--workers 1..64]]");
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
	bool Ready = exampleServerStart(&Example, &Config, Workers) &&
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
