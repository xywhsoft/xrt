#include <xredis.h>
#include "../../tests/support/client_init_peer.h"

/* Self-contained TCP fixture, not a real Redis deployment. These literal
 * credentials are public test data. Select XREDIS_MODULE_XREDIS_CLIENT. */
int main(void)
{
	const redisinitpeerstep Steps[] = {
		{{XRT_BYTES_LITERAL("HELLO"), XRT_BYTES_LITERAL("3"), XRT_BYTES_LITERAL("AUTH"), XRT_BYTES_LITERAL("default"),
		  XRT_BYTES_LITERAL("fixture"), XRT_BYTES_LITERAL("SETNAME"), XRT_BYTES_LITERAL("example")}, 7, XRT_BYTES_LITERAL(REDIS_INIT_HELLO_REPLY), false, false},
		{{XRT_BYTES_LITERAL("SELECT"), XRT_BYTES_LITERAL("2")}, 2, XRT_BYTES_LITERAL("+OK\r\n"), false, false}};
	redisinitpeer Peer; redisInitPeerStart(&Peer, Steps, 2);
	xredisclientinitconfig Init; xredisClientInitConfigInit(&Init); unsigned char Password[] = "fixture";
	Init.Flags = XREDIS_INIT_AUTH | XREDIS_INIT_SETNAME | XREDIS_INIT_SELECT;
	Init.Password = (xbytesview){Password, sizeof(Password) - 1u}; Init.Name = (xbytesview)XRT_BYTES_LITERAL("example"); Init.Database = 2;
	xredisclient* Client = NULL;
	redisPeerRequire(xredisClientConnectInit(Peer.Peer.Engine, &Peer.Peer.Address, 0, NULL, &Init, &Client), "managed connect");
	xrtSecureZero(Password, sizeof(Password)); /* Caller keeps no borrowed credential requirement. */
	xfuture* Ready = xredisClientReady(Client), *Request = NULL; xbytesview Ping = XRT_BYTES_LITERAL("PING");
	redisPeerRequire(xredisClientCallAsync(Client, &Ping, 1, 5000, &Request) == XREDIS_ADMISSION_ACCEPTED, "queue during setup");
	redisPeerRequire(xrtFutureWaitFor(Ready, 10000) == XWAIT_OK, "initialization wait");
	const xredisreadyresult* State = (const xredisreadyresult*)xrtFutureValue(Ready);
	redisPeerRequire(State != NULL && State->Status == XREDIS_READY_OK && State->Protocol == XREDIS_RESP3 && State->Hello != NULL, "negotiated state");
	redisPeerRequire(xrtFutureWaitFor(Request, 10000) == XWAIT_OK, "business wait");
	const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Request);
	redisPeerRequire(Result != NULL && Result->Status == XREDIS_REQUEST_OK && Result->Delivery == XREDIS_REPLIED, "business outcome");
	xfuture* Closed = xredisClientClosed(Client);
	redisPeerRequire(xredisClientClose(Client) && xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "physical retirement");
	xredisClientRelease(Client); xrtFutureDestroy(Request); xrtFutureDestroy(Closed); redisPeerStop(&Peer.Peer);
	redisPeerRequire(State->Status == XREDIS_READY_OK && State->Hello != NULL, "readiness metadata survives engine"); xrtFutureDestroy(Ready);
	puts("[PASS] managed RESP3 AUTH/name/database barrier, borrowed credential cleanup, result lifetime"); return 0;
}
