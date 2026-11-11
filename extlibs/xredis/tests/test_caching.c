#include "test.h"
#include <xredis/tracking.h>
#include "support/client_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "support/tls_peer.h"
#endif

_Static_assert(offsetof(xrediscachingresult,Query)==0,"ordinary result prefix");
_Static_assert(offsetof(xredistrackingresult,Request)==0,"nested result prefix");
typedef struct cachingPeer {
    xatomic32 Prefixes,Queries;
    bool Armed,Yes,Push,Reenter;
    unsigned Mode;
    xfuture* Reentry;
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
    bool Multi;
#endif
} cachingPeer;
static bool cachingCommand(redispeer* P,xredisreply* Reply,ptr Data)
{
    cachingPeer* T=Data;xredisreplyvalue Root,Name,Arg;
    testRequire(xredisReplyGet(Reply,0,&Root) && xredisReplyGet(Reply,Root.FirstChild,&Name),"group peer command");
    if(redisPeerName(Name,"HELLO")){
        static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,Hello,sizeof(Hello)-1);return true;
    }
    if(redisPeerName(Name,"CLIENT")){
        testRequire(!T->Armed && Root.ChildCount==3,"one adjacent prefix");
        xredisReplyGet(Reply,Name.Next,&Arg);testRequire(redisPeerName(Arg,"CACHING"),"typed CACHING prefix");
        xredisReplyGet(Reply,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"YES") || redisPeerName(Arg,"NO"),"prefix choice");
        T->Yes=redisPeerName(Arg,"YES");T->Armed=true;
        if(T->Reenter){
            xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* F=(xfuture*)(uintptr_t)1;
            testRequire(xredisClientCachingCall(P->Client,true,&Ping,1,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"server worker sync refusal");xrtClearError();
            testRequire(xredisClientCallAsync(P->Client,&Ping,1,5000,&T->Reentry)==XREDIS_ADMISSION_ACCEPTED,"worker async reentry");T->Reenter=false;
        }
        if(T->Mode==5 || T->Mode==6)P->Blocked=true;
        if(T->Mode==7)redisPeerSend(P,"$1\r\nz\rX",7);
        else if(T->Mode==1)redisPeerSend(P,"-ERR control\r\n",14);
        else if(T->Mode==3 || T->Mode==6)redisPeerSend(P,"+wrong\r\n",8);
        else redisPeerSend(P,"+OK\r\n",5);
        if(T->Push){static const char Push[]=">2\r\n+invalidate\r\n*1\r\n$1\r\nk\r\n";redisPeerSend(P,Push,sizeof(Push)-1);}
        xrtAtomic32FetchAdd(&T->Prefixes,1,XMEMORY_RELEASE);return true;
    }
    if(redisPeerName(Name,"PAIR") || redisPeerName(Name,"PAIRLARGE")){
        testRequire(T->Armed && Root.ChildCount==2,"no producer interleaves prefix/query");
        xredisReplyGet(Reply,Name.Next,&Arg);
        testRequire(Arg.Data.Size && Arg.Data.Data[0]==(unsigned char)(T->Yes?0xa1:0xa0),"query retains its own caching choice");T->Armed=false;
        /* A deliberately corrupted prefix retires the connection. Record that
         * its adjacent query was parsed, without requiring a response write to
         * succeed after the client has already closed the malformed stream. */
        if(T->Mode==3 || T->Mode==6 || T->Mode==7){xrtAtomic32FetchAdd(&T->Queries,1,XMEMORY_RELEASE);return true;}
        if(T->Mode==4)P->Blocked=true;
        if(T->Mode==2)redisPeerSend(P,"-ERR query\r\n",12);
        else {size_t Bytes=Arg.Data.Size;
            if(redisPeerName(Name,"PAIRLARGE")){testRequire(Bytes>=65536 && Arg.Data.Data[Bytes-1]=='z',"copied complete large TLS query");Bytes=3;}
            char H[40];int N=snprintf(H,sizeof(H),"$%zu\r\n",Bytes);redisPeerSend(P,H,(size_t)N);redisPeerSend(P,Arg.Data.Data,Bytes);redisPeerSend(P,"\r\n",2);}
        xrtAtomic32FetchAdd(&T->Queries,1,XMEMORY_RELEASE);return true;
    }
    testRequire(!T->Armed,"ordinary request cannot consume another producer's flag");
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
    if(redisPeerName(Name,"MULTI")){testRequire(!T->Multi,"mixed transaction starts once");T->Multi=true;redisPeerSend(P,"+OK\r\n",5);return true;}
    if(redisPeerName(Name,"EXEC")){testRequire(T->Multi,"mixed EXEC owns transaction");T->Multi=false;redisPeerSend(P,"*0\r\n",4);return true;}
    testRequire(!T->Multi,"group cannot enter transaction's wire interval");
#endif
    return false;
}
static void cachingStart(redispeer* P,cachingPeer* T,redispeerhook* Hook)
{
    redisPeerStart(P);memset(T,0,sizeof(*T));xrtAtomic32Init(&T->Prefixes,0);xrtAtomic32Init(&T->Queries,0);
    *Hook=(redispeerhook){cachingCommand,T};xrtAtomicPtrStore(&P->Hook,Hook,XMEMORY_RELEASE);
}
static xredisclient* cachingConnect(redispeer* P,const xredisclientconfig* Config)
{
    xredisclient* C=NULL;testRequire(xredisClientConnectInit(P->Engine,&P->Address,0,Config,NULL,&C),"managed caching client");P->Client=C;
    xfuture* F=xredisClientReady(C);xfutureresult R;
    testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && ((const xredisreadyresult*)R.Value)->Status==XREDIS_READY_OK,"managed readiness");xrtFutureDestroy(F);
    testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"pair observer");return C;
}
static const xrediscachingresult* cachingResult(xfuture* F)
{
    xfutureresult R;testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"typed caching result");return R.Value;
}
static xfuture* cachingSubmit(xredisclient* C,bool Yes,int64 Timeout)
{
    unsigned char Payload[]={Yes?0xa1:0xa0,0,'x'};xbytesview A[]={XRT_BYTES_LITERAL("PAIR"),{Payload,sizeof(Payload)}};xfuture* F=NULL;
    testRequire(xredisClientCachingCallAsync(C,Yes,A,2,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"group admission");memset(Payload,0xcc,sizeof(Payload));return F;
}
static xfuture* cachingPing(xredisclient* C)
{
    xbytesview A=XRT_BYTES_LITERAL("PING");xfuture* F=NULL;testRequire(xredisClientTrackingCallAsync(C,&A,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"following observed request");return F;
}
static void cachingClose(redispeer* P,xredisclient* C)
{
    xfuture* F=xredisClientClosed(C);testRequire(F && xredisClientClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"pair physical close");
    xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(C,&I) && !I.Requests && !I.CommandBytes && I.Accepted==I.Completed,"pair duties retired");
    xredisClientRelease(C);xrtFutureDestroy(F);redisPeerStop(P);
}
static void cachingCheckReply(const xrediscachingresult* R,bool Yes,bool Push)
{
    testRequire(R->ControlReceived && R->Control.Request.Status==XREDIS_REQUEST_OK && R->Control.Request.Delivery==XREDIS_REPLIED &&
        R->Query.Request.Status==XREDIS_REQUEST_OK && R->Query.Request.Delivery==XREDIS_REPLIED &&
        R->Query.Sequence==R->Control.Sequence+(Push?2u:1u),"two independent ordered replies");
    xredisreplyvalue V;testRequire(xredisReplyGet(R->Control.Request.Reply,0,&V) && redisPeerName(V,"OK"),"retained prefix reply");
    testRequire(xredisReplyGet(R->Query.Request.Reply,0,&V) && V.Data.Size==3 && V.Data.Data[0]==(unsigned char)(Yes?0xa1:0xa0) && !V.Data.Data[1] && V.Data.Data[2]=='x',"binary copied query reply");
}
typedef struct cachingGate {xnetpost Post;xmutex Gate;xatomic32 Entered,Left;xredisclient* Client;} cachingGate;
static void cachingBlocked(xnetworker* W,ptr Data)
{
    (void)W;cachingGate* G=Data;
    if(G->Client){xbytesview A=XRT_BYTES_LITERAL("PING");xfuture* F=(xfuture*)(uintptr_t)1;
        testRequire(xredisClientCachingCall(G->Client,true,&A,1,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"client worker sync refusal");xrtClearError();}
    xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);xrtMutexLock(&G->Gate);xrtMutexUnlock(&G->Gate);xrtAtomic32Store(&G->Left,1,XMEMORY_RELEASE);
}
static void cachingPostHold(redispeer* P,xredisclient* C,cachingGate* G)
{
    memset(G,0,sizeof(*G));G->Client=C;xrtAtomic32Init(&G->Entered,0);xrtAtomic32Init(&G->Left,0);
    testRequire(xrtMutexInit(&G->Gate) && xrtMutexLock(&G->Gate) && xrtNetPostInit(&G->Post) && xrtNetPost(xrtNetEngineWorker(P->Engine,0),&G->Post,cachingBlocked,G),"hold client worker");
}
static void cachingHold(redispeer* P,xredisclient* C,cachingGate* G)
{cachingPostHold(P,C,G);redisPeerWait(&G->Entered,1);}
static void cachingRelease(cachingGate* G)
{xrtMutexUnlock(&G->Gate);redisPeerWait(&G->Left,1);xrtMutexDestroy(&G->Gate);}
static void cachingBasics(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Push=true;T.Reenter=true;
    xredisclient* C=cachingConnect(&P,NULL);cachingGate G;cachingHold(&P,C,&G);
    xfuture* Yes=cachingSubmit(C,true,5000);xfuture* No=cachingSubmit(C,false,5000);
    xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(C,&I) && I.Requests==2 && I.Accepted==2 && I.CommandBytes>100,"one request per complete group including state charge");
    cachingRelease(&G);const xrediscachingresult* A=cachingResult(Yes);const xrediscachingresult* B=cachingResult(No);cachingCheckReply(A,true,true);cachingCheckReply(B,false,true);
    testRequire(A->Control.Sequence==1 && A->Query.Sequence==3 && B->Control.Sequence==4 && B->Query.Sequence==6,"adjacent groups and pushes preserve wire order");
    redisPeerWait(&T.Prefixes,2);
    testRequire(((const xredistrackingresult*)cachingResult(T.Reentry))->Request.Status==XREDIS_REQUEST_OK,"asynchronous worker reentry survives");
    for(unsigned N=0;N<2;++N){xredistrackingpush V;testRequire(xredisClientTrackingTake(C,&V) && V.Sequence==2+3*N,"pushes do not consume pair replies");xredisReplyRelease(V.Reply);}
    cachingClose(&P,C);cachingCheckReply(A,true,true);cachingCheckReply(B,false,true);xrtFutureDestroy(Yes);xrtFutureDestroy(No);xrtFutureDestroy(T.Reentry);
}
static void cachingErrors(void)
{
    for(unsigned Mode=1;Mode<=3;++Mode){redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Mode=Mode;xredisclient* C=cachingConnect(&P,NULL);
        xfuture* F=cachingSubmit(C,true,5000);const xrediscachingresult* R=cachingResult(F);
        testRequire(R->ControlReceived,"prefix knowledge retained");
        if(Mode==1)testRequire(R->Control.Request.Status==XREDIS_REQUEST_SERVER_ERROR && R->Control.Sequence==1 && R->Query.Request.Status==XREDIS_REQUEST_OK && R->Query.Sequence==2,"control rejection does not hide executed query");
        if(Mode==2)testRequire(R->Control.Request.Status==XREDIS_REQUEST_OK && R->Query.Request.Status==XREDIS_REQUEST_SERVER_ERROR && R->Query.Sequence==2,"query server error remains separate");
        if(Mode==3)testRequire(R->Control.Request.Status==XREDIS_REQUEST_PROTOCOL && !R->Control.Sequence && R->Query.Request.Status==XREDIS_REQUEST_PROTOCOL && R->Query.Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && !R->Query.Sequence,"malformed control retires uncertain group");
        cachingClose(&P,C);xrtFutureDestroy(F);
    }
}
static void cachingStops(void)
{
    for(unsigned Mode=4;Mode<=6;++Mode)for(unsigned Cancel=0;Cancel<2;++Cancel){redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Mode=Mode;xredisclient* C=cachingConnect(&P,NULL);
        xfuture* F=cachingSubmit(C,true,Cancel?5000:100);redisPeerWait(&T.Queries,1);
        if(Mode==4){xredistrackinginfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;do{testRequire(xredisClientTrackingInfo(C,&I) && xrtTimer()<End,"prefix ACK consumed");if(!I.Sequence)xrtThreadYield();}while(!I.Sequence);}
        if(Cancel)testRequire(xrtFutureCancel(F),"cancel sent group");
        const xrediscachingresult* R=cachingResult(F);xrediscachingresult Saved=*R;
        testRequire(R->Query.Request.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT) && R->Query.Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && !R->Query.Sequence,"sent stop never claims query was unsent");
        testRequire(R->ControlReceived==(Mode==4) && R->Control.Sequence==(Mode==4?1u:0u),"stop snapshots independent prefix knowledge");
        xfuture* Next=cachingPing(C);redisPeerReleaseBlocked(&P);const xredistrackingresult* N=(const xredistrackingresult*)cachingResult(Next);
        if(Mode==6)testRequire(N->Request.Status==XREDIS_REQUEST_PROTOCOL,"abandoned malformed prefix still closes connection");
        else testRequire(N->Request.Status==XREDIS_REQUEST_OK && N->Sequence==3,"drain both frames before following query");
        testRequire(!memcmp(&Saved,R,sizeof(Saved)),"late physical drain cannot mutate logical result");cachingClose(&P,C);xrtFutureDestroy(F);xrtFutureDestroy(Next);
    }
}
typedef struct cachingProducer {xredisclient* Client;unsigned Id;} cachingProducer;
static int32 cachingProduce(ptr Data)
{
    cachingProducer* P=Data;for(unsigned N=0;N<16;++N){bool Yes=((P->Id+N)&1)!=0;xfuture* F=cachingSubmit(P->Client,Yes,5000);cachingCheckReply(cachingResult(F),Yes,true);xrtFutureDestroy(F);
        xfuture* Ping=cachingPing(P->Client);testRequire(((const xredistrackingresult*)cachingResult(Ping))->Request.Status==XREDIS_REQUEST_OK,"parallel ordinary outcome");xrtFutureDestroy(Ping);}
    return 0;
}
static void cachingConcurrent(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Push=true;xredisclient* C=cachingConnect(&P,NULL);cachingProducer Work[4];xthread* Threads[4];
    for(unsigned N=0;N<4;++N){Work[N]=(cachingProducer){C,N};Threads[N]=xrtThreadCreate(cachingProduce,&Work[N],0);testRequire(Threads[N]!=NULL,"group producer");}
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],10000)==XWAIT_OK,"group producer join");xrtThreadDestroy(Threads[N]);}
    xredistrackinginfo I={0};I.Size=sizeof(I);testRequire(xredisClientTrackingInfo(C,&I) && I.Sequence==256,"all group and ordinary frames counted once");
    for(unsigned N=0;N<64;++N){xredistrackingpush Push;testRequire(xredisClientTrackingTake(C,&Push),"bounded concurrent pushes");xredisReplyRelease(Push.Reply);}cachingClose(&P,C);
}
static void cachingUnsent(void)
{
    for(unsigned Cancel=0;Cancel<2;++Cancel){redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);xredisclient* C=cachingConnect(&P,NULL);cachingGate G;cachingHold(&P,C,&G);
        xfuture* F=cachingSubmit(C,true,Cancel?5000:0);if(Cancel)testRequire(xrtFutureCancel(F),"cancel unsent group");cachingRelease(&G);const xrediscachingresult* R=cachingResult(F);
        testRequire(!R->ControlReceived && !R->Query.Sequence && R->Query.Request.Delivery==XREDIS_NOT_SENT && R->Control.Request.Delivery==XREDIS_NOT_SENT && R->Query.Request.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT),"unsent group removes both commands");
        xfuture* Next=cachingPing(C);testRequire(((const xredistrackingresult*)cachingResult(Next))->Sequence==1 && !xrtAtomic32Load(&T.Prefixes,XMEMORY_ACQUIRE),"unsent prefix cannot leak to following request");cachingClose(&P,C);xrtFutureDestroy(F);xrtFutureDestroy(Next);
    }
}
static void cachingAdmission(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);xredisclientconfig K;xredisClientConfigInit(&K);K.MaxRequests=K.MaxInFlight=1;
    xredisclient* C=cachingConnect(&P,&K);unsigned char Payload[]={0xa1,0,'x'};xbytesview A[]={XRT_BYTES_LITERAL("PAIR"),{Payload,sizeof(Payload)}};
    const cstr Bad[]={"CLIENT","QUIT","READONLY","READWRITE","ASKING","HELLO","AUTH","SELECT","RESET","MULTI","EXEC","DISCARD","WATCH","UNWATCH","SUBSCRIBE","MONITOR","SYNC"};
    for(size_t N=0;N<sizeof(Bad)/sizeof(*Bad);++N){xbytesview V={(cbytes)Bad[N],strlen(Bad[N])};xfuture* F=(xfuture*)(uintptr_t)1;
        testRequire(xredisClientCachingCallAsync(C,true,&V,1,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"known control query refused");xrtClearError();}
    xbytesview Saved=A[1];testRequire(xredisClientCachingCallAsync(C,true,A,2,100,(xfuture**)&A[1])==XREDIS_ADMISSION_ERROR && !memcmp(&Saved,&A[1],sizeof(Saved)),"output cannot overwrite arguments");xrtClearError();
    cachingGate G;cachingHold(&P,C,&G);xfuture* F=cachingSubmit(C,true,5000);xredisclientinfo I={0};I.Size=sizeof(I);
    testRequire(xredisClientInfo(C,&I) && I.Requests==1 && I.Accepted==1,"two frames consume one logical request");size_t Charged=I.CommandBytes;
    xfuture* Sentinel=(xfuture*)(uintptr_t)1;xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();
    testRequire(xredisClientCachingCallAsync(C,true,A,2,100,&Sentinel)==XREDIS_ADMISSION_OVERLOADED && Sentinel==(xfuture*)(uintptr_t)1 && xrtGetError()==Error,"count pressure preserves output and error");xrtClearError();
    cachingRelease(&G);cachingCheckReply(cachingResult(F),true,false);cachingClose(&P,C);xrtFutureDestroy(F);
    for(unsigned Fits=0;Fits<2;++Fits){cachingStart(&P,&T,&Hook);xredisClientConfigInit(&K);K.MaxCommandBytes=Charged-(Fits?0:1);K.MaxCommandBytesTotal=Charged;C=cachingConnect(&P,&K);F=(xfuture*)(uintptr_t)1;
        xredisadmission Admission=xredisClientCachingCall(C,true,A,2,5000,&F);
        if(Fits){testRequire(Admission==XREDIS_ADMISSION_ACCEPTED,"exact complete group byte budget accepted");cachingCheckReply(cachingResult(F),true,false);xrtFutureDestroy(F);}
        else testRequire(Admission==XREDIS_ADMISSION_OVERLOADED && F==(xfuture*)(uintptr_t)1 && !xrtAtomic32Load(&T.Prefixes,XMEMORY_ACQUIRE),"one byte below group charge refuses before wire");
        cachingClose(&P,C);
    }
    cachingStart(&P,&T,&Hook);C=NULL;testRequire(xredisClientConnect(P.Engine,&P.Address,0,NULL,&C),"raw connection");P.Client=C;F=(xfuture*)(uintptr_t)1;
    testRequire(xredisClientCachingCallAsync(C,true,A,2,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"observer required");xrtClearError();
    testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED && xredisClientCachingCallAsync(C,true,A,2,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"managed setup required");xrtClearError();cachingClose(&P,C);
}
static void cachingOOM(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);xredisclient* C=cachingConnect(&P,NULL);unsigned Success=0,Failure=0;
    for(size_t Limit=1;Limit<=1536;++Limit){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"pair construction quota");
        unsigned char Payload[]={0xa1,0,'x'};xbytesview A[]={XRT_BYTES_LITERAL("PAIR"),{Payload,sizeof(Payload)}};xfuture* F=(xfuture*)(uintptr_t)1;
        xredisadmission Admission=xredisClientCachingCallAsync(C,true,A,2,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"pair scope leave");xrtClearError();
        if(Admission==XREDIS_ADMISSION_ACCEPTED){cachingCheckReply(cachingResult(F),true,false);xrtFutureDestroy(F);++Success;}
        else {testRequire(Admission==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"OOM rollback preserves output");++Failure;}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;
        do {testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<End,"pair construction owners retire");if(I.LiveCount)xrtThreadYield();}while(I.LiveCount);
        testRequire(!I.LiveBytes,"full group and result allocation charge refunded");xrtAllocationBudgetFree(B);
    }
    testRequire(Success && Failure,"all allocation prefixes through successful construction");cachingClose(&P,C);
}
static void cachingRetirement(void)
{
    for(unsigned Stop=0;Stop<2;++Stop){redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Mode=Stop?4:5;xredisclientconfig K;xredisClientConfigInit(&K);K.DrainTimeout=30;
        xredisclient* C=cachingConnect(&P,&K);xfuture* Closed=xredisClientClosed(C);xfuture* F=cachingSubmit(C,true,Stop?5000:30);redisPeerWait(&T.Queries,1);
        if(Stop){xredistrackinginfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;do{testRequire(xredisClientTrackingInfo(C,&I) && xrtTimer()<End,"close after control reply");if(!I.Sequence)xrtThreadYield();}while(!I.Sequence);testRequire(xredisClientClose(C),"close active group");}
        const xrediscachingresult* R=cachingResult(F);testRequire(R->Query.Request.Status==(Stop?XREDIS_REQUEST_CLOSED:XREDIS_REQUEST_TIMEOUT) && R->Query.Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived==(Stop!=0),"close/drain retain precise known prefix");
        testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"incomplete group has bounded physical drain");xrtFutureDestroy(Closed);xrediscachingresult Saved=*R;cachingClose(&P,C);
        testRequire(!memcmp(&Saved,R,sizeof(Saved)),"stopped result survives all owners");if(Stop){xredisreplyvalue V;testRequire(xredisReplyGet(R->Control.Request.Reply,0,&V) && redisPeerName(V,"OK"),"known control reply survives Engine");}xrtFutureDestroy(F);
    }
}
static void cachingCause(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);T.Mode=7;xredisclient* C=cachingConnect(&P,NULL);
    xfuture* F=cachingSubmit(C,true,5000);const xrediscachingresult* R=cachingResult(F);
    testRequire(R->Query.Request.Status==XREDIS_REQUEST_PROTOCOL && !R->ControlReceived && R->Query.Request.Cause && R->Control.Request.Cause==R->Query.Request.Cause,"unreceived prefix and query independently own shared failure cause");
    xerror* Cause=xrtErrorRef(R->Query.Request.Cause);cachingClose(&P,C);
    testRequire(xrtErrorKind(R->Control.Request.Cause)!=0 && xrtErrorKind(R->Query.Request.Cause)!=0,"both causes survive physical owners");xrtFutureDestroy(F);
    testRequire(xrtErrorKind(Cause)!=0,"external cause owner survives both result drops");xrtErrorFree(Cause);
}
static void cachingForgotten(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);xredisclient* C=cachingConnect(&P,NULL);
    for(unsigned N=0;N<128;++N)xrtFutureDestroy(cachingSubmit(C,(N&1)!=0,5000));
    xredisclientinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;
    do {testRequire(xredisClientInfo(C,&I) && xrtTimer()<End,"unobserved groups physically retire");if(I.Requests)xrtThreadYield();}while(I.Requests);
    testRequire(I.Accepted==128 && I.Completed==128 && !I.CommandBytes,"last-observer drop releases both reply trees and state");
    xfuture* Next=cachingPing(C);testRequire(((const xredistrackingresult*)cachingResult(Next))->Request.Status==XREDIS_REQUEST_OK,"observer loss cannot leak one-shot flag");cachingClose(&P,C);xrtFutureDestroy(Next);
}
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
static void cachingTransactions(void)
{
    redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);xredisclient* C=cachingConnect(&P,NULL);cachingGate G;cachingHold(&P,C,&G);
    xfuture* A=cachingSubmit(C,true,5000);xfuture* Tx=NULL;testRequire(xredisClientTransactionAsync(C,NULL,0,NULL,5000,&Tx)==XREDIS_ADMISSION_ACCEPTED,"mixed empty transaction");
    xfuture* B=cachingSubmit(C,false,5000);xfuture* Ping=cachingPing(C);cachingRelease(&G);
    const xrediscachingresult* First=cachingResult(A);const xrediscachingresult* Last=cachingResult(B);
    cachingCheckReply(First,true,false);cachingCheckReply(Last,false,false);
    xfutureresult V;testRequire(xrtFutureWaitFor(Tx,5000)==XWAIT_OK && xrtFutureResult(Tx,&V) && ((const xredistransactionresult*)V.Value)->Execution==XREDIS_TRANSACTION_EXECUTED,"transaction keeps its exclusive interval");
    testRequire(First->Control.Sequence==1 && Last->Control.Sequence==5 && ((const xredistrackingresult*)cachingResult(Ping))->Sequence==7,"group FIFO counts coexist with transaction phases");
    cachingClose(&P,C);xrtFutureDestroy(A);xrtFutureDestroy(Tx);xrtFutureDestroy(B);xrtFutureDestroy(Ping);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void cachingTLS(void)
{
    for(unsigned Stop=0;Stop<3;++Stop){redispeer P;cachingPeer T;redispeerhook Hook;cachingStart(&P,&T,&Hook);
        redistlspeer TLS={0};xrtAtomic32Init(&TLS.Verified,0);redisTlsPeerCredentials(&TLS,&P,false,false);
        xredisclienttlsconfig Secure;redisTlsPeerConfig(&TLS,&Secure);Secure.WriteBytes=3;
        xredisclientconfig K;xredisClientConfigInit(&K);K.WorkRequests=1;K.DrainTimeout=10000;
        xredisclient* C=NULL;testRequire(xredisClientTlsConnect(P.Engine,&P.Address,0,&K,NULL,&Secure,&C),"numeric managed TLS group client");P.Client=C;
        xfuture* Ready=xredisClientReady(C);xfutureresult V;testRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK && xrtFutureResult(Ready,&V) && ((const xredisreadyresult*)V.Value)->Status==XREDIS_READY_OK,"TLS ready");xrtFutureDestroy(Ready);
        testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"TLS group observer");
        unsigned char* Payload=xrtMalloc(65536);testRequire(Payload!=NULL,"large TLS caller input");memset(Payload,'q',65536);Payload[0]=0xa1;Payload[1]=0;Payload[2]='x';Payload[65535]='z';
        xbytesview Args[]={XRT_BYTES_LITERAL("PAIRLARGE"),{Payload,65536}};xfuture* F=NULL;cachingGate Before,Inside;
        if(Stop==1)cachingHold(&P,C,&Before);
        testRequire(xredisClientCachingCallAsync(C,true,Args,2,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"large TLS group");memset(Payload,0xcc,65536);xrtFree(Payload);
        if(Stop==1){
            cachingPostHold(&P,C,&Inside);cachingRelease(&Before);redisPeerWait(&Inside.Entered,1);
#if defined(XREDIS_CACHING_WHITEBOX)
            testRequire(C->TlsWrite && xredisCachingPair(C->TlsWrite) && C->TlsWritten>0 && C->TlsWritten<sizeof(xredisCachingYes)-1,"actual partial preface accepted before cancellation");
#endif
            testRequire(!xrtAtomic32Load(&T.Prefixes,XMEMORY_ACQUIRE) && xrtFutureCancel(F),"cancel inside prefix before server can consume it");cachingRelease(&Inside);
        }
        if(Stop==2){
            redisPeerWait(&T.Prefixes,1);bool Seen=false;
            for(unsigned Attempt=0;Attempt<128 && !Seen;++Attempt){cachingHold(&P,C,&Inside);
#if defined(XREDIS_CACHING_WHITEBOX)
                xredisrequest* R=C->TlsWrite;
                Seen=R && xredisCachingPair(R) && xredisCachingState(R)->Seen==1 && C->TlsWritten>=sizeof(xredisCachingYes)-1 && C->TlsWritten<R->WireBytes;
#else
                xredistrackinginfo I={0};I.Size=sizeof(I);testRequire(xredisClientTrackingInfo(C,&I),"TLS prefix observation");Seen=I.Sequence==1 && !xrtAtomic32Load(&T.Queries,XMEMORY_ACQUIRE);
#endif
                if(Seen)testRequire(xrtFutureCancel(F),"cancel inside TLS query after prefix ACK");
                cachingRelease(&Inside);if(!Seen)xrtThreadYield();
            }
            testRequire(Seen,"observed prefix reply during partial query write");
        }
        const xrediscachingresult* R=cachingResult(F);xrediscachingresult Saved=*R;
        if(Stop)testRequire(R->Query.Request.Status==XREDIS_REQUEST_CANCELLED && R->Query.Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->ControlReceived==(Stop==2),"TLS partial stop reports conservative query and exact prefix");
        else cachingCheckReply(R,true,false);
        xfuture* Next=cachingPing(C);const xredistrackingresult* N=(const xredistrackingresult*)cachingResult(Next);
        testRequire(N->Request.Status==XREDIS_REQUEST_OK && N->Sequence==3 && !memcmp(R,&Saved,sizeof(Saved)),"TLS original group drains before ordinary successor");
        redisPeerWait(&T.Queries,1);cachingClose(&P,C);redisTlsPeerCredentialsDrop(&TLS);xrtFutureDestroy(F);xrtFutureDestroy(Next);
    }
}
#endif
#ifndef XREDIS_CACHING_TEST_MAIN
#define XREDIS_CACHING_TEST_MAIN main
#endif
int XREDIS_CACHING_TEST_MAIN(void)
{
#if defined(XREDIS_CACHING_WHITEBOX)
    printf("[layout] request=%zu wire_offset=%zu pair_state=%zu pair_result=%zu observed_result=%zu ordinary_result=%zu\n",sizeof(xredisrequest),offsetof(xredisrequest,Wire),sizeof(xrediscachingstate),sizeof(xrediscachingresult),sizeof(xredistrackingresult),sizeof(xredisrequestresult));
#endif
    cachingBasics();cachingErrors();cachingStops();cachingUnsent();cachingConcurrent();cachingAdmission();cachingOOM();cachingRetirement();cachingCause();cachingForgotten();
#if defined(XREDIS_FEATURE_XREDIS_TRANSACTION)
    cachingTransactions();
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    cachingTLS();
#endif
    puts("[PASS] caching: adjacent binary groups, explicit control/query outcomes, two workers/reentry, cancellation/deadline drain, immutable retained results and concurrent producers");return 0;
}
