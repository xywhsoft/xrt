#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../xrt.h"

#define INTEROP_PROTOCOL "xrt.interop"
#define INTEROP_ORIGIN "https://interop.test"
#define INTEROP_TIMEOUT_MS 10000u

typedef struct {
	volatile long iOpen;
	volatile long iText;
	volatile long iBinary;
	volatile long iPing;
	volatile long iPong;
	volatile long iClose;
	volatile long iError;
	volatile long iSendOk;
	volatile long iMetadataOk;
	uint16 iRemoteCloseCode;
	char sRemoteCloseReason[XWS_CLOSE_REASON_CAP + 1u];
	char sExpectedText[2048];
	size_t iExpectedTextLen;
	uint8 aExpectedBinary[64];
	size_t iExpectedBinaryLen;
	xwserrorinfo tError;
} interop_ctx;

static long atomic_load(volatile long* value)
{
	return __xrtAtomicCompareExchange32(value, 0, 0);
}

static void atomic_set(volatile long* value, long next)
{
	(void)__xrtAtomicExchange32(value, next);
}

static void atomic_inc(volatile long* value)
{
	(void)__xrtAtomicAddFetch32(value, 1);
}

static bool wait_for(volatile long* value, long minimum, uint32 timeout_ms)
{
	uint32 elapsed = 0u;
	while ( elapsed < timeout_ms ) {
		if ( atomic_load(value) >= minimum ) { return true; }
		xrtSleep(10u);
		elapsed += 10u;
	}
	return atomic_load(value) >= minimum;
}

static bool fill_repeated(char* output, size_t capacity, size_t* output_len,
	const char* unit, size_t count)
{
	size_t unit_len = strlen(unit);
	if ( unit_len == 0u || count > (capacity - 1u) / unit_len ) { return false; }
	for ( size_t i = 0u; i < count; ++i ) {
		memcpy(output + (i * unit_len), unit, unit_len);
	}
	*output_len = unit_len * count;
	output[*output_len] = '\0';
	return true;
}

static void record_close(interop_ctx* ctx, const xwscloseinfo* info)
{
	size_t copy_len;
	if ( !ctx || !info ) { return; }
	ctx->iRemoteCloseCode = info->iRemoteCode;
	copy_len = info->iRemoteReasonLen < XWS_CLOSE_REASON_CAP ?
		info->iRemoteReasonLen : XWS_CLOSE_REASON_CAP;
	if ( copy_len > 0u && info->sRemoteReason ) {
		memcpy(ctx->sRemoteCloseReason, info->sRemoteReason, copy_len);
	}
	ctx->sRemoteCloseReason[copy_len] = '\0';
	atomic_inc(&ctx->iClose);
}

static void record_error(interop_ctx* ctx, const xwserrorinfo* info)
{
	if ( !ctx ) { return; }
	if ( info ) { ctx->tError = *info; }
	atomic_inc(&ctx->iError);
}

static void server_on_open(ptr owner, xwsserver* server, xwsconn* conn)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)server;
	if ( ctx && conn && strcmp(xrtWsConnProtocol(conn), INTEROP_PROTOCOL) == 0 &&
		xrtWsConnPerMessageDeflate(conn) ) {
		atomic_set(&ctx->iMetadataOk, 1);
	}
	if ( ctx ) { atomic_inc(&ctx->iOpen); }
}

static void server_on_text(ptr owner, xwsserver* server, xwsconn* conn,
	const char* data, size_t len)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)server;
	if ( !ctx || !conn || !data ) { return; }
	if ( len == ctx->iExpectedTextLen &&
		memcmp(data, ctx->sExpectedText, len) == 0 ) {
		atomic_inc(&ctx->iText);
	}
	if ( xrtWsConnSendText(conn, data, len) != XRT_NET_OK ) {
		atomic_set(&ctx->iSendOk, 0);
	}
}

static void server_on_binary(ptr owner, xwsserver* server, xwsconn* conn,
	const void* data, size_t len)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)server;
	if ( !ctx || !conn || (!data && len != 0u) ) { return; }
	if ( len == ctx->iExpectedBinaryLen &&
		memcmp(data, ctx->aExpectedBinary, len) == 0 ) {
		atomic_inc(&ctx->iBinary);
	}
	if ( xrtWsConnSendBinary(conn, data, len) != XRT_NET_OK ) {
		atomic_set(&ctx->iSendOk, 0);
	}
}

static void server_on_ping(ptr owner, xwsserver* server, xwsconn* conn,
	const void* data, size_t len)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	static const char expected[] = "python-ping";
	(void)server;
	(void)conn;
	if ( ctx && len == sizeof(expected) - 1u &&
		memcmp(data, expected, sizeof(expected) - 1u) == 0 ) {
		atomic_inc(&ctx->iPing);
	}
}

static void server_on_close(ptr owner, xwsserver* server, xwsconn* conn,
	const xwscloseinfo* info)
{
	(void)server;
	(void)conn;
	record_close((interop_ctx*)owner, info);
}

static void server_on_error(ptr owner, xwsserver* server, xwsconn* conn,
	const xwserrorinfo* info)
{
	(void)server;
	(void)conn;
	record_error((interop_ctx*)owner, info);
}

static int run_server(uint16 port)
{
	static const uint8 expected_binary[] = {0x00u, 0x01u, 'p', 'y', 't', 'h', 'o', 'n', 0xffu};
	interop_ctx ctx;
	xnetengineconfig engine_cfg;
	xwsserverconfig server_cfg;
	xwsserverevents events;
	xnetengine* engine = NULL;
	xwsserver* server = NULL;
	bool ok = false;

	memset(&ctx, 0, sizeof(ctx));
	atomic_set(&ctx.iSendOk, 1);
	if ( !fill_repeated(ctx.sExpectedText, sizeof(ctx.sExpectedText),
		&ctx.iExpectedTextLen, "python-client-", 64u) ) { return 2; }
	memcpy(ctx.aExpectedBinary, expected_binary, sizeof(expected_binary));
	ctx.iExpectedBinaryLen = sizeof(expected_binary);

	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 1u;
	engine = xrtNetEngineCreate(&engine_cfg);
	if ( !engine || xrtNetEngineStart(engine) != XRT_NET_OK ) { goto cleanup; }

	xrtWsServerConfigInit(&server_cfg);
	(void)xrtNetAddrParse(&server_cfg.tBindAddr, "127.0.0.1", port);
	server_cfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
	server_cfg.iCompressMinBytes = 1u;
	(void)xrtWsServerConfigSetProtocol(&server_cfg, INTEROP_PROTOCOL);
	(void)xrtWsServerConfigSetPath(&server_cfg, "/interop");

	xrtWsServerEventsInit(&events);
	events.OnOpen = server_on_open;
	events.OnText = server_on_text;
	events.OnBinary = server_on_binary;
	events.OnPing = server_on_ping;
	events.OnCloseEx = server_on_close;
	events.OnErrorEx = server_on_error;
	server = xrtWsServerCreate(engine, &server_cfg, &events, &ctx);
	xrtWsServerConfigUnit(&server_cfg);
	if ( !server || xrtWsServerStart(server) != XRT_NET_OK ) { goto cleanup; }

	(void)wait_for(&ctx.iClose, 1, INTEROP_TIMEOUT_MS);
	ok = atomic_load(&ctx.iOpen) == 1 && atomic_load(&ctx.iText) == 1 &&
		atomic_load(&ctx.iBinary) == 1 && atomic_load(&ctx.iPing) == 1 &&
		atomic_load(&ctx.iClose) == 1 && atomic_load(&ctx.iError) == 0 &&
		atomic_load(&ctx.iSendOk) == 1 && atomic_load(&ctx.iMetadataOk) == 1 &&
		ctx.iRemoteCloseCode == XWS_CLOSE_NORMAL &&
		strcmp(ctx.sRemoteCloseReason, "python-client-done") == 0;

cleanup:
	if ( server ) { xrtWsServerDestroy(server); }
	if ( engine ) {
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
	}
	if ( !ok && atomic_load(&ctx.iError) > 0 ) {
		fprintf(stderr, "server error: category=%u operation=%u phase=%u message=%s\n",
			(unsigned)ctx.tError.iCategory, (unsigned)ctx.tError.iOperation,
			(unsigned)ctx.tError.iPhase, ctx.tError.sMessage);
	}
	return ok ? 0 : 1;
}

static void client_on_open(ptr owner, xwsclient* client)
{
	static const char ping[] = "xrt-ping";
	interop_ctx* ctx = (interop_ctx*)owner;
	bool metadata_ok;
	bool send_ok;
	if ( !ctx || !client ) { return; }
	metadata_ok = strcmp(xrtWsClientProtocol(client), INTEROP_PROTOCOL) == 0 &&
		xrtWsClientPerMessageDeflate(client);
	send_ok = xrtWsClientSendText(client, ctx->sExpectedText,
		ctx->iExpectedTextLen) == XRT_NET_OK &&
		xrtWsClientSendBinary(client, ctx->aExpectedBinary,
		ctx->iExpectedBinaryLen) == XRT_NET_OK &&
		xrtWsClientPing(client, ping, sizeof(ping) - 1u) == XRT_NET_OK;
	atomic_set(&ctx->iMetadataOk, metadata_ok ? 1 : 0);
	atomic_set(&ctx->iSendOk, send_ok ? 1 : 0);
	atomic_inc(&ctx->iOpen);
}

static void client_on_text(ptr owner, xwsclient* client,
	const char* data, size_t len)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)client;
	if ( ctx && data && len == ctx->iExpectedTextLen &&
		memcmp(data, ctx->sExpectedText, len) == 0 ) {
		atomic_inc(&ctx->iText);
	}
}

static void client_on_binary(ptr owner, xwsclient* client,
	const void* data, size_t len)
{
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)client;
	if ( ctx && data && len == ctx->iExpectedBinaryLen &&
		memcmp(data, ctx->aExpectedBinary, len) == 0 ) {
		atomic_inc(&ctx->iBinary);
	}
}

static void client_on_pong(ptr owner, xwsclient* client,
	const void* data, size_t len)
{
	static const char expected[] = "xrt-ping";
	interop_ctx* ctx = (interop_ctx*)owner;
	(void)client;
	if ( ctx && len == sizeof(expected) - 1u &&
		memcmp(data, expected, sizeof(expected) - 1u) == 0 ) {
		atomic_inc(&ctx->iPong);
	}
}

static void client_on_close(ptr owner, xwsclient* client,
	const xwscloseinfo* info)
{
	(void)client;
	record_close((interop_ctx*)owner, info);
}

static void client_on_error(ptr owner, xwsclient* client,
	const xwserrorinfo* info)
{
	(void)client;
	record_error((interop_ctx*)owner, info);
}

static int run_client(uint16 port)
{
	static const uint8 expected_binary[] = {0x10u, 'x', 'r', 't', 0x00u, 0xffu};
	interop_ctx ctx;
	xnetengineconfig engine_cfg;
	xwsclientconfig client_cfg;
	xwsclientevents events;
	xnetengine* engine = NULL;
	xwsclient* client = NULL;
	xnetfuture* open_future = NULL;
	char url[128];
	bool ok = false;

	memset(&ctx, 0, sizeof(ctx));
	if ( !fill_repeated(ctx.sExpectedText, sizeof(ctx.sExpectedText),
		&ctx.iExpectedTextLen, "xrt-client-", 64u) ) { return 2; }
	memcpy(ctx.aExpectedBinary, expected_binary, sizeof(expected_binary));
	ctx.iExpectedBinaryLen = sizeof(expected_binary);

	xrtNetEngineConfigInit(&engine_cfg);
	engine_cfg.iWorkerCount = 1u;
	engine = xrtNetEngineCreate(&engine_cfg);
	if ( !engine || xrtNetEngineStart(engine) != XRT_NET_OK ) { goto cleanup; }

	xrtWsClientConfigInit(&client_cfg);
	(void)snprintf(url, sizeof(url), "ws://127.0.0.1:%u/interop?peer=xrt",
		(unsigned)port);
	client_cfg.iWebSocketFlags = XWS_F_PERMESSAGE_DEFLATE;
	client_cfg.iCompressMinBytes = 1u;
	(void)xrtWsClientConfigSetURL(&client_cfg, url);
	(void)xrtWsClientConfigSetOrigin(&client_cfg, INTEROP_ORIGIN);
	(void)xrtWsClientConfigSetProtocols(&client_cfg, "other, " INTEROP_PROTOCOL);
	(void)xrtWsClientConfigSetHeader(&client_cfg, "X-Interop", "xrt-client");

	xrtWsClientEventsInit(&events);
	events.OnOpen = client_on_open;
	events.OnText = client_on_text;
	events.OnBinary = client_on_binary;
	events.OnPong = client_on_pong;
	events.OnCloseEx = client_on_close;
	events.OnErrorEx = client_on_error;
	client = xrtWsClientCreate(engine, &client_cfg, &events, &ctx);
	xrtWsClientConfigUnit(&client_cfg);
	open_future = client ? xrtWsClientStartFuture(client) : NULL;
	if ( !open_future || xrtNetFutureWait(open_future, 5000u) != XRT_NET_OK ) {
		goto cleanup;
	}
	xrtNetFutureDestroy(open_future);
	open_future = NULL;
	(void)wait_for(&ctx.iClose, 1, INTEROP_TIMEOUT_MS);
	ok = atomic_load(&ctx.iOpen) == 1 && atomic_load(&ctx.iText) == 1 &&
		atomic_load(&ctx.iBinary) == 1 && atomic_load(&ctx.iPong) == 1 &&
		atomic_load(&ctx.iClose) == 1 && atomic_load(&ctx.iError) == 0 &&
		atomic_load(&ctx.iSendOk) == 1 && atomic_load(&ctx.iMetadataOk) == 1 &&
		ctx.iRemoteCloseCode == XWS_CLOSE_NORMAL &&
		strcmp(ctx.sRemoteCloseReason, "python-server-done") == 0;

cleanup:
	if ( open_future ) { xrtNetFutureDestroy(open_future); }
	if ( client ) { xrtWsClientDestroy(client); }
	if ( engine ) {
		xrtNetEngineStop(engine);
		xrtNetEngineDestroy(engine);
	}
	if ( !ok && atomic_load(&ctx.iError) > 0 ) {
		fprintf(stderr, "client error: category=%u operation=%u phase=%u message=%s\n",
			(unsigned)ctx.tError.iCategory, (unsigned)ctx.tError.iOperation,
			(unsigned)ctx.tError.iPhase, ctx.tError.sMessage);
	}
	return ok ? 0 : 1;
}

int main(int argc, char** argv)
{
	unsigned long port_value;
	char* end = NULL;
	int result;

	if ( argc != 3 || (strcmp(argv[1], "server") != 0 &&
		strcmp(argv[1], "client") != 0) ) {
		fprintf(stderr, "usage: %s server|client port\n", argc > 0 ? argv[0] : "interop_xws_peer");
		return 2;
	}
	port_value = strtoul(argv[2], &end, 10);
	if ( !end || *end != '\0' || port_value == 0u || port_value > 65535u ) {
		fprintf(stderr, "invalid port\n");
		return 2;
	}
	if ( !xrtInit() ) { return 2; }
	result = strcmp(argv[1], "server") == 0 ?
		run_server((uint16)port_value) : run_client((uint16)port_value);
	xrtUnit();
	return result;
}
