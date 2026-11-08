#define XREDIS_MODULE_XREDIS_CLIENT
#include <xredis.h>
#include "../../tests/support/client_peer.h"

/* No external service required: a local deterministic peer demonstrates real
 * TCP ownership. It is not Redis interoperability evidence. Every Future,
 * Client, listener and engine is explicitly retired before main returns. */
int main(void)
{
	redispeer Peer; redisPeerStart(&Peer); xredisclient* Client = NULL;
	redisPeerRequire(xredisClientConnect(Peer.Engine, &Peer.Address, 0, NULL, &Client), "client connect");
	xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("hello\0xrt")}; xfuture* Future = NULL;
	redisPeerRequire(xredisClientCall(Client, Args, 2, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "sync call");
	const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Future);
	xredisreplyvalue Value;
	redisPeerRequire(Result != NULL && Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED &&
		xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 9 && memcmp(Value.Data.Data, "hello\0xrt", 9) == 0, "binary response");
	printf("TCP owned response: %zu binary bytes; replied\n", Value.Data.Size);
	xfuture* Closed = xredisClientClosed(Client);
	redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 5000) == XWAIT_OK, "physical close");
	xredisClientRelease(Client); xrtFutureDestroy(Closed); redisPeerStop(&Peer);
	/* Result ownership is independent even after the engine is destroyed. */
	redisPeerRequire(xredisReplyGet(Result->Reply, 0, &Value) && Value.Data.Size == 9, "retained response");
	xrtFutureDestroy(Future); return 0;
}
