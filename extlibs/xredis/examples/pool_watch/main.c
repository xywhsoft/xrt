/* Run: python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_watch --no-single
 * Local C peer, Python 3 and C11 compiler required. Assertions and cleanup below. */
#include <xredis.h>
#include "../../tests/support/pool_sessions_peer.h"
int main(void)
{
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL);
    xfuture* Acquired = NULL; redisPeerRequire(xredisPoolAcquire(Peer.Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "example lease");
    const xredisleaseresult* Lease = xrtFutureValue(Acquired); xbytesview Key = XRT_BYTES_LITERAL("leased-watch"); xfuture* Started = NULL;
    redisPeerRequire(Lease && Lease->Lease && xredisLeaseWatch(Lease->Lease, &Key, 1, NULL, &Started) == XREDIS_ADMISSION_ACCEPTED, "example nested WATCH");
    const xrediswatchresult* Scope = xrtFutureValue(Started); redisPeerRequire(Scope && Scope->Request.Status == XREDIS_REQUEST_OK, "example WATCH ready");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("read\0decide")}; xfuture* Read = NULL;
    redisPeerRequire(xredisWatchCall(Scope->Watch, Echo, 2, 5000, &Read) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredisrequestresult*)xrtFutureValue(Read))->Status == XREDIS_REQUEST_OK, "example scoped read");
    xredistransactioncommand Command = {Echo, 2}; xfuture* Transaction = NULL;
    redisPeerRequire(xredisWatchCommit(Scope->Watch, &Command, 1, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "example nested EXEC");
    xfuture* ScopeClosed = xredisWatchClosed(Scope->Watch); redisPeerRequire(xrtFutureWaitFor(ScopeClosed, 10000) == XWAIT_OK, "example scope barrier");
    xfuture* Returned = xredisLeaseClosed(Lease->Lease); xredisLeaseReturn(Lease->Lease);
    redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "example lease barrier");
    redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(Read);
    xrtFutureDestroy(Transaction); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(Returned);
    puts("[PASS] pooled WATCH / read / EXEC / scope Closed / lease Closed"); return 0;
}
