#include "app_listener.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

int main(int Argc, char** Argv)
{
	bool Compare = Argc >= 2 && !strcmp(Argv[1], "--compare");
	uint32 Workers = Compare ? 1 : 2;
	if (Compare && Argc >= 4 && !strcmp(Argv[Argc - 2], "--workers")) {
		const char* Text = Argv[Argc - 1];
		char* End;
		unsigned long Count = strtoul(Text, &End, 10);
		if (*Text < '0' || *Text > '9' || *End || !Count || Count > 64) {
			fputs("--workers requires a count from 1 to 64\n", stderr);
			return 2;
		}
		Workers = (uint32)Count;
		Argc -= 2;
	}
	bool Prepare = Compare && Argc == 4 && !strcmp(Argv[3], "--prepare-fds");
	xmqttserverconfig Config;
	if (!xmqttServerConfigInit(&Config)) return 1;
	if (Compare && Argc > 2) {
		char* End;
		unsigned long Limit = strtoul(Argv[2], &End, 10);
		if ((Argc != 3 && !Prepare) || *Argv[2] < '0' || *Argv[2] > '9' || *End || !Limit || Limit > 10000) {
			fputs("usage: examples_server_main --compare [max-connections 1..10000 [--prepare-fds]] [--workers 1..64]\n", stderr);
			return 2;
		}
		Config.MaxConnections = (size_t)Limit;
	}
	if (Prepare) {
#if defined(__linux__)
		/* Expand the process fd table before the worker shares it. Allow
		 * room for the listener, epoll descriptors and inherited streams. */
		int Descriptor = fcntl(STDIN_FILENO, F_DUPFD, (int)Config.MaxConnections + 64);
		if (Descriptor < 0 || close(Descriptor) != 0) {
			perror("prepare file descriptor table");
			return 1;
		}
#else
		fputs("--prepare-fds requires the Linux server example\n", stderr);
		return 2;
#endif
	}
#if defined(XMQTT_FEATURE_XMQTT_CODEC311)
	Config.Allow311 = true;
#endif
	serverexample Example = {0};
	if (!exampleServerSelectBackend(&Example)) return 2;
	const char* ReadMode = getenv("XMQTT_EXAMPLE_READ_MODE");
	if (ReadMode && !strcmp(ReadMode, "direct")) Example.ReadMode = XNET_STREAM_READ_DIRECT;
	else if (ReadMode && strcmp(ReadMode, "adaptive")) {
		fputs("XMQTT_EXAMPLE_READ_MODE requires adaptive or direct\n", stderr);
		return 2;
	}
	if (!exampleServerStart(&Example, &Config, Workers)) {
		const xerror* Error = xrtGetError();
		fprintf(stderr, "MQTT broker startup failed: %s\n", Error ? xrtErrorMessage(Error) : "listener unavailable");
		return 1;
	}
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
