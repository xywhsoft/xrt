#define XMQTT_EXAMPLE_SERVER_AUTH
#define XMQTT_EXAMPLE_SERVER_WS
#include "app_listener.h"
#include "app_tls.h"
#include "app_ws.h"
#include "app_auth.h"
#include <stdlib.h>

int main(int Argc, char** Argv)
{
	bool RequireMethod = Argc > 1 && !strcmp(Argv[1], "--require-method");
	if (RequireMethod) {
		--Argc;
		++Argv;
	}
	const char* Mode = Argc > 1 ? Argv[1] : "tcp";
	bool WebSocket = !strcmp(Mode, "ws") || !strcmp(Mode, "wss");
	bool Secure = !strcmp(Mode, "tls") || !strcmp(Mode, "wss");
	int Limits = Secure ? 4 : 2;
	const char* Secret = getenv("XMQTT_EXAMPLE_AUTH_KEY");
	if ((!WebSocket && !Secure && strcmp(Mode, "tcp")) ||
		(Argc != 1 && Argc != Limits && Argc != Limits + 2) || !Secret || strlen(Secret) < 32) {
		puts("Set XMQTT_EXAMPLE_AUTH_KEY to a test secret with at least 32 bytes.");
		puts("usage: examples_server_auth [--require-method] [tcp|ws [max-rounds max-bytes]]");
		puts("       examples_server_auth [--require-method] tls|wss certificate.der key.der [max-rounds max-bytes]");
		return Argc == 1 && !RequireMethod && !Secret ? 0 : 1;
	}
	xtlsserverconfig Tls;
	if (Secure && !exampleServerLoadTls(&Tls, Argv[2], Argv[3])) return 1;

	exampleauth State;
	xmqttserverauthconfig Auth;
	exampleAuthInit(&State, &Auth, Secret);
	Auth.ReauthTimeout = 1000;
	if (Argc == Limits + 2) {
		Auth.MaxRounds = strtoul(Argv[Limits], NULL, 10);
		Auth.MaxExchangeBytes = strtoul(Argv[Limits + 1], NULL, 10);
	}
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.ConnectTimeout = 1750;
	serverexample Example = {.Auth = &Auth, .RequireMethod = RequireMethod,
		.WebSocket = WebSocket, .Tls = Secure ? &Tls : NULL};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	if (Secure && Closed) exampleServerReleaseTls(&Tls);
	return Ready && Closed && !xrtAtomic32Load(&State.Active, XMEMORY_RELAXED) ? 0 : 1;
}
