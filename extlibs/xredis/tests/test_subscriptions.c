#include <xredis/subscriptions.h>
#define XREDIS_SUBSCRIBER_TEST_MAIN subscriptionBaseCoverageReference
#include "test_subscriber.c"
#undef XREDIS_SUBSCRIBER_TEST_MAIN
#include "subscriptions_slot_vectors.h"
static xredissubscriptions* subscriptionsStore(void)
{xredissubscriptions* S=NULL;testRequire(xredisSubscriptionsCreate(NULL,&S),"desired store create");return S;}
static uint64 subscriptionsEdit(xredissubscriptions* S,xredispubsubcommand Op,const xbytesview* Names,size_t Count)
{uint64 Revision=UINT64_MAX;testRequire(xredisSubscriptionsApply(S,Op,Names,Count,&Revision)==XREDIS_ADMISSION_ACCEPTED,"atomic desired edit");return Revision;}
static bool subscriptionsHas(xredissubscriptionsnapshot* P,unsigned Family,xbytesview Name)
{
    xredissubscriptionsinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriptionSnapshotInfo(P,&I),"snapshot info");
    for(size_t N=0;N<I.Entries;++N){xredissubscriptionview V;testRequire(xredisSubscriptionSnapshotAt(P,N,&V),"snapshot entry");if((unsigned)V.Family==Family && V.Name.Size==Name.Size && (!Name.Size || !memcmp(V.Name.Data,Name.Data,Name.Size)))return true;}
    return false;
}
static void subscriptionsModel(void)
{
    xredissubscriptions* S=subscriptionsStore();bool Model[3][16]={{false}};uint64 Revision=0;uint32 Random=0x52ab718du;unsigned char NamesData[16][3];
    for(unsigned N=0;N<16;++N){NamesData[N][0]=(unsigned char)N;NamesData[N][1]=0;NamesData[N][2]=(unsigned char)(255-N);}
    for(unsigned Turn=0;Turn<4096;++Turn){Random=Random*1664525u+1013904223u;unsigned Op=Random%6,Family=Op%3;size_t Count=(Random>>8)%5;if(Op<3 && !Count)Count=1;
        xbytesview Names[4];unsigned Keys[4];for(size_t N=0;N<Count;++N){Random=Random*1664525u+1013904223u;Keys[N]=(Random>>12)%16;Names[N]=(xbytesview){NamesData[Keys[N]],Keys[N]?3:0};}
        bool Changed=false;if(!Count){for(unsigned N=0;N<16;++N){Changed|=Model[Family][N];Model[Family][N]=false;}}
        else for(size_t N=0;N<Count;++N){bool Want=Op<3;Changed|=Model[Family][Keys[N]]!=Want;Model[Family][Keys[N]]=Want;}
        if(Changed)++Revision;
        testRequire(subscriptionsEdit(S,(xredispubsubcommand)Op,Names,Count)==Revision,"revision iff semantic set changes");
        xredissubscriptionsnapshot* P=NULL;testRequire(xredisSubscriptionsSnapshot(S,&P),"constant-time snapshot ref");size_t Expected=0;
        for(unsigned F=0;F<3;++F)for(unsigned N=0;N<16;++N){xbytesview Name={NamesData[N],N?3:0};testRequire(subscriptionsHas(P,F,Name)==Model[F][N],"independent desired-set model");Expected+=Model[F][N];}
        xredissubscriptionsinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriptionSnapshotInfo(P,&I) && I.Entries==Expected && I.Revision==Revision,"exact snapshot cardinality");xredisSubscriptionSnapshotRelease(P);
    }
    xredisSubscriptionsRelease(S);
}
static void subscriptionsSnapshots(void)
{
    xredissubscriptions* S=subscriptionsStore();unsigned char Data[]={'a',0,'b'};xbytesview N={Data,sizeof(Data)};subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&N,1);
    xredissubscriptionsnapshot *Old=NULL,*Same=NULL,*Next=NULL;testRequire(xredisSubscriptionsSnapshot(S,&Old) && xredisSubscriptionsSnapshot(S,&Same) && Old==Same,"snapshots share immutable publication");
    memset(Data,0xcc,sizeof(Data));xbytesview Other=XRT_BYTES_LITERAL("new");subscriptionsEdit(S,XREDIS_PUBSUB_CMD_PSUBSCRIBE,&Other,1);testRequire(xredisSubscriptionsSnapshot(S,&Next) && Next!=Old,"new revision snapshot");
    xredissubscriptionview V;testRequire(xredisSubscriptionSnapshotAt(Old,0,&V) && V.Name.Size==3 && !memcmp(V.Name.Data,"a\0b",3),"input copied once");const unsigned char* Original=V.Name.Data;
    bool Shared=false;xredissubscriptionsinfo I={0};I.Size=sizeof(I);xredisSubscriptionSnapshotInfo(Next,&I);
    for(size_t K=0;K<I.Entries;++K){testRequire(xredisSubscriptionSnapshotAt(Next,K,&V),"shared entry view");if(V.Family==XREDIS_PUBSUB_CMD_SUBSCRIBE){Shared=V.Name.Data==Original;}}
    testRequire(Shared,"unchanged names are not recopied");subscriptionsEdit(S,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0);xredisSubscriptionsRelease(S);
    testRequire(subscriptionsHas(Old,0,(xbytesview)XRT_BYTES_LITERAL("a\0b")) && subscriptionsHas(Next,0,(xbytesview)XRT_BYTES_LITERAL("a\0b")),"old names survive all later owners");
    xredissubscriptionview Unchanged={(xredispubsubcommand)99,{NULL,999}},Before=Unchanged;testRequire(!xredisSubscriptionSnapshotAt(Old,SIZE_MAX,&Unchanged) && !memcmp(&Before,&Unchanged,sizeof(Before)),"end preserves view");
    xredisSubscriptionSnapshotRelease(Old);xredisSubscriptionSnapshotRelease(Same);xredisSubscriptionSnapshotRelease(Next);
}
static void subscriptionsOOM(void)
{
    unsigned Failed=0,Accepted=0;
    for(size_t Limit=1;Limit<=768;++Limit){xredissubscriptions* S=subscriptionsStore();xbytesview Seed=XRT_BYTES_LITERAL("seed");subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Seed,1);xredissubscriptionsnapshot* Old=NULL;testRequire(xredisSubscriptionsSnapshot(S,&Old),"OOM old snapshot");
        xbytesview Added[]={XRT_BYTES_LITERAL("one"),XRT_BYTES_LITERAL("two"),XRT_BYTES_LITERAL("one"),{NULL,0}};xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"desired edit quota");
        uint64 Revision=123456;xredisadmission A=xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Added,4,&Revision);testRequire(xrtAllocationBudgetLeave(&Scope),"edit scope leave");
        xredissubscriptionsnapshot* Now=NULL;testRequire(xredisSubscriptionsSnapshot(S,&Now),"post-edit snapshot");
        if(A==XREDIS_ADMISSION_ERROR){++Failed;testRequire(Revision==123456 && Now==Old,"every failed allocation preserves whole publication");xrtClearError();}
        else {++Accepted;testRequire(A==XREDIS_ADMISSION_ACCEPTED && Revision==2 && Now!=Old,"fully constructed publication only");}
        xredisSubscriptionSnapshotRelease(Now);xredisSubscriptionSnapshotRelease(Old);xredisSubscriptionsRelease(S);xallocationbudgetinfo I={0};I.Size=sizeof(I);
        testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"all edit allocations returned");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Accepted,"all observed edit allocation prefixes through success");
}
static xredisclient* subscriptionsClient(redispeer* P)
{
    xredisclient* C=NULL;testRequire(xredisClientConnect(P->Engine,&P->Address,0,NULL,&C),"fresh restoration transport");P->Client=C;xfuture* F=xredisClientReady(C);testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"fresh ready");xrtFutureDestroy(F);return C;
}
static xredissubscriptionreplayresult subscriptionsDrive(xredissubscriptionreplay* R)
{
    double End=xrtTimer()+10;for(;;){xredissubscriptionstep S=xredisSubscriptionReplayStep(R);testRequire(S!=XREDIS_SUBSCRIPTIONS_STEP_ERROR && xrtTimer()<End,"bounded replay drive");
        if(S==XREDIS_SUBSCRIPTIONS_DONE)break;
        xfuture* F=xredisSubscriptionReplayPending(R);if(F){testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"native replay observer");xrtFutureDestroy(F);}else xrtThreadYield();}
    xredissubscriptionreplayresult Result={0};Result.Size=sizeof(Result);testRequire(xredisSubscriptionReplayResult(R,&Result),"replay result");return Result;
}
static void subscriptionsFill(xredissubscriptions* Store)
{
    xbytesview A[]={XRT_BYTES_LITERAL("a\0b"),XRT_BYTES_LITERAL("a\0b"),{NULL,0}},B[]={XRT_BYTES_LITERAL("p*"),{NULL,0}},C=XRT_BYTES_LITERAL("s");
    subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,A,3);subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_PSUBSCRIBE,B,2);subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SSUBSCRIBE,&C,1);
}
static void subscriptionsRestore(bool Resp3)
{
    xredissubscriptions* Store=subscriptionsStore();subscriptionsFill(Store);
    for(unsigned Generation=0;Generation<2;++Generation){if(Generation){xbytesview Removed=XRT_BYTES_LITERAL("a\0b");subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,&Removed,1);}
        redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=Resp3;S.Messages=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
        xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=1;K.MaxBatchBytes=32;xredissubscriptionreplay* R=NULL;
        testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"capture desired generation");xfuture* Refused=(xfuture*)(uintptr_t)1;testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_PING,NULL,0,100,&Refused)==XREDIS_ADMISSION_OVERLOADED && Refused==(xfuture*)(uintptr_t)1,"replay owns control admission");xredissubscriptionreplayresult Result=subscriptionsDrive(R);
        testRequire(Result.Status==XREDIS_SUBSCRIPTIONS_OK && Result.Acknowledged==(Generation?4u:5u) && Result.Batches==Result.Acknowledged && Result.Subscriptions==(Generation?3u:4u) && Result.ShardSubscriptions==1,"exact restored ACKs and count domains");
        size_t Messages=0;xredisreply* Reply=NULL;double End=xrtTimer()+10;while(Messages<Result.Acknowledged){if(xredisClientTakePush(C,&Reply)){++Messages;xredisReplyRelease(Reply);}else {testRequire(xrtTimer()<End,"messages arriving during restore");xrtThreadYield();}}
        xfuture* Ping=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,1000,&Ping)==XREDIS_ADMISSION_ACCEPTED && subResult(Ping)->Request.Status==XREDIS_REQUEST_OK,"completed replay releases control claim");xrtFutureDestroy(Ping);
        xredisSubscriptionReplayDestroy(R);subClose(&P,C);
    }
    xredisSubscriptionsRelease(Store);
}
static void subscriptionsStop(unsigned Mode)
{
    xredissubscriptions* Store=subscriptionsStore();subscriptionsFill(Store);redispeer P;redisPeerStart(&P);subpeer S={0};S.Hold=Mode<3;S.Malformed=Mode==3?3:Mode==4?1:0;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=1;K.Timeout=Mode==2?0:5000;if(Mode==5)K.MaxBatchBytes=1;xredissubscriptionreplay* R=NULL;
    testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"stopped replay admission");
    if(Mode<2){testRequire(xredisSubscriptionReplayStep(R)==XREDIS_SUBSCRIPTIONS_WAIT,"one control in flight");redisPeerWait(&P.Commands,1);
        if(!Mode)testRequire(xredisSubscriptionReplayCancel(R),"logical cancellation closes partial client");else subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0);}
    xredissubscriptionreplayresult V=subscriptionsDrive(R);const xredissubscriptionstatus Expected[]={XREDIS_SUBSCRIPTIONS_CANCELLED,XREDIS_SUBSCRIPTIONS_STALE,XREDIS_SUBSCRIPTIONS_TIMEOUT,XREDIS_SUBSCRIPTIONS_SERVER_ERROR,XREDIS_SUBSCRIPTIONS_PROTOCOL,XREDIS_SUBSCRIPTIONS_LIMIT};
    testRequire(V.Status==Expected[Mode],"typed failure distinct from success");
    xfuture* Closed=xredisClientClosed(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"failed replay retires partial transport");xredisClientRelease(C);redisPeerStop(&P);
    xredissubscriptionreplayresult After={0};After.Size=sizeof(After);testRequire(xredisSubscriptionReplayResult(R,&After) && !memcmp(&After,&V,sizeof(V)),"terminal result survives physical owners");
    if(Mode==3){xredisreplyvalue Error;testRequire(V.ErrorReply && xredisReplyGet(V.ErrorReply,0,&Error) && Error.Type==XREDIS_REPLY_SIMPLE_ERROR,"retained server error");}
    xrtFutureDestroy(Closed);xredisSubscriptionReplayDestroy(R);xredisSubscriptionsRelease(Store);
}
static void subscriptionsCreateOOM(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);xredissubscriptions* Store=subscriptionsStore();subscriptionsFill(Store);
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=1;K.Registry.MaxSubscriptions=8;K.Registry.MaxNameBytes=128;unsigned Failed=0;
    for(size_t Limit=1;Limit<=2048;++Limit){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&Scope),"replay construction budget");
        xredissubscriptionreplay* R=(xredissubscriptionreplay*)(uintptr_t)1;xredisadmission A=xredisSubscriptionReplayCreate(Store,C,&K,&R);testRequire(xrtAllocationBudgetLeave(&Scope),"replay construction leave");
        if(A!=XREDIS_ADMISSION_ACCEPTED){++Failed;testRequire(A==XREDIS_ADMISSION_ERROR && R==(xredissubscriptionreplay*)(uintptr_t)1,"failed creation preserves ownership and output");xrtClearError();}
        else{xredisSubscriptionReplayDestroy(R);subClose(&P,C);}
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"creation rollback/physical close returns quota");xrtAllocationBudgetFree(B);
        if(A==XREDIS_ADMISSION_ACCEPTED){testRequire(Failed!=0,"construction prefixes observed before success");xredisSubscriptionsRelease(Store);return;}
    }
    testRequire(false,"replay quota sweep must reach success");
}

static void subscriptionsRequestOOM(void)
{
    const size_t Limits[]={1,64,128,256,512,768,1024,1536,2048};unsigned Failed=0,Accepted=0;
    for(size_t T=0;T<sizeof(Limits)/sizeof(*Limits);++T){xredissubscriptions* Store=subscriptionsStore();xbytesview Name=XRT_BYTES_LITERAL("x");subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);
        redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,NULL,&R)==XREDIS_ADMISSION_ACCEPTED,"request OOM replay");
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Limits[T],&B) && xrtAllocationBudgetEnter(B,&Scope),"native admission quota");xredissubscriptionstep Step=xredisSubscriptionReplayStep(R);testRequire(xrtAllocationBudgetLeave(&Scope),"native admission quota leave");
        xredissubscriptionreplayresult V=subscriptionsDrive(R);if(Step==XREDIS_SUBSCRIPTIONS_DONE){++Failed;testRequire(V.Status==XREDIS_SUBSCRIPTIONS_MEMORY && !xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"typed memory failure sends no control");xrtClearError();}
        else {++Accepted;testRequire(V.Status==XREDIS_SUBSCRIPTIONS_OK && V.Acknowledged==1,"fully admitted control remains valid");}
        xredisSubscriptionReplayDestroy(R);subClose(&P,C);xredisSubscriptionsRelease(Store);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"native request quota retires physically");xrtAllocationBudgetFree(B);
    }
    testRequire(Failed && Accepted,"both request quota branches exercised");
}
static void subscriptionsOwnerOOM(void)
{
    xredissubscriptions* Store=subscriptionsStore();xbytesview Name=XRT_BYTES_LITERAL("x");subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.Timeout=XRT_WAIT_FOREVER;xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"owner OOM replay");
    subbudget B={0};xrtAtomic32Init(&B.Armed,0);xrtAtomic32Init(&B.Left,0);testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetEnginePost(P.Engine,0,subBudgetArm,&B),"arm native entry OOM");redisPeerWait(&B.Armed,1);
    testRequire(subscriptionsDrive(R).Status==XREDIS_SUBSCRIPTIONS_MEMORY && !xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"worker registry OOM stays typed and not sent");
    testRequire(xrtNetEnginePost(P.Engine,0,subBudgetLeave,&B),"leave owning-worker quota");redisPeerWait(&B.Left,1);xredisSubscriptionReplayDestroy(R);subClose(&P,C);xredisSubscriptionsRelease(Store);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveCount && !I.LiveBytes,"owner OOM leaves no duty");xrtAllocationBudgetFree(B.Budget);
}
static void subscriptionsDestroyPending(void)
{
    xredissubscriptions* Store=subscriptionsStore();subscriptionsFill(Store);redispeer P;redisPeerStart(&P);subpeer S={0};S.Hold=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);xredissubscriptionreplay* R=NULL;
    testRequire(xredisSubscriptionReplayCreate(Store,C,NULL,&R)==XREDIS_ADMISSION_ACCEPTED && xredisSubscriptionReplayStep(R)==XREDIS_SUBSCRIPTIONS_WAIT,"pending replay before destruction");redisPeerWait(&P.Commands,1);xfuture* F=xredisSubscriptionReplayPending(R);testRequire(F!=NULL,"retain actual pending native result");xredisSubscriptionsRelease(Store);xredisSubscriptionReplayDestroy(R);
    testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"native result outlives replay and desired set");subClose(&P,C);xfutureresult V;testRequire(xrtFutureResult(F,&V) && V.State==XFUTURE_RESOLVED && V.Value,"retained native completion");xrtFutureDestroy(F);
}
static void subscriptionsEdges(void)
{
    for(unsigned Case=0;Case<4;++Case){xredissubscriptions* Store=subscriptionsStore();xbytesview Name=XRT_BYTES_LITERAL("x");if(Case)subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);
        redispeer P;redisPeerStart(&P);subpeer S={0};S.Hold=Case==3;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
        xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchBytes=Case==1?25:26;xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"edge replay");
        if(Case==3){testRequire(xredisSubscriptionReplayStep(R)==XREDIS_SUBSCRIPTIONS_WAIT,"pending native cancellation");redisPeerWait(&P.Commands,1);xfuture* F=xredisSubscriptionReplayPending(R);testRequire(F && xrtFutureCancel(F),"cancel actual native observer");testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"native cancellation resolves");xrtFutureDestroy(F);}
        xredissubscriptionreplayresult V=subscriptionsDrive(R);xredissubscriptionstatus Expected=Case==1?XREDIS_SUBSCRIPTIONS_LIMIT:Case==3?XREDIS_SUBSCRIPTIONS_CANCELLED:XREDIS_SUBSCRIPTIONS_OK;
        testRequire(V.Status==Expected && V.Acknowledged==(Case==2?1u:0u) && (Case || !V.Batches),"empty replay and exact encoded-byte boundary");xredisSubscriptionReplayDestroy(R);subClose(&P,C);xredisSubscriptionsRelease(Store);
    }
}
static void subscriptionsLimits(void)
{
    xredissubscriptionsconfig K;xredisSubscriptionsConfigInit(&K);K.MaxSubscriptions=2;K.MaxNameBytes=4;K.MaxChangeBytes=8;xredissubscriptions* S=NULL;testRequire(xredisSubscriptionsCreate(&K,&S),"small desired limits");
    xbytesview A=XRT_BYTES_LITERAL("a");subscriptionsEdit(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,&A,1);xredissubscriptionsnapshot *Before=NULL,*After=NULL;xredisSubscriptionsSnapshot(S,&Before);uint64 Revision=333;
    xbytesview Extra[]={XRT_BYTES_LITERAL("b"),XRT_BYTES_LITERAL("c")};testRequire(xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Extra,2,&Revision)==XREDIS_ADMISSION_OVERLOADED && Revision==333,"distinct name cap atomic");
    Extra[0]=(xbytesview)XRT_BYTES_LITERAL("long");testRequire(xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Extra,1,&Revision)==XREDIS_ADMISSION_OVERLOADED,"stored name-byte cap");
    Extra[0]=(xbytesview)XRT_BYTES_LITERAL("123456789");testRequire(xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_SUBSCRIBE,Extra,1,&Revision)==XREDIS_ADMISSION_OVERLOADED,"input scan/copy byte cap");
    testRequire(xredisSubscriptionsApply(S,XREDIS_PUBSUB_CMD_PING,NULL,0,&Revision)==XREDIS_ADMISSION_ERROR && Revision==333,"local intent excludes PING");xrtClearError();
    testRequire(xredisSubscriptionsSnapshot(S,&After) && Before==After,"all refusals preserve entire snapshot");xredisSubscriptionSnapshotRelease(Before);xredisSubscriptionSnapshotRelease(After);xredisSubscriptionsRelease(S);
}
typedef struct subscriptionrace {xredissubscriptions* Store;xatomic32 Start,Done;} subscriptionrace;
static int32 subscriptionsWriter(ptr Data)
{
    subscriptionrace* R=Data;redisPeerWait(&R->Start,1);xbytesview Name=XRT_BYTES_LITERAL("parallel\0name");
    for(unsigned N=0;N<256;++N){uint64 V;for(;;){xredisadmission A=xredisSubscriptionsApply(R->Store,(N&1u)?XREDIS_PUBSUB_CMD_UNSUBSCRIBE:XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&V);if(A==XREDIS_ADMISSION_ACCEPTED)break;testRequire(A==XREDIS_ADMISSION_OVERLOADED,"only serialized construction can defer edit");xrtThreadYield();}}
    xrtAtomic32FetchAdd(&R->Done,1,XMEMORY_RELEASE);return 0;
}
static int32 subscriptionsReader(ptr Data)
{
    subscriptionrace* R=Data;redisPeerWait(&R->Start,1);uint64 Previous=0;
    for(unsigned N=0;N<1024;++N){xredissubscriptionsnapshot* P=NULL;testRequire(xredisSubscriptionsSnapshot(R->Store,&P),"concurrent snapshot acquire");xredissubscriptionsinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriptionSnapshotInfo(P,&I) && I.Revision>=Previous && I.Entries<=1,"no partial publication or backwards revision");Previous=I.Revision;
        if(I.Entries){xredissubscriptionview V;testRequire(xredisSubscriptionSnapshotAt(P,0,&V) && V.Name.Size==13 && !memcmp(V.Name.Data,"parallel\0name",13),"stable concurrent payload");}xredisSubscriptionSnapshotRelease(P);}
    xrtAtomic32FetchAdd(&R->Done,1,XMEMORY_RELEASE);return 0;
}
static void subscriptionsConcurrent(void)
{
    subscriptionrace R;R.Store=subscriptionsStore();xrtAtomic32Init(&R.Start,0);xrtAtomic32Init(&R.Done,0);xthread* T[4];
    for(unsigned N=0;N<4;++N){T[N]=xrtThreadCreate(N<2?subscriptionsWriter:subscriptionsReader,&R,0);testRequire(T[N]!=NULL,"desired state thread");}xrtAtomic32Store(&R.Start,1,XMEMORY_RELEASE);redisPeerWait(&R.Done,4);
    for(unsigned N=0;N<4;++N){testRequire(xrtThreadWaitFor(T[N],10000)==XWAIT_OK,"state thread join");xrtThreadDestroy(T[N]);}xredisSubscriptionsRelease(R.Store);
}
static bool subscriptionsSlotPeer(redispeer* P,xredisreply* Reply,ptr Data)
{
    xredisreplyvalue Root,Word;xredisReplyGet(Reply,0,&Root);xredisReplyGet(Reply,Root.FirstChild,&Word);
    if(redisPeerName(Word,"SSUBSCRIBE")){unsigned Slot=UINT32_MAX;
        for(size_t Node=Word.Next;Node!=XREDIS_REPLY_NONE;){xredisreplyvalue V;xredisReplyGet(Reply,Node,&V);unsigned Expected=UINT32_MAX;
            for(size_t N=0;N<17;++N)if(V.Data.Size==subscriptionSlotVectors[N].Bytes && (!V.Data.Size || !memcmp(V.Data.Data,subscriptionSlotVectors[N].Data,V.Data.Size))){Expected=subscriptionSlotVectors[N].Slot;break;}
            testRequire(Expected!=UINT32_MAX && (Slot==UINT32_MAX || Expected==Slot),"one shard slot per wire command, independent oracle");Slot=Expected;Node=V.Next;}
    }
    return subCommand(P,Reply,Data);
}
static void subscriptionsShardSlots(void)
{
    xredissubscriptions* Store=subscriptionsStore();xbytesview Views[17];for(size_t N=0;N<17;++N)Views[N]=(xbytesview){subscriptionSlotVectors[N].Data,subscriptionSlotVectors[N].Bytes};subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SSUBSCRIBE,Views,17);
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;redispeerhook Hook={subscriptionsSlotPeer,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=4;xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED,"slot-grouped replay");
    xredissubscriptionreplayresult V=subscriptionsDrive(R);testRequire(V.Status==XREDIS_SUBSCRIPTIONS_OK && V.Acknowledged==17 && V.Subscriptions==0 && V.ShardSubscriptions==17 && V.Batches<17,"same-slot names batch, different slots separate");xredisSubscriptionReplayDestroy(R);subClose(&P,C);xredisSubscriptionsRelease(Store);
}
#if defined(XREDIS_SUBSCRIPTIONS_WHITEBOX)
static void subscriptionsWhitebox(void)
{
    for(size_t N=0;N<sizeof(subscriptionSlotVectors)/sizeof(*subscriptionSlotVectors);++N)testRequire(xredisSubscriptionSlot((xbytesview){subscriptionSlotVectors[N].Data,subscriptionSlotVectors[N].Bytes})==subscriptionSlotVectors[N].Slot,"CRC16/first hash-tag independent vectors");
    xredissubscriptions* Store=subscriptionsStore();Store->Current->Revision=UINT64_MAX;uint64 V=1;
    testRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0,&V)==XREDIS_ADMISSION_ACCEPTED && V==UINT64_MAX,"no-op permitted at exhausted revision");
    xbytesview Name=XRT_BYTES_LITERAL("x");V=9;testRequire(xredisSubscriptionsApply(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&V)==XREDIS_ADMISSION_ERROR && V==9,"revision never wraps");xrtClearError();xredisSubscriptionsRelease(Store);
    Store=subscriptionsStore();subscriptionsEdit(Store,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1);redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subscriptionsClient(&P);
    xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,NULL,&R)==XREDIS_ADMISSION_ACCEPTED,"deadline probe replay");subhold H;subHoldInit(&H);testRequire(xrtNetEnginePost(P.Engine,0,subHoldWorker,&H),"hold native deadline owner");redisPeerWait(&H.Entered,1);
    testRequire(xredisSubscriptionReplayStep(R)==XREDIS_SUBSCRIPTIONS_WAIT,"queue under held owner");xrtMutexLock(&C->Gate);testRequire(C->SubmitHead && C->SubmitHead->Deadline==R->Deadline,"native controls keep exact original deadline");xrtMutexUnlock(&C->Gate);xrtAtomic32Store(&H.Released,1,XMEMORY_RELEASE);
    testRequire(subscriptionsDrive(R).Status==XREDIS_SUBSCRIPTIONS_OK,"deadline probe finishes");xredisSubscriptionReplayDestroy(R);subClose(&P,C);xredisSubscriptionsRelease(Store);
}
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void subscriptionsTLS(void)
{
    xredissubscriptions* Store=subscriptionsStore();subscriptionsFill(Store);redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);redistlspeer TLS={0};xrtAtomic32Init(&TLS.Verified,0);redisTlsPeerCredentials(&TLS,&P,false,false);xredisclienttlsconfig Secure;redisTlsPeerConfig(&TLS,&Secure);Secure.WriteBytes=3;
    xredisclient* C=NULL;testRequire(xredisClientTlsConnect(P.Engine,&P.Address,0,NULL,NULL,&Secure,&C),"TLS recovery factory");P.Client=C;xfuture* F=xredisClientReady(C);testRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK,"TLS ready");xrtFutureDestroy(F);
    xredissubscriptionreplayconfig K;xredisSubscriptionReplayConfigInit(&K);K.MaxBatchNames=2;xredissubscriptionreplay* R=NULL;testRequire(xredisSubscriptionReplayCreate(Store,C,&K,&R)==XREDIS_ADMISSION_ACCEPTED && subscriptionsDrive(R).Status==XREDIS_SUBSCRIPTIONS_OK,"three-byte TLS restoration");xredisSubscriptionReplayDestroy(R);subClose(&P,C);redisTlsPeerCredentialsDrop(&TLS);xredisSubscriptionsRelease(Store);
}
#endif
int main(void)
{
    subscriptionsModel();subscriptionsLimits();subscriptionsEdges();subscriptionsConcurrent();subscriptionsShardSlots();subscriptionsSnapshots();subscriptionsOOM();subscriptionsRestore(false);subscriptionsRestore(true);for(unsigned N=0;N<6;++N)subscriptionsStop(N);subscriptionsCreateOOM();subscriptionsRequestOOM();subscriptionsOwnerOOM();subscriptionsDestroyPending();
#if defined(XREDIS_SUBSCRIPTIONS_WHITEBOX)
    subscriptionsWhitebox();
#endif
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
    subscriptionsTLS();
#endif
    puts("[PASS] desired subscriptions: atomic versioned sets, shared immutable names, snapshot lifetime/OOM, bounded multi-generation replay, stale/cancel/timeout/error/limit and creation rollback");return 0;
}
