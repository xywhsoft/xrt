#define XMQTT_EXAMPLE_SERVER_AUTH
#include "app_listener.h"
#include <xrt/crypto.h>
#include <xrt/memory.h>
#include <xrt/random.h>
#include <stdlib.h>
#include <string.h>

/* Demonstrates callbacks on loopback with a pre-shared test secret.
 * Select a standardized method and appropriate TLS and policy in deployment. */
static const char ExampleMethod[] = "example-hmac-sha256";

typedef struct exampleauth {
	const char* Secret;
	size_t SecretBytes;
	xatomic32 Active;
} exampleauth;

typedef struct exampleexchange {
	unsigned char Nonce[32], Final[32];
	unsigned Challenges;
} exampleexchange;

static bool exampleAuthMac(const xmqttserverauthrequest* Request, const exampleauth* Auth,
	const exampleexchange* Exchange, unsigned char Role, unsigned char Mac[32])
{
	xhmacsha256 Hmac;
	bool Signed = xrtHmacSha256Init(&Hmac, Auth->Secret, Auth->SecretBytes) &&
		xrtHmacSha256Update(&Hmac, &Role, 1) &&
		xrtHmacSha256Update(&Hmac, Exchange->Nonce, sizeof(Exchange->Nonce)) &&
		xrtHmacSha256Update(&Hmac, Request->ClientId.Data, Request->ClientId.Size) &&
		xrtHmacSha256Final(&Hmac, Mac);
	xrtSecureZero(&Hmac, sizeof(Hmac));
	return Signed;
}

static xmqttserverauthdecision exampleAuthStep(const xmqttserverauthrequest* Request,
	ptr* State, xbytesview* Reply, ptr User)
{
	exampleauth* Auth = User;
	if (Request->Method.Size != sizeof(ExampleMethod) - 1 ||
		memcmp(Request->Method.Data, ExampleMethod, sizeof(ExampleMethod) - 1))
		return XMQTT_SERVER_AUTH_UNSUPPORTED;
	exampleexchange* Exchange = *State;
	if (Request->Phase != XMQTT_SERVER_AUTH_CONTINUE) {
		if (Request->Data.Data) return XMQTT_SERVER_AUTH_DENY;
		Exchange = xrtCalloc(1, sizeof(*Exchange));
		if (!Exchange) return XMQTT_SERVER_AUTH_DENY;
		*State = Exchange;
		xrtAtomic32FetchAdd(&Auth->Active, 1, XMEMORY_RELAXED);
	} else {
		unsigned char Proof[32];
		if (!Exchange || Request->Data.Size != sizeof(Proof) ||
			!exampleAuthMac(Request, Auth, Exchange, 'C', Proof)) return XMQTT_SERVER_AUTH_DENY;
		unsigned Difference = 0;
		for (size_t Index = 0; Index < sizeof(Proof); ++Index)
			Difference |= Proof[Index] ^ Request->Data.Data[Index];
		xrtSecureZero(Proof, sizeof(Proof));
		if (Difference) return XMQTT_SERVER_AUTH_DENY;
	}
	if (Exchange->Challenges < 2) {
		if (!xrtSecureRandom(Exchange->Nonce, sizeof(Exchange->Nonce))) return XMQTT_SERVER_AUTH_DENY;
		++Exchange->Challenges;
		*Reply = (xbytesview){Exchange->Nonce, sizeof(Exchange->Nonce)};
		return XMQTT_SERVER_AUTH_CHALLENGE;
	}
	if (!exampleAuthMac(Request, Auth, Exchange, 'S', Exchange->Final)) return XMQTT_SERVER_AUTH_DENY;
	*Reply = (xbytesview){Exchange->Final, sizeof(Exchange->Final)};
	return XMQTT_SERVER_AUTH_ACCEPT;
}

static void exampleAuthRelease(ptr State, ptr User)
{
	xrtSecureZero(State, sizeof(exampleexchange));
	xrtFree(State);
	xrtAtomic32FetchSub(&((exampleauth*)User)->Active, 1, XMEMORY_RELAXED);
}

int main(int Argc, char** Argv)
{
	const char* Secret = getenv("XMQTT_EXAMPLE_AUTH_KEY");
	if (!Secret || strlen(Secret) < 32 || (Argc != 1 && Argc != 3)) {
		puts("Set XMQTT_EXAMPLE_AUTH_KEY to a test secret with at least 32 bytes.");
		puts("usage: examples_server_auth [max-rounds max-bytes]");
		return Argc == 1 && !Secret ? 0 : 1;
	}
	exampleauth State = {.Secret = Secret, .SecretBytes = strlen(Secret)};
	xrtAtomic32Init(&State.Active, 0);
	xmqttserverauthconfig Auth;
	xmqttServerAuthConfigInit(&Auth);
	Auth.Step = exampleAuthStep;
	Auth.Release = exampleAuthRelease;
	Auth.User = &State;
	Auth.ReauthTimeout = 1000;
	if (Argc == 3) {
		Auth.MaxRounds = strtoul(Argv[1], NULL, 10);
		Auth.MaxExchangeBytes = strtoul(Argv[2], NULL, 10);
	}
	xmqttserverconfig Config;
	xmqttServerConfigInit(&Config);
	Config.ConnectTimeout = 1750;
	serverexample Example = {.Auth = &Auth};
	char Endpoint[80];
	bool Ready = exampleServerStart(&Example, &Config, 2) &&
		xrtNetAddrEndpointText(&Example.Address, Endpoint, sizeof(Endpoint));
	if (Ready) {
		puts(Endpoint);
		fflush(stdout);
		(void)getchar();
	}
	bool Closed = exampleServerStop(&Example);
	return Ready && Closed && !xrtAtomic32Load(&State.Active, XMEMORY_RELAXED) ? 0 : 1;
}
