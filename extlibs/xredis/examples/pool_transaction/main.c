/* Python 3 + C11 compiler, from repository root:
 * python tools/build.py --compiler gcc --manifest extlibs/xredis/config/modules.json --suite xredis_pool_transaction --no-single
 * Local deterministic peer; independent Redis acceptance uses the opt-in runner.
 * Assert EXEC/lease retirement and release every client/Future/engine below. */
#include <xredis.h>
#include "../../tests/support/pool_sessions_peer.h"
int main(void)
{
    redispoolsessionspeer Peer; redisPoolSessionsPeerStart(&Peer, W_NORMAL, TX_NORMAL, false, NULL);
    xfuture* Acquired = NULL; redisPeerRequire(xredisPoolAcquire(Peer.Pool, 5000, &Acquired) == XREDIS_ADMISSION_ACCEPTED, "example acquire");
    const xredisleaseresult* Lease = xrtFutureValue(Acquired); redisPeerRequire(Lease && Lease->Lease, "example lease");
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("leased\0EXEC")}; xredistransactioncommand Command = {Echo, 2}; xfuture* Transaction = NULL;
    redisPeerRequire(xredisLeaseTransaction(Lease->Lease, &Command, 1, NULL, 5000, &Transaction) == XREDIS_ADMISSION_ACCEPTED &&
        ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution == XREDIS_TRANSACTION_EXECUTED, "example native EXEC result");
    xfuture* Returned = xredisLeaseClosed(Lease->Lease); xredisLeaseReturn(Lease->Lease);
    redisPeerRequire(xrtFutureWaitFor(Returned, 10000) == XWAIT_OK && ((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable, "example physical lease return");
    redisPoolSessionsPeerStop(&Peer); xrtFutureDestroy(Acquired); xrtFutureDestroy(Transaction); xrtFutureDestroy(Returned);
    puts("[PASS] pooled native transaction / explicit physical Return / independent metadata"); return 0;
}
