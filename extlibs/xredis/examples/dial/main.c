#include <xredis.h>
#include "../../tests/support/dial_peer.h"

/* Self-contained deterministic Resolver/TCP fixture; no Redis installation.
 * Select XREDIS_MODULE_XREDIS_DIAL. Application supplies Engine + Resolver. */
int main(void)
{
	for (size_t Mode = 0; Mode < 2; ++Mode) {
		redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0); xredisclient* Client = NULL;
		if (Mode == 0) {
			char Host[] = "redis.fixture";
			redisPeerRequire(xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, (xstrview){Host, sizeof(Host) - 1u},
				Peer.Init.Peer.Address.Port, 0, NULL, NULL, NULL, &Client), "hostname submission");
			memset(Host, 0, sizeof(Host)); /* The factory already owns its snapshot. */
		} else {
			xnetaddr Candidates[] = {Peer.RefusedAddress, Peer.Init.Peer.Address};
			xnetaddrlist* Addresses = xrtNetAddrListCreate(Candidates, 2);
			redisPeerRequire(Addresses != NULL && xredisClientDialAddresses(Peer.Init.Peer.Engine, Addresses,
				0, NULL, NULL, NULL, &Client), "prepared endpoints"); xrtNetAddrListDestroy(Addresses);
		}
		xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("binary\0value")}; xfuture* Reply = NULL;
		redisPeerRequire(xredisClientCallAsync(Client, Args, 2, 5000, &Reply) == XREDIS_ADMISSION_ACCEPTED, "queue before ready");
		xfuture* Ready = redisDialReady(Client); const xredisreadyresult* State = (const xredisreadyresult*)xrtFutureValue(Ready);
		redisPeerRequire(State != NULL && State->Status == XREDIS_READY_OK && State->Protocol == XREDIS_RESP3, "HELLO readiness");
		redisPeerRequire(xrtFutureWaitFor(Reply, 10000) == XWAIT_OK, "binary echo wait");
		const xredisrequestresult* Result = (const xredisrequestresult*)xrtFutureValue(Reply); xredisreplyvalue Value;
		redisPeerRequire(Result != NULL && Result->Status == XREDIS_REQUEST_OK && xredisReplyGet(Result->Reply, 0, &Value) &&
			Value.Data.Size == Args[1].Size && memcmp(Value.Data.Data, Args[1].Data, Value.Data.Size) == 0, "binary echo");
		redisDialClientClose(Client); redisDialPeerStop(&Peer);
		redisPeerRequire(State->Hello != NULL && Value.Data.Size == Args[1].Size, "retained results after service teardown");
		xrtFutureDestroy(Reply); xrtFutureDestroy(Ready);
	}
	puts("[PASS] xredis hostname/prepared-address fallback, queued binary call, physical close and independent Resolver join"); return 0;
}
