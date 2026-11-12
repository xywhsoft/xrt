/* Compile against exactly the features shipped by this package. */
#include <xredis.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
#include "../support/pool_connect_peer.h"
static int testPoolConnectPackage(void)
{
    redisconnectpeer Peer; redisConnectPeerStart(&Peer); redisconnectfactory Factory; xredispoolconnectconfig Config;
    redisConnectFactoryConfig(&Factory, &Peer, &Config); Config.RetryMin = Config.RetryMax = 1;
    xredispool* Pool = NULL;
    if (xredisPoolConnect(Peer.Engine, 0, 1, NULL, &Config, &Pool) != XREDIS_ADMISSION_ACCEPTED) return 60;
    for (unsigned Index = 0; Index < 2; ++Index) {
        xfuture *Acquired = NULL, *Call = NULL; xbytesview Ping = XRT_BYTES_LITERAL("PING");
        if (xredisPoolAcquire(Pool, 5000, &Acquired) != XREDIS_ADMISSION_ACCEPTED) return 61;
        const xredisleaseresult* Result = xrtFutureValue(Acquired); if (!Result || !Result->Lease) return 62;
        if (xredisLeaseCall(Result->Lease, &Ping, 1, 5000, &Call) != XREDIS_ADMISSION_ACCEPTED ||
            ((const xredisrequestresult*)xrtFutureValue(Call))->Status != XREDIS_REQUEST_OK) return 63;
        xfuture* Returned = xredisLeaseClosed(Result->Lease); if (!Index) xredisLeaseAbort(Result->Lease); else xredisLeaseReturn(Result->Lease);
        if (xrtFutureWaitFor(Returned, 5000) != XWAIT_OK) return 64;
        xrtFutureDestroy(Acquired); xrtFutureDestroy(Call); xrtFutureDestroy(Returned);
    }
    xredispoolconnectinfo Info = {0}; Info.Size = sizeof(Info); if (!xredisPoolConnectInfo(Pool, &Info) || Info.Connected != 2) return 65;
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool); if (xrtFutureWaitFor(Closed, 5000) != XWAIT_OK ||
        xrtAtomic32Load(&Factory.Drops, XMEMORY_ACQUIRE) != 1) return 66;
    xredisPoolRelease(Pool); xrtFutureDestroy(Closed); redisConnectPeerStop(&Peer); return 0;
}
#endif
#include <string.h>
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION) || defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
#include "../support/pool_sessions_peer.h"
static int testPoolSessionsPackage(void)
{
    redispoolsessionspeer Fixture; redisPoolSessionsPeerStart(&Fixture, W_NORMAL, TX_NORMAL, false, NULL);
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("package-session\0binary")}; xredistransactioncommand Command = {Echo, 2};
    xfuture* Acquired = NULL;
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION)
    if (xredisPoolAcquire(Fixture.Pool, 5000, &Acquired) != XREDIS_ADMISSION_ACCEPTED) return 40;
    const xredisleaseresult* Lease = xrtFutureValue(Acquired); if (!Lease || !Lease->Lease) return 41;
    xfuture* Transaction = NULL;
    if (xredisLeaseTransaction(Lease->Lease, &Command, 1, NULL, 5000, &Transaction) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredistransactionresult*)xrtFutureValue(Transaction))->Execution != XREDIS_TRANSACTION_EXECUTED) return 42;
    xfuture* Returned = xredisLeaseClosed(Lease->Lease); xredisLeaseReturn(Lease->Lease);
    if (xrtFutureWaitFor(Returned, 10000) != XWAIT_OK || !((const xredisleaseclosedresult*)xrtFutureValue(Returned))->Reusable) return 43;
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Transaction); xrtFutureDestroy(Returned); Acquired = NULL;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    if (xredisPoolAcquire(Fixture.Pool, 5000, &Acquired) != XREDIS_ADMISSION_ACCEPTED) return 44;
    const xredisleaseresult* WatchLease = xrtFutureValue(Acquired); if (!WatchLease || !WatchLease->Lease) return 45;
    xbytesview Key = XRT_BYTES_LITERAL("package-watch"); xfuture* Started = NULL;
    if (xredisLeaseWatch(WatchLease->Lease, &Key, 1, NULL, &Started) != XREDIS_ADMISSION_ACCEPTED) return 46;
    const xrediswatchresult* Scope = xrtFutureValue(Started); if (!Scope || Scope->Request.Status != XREDIS_REQUEST_OK) return 47;
    xfuture* Commit = NULL;
    if (xredisWatchCommit(Scope->Watch, &Command, 1, 5000, &Commit) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredistransactionresult*)xrtFutureValue(Commit))->Execution != XREDIS_TRANSACTION_EXECUTED) return 48;
    xfuture* ScopeClosed = xredisWatchClosed(Scope->Watch); if (xrtFutureWaitFor(ScopeClosed, 10000) != XWAIT_OK) return 49;
    xfuture* LeaseClosed = xredisLeaseClosed(WatchLease->Lease); xredisLeaseReturn(WatchLease->Lease);
    if (xrtFutureWaitFor(LeaseClosed, 10000) != XWAIT_OK || !((const xredisleaseclosedresult*)xrtFutureValue(LeaseClosed))->Reusable) return 50;
    xrtFutureDestroy(Acquired); xrtFutureDestroy(Started); xrtFutureDestroy(Commit); xrtFutureDestroy(ScopeClosed); xrtFutureDestroy(LeaseClosed);
#endif
    redisPoolSessionsPeerStop(&Fixture); return 0;
}
#endif
#if !defined(XREDIS_FEATURE_XREDIS_COMMAND) && !defined(XREDIS_FEATURE_XREDIS_READER)
#error "package consumer requires the actual selected package feature definitions"
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL)
#include "../support/pool_peer.h"
static int testPoolPackage(void)
{
    redispoolpeer Group; redisPoolPeerStart(&Group, 2); xredispool* Pool = NULL;
    if (xredisPoolCreate(Group.Peers[0].Engine, 0, Group.Clients, 2, NULL, &Pool) != XREDIS_ADMISSION_ACCEPTED) return 30;
    xfuture *A = NULL, *B = NULL;
    if (xredisPoolAcquire(Pool, 5000, &A) != XREDIS_ADMISSION_ACCEPTED || xredisPoolAcquire(Pool, 5000, &B) != XREDIS_ADMISSION_ACCEPTED) return 31;
    const xredisleaseresult *LA = xrtFutureValue(A), *LB = xrtFutureValue(B);
    if (!LA || !LB || !LA->Lease || !LB->Lease || LA->Lease == LB->Lease) return 32;
    xbytesview Echo[] = {XRT_BYTES_LITERAL("ECHO"), XRT_BYTES_LITERAL("package-pool\0binary")}; xfuture* Call = NULL;
    if (xredisLeaseCall(LA->Lease, Echo, 2, 5000, &Call) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredisrequestresult*)xrtFutureValue(Call))->Status != XREDIS_REQUEST_OK) return 33;
    xfuture *CA = xredisLeaseClosed(LA->Lease), *CB = xredisLeaseClosed(LB->Lease);
    xredisLeaseReturn(LA->Lease); xredisLeaseReturn(LB->Lease);
    if (xrtFutureWaitFor(CA, 10000) != XWAIT_OK || xrtFutureWaitFor(CB, 10000) != XWAIT_OK ||
        !((const xredisleaseclosedresult*)xrtFutureValue(CA))->Reusable || !((const xredisleaseclosedresult*)xrtFutureValue(CB))->Reusable) return 34;
    xfuture* Closed = xredisPoolClosed(Pool); xredisPoolClose(Pool);
    if (xrtFutureWaitFor(Closed, 10000) != XWAIT_OK) return 35;
    xredisPoolRelease(Pool); redisPoolPeerStop(&Group);
    xrtFutureDestroy(A); xrtFutureDestroy(B); xrtFutureDestroy(CA); xrtFutureDestroy(CB); xrtFutureDestroy(Closed); xrtFutureDestroy(Call); return 0;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
#include "../support/watch_peer.h"
#elif defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
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
#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
static int testPubSubPackage(void)
{
    xbytesview Name=XRT_BYTES_LITERAL("package\0channel");size_t Required=0,Written=0;unsigned char Command[96];
    if(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Required) || !xredisPubSubCommandWrite(Command,sizeof(Command),XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Written) || Required!=Written)return 70;
    static const char Wire[]=">3\r\n+message\r\n$3\r\na\0b\r\n$0\r\n\r\n";xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;
    if(!xredisReplyReaderCreate(NULL,&Reader) || xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Wire,sizeof(Wire)-1},&Used,&Reply)!=XREDIS_REPLY_READY || Used!=sizeof(Wire)-1)return 71;
    xredisReplyReaderDestroy(Reader);xredispubsubevent E;
    if(xredisPubSubEventRead(Reply,&E)!=XREDIS_PUBSUB_EVENT || E.Kind!=XREDIS_PUBSUB_MESSAGE || E.Channel.Size!=3 || memcmp(E.Channel.Data,"a\0b",3) || E.Payload.Size)return 72;
    if(xredisPubSubPongRead(Reply,NULL,&E)!=XREDIS_PUBSUB_OTHER)return 73;
    xredisReplyRelease(Reply);return 0;
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
#define main subscriberPackageExample
#include "../../examples/subscriber/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
#define main subscriberNotifyPackageExample
#include "../../examples/subscriber_notify/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
#define main subscriptionsPackageExample
#include "../../examples/subscriptions/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
#define main sessionPackageExample
#include "../../examples/subscription_session/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE)
#define main rolePackageExample
#include "../../examples/role/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
#define main roleClientPackageExample
#include "../../examples/role_client/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
#define main sentinelPackageExample
#include "../../examples/sentinel/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
#define main sentinelPoolPackageExample
#include "../../examples/sentinel_pool/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
#define main clusterTopologyPackageExample
#include "../../examples/cluster_topology/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
#define main poolAskingPackageExample
#include "../../examples/pool_asking/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
#define main sharedPoolPackageExample
#include "../../examples/pool_shared/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
#define main sharedConnectPackageExample
#include "../../examples/pool_shared_connect/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
#define main sharedAskingPackageExample
#include "../../examples/pool_shared_asking/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
#define main clusterRoutePackageExample
#include "../../examples/cluster_route/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
#define main clusterNodesPackageExample
#include "../../examples/cluster_nodes/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
#define main clusterRefreshPackageExample
#include "../../examples/cluster_refresh/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
#define main clusterClientPackageExample
#include "../../examples/cluster_client/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
#define main commandKeysPackageExample
#include "../../examples/command_keys/main.c"
#undef main
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
#define main clusterCommandPackageExample
#include "../../examples/cluster_command/main.c"
#undef main
#endif
int main(void)
{
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_COMMAND)
    if(clusterCommandPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_COMMAND_KEYS)
    if(commandKeysPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_CLIENT)
    if(clusterClientPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_REFRESH)
    if(clusterRefreshPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_NODES)
    if(clusterNodesPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_ROUTE)
    if(clusterRoutePackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_ASKING)
    if(sharedAskingPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED_CONNECT)
    if (sharedConnectPackageExample()) return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_SHARED)
    if (sharedPoolPackageExample()) return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_ASKING)
    if(poolAskingPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_CLUSTER_TOPOLOGY)
    if(clusterTopologyPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL_POOL)
    if(sentinelPoolPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SENTINEL)
    if(sentinelPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE_CLIENT)
    if(roleClientPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_ROLE)
    if(rolePackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTION_SESSION)
    if(sessionPackageExample())return 1;
#endif
#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIPTIONS)
    int SubscriptionsResult=subscriptionsPackageExample();if(SubscriptionsResult)return SubscriptionsResult;
#endif

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER_NOTIFY)
    int NotifyResult=subscriberNotifyPackageExample();if(NotifyResult)return NotifyResult;
#endif

#if defined(XREDIS_FEATURE_XREDIS_SUBSCRIBER)
    int SubscriberResult=subscriberPackageExample();if(SubscriberResult)return SubscriberResult;
#endif

#if defined(XREDIS_FEATURE_XREDIS_PUBSUB)
    int PubSubResult=testPubSubPackage();if(PubSubResult)return PubSubResult;
#endif
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
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
    rediswatchpeer Peer; redisWatchPeerStart(&Peer, W_NORMAL, TX_NORMAL, false);
    Client = redisTransactionPeerConnect(&Peer.Tx, NULL);
#elif defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
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
#if defined(XREDIS_FEATURE_XREDIS_WATCH)
    redisWatchPeerReady(Client); xbytesview Key = XRT_BYTES_LITERAL("package-watch\0key"); xfuture* Started = NULL;
    if (xredisClientWatch(Client, &Key, 1, NULL, &Started) != XREDIS_ADMISSION_ACCEPTED) return 24;
    const xrediswatchresult* Scope = xrtFutureValue(Started); if (!Scope || !Scope->Watch) return 25;
    xfuture* ScopedRead = NULL;
    if (xredisWatchCall(Scope->Watch, Echo, 2, 5000, &ScopedRead) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredisrequestresult*)xrtFutureValue(ScopedRead))->Status != XREDIS_REQUEST_OK) return 26;
    xfuture* ScopedCommit = NULL;
    if (xredisWatchCommit(Scope->Watch, Commands, 2, 5000, &ScopedCommit) != XREDIS_ADMISSION_ACCEPTED ||
        ((const xredistransactionresult*)xrtFutureValue(ScopedCommit))->Execution != XREDIS_TRANSACTION_EXECUTED) return 27;
    xfuture* ScopedClosed = xredisWatchClosed(Scope->Watch);
    if (xrtFutureWaitFor(ScopedClosed, 10000) != XWAIT_OK || !((const xrediswatchclosedresult*)xrtFutureValue(ScopedClosed))->Reusable) return 28;
    redisTransactionPeerStop(&Peer.Tx); xrtFutureDestroy(Started); xrtFutureDestroy(ScopedRead); xrtFutureDestroy(ScopedCommit); xrtFutureDestroy(ScopedClosed);
#else
    redisTransactionPeerStop(&Peer);
#endif
    xrtFutureDestroy(Transaction);
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
#if defined(XREDIS_FEATURE_XREDIS_POOL)
    int PoolResult = testPoolPackage(); if (PoolResult) return PoolResult;
#if defined(XREDIS_FEATURE_XREDIS_POOL_CONNECT)
    int ConnectResult = testPoolConnectPackage(); if (ConnectResult) return ConnectResult;
#endif
#if defined(XREDIS_FEATURE_XREDIS_POOL_TRANSACTION) || defined(XREDIS_FEATURE_XREDIS_POOL_WATCH)
    return testPoolSessionsPackage();
#else
    return 0;
#endif
#else
    return 0;
#endif
}
