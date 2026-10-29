/* Compile against exactly the features shipped by this package. */
#include <xredis.h>
#include <string.h>
#if !defined(XREDIS_FEATURE_XREDIS_COMMAND) && !defined(XREDIS_FEATURE_XREDIS_READER)
#error "package consumer requires the actual selected package feature definitions"
#endif
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
#include "../support/transaction_peer.h"
#elif defined(XREDIS_FEATURE_XREDIS_DIAL)
#include "../support/dial_peer.h"
#elif defined(XREDIS_FEATURE_XREDIS_CLIENT)
#include "../support/client_peer.h"
#endif
#if defined(XREDIS_FEATURE_XREDIS_READER)
static xredisreadaction readReply(const xredisreadevent* Event, ptr User)
{
    size_t* Values = (size_t*)User;
    if (Event->Kind == XREDIS_READ_END) ++*Values;
    return XREDIS_READ_NEXT;
}
#endif
int main(void)
{
#if defined(XREDIS_FEATURE_XREDIS_COMMAND)
    {
        xbytesview Argument = XRT_BYTES_LITERAL("PING"); unsigned char Output[14]; size_t Written = 0;
        if (!xredisCommandWrite(Output, sizeof(Output), &Argument, 1, &Written)) return 1;
        if (Written != sizeof(Output) || memcmp(Output, "*1\r\n$4\r\nPING\r\n", Written)) return 2;
        if (xredisCommandSize(NULL, 1, &Written)) return 3;
        if (xrtErrorKind(xrtGetError()) != XERR_ARGUMENT || strcmp(xrtErrorDomain(xrtGetError()), "xrt.redis")) return 4;
        xrtClearError();
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_READER)
    {
        xredisreader* Reader = NULL; size_t Values = 0, Consumed = 0;
        if (!xredisReaderCreate(NULL, readReply, &Values, &Reader)) return 5;
        const xbytesview Reply = XRT_BYTES_LITERAL("|0\r\n*2\r\n:1\r\n$?\r\n;2\r\nOK\r\n;0\r\n");
        if (xredisReaderFeed(Reader, Reply, &Consumed) != XREDIS_READER_READY || Consumed != Reply.Size || Values != 4) return 6;
        if (!xredisReaderFinish(Reader) || !xredisReaderDestroy(Reader)) return 7;
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_REPLY)
    {
        xredisreplyreader* Owned = NULL; xredisreply* Frame = NULL; size_t Consumed = 0;
        if (!xredisReplyReaderCreate(NULL, &Owned)) return 8;
        if (xredisReplyReaderFeed(Owned, XRT_BYTES_LITERAL("|0\r\n*2\r\n$2\r\nOK\r\n,1.25\r\n"), &Consumed, &Frame) != XREDIS_REPLY_READY || !xredisReplyReaderFinish(Owned)) return 9;
        xredisReplyReaderDestroy(Owned);
        xredisreplyinfo Info = {0}; Info.Size = sizeof(Info); xredisreplyvalue Root, Text; double Number;
        if (!xredisReplyInfo(Frame, &Info) || !xredisReplyGet(Frame, Info.Root, &Root) || Root.Attributes != 0 ||
            !xredisReplyGet(Frame, Root.FirstChild, &Text) || Text.Data.Size != 2 || memcmp(Text.Data.Data, "OK", 2)) return 10;
        if (!xredisReplyDouble(Frame, Text.Next, &Number) || Number != 1.25 || !xredisReplyRetain(Frame)) return 11;
        xredisReplyRelease(Frame); xredisReplyRelease(Frame);
    }
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLIENT)
    xredisclient* Client = NULL;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
    redistransactionpeer Peer; redisTransactionPeerStart(&Peer, TX_NORMAL, false);
    Client = redisTransactionPeerConnect(&Peer, NULL);
#elif defined(XREDIS_FEATURE_XREDIS_DIAL)
    redisdialpeer Peer; redisDialPeerStart(&Peer, 0, 0, 0);
    xredisclientinitconfig Init; xredisclientdialconfig Dial;
    if (!xredisClientInitConfigInit(&Init) || !xredisClientDialConfigInit(&Dial) ||
        !xredisClientDial(Peer.Init.Peer.Engine, Peer.Resolver, XRT_STR_LITERAL("redis.fixture"), Peer.Init.Peer.Address.Port, 0, NULL, &Init, &Dial, &Client)) return 12;
#else
    redispeer Peer; redisPeerStart(&Peer);
    if (!xredisClientConnect(Peer.Engine, &Peer.Address, 0, NULL, &Client)) return 12;
#endif
    xfuture* Ready = xredisClientReady(Client); if (!Ready || xrtFutureWaitFor(Ready, 5000) != XWAIT_OK) return 17;
    const xredisreadyresult* State = xrtFutureValue(Ready); if (!State || State->Status != XREDIS_READY_OK) return 18;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION) || defined(XREDIS_FEATURE_XREDIS_DIAL)
    if (State->Protocol != XREDIS_RESP3 || State->Hello == NULL) return 19;
#endif
    xbytesview Ping = XRT_BYTES_LITERAL("PING"); xfuture* Future = NULL;
    if (xredisClientCall(Client, &Ping, 1, 5000, &Future) != XREDIS_ADMISSION_ACCEPTED) return 13;
    const xredisrequestresult* Result = xrtFutureValue(Future);
    if (!Result || Result->Status != XREDIS_REQUEST_OK || Result->Delivery != XREDIS_REPLIED) return 14;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
    xredistransactionconfig Options; if (!xredisClientTransactionConfigInit(&Options)) return 20;
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("package\0binary")}; xredistransactioncommand Commands[] = {{Echo, 2}, {Echo, 2}};
    xfuture* Transaction = NULL;
    if (xredisClientTransactionAsync(Client, Commands, 2, &Options, 5000, &Transaction) != XREDIS_ADMISSION_ACCEPTED || xrtFutureWaitFor(Transaction, 10000) != XWAIT_OK) return 21;
    const xredistransactionresult* Value = xrtFutureValue(Transaction); xredisreplyvalue Root;
    if (!Value || Value->Request.Status != XREDIS_REQUEST_OK || Value->Execution != XREDIS_TRANSACTION_EXECUTED ||
        !xredisReplyGet(Value->Request.Reply, 0, &Root) || Root.ChildCount != 2) return 22;
    xrtFutureDestroy(Transaction); Transaction = NULL;
    if (xredisClientTransaction(Client, NULL, 0, NULL, 5000, &Transaction) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution != XREDIS_TRANSACTION_EXECUTED) return 23;
    redisTransactionPeerStop(&Peer); xrtFutureDestroy(Transaction);
#else
    xfuture* Closed = xredisClientClosed(Client);
    if (!xredisClientClose(Client) || xrtFutureWaitFor(Closed, 5000) != XWAIT_OK) return 15;
    xredisClientRelease(Client); xrtFutureDestroy(Closed);
#if defined(XREDIS_FEATURE_XREDIS_DIAL)
    redisDialPeerStop(&Peer);
#else
    redisPeerStop(&Peer);
#endif
#endif
    xredisreplyvalue Text;
    if (!xredisReplyGet(Result->Reply, 0, &Text) || Text.Data.Size != 4 || memcmp(Text.Data.Data, "PONG", 4)) return 16;
    if (State->Status != XREDIS_READY_OK) return 19;
    xrtFutureDestroy(Future); xrtFutureDestroy(Ready);
#endif
    return 0;
}
