#include <xredis.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Prerequisite: a TLS-enabled Redis and a PEM CA bundle. Select
 * XREDIS_MODULE_XREDIS_TLS. With no args print usage (no service is contacted).
 * The optional last argument names a runtime environment variable containing
 * a Redis password. Credentials are never command-line values or log output. */
int main(int argc, char** argv)
{
	if (argc == 1) { puts("Usage: xredis-tls HOST PORT CA.pem [PASSWORD_ENV_NAME]"); return 0; }
	if (argc != 4 && argc != 5) return 2;
	char* End = NULL; unsigned long Port = strtoul(argv[2], &End, 10); if (*End != 0 || Port == 0 || Port > UINT16_MAX) return 2;
	xnetengine* Engine = NULL; xnetresolver* Resolver = NULL; xx509store* Store = NULL; xtlsverifier* Verifier = NULL;
	xredisclient* Client = NULL; xfuture *Ready = NULL, *Request = NULL, *Closed = NULL; int Code = 1;
	FILE* File = fopen(argv[3], "rb"); if (File == NULL) return 1;
	unsigned char Trust[65536]; size_t TrustBytes = fread(Trust, 1, sizeof(Trust), File);
	bool ReadOK = TrustBytes != 0 && TrustBytes < sizeof(Trust) && !ferror(File); if (fclose(File) != 0 || !ReadOK) goto Done;
	Store = xrtX509StoreCreate(); if (Store == NULL || !xrtX509StoreAddData(Store, (xbytesview){Trust, TrustBytes}, NULL)) goto Done;
	xtlsverifierconfig Verify; xrtTlsVerifierConfigInit(&Verify); Verify.Store = Store;
	xtlsverifylimits Limits; xrtTlsVerifyLimitsInit(&Limits); Verifier = xrtTlsVerifierCreateLimited(&Verify, &Limits); if (Verifier == NULL) goto Done;
	xnetengineconfig Network; xrtNetEngineConfigInit(&Network); Network.Workers = 1;
	Engine = xrtNetEngineCreate(&Network); if (Engine == NULL || !xrtNetEngineStart(Engine)) goto Done;
	Resolver = xrtNetResolverCreate(NULL); if (Resolver == NULL) goto Done;
	xredisclienttlsconfig TLS; xredisClientTlsConfigInit(&TLS); TLS.Session.Verifier = Verifier;
	xredisclientinitconfig Init; xredisClientInitConfigInit(&Init);
	if (argc == 5) {
		cstr Password = getenv(argv[4]); if (Password == NULL) goto Done;
		Init.Flags = XREDIS_INIT_AUTH; Init.Password = (xbytesview){(cbytes)Password, strlen(Password)};
	}
	if (!xredisClientTlsDial(Engine, Resolver, (xstrview){argv[1], strlen(argv[1])}, (uint16)Port, 0, NULL, &Init, &TLS, &Client)) goto Done;
	Ready = xredisClientReady(Client); if (Ready == NULL || xrtFutureWaitFor(Ready, 10000) != XWAIT_OK) goto Done;
	const xredisreadyresult* State = (const xredisreadyresult*)xrtFutureValue(Ready); if (State == NULL || State->Status != XREDIS_READY_OK) goto Done;
	xbytesview Ping = XRT_BYTES_LITERAL("PING"); if (xredisClientCall(Client, &Ping, 1, 5000, &Request) != XREDIS_ADMISSION_ACCEPTED) goto Done;
	const xredisrequestresult* Reply = (const xredisrequestresult*)xrtFutureValue(Request); xredisreplyvalue Value;
	if (Reply == NULL || Reply->Status != XREDIS_REQUEST_OK || !xredisReplyGet(Reply->Reply, 0, &Value) || Value.Data.Size != 4 || memcmp(Value.Data.Data, "PONG", 4)) goto Done;
	puts("[PASS] trusted TLS peer, RESP3 initialization and PONG"); Code = 0;
Done:
	if (Client != NULL) {
		Closed = xredisClientClosed(Client);
		if (!xredisClientClose(Client) || Closed == NULL || xrtFutureWaitFor(Closed, 10000) != XWAIT_OK) Code = 1;
		xredisClientRelease(Client);
	}
	if (Resolver != NULL && !xrtNetResolverDestroy(Resolver)) Code = 1;
	if (Engine != NULL) { if (!xrtNetEngineStop(Engine) || !xrtNetEngineDestroy(Engine)) Code = 1; }
	xrtFutureDestroy(Closed); xrtFutureDestroy(Request); xrtFutureDestroy(Ready); xrtTlsVerifierRelease(Verifier); xrtX509StoreFree(Store);
	if (Code != 0) fputs("TLS example failed; check typed readiness/request status and explicit trust configuration.\n", stderr);
	return Code;
}
