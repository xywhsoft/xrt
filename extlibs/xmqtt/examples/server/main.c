#include "app_listener.h"
#include <stdio.h>

int main(int Argc, char** Argv)
{
	(void)Argv;
	xmqttserverconfig Config;
	if (!xmqttServerConfigInit(&Config)) return 1;
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {0};
	if (!exampleServerStart(&Example, &Config, 2)) return 1;
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
