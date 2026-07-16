#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../xrt.h"

#define STRESS_TIMEOUT_MS 5000u
#define STRESS_PROTOCOL "xrt.stress"

typedef struct {
	volatile long iOpen;
	volatile long iText;
	volatile long iClose;
	volatile long iError;
	volatile long iLimitError;
	volatile long iMetadata;
	xwserrorinfo tLastError;
} stress_server_ctx;

typedef struct {
	volatile long iText;
	volatile long iClose;
	volatile long iError;
	uint16 iRemoteCloseCode;
	char sExpected[256];
	size_t iExpectedLen;
	xwserrorinfo tLastError;
} stress_client_ctx;

static long stress_load(volatile long* value)
{
	return __xrtAtomicCompareExchange32(value, 0, 0);
}

static void stress_inc(volatile long* value)
{
	(void)__xrtAtomicAddFetch32(value, 1);
}

static bool stress_wait(volatile long* value, long expected, uint32 timeout_ms)
{
	uint32 elapsed = 0u;
	while ( elapsed < timeout_ms ) {
		if ( stress_load(value) >= expected ) { return true; }
		xrtSleep(5u);
		elapsed += 5u;
	}
	return stress_load(value) >= expected;
}

static void stress_server_open(ptr owner, xwsserver* server, xwsconn* conn)
{
	stress_server_ctx* ctx = (stress_server_ctx*)owner;
	(void)server;
	if ( ctx && conn && strcmp(xrtWsConnProtocol(conn), STRESS_PROTOCOL) == 0 &&
		xrtWsConnPerMessageDeflate(conn) ) {
		stress_inc(&ctx->iMetadata);
	}
	if ( ctx ) { stress_inc(&ctx->iOpen); }
}

static void stress_server_text(ptr owner, xwsserver* server, xwsconn* conn,
	const char* data, size_t len)
{
	stress_server_ctx* ctx = (stress_server_ctx*)owner;
	(void)server;
	if ( ctx ) { stress_inc(&ctx->iText); }
	if ( conn && data ) { (void)xrtWsConnSendText(conn, data, len); }
}

static void stress_server_close(ptr owner, xwsserver* server, xwsconn* conn,
	const xwscloseinfo* info)
{
	stress_server_ctx* ctx = (stress_server_ctx*)owner;
	(void)server;
	(void)conn;
	(void)info;
	if ( ctx ) { stress_inc(&ctx->iClose); }
}

static void stress_server_error(ptr owner, xwsserver* server, xwsconn* conn,
	const xwserrorinfo* info)
{
	stress_server_ctx* ctx = (stress_server_ctx*)owner;
	(void)server;
	(void)conn;
	if ( !ctx ) { return; }
	if ( info ) { ctx->tLastError = *info; }
	if ( info && info->iCategory == XWS_ERROR_CATEGORY_LIMIT &&
		info->iOperation == XWS_ERROR_OP_RECEIVE &&
		info->iCloseCode == XWS_CLOSE_TOO_BIG ) {
		stress_inc(&ctx->iLimitError);
	} else {
		stress_inc(&ctx->iError);
	}
}

static void stress_client_text(ptr owner, xwsclient* client,
	const char* data, size_t len)
{
	stress_client_ctx* ctx = (stress_client_ctx*)owner;
	(void)client;
	if ( ctx && data && len == ctx->iExpectedLen &&
		memcmp(data, ctx->sExpected, len) == 0 ) {
		stress_inc(&ctx->iText);
	}
}

static void stress_client_close(ptr owner, xwsclient* client,
	const xwscloseinfo* info)
{
	stress_client_ctx* ctx = (stress_client_ctx*)owner;
	(void)client;
	if ( ctx ) {
		ctx->iRemoteCloseCode = info ? info->iRemoteCode : 0u;
		stress_inc(&ctx->iClose);
	}
}

static void stress_client_error(ptr owner, xwsclient* client,
	const xwserrorinfo* info)
{
	stress_client_ctx* ctx = (stress_client_ctx*)owner;
	(void)client;
	if ( !ctx ) { return; }
	if ( info ) { ctx->tLastError = *info; }
	stress_inc(&ctx->iError);
}

static void print_error(const char* side, const xwserrorinfo* error)
{
	if ( !error ) { return; }
	fprintf(stderr, "%s error: result=%d category=%u operation=%u phase=%u message=%s\n",
		side, (int)error->iResult, (unsigned)error->iCategory,
		(unsigned)error->iOperation, (unsigned)error->iPhase, error->sMessage);
}

static int run_stress(uint32 rounds)
{
	stress_server_ctx server_ctx;
	xnetengineconfig engine_cfg;
	xwsserverconfig server_cfg;
	xwsserverevents server_events;
	xwsclientevents client_events;
	xnetengine* server_engine = NULL;
	xnetengine* client_engine = NULL;
	xwsserver* server = NULL;
	char url[256];
	uint32 completed = 0u;
	int result = 1;

	memset(&server_ctx, 0, sizeof(server_ctx));
	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 2u;
	server_engine = xrtNetEngineCreate(&engine_cfg);
	client_engine = xrtNetEngineCreate(&engine_cfg);
	if ( !server_engine || !client_engine ||
		xrtNetEngineStart(server_engine) != XRT_NET_OK ||
		xrtNetEngineStart(client_engine) != XRT_NET_OK ) {
		fprintf(stderr, "failed to start stress engines\n");
		goto cleanup;
	}

	xrtWsServerConfigInit(&server_cfg);
	(void)xrtNetAddrParse(&server_cfg.tBindAddr, "127.0.0.1", 0u);
	server_cfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
	server_cfg.iCompressMinBytes = 1u;
	server_cfg.iCloseTimeoutMs = 1000u;
	server_cfg.iMaxFrameBytes = 1024u;
	(void)xrtWsServerConfigSetProtocol(&server_cfg, STRESS_PROTOCOL);
	(void)xrtWsServerConfigSetPath(&server_cfg, "/stress");
	xrtWsServerEventsInit(&server_events);
	server_events.OnOpen = stress_server_open;
	server_events.OnText = stress_server_text;
	server_events.OnCloseEx = stress_server_close;
	server_events.OnErrorEx = stress_server_error;
	server = xrtWsServerCreate(server_engine, &server_cfg, &server_events, &server_ctx);
	xrtWsServerConfigUnit(&server_cfg);
	if ( !server || xrtWsServerStart(server) != XRT_NET_OK ||
		xrtWsServerBoundPort(server) == 0u ) {
		fprintf(stderr, "failed to start stress server\n");
		goto cleanup;
	}
	(void)snprintf(url, sizeof(url), "ws://127.0.0.1:%u/stress",
		(unsigned)xrtWsServerBoundPort(server));

	xrtWsClientEventsInit(&client_events);
	client_events.OnText = stress_client_text;
	client_events.OnCloseEx = stress_client_close;
	client_events.OnErrorEx = stress_client_error;

	for ( uint32 round = 0u; round < rounds; ++round ) {
		uint8 oversized[1536];
		stress_client_ctx client_ctx;
		xwsclientconfig client_cfg;
		xwsclient* client = NULL;
		xnetfuture* open_future = NULL;
		xnetfuture* close_future = NULL;
		xnet_result close_result;
		uint32 expected_limit = (round + 1u) / 10u;
		uint32 expected_text = (round + 1u) - expected_limit;
		bool limit_round = ((round + 1u) % 10u) == 0u;
		bool round_ok;

		memset(&client_ctx, 0, sizeof(client_ctx));
		client_ctx.iExpectedLen = (size_t)snprintf(client_ctx.sExpected,
			sizeof(client_ctx.sExpected), "round-%u-xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx",
			(unsigned)round);
		xrtWsClientConfigInit(&client_cfg);
		client_cfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
		client_cfg.iCompressMinBytes = 1u;
		client_cfg.iCloseTimeoutMs = 1000u;
		(void)xrtWsClientConfigSetURL(&client_cfg, url);
		(void)xrtWsClientConfigSetProtocols(&client_cfg, STRESS_PROTOCOL);
		client = xrtWsClientCreate(client_engine, &client_cfg,
			&client_events, &client_ctx);
		xrtWsClientConfigUnit(&client_cfg);
		open_future = client ? xrtWsClientStartFuture(client) : NULL;
		round_ok = open_future &&
			xrtNetFutureWait(open_future, STRESS_TIMEOUT_MS) == XRT_NET_OK &&
			stress_wait(&server_ctx.iOpen, (long)round + 1, STRESS_TIMEOUT_MS) &&
			stress_wait(&server_ctx.iMetadata, (long)round + 1, STRESS_TIMEOUT_MS) &&
			xrtWsClientPerMessageDeflate(client);
		if ( open_future ) { xrtNetFutureDestroy(open_future); }
		close_future = client ? xrtWsClientCloseFuture(client) : NULL;
		if ( limit_round ) {
			uint32 state = round + 1u;
			for ( size_t i = 0u; i < sizeof(oversized); ++i ) {
				state ^= state << 13u;
				state ^= state >> 17u;
				state ^= state << 5u;
				oversized[i] = (uint8)(state >> 24u);
			}
			close_result = client ? xrtWsClientSendBinary(client,
				oversized, sizeof(oversized)) : XRT_NET_ERROR;
			round_ok = round_ok && close_result == XRT_NET_OK;
		} else {
			round_ok = round_ok && client &&
				xrtWsClientSendText(client, client_ctx.sExpected,
					client_ctx.iExpectedLen) == XRT_NET_OK &&
				stress_wait(&client_ctx.iText, 1, STRESS_TIMEOUT_MS) &&
				stress_wait(&server_ctx.iText, (long)expected_text, STRESS_TIMEOUT_MS);
			close_result = client ? xrtWsClientClose(client, XWS_CLOSE_NORMAL,
				"stress") : XRT_NET_ERROR;
			round_ok = round_ok && close_result == XRT_NET_OK;
		}
		round_ok = round_ok && close_future &&
			xrtNetFutureWait(close_future, STRESS_TIMEOUT_MS) == XRT_NET_CLOSED &&
			stress_wait(&client_ctx.iClose, 1, STRESS_TIMEOUT_MS) &&
			stress_wait(&server_ctx.iClose, (long)round + 1, STRESS_TIMEOUT_MS) &&
			stress_wait(&server_ctx.iLimitError, (long)expected_limit, STRESS_TIMEOUT_MS) &&
			(!limit_round || client_ctx.iRemoteCloseCode == XWS_CLOSE_TOO_BIG) &&
			stress_load(&client_ctx.iError) == 0 &&
			stress_load(&server_ctx.iError) == 0;
		if ( close_future ) { xrtNetFutureDestroy(close_future); }
		if ( client ) { xrtWsClientDestroy(client); }
		if ( !round_ok ) {
			fprintf(stderr,
				"reconnect failed at round %u: limit=%d server(open=%ld text=%ld close=%ld metadata=%ld limit_errors=%ld errors=%ld) client(text=%ld close=%ld remote=%u errors=%ld)\n",
				(unsigned)round, limit_round ? 1 : 0,
				stress_load(&server_ctx.iOpen),
				stress_load(&server_ctx.iText), stress_load(&server_ctx.iClose),
				stress_load(&server_ctx.iMetadata), stress_load(&server_ctx.iLimitError),
				stress_load(&server_ctx.iError),
				stress_load(&client_ctx.iText), stress_load(&client_ctx.iClose),
				(unsigned)client_ctx.iRemoteCloseCode,
				stress_load(&client_ctx.iError));
			if ( stress_load(&server_ctx.iError) > 0 ) {
				print_error("server", &server_ctx.tLastError);
			}
			if ( stress_load(&client_ctx.iError) > 0 ) {
				print_error("client", &client_ctx.tLastError);
			}
			goto cleanup;
		}
		completed++;
		if ( completed % 100u == 0u ) {
			printf("completed %u reconnect rounds\n", (unsigned)completed);
			fflush(stdout);
		}
	}
	result = 0;

cleanup:
	if ( server ) { xrtWsServerDestroy(server); }
	if ( client_engine ) {
		xrtNetEngineStop(client_engine);
		xrtNetEngineDestroy(client_engine);
	}
	if ( server_engine ) {
		xrtNetEngineStop(server_engine);
		xrtNetEngineDestroy(server_engine);
	}
	if ( result == 0 ) {
		printf("WebSocket reconnect stress: PASS (%u rounds)\n", (unsigned)completed);
	}
	return result;
}

int main(int argc, char** argv)
{
	uint32 rounds = 500u;
	if ( argc > 1 ) {
		char* end = NULL;
		unsigned long value = strtoul(argv[1], &end, 10);
		if ( !end || *end != '\0' || value == 0u || value > 100000u ) {
			fprintf(stderr, "invalid round count\n");
			return 2;
		}
		rounds = (uint32)value;
	}
	if ( !xrtInit() ) { return 2; }
	{
		int result = run_stress(rounds);
		xrtUnit();
		return result;
	}
}
