/* Self-contained transaction/ordinary-request ordering example. The local peer
 * is fixture infrastructure. Real Redis interoperability uses the opt-in runner. */
#include <xredis.h>
#include "../../tests/support/transaction_peer.h"
int main(void)
{
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    xredisclient* Client = redisTransactionPeerConnect(&Peer, NULL);
    xbytesview Args[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("transaction\0binary")};
    xredistransactioncommand Commands[] = {{Args, 2}, {Args, 2}}; xfuture* Future = NULL;
    redisPeerRequire(xredisClientTransaction(Client, Commands, 2, NULL, 5000, &Future) == XREDIS_ADMISSION_ACCEPTED, "example admission");
    const xredistransactionresult* Result = xrtFutureValue(Future); xredisreplyvalue Root;
    redisPeerRequire(Result && Result->Execution == XREDIS_TRANSACTION_EXECUTED && Result->Request.Status == XREDIS_REQUEST_OK &&
        xredisReplyGet(Result->Request.Reply, 0, &Root) && Root.ChildCount == 2, "example EXEC outcome");
    redisTransactionPeerStop(&Peer); xrtFutureDestroy(Future);
    puts("[PASS] isolated two-command EXEC, owned result and physical cleanup"); return 0;
}
