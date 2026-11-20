#include "app_listener.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int Argc, char** Argv)
{
	bool Compare = Argc >= 2 && !strcmp(Argv[1], "--compare");
	xmqttserverconfig Config;
	if (!xmqttServerConfigInit(&Config)) return 1;
	if (Compare && Argc > 2) {
		char* End;
		unsigned long Limit = strtoul(Argv[2], &End, 10);
		if (Argc != 3 || *Argv[2] < '0' || *Argv[2] > '9' || *End || !Limit || Limit > 10000) {
			fputs("usage: examples_server_main --compare [max-connections 1..10000]\n", stderr);
			return 2;
		}
		Config.MaxConnections = (size_t)Limit;
	}
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {0};
	if (!exampleServerStart(&Example, &Config, Compare ? 1 : 2)) return 1;
	char Endpoint[80];
	bool Ready = xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint)) != 0;
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		/* With an argument, wait for a line from the APP driving this sample. */
		if (Argc > 1) (void)getchar();
	}
	return exampleServerStop(&Example) && Ready ? 0 : 1;
}
