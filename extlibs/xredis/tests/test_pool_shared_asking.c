#include "test.h"
#include "support/pool_asking_peer.h"
static const xredisaskingresult* saWait(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F, 15000) == XWAIT_OK && xrtFutureValue(F), "shared ASKING typed wait"); return xrtFutureValue(F);}
static xfuture* saSend(redisaskingpeer* P, int64 Timeout)
{
    unsigned char Bytes[] = {'a', 0, 0xff}; xbytesview A[] = {XRT_BYTES_LITERAL("APPLY"), {Bytes, sizeof(Bytes)}}; xfuture* F = NULL;
    testRequire(xredisSharedPoolAskingCallAsync(P->Pool, A, 2, Timeout, &F) == XREDIS_ADMISSION_ACCEPTED, "shared ASKING admission"); memset(Bytes, 0xcc, sizeof(Bytes)); return F;
}
static void saReply(const xredisaskingresult* R)
{
    xredisreplyvalue V; testRequire(R->Query.Status == XREDIS_REQUEST_OK && R->Query.Delivery == XREDIS_REPLIED && R->ControlReceived && R->Control.Status == XREDIS_REQUEST_OK, "shared separate outcomes");
    testRequire(xredisReplyGet(R->Query.Reply, 0, &V) && V.Data.Size == 3 && !memcmp(V.Data.Data, "a\0\xff", 3), "shared owned query bytes");
    xredisreplyinfo I = {0}; I.Size = sizeof(I); testRequire(xredisReplyInfo(R->Control.Reply, &I) && xredisReplyGet(R->Control.Reply, I.Root, &V) && V.Data.Size == 2 && !memcmp(V.Data.Data, "OK", 2), "shared attributed control root");
}
static xfuture* saOrdinary(redisaskingpeer* P, cstr Command)
{
    xbytesview A = {(cbytes)Command, strlen(Command)}; xfuture* F = NULL;
    testRequire(xredisSharedPoolCallAsync(P->Pool, &A, 1, 5000, &F) == XREDIS_ADMISSION_ACCEPTED, "shared ordinary admission"); return F;
}
static void saZero(redisaskingpeer* P)
{
    double Due = xrtTimer() + 10;
    for (;;) {xredispoolinfo I = {0}; I.Size = sizeof(I); testRequire(xredisPoolInfo(P->Pool, &I) && !I.Pending && !I.Leased && !I.Acquired && !I.Returned && xrtTimer() < Due, "shared no lease/physical accounting");
        if (!I.Requests && !I.CommandBytes) return;
        xrtThreadYield();}
}
static void saBasic(bool Secure, xredisinitmode Protocol)
{
    redisaskingpeer* P = redisSharedAskingPeerStart(AK_NORMAL, Secure, Protocol, NULL, NULL); xfuture *F = saSend(P, 5000), *O = saOrdinary(P, "PING"); saReply(saWait(F));
    testRequire(saWait(O)->Query.Status == XREDIS_REQUEST_OK, "ordinary after pair has no one-shot flag"); saZero(P); redisAskingPeerStop(P); saReply(saWait(F)); xrtFutureDestroy(F); xrtFutureDestroy(O);
}
static void saErrors(void)
{
    unsigned Modes[] = {AK_CONTROL_ERROR, AK_QUERY_ERROR, AK_BAD_CONTROL, AK_BAD_WIRE, AK_DROP_PREFIX, AK_DROP_QUERY};
    for (size_t N = 0; N < sizeof(Modes) / sizeof(*Modes); ++N) {
        unsigned Mode = Modes[N]; redisaskingpeer* P = redisSharedAskingPeerStart(Mode, false, XREDIS_INIT_RESP3, NULL, NULL); xfuture* F = saSend(P, 5000); const xredisaskingresult* R = saWait(F);
        if (Mode == AK_CONTROL_ERROR) testRequire(R->ControlReceived && R->Control.Status == XREDIS_REQUEST_SERVER_ERROR && R->Query.Status == XREDIS_REQUEST_OK, "denied control cannot hide executed query");
        else if (Mode == AK_QUERY_ERROR) testRequire(R->ControlReceived && R->Control.Status == XREDIS_REQUEST_OK && R->Query.Status == XREDIS_REQUEST_SERVER_ERROR, "shared redirect stays explicit");
        else testRequire(R->Query.Status == (Mode == AK_BAD_CONTROL || Mode == AK_BAD_WIRE ? XREDIS_REQUEST_PROTOCOL : XREDIS_REQUEST_DISCONNECTED) && R->Query.Delivery == XREDIS_MAY_HAVE_EXECUTED, "shared malformed/disconnect uncertainty");
        if (N < 2) {xfuture* O = saOrdinary(P, "PING"); testRequire(saWait(O)->Query.Status == XREDIS_REQUEST_OK, "server error leaves synchronized shared member"); xrtFutureDestroy(O);}
        redisAskingPeerStop(P); testRequire(R->Query.Status != XREDIS_REQUEST_OK || R->Query.Reply, "retained group metadata"); xrtFutureDestroy(F);
    }
}
static void saPushBarrier(redisaskingpeer* P)
{
    double Due = xrtTimer() + 5; xredisreply* Push = NULL;
    while (!xredisClientTakePush(P->Client, &Push)) {testRequire(xrtTimer() < Due, "shared received control barrier"); xrtThreadYield();}
    xredisReplyRelease(Push);
}
static void saStops(void)
{
    for (unsigned Known = 0; Known < 2; ++Known) for (unsigned Cancel = 0; Cancel < 2; ++Cancel) {
        xredispoolconfig K; xredisPoolConfigInit(&K); K.LeaseTimeout = 1; K.MaxRequests = 2;
        redisaskingpeer* P = redisSharedAskingPeerStart(Known ? AK_HOLD_QUERY : AK_HOLD_CONTROL, false, XREDIS_INIT_RESP3, NULL, &K);
        xfuture* F = saSend(P, Cancel ? 5000 : 300); redisPeerWait(&P->Queries, 1); if (Known) saPushBarrier(P); if (Cancel) xrtFutureCancel(F);
        const xredisaskingresult* R = saWait(F); xredisaskingresult Saved = *R;
        testRequire(R->Query.Status == (Cancel ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT) && R->Query.Delivery == XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived == (Known != 0), "shared immutable stop knowledge without lease hold");
        xredispoolinfo I = {0}; I.Size = sizeof(I); testRequire(xredisPoolInfo(P->Pool, &I) && I.Requests == 1 && I.CommandBytes, "one stopped native group retains quota");
        xfuture* O = saOrdinary(P, "PING"); testRequire(xrtFutureState(O) == XFUTURE_PENDING, "ordinary reply escaped blocked group FIFO");
        redisPeerReleaseBlocked(P->Peer); testRequire(saWait(O)->Query.Status == XREDIS_REQUEST_OK, "ordinary synchronized after stopped group drain"); saZero(P);
        testRequire(!memcmp(R, &Saved, sizeof(Saved)), "late group drain mutated result"); redisAskingPeerStop(P); xrtFutureDestroy(F); xrtFutureDestroy(O);
    }
    xredisclientconfig C; xredisClientConfigInit(&C); C.MaxInFlight = 1;
    redisaskingpeer* P = redisSharedAskingPeerStart(AK_NORMAL, false, XREDIS_INIT_RESP3, &C, NULL); xfuture* Hold = saOrdinary(P, "HOLD"); redisPeerWait(&P->Ordinary, 1);
    for (unsigned Cancel = 0; Cancel < 2; ++Cancel) {xfuture* F = saSend(P, Cancel ? 5000 : 0); if (Cancel) xrtFutureCancel(F);
        const xredisaskingresult* R = saWait(F); testRequire(R->Query.Status == (Cancel ? XREDIS_REQUEST_CANCELLED : XREDIS_REQUEST_TIMEOUT) && R->Query.Delivery == XREDIS_NOT_SENT && !R->ControlReceived, "unsent shared pair has no one-shot state"); xrtFutureDestroy(F);}
    testRequire(!xrtAtomic32Load(&P->Prefixes, XMEMORY_ACQUIRE), "pending group prefix reached server"); redisPeerReleaseBlocked(P->Peer); testRequire(saWait(Hold)->Query.Status == XREDIS_REQUEST_OK, "shared blocker completes"); redisAskingPeerStop(P); xrtFutureDestroy(Hold);
}
static void saBudgetZero(xallocationbudget* B)
{
    double Due = xrtTimer() + 10;
    for (;;) {xallocationbudgetinfo I = {0}; I.Size = sizeof(I); testRequire(xrtAllocationBudgetInfo(B, &I) && xrtTimer() < Due, "shared group budget retirement"); if (!I.LiveCount && !I.LiveBytes) return; xrtThreadYield();}
}
static void saMemoryLimits(void)
{
    redisaskingpeer* P = redisSharedAskingPeerStart(AK_NORMAL, false, XREDIS_INIT_RESP3, NULL, NULL); unsigned Accepted = 0, Failed = 0;
    xbytesview A[] = {XRT_BYTES_LITERAL("APPLY"), XRT_BYTES_LITERAL("x")};
    for (size_t Limit = 1; Limit <= 4096; Limit += 17) {
        xallocationbudget* B = NULL; xallocationscope Scope = {0}; testRequire(xrtAllocationBudgetCreate(Limit, &B) && xrtAllocationBudgetEnter(B, &Scope), "shared group OOM scope"); xfuture* F = (void*)(uintptr_t)1;
        xredisadmission S = xredisSharedPoolAskingCallAsync(P->Pool, A, 2, 5000, &F); testRequire(xrtAllocationBudgetLeave(&Scope), "shared group scope leave");
        if (S == XREDIS_ADMISSION_ACCEPTED) {++Accepted; testRequire(saWait(F)->Query.Status == XREDIS_REQUEST_OK, "shared OOM accepted request"); xrtFutureDestroy(F);}
        else {++Failed; testRequire(S == XREDIS_ADMISSION_ERROR && F == (void*)(uintptr_t)1, "shared OOM changed output"); xrtClearError();}
        saZero(P); saBudgetZero(B); xrtAllocationBudgetFree(B);
    }
    testRequire(Accepted && Failed, "shared group OOM boundary"); redisAskingPeerStop(P);
    xredispoolconfig K; xredisPoolConfigInit(&K); K.MaxRequests = 1; P = redisSharedAskingPeerStart(AK_HOLD_CONTROL, false, XREDIS_INIT_RESP3, NULL, &K);
    xfuture* F = saSend(P, 5000); redisPeerWait(&P->Queries, 1); xredispoolinfo I = {0}; I.Size = sizeof(I); testRequire(xredisPoolInfo(P->Pool, &I) && I.Requests == 1, "shared pair uses one native request");
    size_t Charged = (size_t)I.CommandBytes, Wire = 0, Body = 0; testRequire(xredisCommandSize(A, 2, &Body), "shared group wire sizing"); Wire = Body + 16;
    testRequire(Charged > Wire, "shared prefix/state charged"); xfuture* Out = (void*)(uintptr_t)1;
    testRequire(xredisSharedPoolAskingCallAsync(P->Pool, A, 2, 5000, &Out) == XREDIS_ADMISSION_OVERLOADED && Out == (void*)(uintptr_t)1, "shared aggregate request limit");
    redisPeerReleaseBlocked(P->Peer); testRequire(saWait(F)->Query.Status == XREDIS_REQUEST_OK, "quota blocker drains"); redisAskingPeerStop(P); xrtFutureDestroy(F);
    /* Measure this exact command's charge, then enforce wire-only versus full cap. */
    P = redisSharedAskingPeerStart(AK_HOLD_CONTROL, false, XREDIS_INIT_RESP3, NULL, NULL); F = NULL;
    testRequire(xredisSharedPoolAskingCallAsync(P->Pool, A, 2, 5000, &F) == XREDIS_ADMISSION_ACCEPTED, "exact charge sample"); redisPeerWait(&P->Queries, 1); testRequire(xredisPoolInfo(P->Pool, &I), "exact charge info"); Charged = (size_t)I.CommandBytes; redisPeerReleaseBlocked(P->Peer); saWait(F); redisAskingPeerStop(P); xrtFutureDestroy(F);
    for (unsigned Full = 0; Full < 2; ++Full) {xredisPoolConfigInit(&K); K.MaxCommandBytes = Full ? Charged : Wire; P = redisSharedAskingPeerStart(AK_NORMAL, false, XREDIS_INIT_RESP3, NULL, &K); Out = (void*)(uintptr_t)1;
        xredisadmission S = xredisSharedPoolAskingCallAsync(P->Pool, A, 2, 5000, &Out);
        if (Full) {testRequire(S == XREDIS_ADMISSION_ACCEPTED && saWait(Out)->Query.Status == XREDIS_REQUEST_OK, "exact shared group full cap"); xrtFutureDestroy(Out);}
        else testRequire(S == XREDIS_ADMISSION_OVERLOADED && Out == (void*)(uintptr_t)1, "shared state bypassed wire-only cap");
        redisAskingPeerStop(P);}
}
typedef struct saproducer {redisaskingpeer* Peer;} saproducer;
static int32 saProduce(ptr Data)
{
    redisaskingpeer* P = ((saproducer*)Data)->Peer;
    for (unsigned N = 0; N < 24; ++N) {xfuture* F = saSend(P, 5000); saReply(saWait(F)); xrtFutureDestroy(F); F = saOrdinary(P, "PING"); testRequire(saWait(F)->Query.Status == XREDIS_REQUEST_OK, "shared producer ordinary"); xrtFutureDestroy(F);}
    return 0;
}
static void saConcurrent(void)
{
    redisaskingpeer* P = redisSharedAskingPeerStart(AK_NORMAL, false, XREDIS_INIT_RESP3, NULL, NULL); saproducer Work = {P}; xthread* Threads[4];
    for (unsigned N = 0; N < 4; ++N) {Threads[N] = xrtThreadCreate(saProduce, &Work, 0); testRequire(Threads[N] != NULL, "shared group producer");}
    for (unsigned N = 0; N < 4; ++N) {testRequire(xrtThreadWaitFor(Threads[N], 15000) == XWAIT_OK && !xrtThreadExitCode(Threads[N]), "shared group producer join"); xrtThreadDestroy(Threads[N]);}
    testRequire(xrtAtomic32Load(&P->Prefixes, XMEMORY_ACQUIRE) == 96 && xrtAtomic32Load(&P->Queries, XMEMORY_ACQUIRE) == 96, "shared producers interleaved group wire"); redisAskingPeerStop(P);
    P = redisSharedAskingPeerStart(AK_SYNC, false, XREDIS_INIT_RESP3, NULL, NULL); xfuture* F = saSend(P, 5000); saReply(saWait(F)); redisPeerWait(&P->SyncChecked, 1);
    testRequire(saWait(P->Reentry)->Query.Status == XREDIS_REQUEST_OK, "shared async worker reentry"); xfuture* Nested = P->Reentry; redisAskingPeerStop(P); xrtFutureDestroy(F); xrtFutureDestroy(Nested);
}
static void saClose(void)
{
    redisaskingpeer* P = redisSharedAskingPeerStart(AK_NORMAL, false, XREDIS_INIT_RESP3, NULL, NULL);
    for (unsigned N = 0; N < 64; ++N) xrtFutureDestroy(saSend(P, 5000));
    saZero(P); redisAskingPeerStop(P);
    P = redisSharedAskingPeerStart(AK_HOLD_CONTROL, false, XREDIS_INIT_RESP3, NULL, NULL); xfuture* F = saSend(P, 5000); redisPeerWait(&P->Queries, 1); xredisPoolClose(P->Pool);
    testRequire(saWait(F)->Query.Delivery == XREDIS_MAY_HAVE_EXECUTED, "shared close uncertainty"); redisAskingPeerStop(P); xrtFutureDestroy(F);
    xredisclientconfig C; xredisClientConfigInit(&C); C.DrainTimeout = 100;
    P = redisSharedAskingPeerStart(AK_HOLD_CONTROL, false, XREDIS_INIT_RESP3, &C, NULL); F = saSend(P, 5000); redisPeerWait(&P->Queries, 1); xrtFutureCancel(F); saWait(F);
    xfuture* Closed = xredisClientClosed(P->Client); testRequire(xrtFutureWaitFor(Closed, 10000) == XWAIT_OK, "shared group drain expiry retires member"); redisAskingPeerStop(P); xrtFutureDestroy(F); xrtFutureDestroy(Closed);
}
#if defined(XREDIS_SHARED_ASKING_WHITEBOX) && defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
typedef struct sagate {xnetpost Post;xmutex Gate;xatomic32 Entered,Left;} sagate;
static void saBlocked(xnetworker* W,ptr Data)
{(void)W;sagate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);xrtMutexLock(&G->Gate);xrtMutexUnlock(&G->Gate);xrtAtomic32Store(&G->Left,1,XMEMORY_RELEASE);}
static void saPostHold(redisaskingpeer* P,sagate* G)
{memset(G,0,sizeof(*G));testRequire(xrtMutexInit(&G->Gate) && xrtMutexLock(&G->Gate) && xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(P->Peer->Engine,0),&G->Post,saBlocked,G),"pause client wire turn");}
static void saHold(redisaskingpeer* P,sagate* G){saPostHold(P,G);redisPeerWait(&G->Entered,1);}
static void saRelease(sagate* G){xrtMutexUnlock(&G->Gate);redisPeerWait(&G->Left,1);xrtMutexDestroy(&G->Gate);}
static void saPartialTls(void)
{
    for(unsigned Stop=1;Stop<=2;++Stop){xredisclientconfig C;xredisClientConfigInit(&C);C.WorkRequests=1;C.DrainTimeout=10000;redisaskingpeer* P=redisSharedAskingPeerStart(AK_NORMAL,true,XREDIS_INIT_RESP3,&C,NULL);unsigned char* Bytes=xrtMalloc(65536);testRequire(Bytes!=NULL,"partial TLS fixture");memset(Bytes,'a',65536);Bytes[65535]='z';xbytesview Args[]={XRT_BYTES_LITERAL("APPLYLARGE"),{Bytes,65536}};xfuture* F=NULL;sagate Before,Inside;
        if(Stop==1)saHold(P,&Before);
        testRequire(xredisSharedPoolAskingCallAsync(P->Pool,Args,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"partial TLS admission");memset(Bytes,0xcc,65536);xrtFree(Bytes);
        if(Stop==1){saPostHold(P,&Inside);saRelease(&Before);redisPeerWait(&Inside.Entered,1);xredisclient* Native=P->Client;
            testRequire(Native->TlsWrite && xredisAskingPair(Native->TlsWrite) && Native->TlsWritten>0 && Native->TlsWritten<sizeof(xredisAskingWire)-1,"actual partial ASKING accepted before cancel");
            testRequire(!xrtAtomic32Load(&P->Prefixes,XMEMORY_ACQUIRE) && xrtFutureCancel(F),"stop inside prefix before server consumes it");saRelease(&Inside);
        }else{redisPeerWait(&P->Prefixes,1);bool Seen=false;
            for(unsigned Attempt=0;Attempt<128 && !Seen;++Attempt){saHold(P,&Inside);xredisclient* Native=P->Client;xredisrequest* R=Native->TlsWrite;
                Seen=R && xredisAskingPair(R) && xredisAskingState(R)->Seen==1 && Native->TlsWritten>=sizeof(xredisAskingWire)-1 && Native->TlsWritten<R->WireBytes;
                if(Seen){testRequire(xrtFutureCancel(F),"stop after prefix ACK during partial query");}
                saRelease(&Inside);if(!Seen)xrtThreadYield();}
            testRequire(Seen,"observed exact prefix ACK during partial query write");
        }
        const xredisaskingresult* R=saWait(F);xredisaskingresult Saved=*R;testRequire(R->Query.Status==XREDIS_REQUEST_CANCELLED && R->Query.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived==(Stop==2),"partial stop has exact immutable control knowledge");xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* O=NULL;testRequire(xredisSharedPoolCallAsync(P->Pool,&Ping,1,15000,&O)==XREDIS_ADMISSION_ACCEPTED,"ordinary queued behind partial stopped shared group");testRequire(saWait(O)->Query.Status==XREDIS_REQUEST_OK,"ordinary follows whole partial TLS group");saZero(P);testRequire(!memcmp(R,&Saved,sizeof(Saved)),"late TLS drain does not mutate retained result");
        redisAskingPeerStop(P);xrtFutureDestroy(F);xrtFutureDestroy(O);
    }
}
#endif
int main(void)
{
    saBasic(false, XREDIS_INIT_RESP2); saBasic(false, XREDIS_INIT_RESP3); saErrors(); saStops(); saMemoryLimits(); saConcurrent(); saClose();
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    saBasic(true, XREDIS_INIT_RESP2); saBasic(true, XREDIS_INIT_RESP3);
#if defined(XREDIS_SHARED_ASKING_WHITEBOX)
    saPartialTls();
#endif
#endif
    puts("[PASS] shared ASKING: native adjacent groups, shared FIFO/stops/quotas, explicit outcomes, OOM, producers and lifetime"); return 0;
}
