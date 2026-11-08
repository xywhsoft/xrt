/* Run from the repository root:
 * python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_watch --no-single
 * The peer is a deterministic local fixture, not an independent Redis oracle.
 * All clients, observers and the engine are closed below. */
#include <xredis.h>
#include "../../tests/support/watch_peer.h"
int main(void)
{
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer.Tx, NULL); redisWatchPeerReady(Client);
    xbytesview Key = XRT_BYTES_LITERAL("example\0binary"); xfuture* Started = NULL;
    redisPeerRequire(xredisClientWatch(Client, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "WATCH example admission");
    const xrediswatchresult* Result = xrtFutureValue(Started); redisPeerRequire(Result && Result->Watch, "WATCH acknowledged");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("read then decide")}; xfuture* Read = NULL;
    redisPeerRequire(xredisWatchCall(Result->Watch, Echo, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisrequestresult*)xrtFutureValue(Read))->Status == XREDIS_REQUEST_OK, "scoped read");
    xredistransactioncommand Command = {Echo, 2}; xfuture* Commit = NULL;
    redisPeerRequire(xredisWatchCommit(Result->Watch, &Command, 1, 5000, &Commit) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredistransactionresult*)xrtFutureValue(Commit))->Execution == XREDIS_TRANSACTION_EXECUTED, "scoped commit");
    xfuture* Closed = xredisWatchClosed(Result->Watch);
    redisPeerRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK && ((const xrediswatchclosedresult*)xrtFutureValue(Closed))->Reusable,
        "WATCH scope physically retired");
    redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(Read); xrtFutureDestroy(Commit); xrtFutureDestroy(Closed);
    puts("[PASS] WATCH / scoped read / sealed commit / reusable physical retirement"); return 0;
}
