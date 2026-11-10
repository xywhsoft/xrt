#include "test.h"
#include <xredis/tracking.h>
#include "support/client_peer.h"
static xredisclient* testClientConnect(redispeer* P,const xredisclientconfig* K)
{xredisclient* C=NULL;testRequire(xredisClientConnect(P->Engine,&P->Address,0,K,&C),"tracking test client");P->Client=C;return C;}
static const xredisrequestresult* testClientResult(xfuture* F)
{testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK,"tracking result wait");xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"typed tracking outcome");return R.Value;}
static xfuture* testClientSubmit(xredisclient* C,cstr Name,cstr Value,int64 Timeout)
{xbytesview A[]={{(cbytes)Name,strlen(Name)},{(cbytes)Value,Value?strlen(Value):0}};xfuture* F=NULL;testRequire(xredisClientCallAsync(C,A,Value?2:1,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"ordinary fixture command");return F;}
static void testClientClose(redispeer* P,xredisclient* C)
{xfuture* F=xredisClientClosed(C);testRequire(F && xredisClientClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"tracking physical close");xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(C,&I) && I.State==XREDIS_CLIENT_CLOSED && !I.Requests && !I.CommandBytes && I.Accepted==I.Completed,"physical obligations retired");xredisClientRelease(C);xrtFutureDestroy(F);redisPeerStop(P);}

static const unsigned char trackingKeys[]="|1\r\n+source\r\n+test\r\n>2\r\n$10\r\ninvalidate\r\n*2\r\n$3\r\nk\0x\r\n$0\r\n\r\n";
typedef struct trackingPeer {xatomic32 Configured;bool CheckPrefix,CheckSync;unsigned ReplyMode;} trackingPeer;
static bool trackingCommand(redispeer* P,xredisreply* R,ptr Data)
{
    trackingPeer* T=Data;xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);xredisreplyvalue Root,Name;xredisReplyGet(R,I.Root,&Root);xredisReplyGet(R,Root.FirstChild,&Name);
    if(redisPeerName(Name,"BEFORE") || redisPeerName(Name,"AFTER")){
        bool First=redisPeerName(Name,"BEFORE");
        if(T->CheckSync){xfuture* F=(xfuture*)(uintptr_t)1;xbytesview A=XRT_BYTES_LITERAL("PING");
            testRequire(xredisClientTrackingCall(P->Client,&A,1,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"every Engine worker refuses synchronous observation");xrtClearError();}
        if(First)redisPeerSend(P,trackingKeys,sizeof(trackingKeys)-1);
        redisPeerSend(P,"+same\r\n",7);
        if(!First)redisPeerSend(P,trackingKeys,sizeof(trackingKeys)-1);
        return true;
    }
    if(redisPeerName(Name,"CLIENT")){
        xredisreplyvalue Sub;xredisReplyGet(R,Name.Next,&Sub);testRequire(redisPeerName(Sub,"TRACKING"),"tracking subcommand");
        if(T->CheckPrefix){xredisreplyvalue Arg;xredisReplyGet(R,Sub.Next,&Arg);testRequire(redisPeerName(Arg,"ON"),"ON snapshot");
            xredisReplyGet(R,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"REDIRECT"),"redirect keyword");xredisReplyGet(R,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"9223372036854775807"),"lossless signed64 client id");
            xredisReplyGet(R,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"PREFIX"),"prefix keyword");xredisReplyGet(R,Arg.Next,&Arg);testRequire(Arg.Data.Size==3 && !memcmp(Arg.Data.Data,"p\0x",3),"copied binary prefix");
            xredisReplyGet(R,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"BCAST"),"broadcast flag");xredisReplyGet(R,Arg.Next,&Arg);testRequire(redisPeerName(Arg,"NOLOOP") && Arg.Next==XREDIS_REPLY_NONE,"noloop final flag");}
        if(T->ReplyMode==1)redisPeerSend(P,"-ERR mode\r\n",11);
        else if(T->ReplyMode==2 || T->ReplyMode==4){if(T->ReplyMode==4)P->Blocked=true;redisPeerSend(P,"+wrong\r\n",8);}
        else if(T->ReplyMode==3)redisPeerSend(P,":1\r\n",4);
        else redisPeerSend(P,"+OK\r\n",5);
        xrtAtomic32FetchAdd(&T->Configured,1,XMEMORY_RELEASE);return true;
    }
    return false;
}
static xredisreply* trackingParse(const void* Wire,size_t Size)
{
    xredisreplyreader* Reader=NULL;testRequire(xredisReplyReaderCreate(NULL,&Reader),"event parser");xredisreply* R=NULL;size_t Used=0;
    testRequire(xredisReplyReaderFeed(Reader,(xbytesview){Wire,Size},&Used,&R)==XREDIS_REPLY_READY && Used==Size,"literal event framing");xredisReplyReaderDestroy(Reader);return R;
}
static void trackingDecode(void)
{
    struct {cstr Wire;xredistrackingstatus Status;xredistrackingeventkind Kind;} Cases[]={
        {">2\r\n+invalidate\r\n_\r\n",XREDIS_TRACKING_EVENT,XREDIS_TRACKING_FLUSH},
        {">2\r\n+invalidate\r\n*0\r\n",XREDIS_TRACKING_EVENT,XREDIS_TRACKING_KEYS},
        {">2\r\n+tracking-redir-broken\r\n:9223372036854775807\r\n",XREDIS_TRACKING_EVENT,XREDIS_TRACKING_REDIRECT_BROKEN},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n*-1\r\n",XREDIS_TRACKING_EVENT,XREDIS_TRACKING_FLUSH},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n*1\r\n$3\r\nkey\r\n",XREDIS_TRACKING_EVENT,XREDIS_TRACKING_KEYS},
        {">2\r\n+notice\r\n:1\r\n",XREDIS_TRACKING_OTHER,0},
        {"*2\r\n+invalidate\r\n*0\r\n",XREDIS_TRACKING_OTHER,0},
        {">2\r\n+invalidate\r\n:1\r\n",XREDIS_TRACKING_MALFORMED,0},
        {">3\r\n+invalidate\r\n*0\r\n:1\r\n",XREDIS_TRACKING_MALFORMED,0},
        {">2\r\n+invalidate\r\n*1\r\n$-1\r\n",XREDIS_TRACKING_MALFORMED,0},
        {">2\r\n+invalidate\r\n*1\r\n*0\r\n",XREDIS_TRACKING_MALFORMED,0},
        {">2\r\n+tracking-redir-broken\r\n:-1\r\n",XREDIS_TRACKING_MALFORMED,0},
        {"*2\r\n+message\r\n+__redis__:invalidate\r\n",XREDIS_TRACKING_MALFORMED,0}
    };
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){xredisreply* R=trackingParse(Cases[N].Wire,strlen(Cases[N].Wire));xredistrackingevent E,Before;memset(&E,0x55,sizeof(E));Before=E;
        xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(xredisTrackingEventRead(R,&E)==Cases[N].Status && xrtGetError()==Error,"recognized/other/malformed preserve ambient error");
        if(Cases[N].Status==XREDIS_TRACKING_EVENT)testRequire(E.Kind==Cases[N].Kind && E.Reply==R,"typed event");else testRequire(!memcmp(&E,&Before,sizeof(E)),"no partial decoder output");
        xrtClearError();xredisReplyRelease(R);
    }
    xredisreply* R=trackingParse(trackingKeys,sizeof(trackingKeys)-1);xredistrackingevent E;
    testRequire(xredisTrackingEventRead(R,&E)==XREDIS_TRACKING_EVENT && E.Keys==2,"binary and empty keys");xredisreplyvalue V;xredisReplyGet(R,E.FirstKey,&V);
    testRequire(V.Data.Size==3 && !memcmp(V.Data.Data,"k\0x",3),"binary key preserved");xredisReplyGet(R,V.Next,&V);testRequire(!V.Data.Size && V.Next==XREDIS_REPLY_NONE,"empty last key");
    unsigned char Saved[32];memcpy(Saved,R,sizeof(Saved));testRequire(xredisTrackingEventRead(R,(xredistrackingevent*)R)==XREDIS_TRACKING_ERROR && !memcmp(Saved,R,sizeof(Saved)),"reply-header output alias");xrtClearError();
    xredisreplyvalue Key;xredisReplyGet(R,E.FirstKey,&Key);testRequire(xredisTrackingEventRead(R,(xredistrackingevent*)Key.Data.Data)==XREDIS_TRACKING_ERROR && !memcmp(Key.Data.Data,"k\0x",3),"payload output alias");xrtClearError();
    xrtMemStatsEnable(true);xmemstats Before,After;xrtMemStatsGet(&Before);for(unsigned N=0;N<4096;++N)testRequire(xredisTrackingEventRead(R,&E)==XREDIS_TRACKING_EVENT,"warm decoder");xrtMemStatsGet(&After);xrtMemStatsEnable(false);
    testRequire(After.BlockAllocCalls==Before.BlockAllocCalls,"decoded views allocate nothing");xredisReplyRelease(R);
}
static const xredistrackingresult* trackingResult(xfuture* F)
{return (const xredistrackingresult*)testClientResult(F);}
static xfuture* trackingSubmit(xredisclient* C,cstr Name,int64 Timeout)
{xbytesview A={(cbytes)Name,strlen(Name)};xfuture* F=NULL;testRequire(xredisClientTrackingCallAsync(C,&A,1,Timeout,&F)==XREDIS_ADMISSION_ACCEPTED,"observed admission");return F;}
static xredistrackingpush trackingTake(xredisclient* C)
{
    xredistrackingpush P;double End=xrtTimer()+5;while(!xredisClientTrackingTake(C,&P)){testRequire(xrtTimer()<End,"ordered push deadline");xrtThreadYield();}return P;
}
static void trackingOrder(bool Before,bool Enable)
{
    redispeer P;redisPeerStart(&P);trackingPeer T={0};xrtAtomic32Init(&T.Configured,0);T.CheckSync=Enable;redispeerhook Hook={trackingCommand,&T};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.ReadBytes=1;K.ReadFrames=1;xredisclient* C=testClientConnect(&P,&K);
    if(Enable)testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"enable observations");
    xfuture* F=Enable?trackingSubmit(C,Before?"BEFORE":"AFTER",5000):testClientSubmit(C,Before?"BEFORE":"AFTER",NULL,5000);
    const xredisrequestresult* Result=testClientResult(F);testRequire(Result->Status==XREDIS_REQUEST_OK,"ordered request success");xredisreplyvalue V;xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(Result->Reply,&I);xredisReplyGet(Result->Reply,I.Root,&V);testRequire(redisPeerName(V,"same"),"identical business result for two wire orders");
    xredistrackingpush Push={0};
    if(Enable){Push=trackingTake(C);const xredistrackingresult* R=(const xredistrackingresult*)Result;
        testRequire(R->Sequence==(Before?2u:1u) && Push.Sequence==(Before?1u:2u),"actual parser order survives collecting reply before push");}
    else {double End=xrtTimer()+5;while(!xredisClientTakePush(C,&Push.Reply)){testRequire(xrtTimer()<End,"plain push deadline");xrtThreadYield();}}
    xredistrackingevent E;testRequire(xredisTrackingEventRead(Push.Reply,&E)==XREDIS_TRACKING_EVENT && E.Keys==2,"same invalidation in both wire orders");
    xredistrackingpush Sentinel;memset(&Sentinel,0x44,sizeof(Sentinel));xredistrackingpush Saved=Sentinel;
    if(Enable){xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(!xredisClientTrackingTake(C,&Sentinel) && !memcmp(&Sentinel,&Saved,sizeof(Sentinel)) && xrtGetError()==Error,"empty take leaves output and error");xrtClearError();}
    testClientClose(&P,C);testRequire(xredisTrackingEventRead(Push.Reply,&E)==XREDIS_TRACKING_EVENT && testClientResult(F)->Reply,"ordered result and event outlive all connection owners");xredisReplyRelease(Push.Reply);xrtFutureDestroy(F);
}
typedef struct trackingGate {xnetpost Post;xmutex Gate;xatomic32 Entered,Left;} trackingGate;
static void trackingBlocked(xnetworker* W,ptr Data)
{(void)W;trackingGate* G=Data;xrtAtomic32Store(&G->Entered,1,XMEMORY_RELEASE);xrtMutexLock(&G->Gate);xrtMutexUnlock(&G->Gate);xrtAtomic32Store(&G->Left,1,XMEMORY_RELEASE);}
static void trackingConfig(void)
{
    redispeer P;redisPeerStart(&P);trackingPeer T={0};xrtAtomic32Init(&T.Configured,0);T.CheckPrefix=true;redispeerhook Hook={trackingCommand,&T};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=testClientConnect(&P,NULL);xfuture* F=(xfuture*)(uintptr_t)1;xbytesview Ping=XRT_BYTES_LITERAL("PING");
    testRequire(xredisClientTrackingCallAsync(C,&Ping,1,5000,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"disabled observation rejects without future transfer");xrtClearError();
    xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();testRequire(xredisClientTrackingObserve(C,1)==XREDIS_ADMISSION_OVERLOADED && xrtGetError()==Error,"budget refusal does not allocate a diagnostic");xrtClearError();
    testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"budgeted observer");xredistrackinginfo Info={0};Info.Size=sizeof(Info);testRequire(xredisClientTrackingInfo(C,&Info) && Info.Enabled && Info.Rejected==1 && !Info.Reserved,"observer accounting");
    trackingGate Gate={0};xrtAtomic32Init(&Gate.Entered,0);xrtAtomic32Init(&Gate.Left,0);testRequire(xrtMutexInit(&Gate.Gate) && xrtMutexLock(&Gate.Gate) && xrtNetPostInit(&Gate.Post) && xrtNetPost(xrtNetEngineWorker(P.Engine,0),&Gate.Post,trackingBlocked,&Gate),"block actual client owner");redisPeerWait(&Gate.Entered,1);
    xredistrackingconfig K;xredisTrackingConfigInit(&K);unsigned char Text[]={'p',0,'x'};xbytesview Prefix={Text,sizeof(Text)};K.Flags=XREDIS_TRACKING_BCAST|XREDIS_TRACKING_NOLOOP|XREDIS_TRACKING_REDIRECT;K.Redirect=INT64_MAX;K.Prefixes=&Prefix;K.PrefixCount=1;
    testRequire(xredisClientTrackingConfigureAsync(C,&K,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"copied tracking configuration");memset(Text,'z',sizeof(Text));memset(&Prefix,0,sizeof(Prefix));memset(&K,0,sizeof(K));
    xrtMutexUnlock(&Gate.Gate);redisPeerWait(&Gate.Left,1);xrtMutexUnit(&Gate.Gate);testRequire(trackingResult(F)->Request.Status==XREDIS_REQUEST_OK && trackingResult(F)->Sequence==1,"typed configuration success");xrtFutureDestroy(F);
    xredisTrackingConfigInit(&K);F=(xfuture*)(uintptr_t)1;
    testRequire(xredisClientTrackingConfigureAsync(C,&K,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"unknown raw protocol cannot claim direct invalidations");xrtClearError();
    const uint32 Bad[]={32,XREDIS_TRACKING_OPTIN|XREDIS_TRACKING_OPTOUT,XREDIS_TRACKING_BCAST|XREDIS_TRACKING_OPTIN};
    for(size_t N=0;N<sizeof(Bad)/sizeof(*Bad);++N){K.Flags=Bad[N];F=(xfuture*)(uintptr_t)1;testRequire(xredisClientTrackingConfigureAsync(C,&K,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"invalid flags fail atomically");xrtClearError();}
    xredisTrackingConfigInit(&K);K.Prefixes=&Prefix;K.PrefixCount=1;testRequire(xredisClientTrackingConfigureAsync(C,&K,100,&F)==XREDIS_ADMISSION_ERROR,"prefix requires BCAST");xrtClearError();
    xredisTrackingConfigInit(&K);K.Enabled=false;K.Flags=XREDIS_TRACKING_NOLOOP;testRequire(xredisClientTrackingConfigureAsync(C,&K,100,&F)==XREDIS_ADMISSION_ERROR,"OFF cannot hide active options");xrtClearError();
    xrtMemStatsEnable(true);xmemstats B,A;xrtMemStatsGet(&B);for(unsigned N=0;N<2048;++N)testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED && xredisClientTrackingInfo(C,&Info),"idempotent warm observation");xrtMemStatsGet(&A);xrtMemStatsEnable(false);testRequire(A.BlockAllocCalls==B.BlockAllocCalls,"no repeated observation allocation");
    testClientClose(&P,C);
}
static void trackingConfigReplies(void)
{
    for(unsigned Mode=1;Mode<=4;++Mode){redispeer P;redisPeerStart(&P);trackingPeer T={0};xrtAtomic32Init(&T.Configured,0);T.ReplyMode=Mode;redispeerhook Hook={trackingCommand,&T};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
        xredisclient* C=testClientConnect(&P,NULL);testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"configuration reply observer");xredistrackingconfig K;xredisTrackingConfigInit(&K);K.Flags=XREDIS_TRACKING_REDIRECT;K.Redirect=1;xfuture* F=NULL;
        testRequire(xredisClientTrackingConfigureAsync(C,&K,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"configuration reply admission");redisPeerWait(&T.Configured,1);
        if(Mode==4){testRequire(xrtFutureCancel(F),"cancel configuration before malformed reply");testRequire(trackingResult(F)->Request.Status==XREDIS_REQUEST_CANCELLED && !trackingResult(F)->Sequence,"cancelled configuration result");redisPeerReleaseBlocked(&P);}
        else if(Mode==1){testRequire(trackingResult(F)->Request.Status==XREDIS_REQUEST_SERVER_ERROR && trackingResult(F)->Sequence==1,"server mode rejection stays explicit and ordered");xfuture* Next=trackingSubmit(C,"PING",5000);testRequire(trackingResult(Next)->Request.Status==XREDIS_REQUEST_OK && trackingResult(Next)->Sequence==2,"server rejection preserves connection FIFO");xrtFutureDestroy(Next);}
        if(Mode!=1){xfuture* Closed=xredisClientClosed(C);testRequire(xrtFutureWaitFor(Closed,5000)==XWAIT_OK,"malformed control closes physically");xrtFutureDestroy(Closed);xredisclientinfo I={0};I.Size=sizeof(I);testRequire(xredisClientInfo(C,&I) && I.CloseStatus==XREDIS_REQUEST_PROTOCOL,"expected simple OK enforced even for abandoned controls");
            if(Mode!=4)testRequire(trackingResult(F)->Request.Status==XREDIS_REQUEST_PROTOCOL && !trackingResult(F)->Sequence,"malformed control cannot confirm mode");}
        testClientClose(&P,C);xrtFutureDestroy(F);
    }
}
static void trackingStops(void)
{
    for(unsigned Cancel=0;Cancel<2;++Cancel){redispeer P;redisPeerStart(&P);xredisclient* C=testClientConnect(&P,NULL);testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"stop observer");
        xfuture* F=trackingSubmit(C,"HOLD",Cancel?5000:20);redisPeerWait(&P.Commands,1);if(Cancel)testRequire(xrtFutureCancel(F),"cancel observed sent request");
        const xredistrackingresult* R=trackingResult(F);testRequire(R->Request.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT) && !R->Sequence,"local completion publishes no reply order");
        redisPeerReleaseBlocked(&P);xfuture* Next=trackingSubmit(C,"PING",5000);testRequire(trackingResult(Next)->Request.Status==XREDIS_REQUEST_OK && trackingResult(Next)->Sequence==2 && !R->Sequence,"abandoned wire frame drains and cannot mutate prior result");
        testClientClose(&P,C);xrtFutureDestroy(F);xrtFutureDestroy(Next);
    }
}
static void trackingOOM(void)
{
    const size_t Limits[]={1,16,1024,1040,4096};
    for(size_t N=0;N<sizeof(Limits)/sizeof(*Limits);++N){redispeer P;redisPeerStart(&P);xredisclient* C=testClientConnect(&P,NULL);xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[N],&B) && xrtAllocationBudgetEnter(B,&Scope),"observer budget");
        xredisadmission A=xredisClientTrackingObserve(C,0);testRequire(A==XREDIS_ADMISSION_ACCEPTED || A==XREDIS_ADMISSION_ERROR,"observer OOM admission");testRequire(xrtAllocationBudgetLeave(&Scope),"observer budget leave");xrtClearError();
        xredistrackinginfo Info={0};Info.Size=sizeof(Info);testRequire(xredisClientTrackingInfo(C,&Info) && !Info.Preparing && !Info.Reserved && Info.Enabled==(A==XREDIS_ADMISSION_ACCEPTED),"observer allocation rollback");testClientClose(&P,C);
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"observer budget completely released");xrtAllocationBudgetFree(B);
    }
}
static void trackingCallOOM(void)
{
    redispeer P;redisPeerStart(&P);trackingPeer T={0};xrtAtomic32Init(&T.Configured,0);redispeerhook Hook={trackingCommand,&T};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=testClientConnect(&P,NULL);testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"query OOM observer");unsigned Accepted=0,Failed=0;
    for(size_t Limit=1;Limit<=1024;++Limit){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"query construction quota");
        xbytesview A=XRT_BYTES_LITERAL("BEFORE");xfuture* F=(xfuture*)(uintptr_t)1;xredisadmission Admission=xredisClientTrackingCallAsync(C,&A,1,5000,&F);testRequire(xrtAllocationBudgetLeave(&Scope),"query construction leave");xrtClearError();
        if(Admission==XREDIS_ADMISSION_ACCEPTED){testRequire(trackingResult(F)->Request.Status==XREDIS_REQUEST_OK && trackingResult(F)->Sequence,"observed result under allocation prefix");xredistrackingpush Push=trackingTake(C);xredisReplyRelease(Push.Reply);xrtFutureDestroy(F);++Accepted;}
        else {testRequire(Admission==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"construction failure preserves output");++Failed;}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+5;
        do {testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<End,"construction owners eventually retire");if(I.LiveCount)xrtThreadYield();}while(I.LiveCount);
        testRequire(!I.LiveBytes,"all requested construction bytes refunded");xrtAllocationBudgetFree(B);
    }
    testRequire(Accepted && Failed,"observed all construction prefixes through success");testClientClose(&P,C);
}
typedef struct trackingRace {xredisclient* Client;xatomic32 Done;} trackingRace;
static int32 trackingProducer(ptr Data)
{
    trackingRace* R=Data;
    for(unsigned N=0;N<16;++N){xfuture* F=trackingSubmit(R->Client,"BEFORE",5000);const xredistrackingresult* V=trackingResult(F);
        testRequire(V->Request.Status==XREDIS_REQUEST_OK && V->Sequence && !(V->Sequence&1u),"parallel producer ordered outcome");xrtFutureDestroy(F);}
    xredisClientRelease(R->Client);xrtAtomic32FetchAdd(&R->Done,1,XMEMORY_RELEASE);return 0;
}
static void trackingConcurrent(void)
{
    redispeer P;redisPeerStart(&P);trackingPeer T={0};xrtAtomic32Init(&T.Configured,0);redispeerhook Hook={trackingCommand,&T};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=testClientConnect(&P,NULL);testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"parallel observer");trackingRace R={C,{0}};xrtAtomic32Init(&R.Done,0);xthread* Threads[4];
    for(unsigned N=0;N<4;++N){testRequire(xredisClientRetain(C),"producer owner");Threads[N]=xrtThreadCreate(trackingProducer,&R,0);testRequire(Threads[N]!=NULL,"producer thread");}redisPeerWait(&R.Done,4);
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(Threads[N],5000)==XWAIT_OK,"producer join");xrtThreadDestroy(Threads[N]);}
    for(unsigned N=0;N<64;++N){xredistrackingpush Push=trackingTake(C);testRequire(Push.Sequence==2*N+1,"push FIFO and shared receive cursor under concurrent callers");xredisReplyRelease(Push.Reply);}
    xredistrackinginfo I={0};I.Size=sizeof(I);testRequire(xredisClientTrackingInfo(C,&I) && I.Sequence==128 && !I.Preparing && !I.Reserved,"shared counter exact after all worker frames");testClientClose(&P,C);
}
static void trackingExistingPushes(void)
{
    redispeer P;redisPeerStart(&P);xredisclient* C=testClientConnect(&P,NULL);xfuture* Old=testClientSubmit(C,"PUSH",NULL,5000);testRequire(testClientResult(Old)->Status==XREDIS_REQUEST_OK,"pre-observation data");
    testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_ACCEPTED,"activate observation epoch");xredistrackingpush Push=trackingTake(C);testRequire(!Push.Sequence,"existing queue precedes observed epoch");xredisReplyRelease(Push.Reply);
    xfuture* New=trackingSubmit(C,"PING",5000);testRequire(trackingResult(New)->Sequence==1,"first observed frame starts at one");testClientClose(&P,C);xrtFutureDestroy(Old);xrtFutureDestroy(New);
}

#ifndef XREDIS_TRACKING_TEST_MAIN
#define XREDIS_TRACKING_TEST_MAIN main
#endif
int XREDIS_TRACKING_TEST_MAIN(void)
{
    trackingDecode();trackingOrder(true,false);trackingOrder(false,false);trackingOrder(true,true);trackingOrder(false,true);trackingConfig();trackingConfigReplies();trackingStops();trackingOOM();trackingCallOOM();trackingExistingPushes();trackingConcurrent();
    puts("[PASS] tracking: exact receive order, copied binary configuration, typed invalidation/flush/redirection, aliases, deadlines, cancellation, zero-allocation reads and OOM");return 0;
}
