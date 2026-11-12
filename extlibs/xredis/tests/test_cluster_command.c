#include "test.h"
#include "support/cluster_command_peer.h"
typedef struct qpbarrier {xatomic32 Entered,Continue;} qpbarrier;
static void qpBlock(xnetworker* W,ptr Data)
{(void)W;qpbarrier* B=Data;xrtAtomic32Store(&B->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&B->Continue,XMEMORY_ACQUIRE))xrtSleep(1);}
static void qpBudgetDrain(xallocationbudget* B)
{double Due=xrtTimer()+10;for(;;){xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && xrtTimer()<Due,"command budget drain");if(!I.LiveBytes && !I.LiveCount)return;xrtSleep(1);}}
static void qpBasic(void)
{
    for(unsigned Warm=0;Warm<2;++Warm){rffixture* F=qpStart();if(!Warm)xrtAtomic32Store(&F->Nodes[0].Mode,RF_HOLD,XMEMORY_RELEASE);xredisclusterclient* C=cfCreate(F,NULL);qpbarrier Gate={0};
        if(Warm){cfReady(C);testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,qpBlock,&Gate),"warm input barrier");redisPeerWait(&Gate.Entered,1);}else redisPeerWait(&F->Nodes[0].Queries,1);
        xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&B) && xrtAllocationBudgetEnter(B,&Scope),"catalogue owner budget");xrediscommandcatalog* K=qpCatalogue(Warm,NULL);testRequire(xrtAllocationBudgetLeave(&Scope),"catalogue budget leave");
        unsigned char Name[]="GET",Key[]="owned\0{22179}";xbytesview A[]={{Name,3},{Key,13}};xfuture* R=NULL;
        testRequire(xredisClusterClientCallCommand(C,K,A,2,97,5000,&R)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"warm/cold auto slot");memset(Name,'?',3);memset(Key,'?',13);memset(A,0,sizeof(A));xredisCommandCatalogRelease(K);
        xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveBytes && !I.LiveCount,"pending command retained catalogue");xrtAllocationBudgetFree(B);
        if(Warm)xrtAtomic32Store(&Gate.Continue,1,XMEMORY_RELEASE);else rfRelease(F,0);
        qpValue(R);cfStop(F,C);qpValue(R);xrtFutureDestroy(R);}
}
static void qpRefusals(void)
{
    rffixture* F=qpStart();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrediscommandcatalog* K=qpCatalogue(1,NULL);uint64 Accepted=cfInfo(C).Accepted;uint32 Business=xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE),Auth=xrtAtomic32Load(&F->Base->Auth,XMEMORY_ACQUIRE);
    xbytesview Missing[]={XRT_BYTES_LITERAL("__unknown__"),XRT_BYTES_LITERAL("key")},Exotic[]={XRT_BYTES_LITERAL("sort"),XRT_BYTES_LITERAL("key")},Cross[]={XRT_BYTES_LITERAL("mset"),XRT_BYTES_LITERAL("a{a}"),XRT_BYTES_LITERAL("1"),XRT_BYTES_LITERAL("b{b}"),XRT_BYTES_LITERAL("2")},Arity[]={XRT_BYTES_LITERAL("get")},NoKey[]={XRT_BYTES_LITERAL("ping")};
    struct {xbytesview* A;size_t Count;xredisclustercommandadmission Status;} Cases[]={{Missing,2,XREDIS_CLUSTER_COMMAND_UNKNOWN},{Exotic,2,XREDIS_CLUSTER_COMMAND_UNSUPPORTED},{Cross,5,XREDIS_CLUSTER_COMMAND_CROSS_SLOT},{Arity,1,XREDIS_CLUSTER_COMMAND_ARGUMENTS},{NoKey,1,XREDIS_CLUSTER_COMMAND_KEYLESS},{NULL,0,XREDIS_CLUSTER_COMMAND_ARGUMENTS}};
    xrtSetErrorKind(XERR_STATE);const xerror* Error=xrtGetError();xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"refusal exhausted quota");
    for(size_t I=0;I<sizeof(Cases)/sizeof(*Cases);++I){xfuture* R=(void*)(uintptr_t)1;testRequire(xredisClusterClientCallCommand(C,K,Cases[I].A,Cases[I].Count,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&R)==Cases[I].Status && R==(void*)(uintptr_t)1 && xrtGetError()==Error,"typed refusal or future/error changed");}
    testRequire(xrtAllocationBudgetLeave(&Scope),"refusal scope leave");xallocationbudgetinfo BI={0};BI.Size=sizeof(BI);testRequire(xrtAllocationBudgetInfo(B,&BI) && !BI.PeakBytes && !BI.Rejections,"metadata refusals allocated");xrtAllocationBudgetFree(B);xrtClearError();
    testRequire(cfInfo(C).Accepted==Accepted && xrtAtomic32Load(&F->Base->Business,XMEMORY_ACQUIRE)==Business && xrtAtomic32Load(&F->Base->Auth,XMEMORY_ACQUIRE)==Auth,"metadata refusal submitted/authorized business");
    xfuture* Ping=NULL;testRequire(xredisClusterClientCallCommand(C,K,NoKey,1,1,-1,&Ping)==XREDIS_CLUSTER_COMMAND_ACCEPTED && cfWait(Ping)->Route.Query.Status==XREDIS_REQUEST_OK,"explicit keyless policy/default timeout");xrtFutureDestroy(Ping);
    xbytesview Empty[]={XRT_BYTES_LITERAL("GET"),{NULL,0}};xfuture* EmptyFuture=NULL;testRequire(xredisClusterClientCallCommand(C,K,Empty,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&EmptyFuture)==XREDIS_CLUSTER_COMMAND_ACCEPTED,"empty key was treated as keyless");const xredisclusterclientresult* EmptyResult=cfWait(EmptyFuture);xredisreplyvalue EmptyValue;testRequire(EmptyResult->Status==XREDIS_CLUSTER_CLIENT_COMPLETE && EmptyResult->Route.Query.Status==XREDIS_REQUEST_OK && xredisReplyGet(EmptyResult->Route.Query.Reply,0,&EmptyValue) && EmptyValue.Type==XREDIS_REPLY_BULK_STRING && !EmptyValue.Data.Size,"slot-zero empty key result");xrtFutureDestroy(EmptyFuture);
    xrediscommandkeylimits Limits;xredisCommandKeyLimitsInit(&Limits);Limits.MaxKeyBytes=1;xrediscommandcatalog* Small=qpCatalogue(0,&Limits);xbytesview Get[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("{22179}")};xfuture* R=(void*)(uintptr_t)1;
    testRequire(xredisClusterClientCallCommand(C,Small,Get,2,XREDIS_CLUSTER_COMMAND_NO_SLOT,5000,&R)==XREDIS_CLUSTER_COMMAND_LIMIT && R==(void*)(uintptr_t)1,"catalogue limit mapping");xredisCommandCatalogRelease(Small);
    Limits.MaxKeyBytes=13;Small=qpCatalogue(0,&Limits);xbytesview Repeated[]={XRT_BYTES_LITERAL("repeated"),XRT_BYTES_LITERAL("owned\0{22179}")};testRequire(xredisClusterClientCallCommand(C,Small,Repeated,2,1,5000,&R)==XREDIS_CLUSTER_COMMAND_LIMIT && R==(void*)(uintptr_t)1,"overlapping key specs bypassed aggregate hashing cap");xredisCommandCatalogRelease(Small);
    static const char SearchWire[]="*1\r\n*9\r\n+xread\r\n:-4\r\n*0\r\n:0\r\n:0\r\n:0\r\n*0\r\n*0\r\n*1\r\n*6\r\n+flags\r\n*1\r\n+RO\r\n+begin_search\r\n*4\r\n+type\r\n+keyword\r\n+spec\r\n*4\r\n+keyword\r\n+STREAMS\r\n+startfrom\r\n:1\r\n+find_keys\r\n*4\r\n+type\r\n+range\r\n+spec\r\n*6\r\n+lastkey\r\n:-1\r\n+keystep\r\n:1\r\n+limit\r\n:2\r\n";
    xredisCommandKeyLimitsInit(&Limits);Limits.MaxSearchWork=40;Small=qpCatalogueWire(SearchWire,&Limits);xbytesview Search[64];Search[0]=(xbytesview)XRT_BYTES_LITERAL("XREAD");for(unsigned I=1;I<64;++I)Search[I]=(xbytesview)XRT_BYTES_LITERAL("x");testRequire(xredisClusterClientCallCommand(C,Small,Search,64,1,5000,&R)==XREDIS_CLUSTER_COMMAND_LIMIT && R==(void*)(uintptr_t)1,"keyword work exhaustion guessed keyless command");xredisCommandCatalogRelease(Small);
    testRequire(xredisClusterClientCallCommand(C,K,Get,2,20000,5000,&R)==XREDIS_CLUSTER_COMMAND_ERROR && R==(void*)(uintptr_t)1,"bad keyless policy");xrtClearError();
    testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,-2,&R)==XREDIS_CLUSTER_COMMAND_ERROR,"bad timeout");xrtClearError();
    testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,5000,(void*)K)==XREDIS_CLUSTER_COMMAND_ERROR,"Future overwrote catalogue");xrtClearError();
    testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,5000,(void*)Get)==XREDIS_CLUSTER_COMMAND_ERROR,"Future overwrote descriptors");xrtClearError();
    xbytesview Alias[]={XRT_BYTES_LITERAL("GET"),{(cbytes)&R,sizeof(R)}};testRequire(xredisClusterClientCallCommand(C,K,Alias,2,1,5000,&R)==XREDIS_CLUSTER_COMMAND_ERROR && R==(void*)(uintptr_t)1,"Future overwrote argument bytes");xrtClearError();
    xredisreplyreader* Reader=NULL;xredisreply* Reply=NULL;size_t Used=0;testRequire(xredisReplyReaderCreate(NULL,&Reader) && xredisReplyReaderFeed(Reader,(xbytesview)XRT_BYTES_LITERAL("*1\r\n*6\r\n+get\r\n:2\r\n*0\r\n:1\r\n:1\r\n:1\r\n"),&Used,&Reply)==XREDIS_REPLY_READY,"reply-alias catalogue input");xredisReplyReaderDestroy(Reader);xrediscommandcatalog* Aliased=NULL;testRequire(xredisCommandCatalogCreate(Reply,NULL,&Aliased)==XREDIS_KEYS_OK,"reply-alias catalogue");
    testRequire(xredisClusterClientCallCommand(C,Aliased,Get,2,1,5000,(void*)Reply)==XREDIS_CLUSTER_COMMAND_ERROR,"Future overwrote catalogue Reply");xrtClearError();xredisCommandCatalogRelease(Aliased);xredisReplyRelease(Reply);
    xredisclustermap* Map=NULL;bool Stale=true;testRequire(xredisClusterClientSnapshot(C,&Map,&Stale)==XREDIS_CLUSTER_VALUE,"map alias snapshot");testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,5000,(void*)Map)==XREDIS_CLUSTER_COMMAND_ERROR,"Future overwrote map");xrtClearError();xredisClusterMapRelease(Map);
    xfuture* Due=qpGet(C,K,0);testRequire(cfWait(Due)->Status==XREDIS_CLUSTER_CLIENT_TIMEOUT,"due call acquired fresh metadata deadline");xrtFutureDestroy(Due);cfClose(C);R=(void*)(uintptr_t)1;testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,5000,&R)==XREDIS_CLUSTER_COMMAND_CLOSED && R==(void*)(uintptr_t)1,"native close mapping");xredisCommandCatalogRelease(K);cfStop(F,C);
    F=qpStart();xredisclusterclientconfig Config;xredisClusterClientConfigInit(&Config);Config.Route.MaxArguments=1;C=cfCreate(F,&Config);K=qpCatalogue(0,NULL);R=(void*)(uintptr_t)1;testRequire(xredisClusterClientCallCommand(C,K,Get,2,1,5000,&R)==XREDIS_CLUSTER_COMMAND_LIMIT && R==(void*)(uintptr_t)1,"client argument cap");xredisCommandCatalogRelease(K);cfStop(F,C);
}
static void qpRedirectsAndStops(void)
{
    for(unsigned Kind=1;Kind<=2;++Kind){rffixture* F=qpStart();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrediscommandcatalog* K=qpCatalogue(Kind&1,NULL);xrtAtomic32Store(&F->Base->Nodes[0].Redirect,Kind,XMEMORY_RELEASE);xrtAtomic32Store(&F->Base->Nodes[0].Target,1,XMEMORY_RELEASE);if(Kind==1)xrtAtomic32Store(&F->Nodes[0].Target,1,XMEMORY_RELEASE);
        xfuture* R=qpGet(C,K,5000);qpValue(R);const xredisclusterclientresult* V=cfWait(R);testRequire(V->Route.Attempts==2 && V->Route.Moved==(Kind==1) && V->Route.Asks==(Kind==2),"computed slot did not preserve matching redirect");xredisCommandCatalogRelease(K);cfStop(F,C);qpValue(R);xrtFutureDestroy(R);}
    for(unsigned Kind=0;Kind<3;++Kind){rffixture* F=qpStart();xredisclusterclient* C=cfCreate(F,NULL);cfReady(C);xrediscommandcatalog* K=qpCatalogue(0,NULL);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);xfuture* R=qpGet(C,K,Kind?5000:100);redisPeerWait(&F->Base->Business,1);xredisCommandCatalogRelease(K);
        if(Kind==1)xrtFutureCancel(R);
        if(Kind==2)xredisClusterClientClose(C);
        const xredisclusterclientresult* V=cfWait(R);testRequire(V->Status==(Kind==0?XREDIS_CLUSTER_CLIENT_TIMEOUT:Kind==1?XREDIS_CLUSTER_CLIENT_CANCELLED:XREDIS_CLUSTER_CLIENT_CLOSED) && V->Native,"metadata command stop mapping");xredisclusterclientresult Copy=*V;cfStop(F,C);testRequire(!memcmp(V,&Copy,sizeof(Copy)),"retained stop metadata changed");xrtFutureDestroy(R);}
}
static void qpAllocationsAndOOM(void)
{
    rffixture* F=qpStart();xredisclusterclientconfig Config;xredisClusterClientConfigInit(&Config);Config.MaxRequests=2;xredisclusterclient* C=cfCreate(F,&Config);cfReady(C);xrediscommandcatalog* K=qpCatalogue(0,NULL);qpbarrier Gate={0};testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,qpBlock,&Gate),"allocation barrier");redisPeerWait(&Gate.Entered,1);
    xfuture* R[2];xallocationbudget* Budgets[2];xallocationbudgetinfo Info[2];xbytesview A[]={XRT_BYTES_LITERAL("GET"),XRT_BYTES_LITERAL("owned\0{22179}")};
    for(unsigned I=0;I<2;++I){xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1048576,&Budgets[I]) && xrtAllocationBudgetEnter(Budgets[I],&Scope),"caller allocation budget");
        if(I)R[I]=qpGet(C,K,5000);else testRequire(xredisClusterClientCall(C,1,A,2,5000,&R[I])==XREDIS_ADMISSION_ACCEPTED,"explicit caller baseline");
        testRequire(xrtAllocationBudgetLeave(&Scope),"caller allocation leave");Info[I].Size=sizeof(Info[I]);testRequire(xrtAllocationBudgetInfo(Budgets[I],&Info[I]),"caller allocation info");}
    testRequire(Info[0].LiveCount==Info[1].LiveCount && Info[0].LiveBytes==Info[1].LiveBytes && Info[1].LiveCount<=4 && !Info[0].Rejections && !Info[1].Rejections,"metadata routing added caller allocation/storage");xfuture* Full=(void*)(uintptr_t)1;testRequire(xredisClusterClientCallCommand(C,K,A,2,1,5000,&Full)==XREDIS_CLUSTER_COMMAND_OVERLOADED && Full==(void*)(uintptr_t)1,"native overload mapping");xrtAtomic32Store(&Gate.Continue,1,XMEMORY_RELEASE);
    for(unsigned I=0;I<2;++I){qpValue(R[I]);xrtFutureDestroy(R[I]);qpBudgetDrain(Budgets[I]);xrtAllocationBudgetFree(Budgets[I]);}
    unsigned Accepted=0,Failed=0;for(size_t Limit=1;Limit<=8192;Limit+=127){xallocationbudget* B=NULL;xallocationscope S={0};testRequire(xrtAllocationBudgetCreate(Limit,&B) && xrtAllocationBudgetEnter(B,&S),"call OOM budget");xfuture* Result=(void*)(uintptr_t)1;xredisclustercommandadmission Admission=xredisClusterClientCallCommand(C,K,A,2,1,5000,&Result);testRequire(xrtAllocationBudgetLeave(&S),"call OOM leave");
        if(Admission==XREDIS_CLUSTER_COMMAND_ACCEPTED){++Accepted;qpValue(Result);xrtFutureDestroy(Result);}else {++Failed;testRequire(Admission==XREDIS_CLUSTER_COMMAND_ERROR && Result==(void*)(uintptr_t)1,"OOM changed output or status");xrtClearError();}qpBudgetDrain(B);xrtAllocationBudgetFree(B);}
    testRequire(Accepted && Failed,"call OOM paths not exercised");xredisCommandCatalogRelease(K);cfStop(F,C);
}
typedef struct qpproducer {xredisclusterclient* Client;const xrediscommandcatalog* Catalogue;xfuture* Saved;} qpproducer;
static int32 qpProduce(ptr Data)
{qpproducer* P=Data;for(unsigned I=0;I<16;++I){xfuture* R=qpGet(P->Client,P->Catalogue,5000);qpValue(R);if(I)xrtFutureDestroy(R);else P->Saved=R;}return 0;}
static void qpConcurrent(void)
{rffixture* F=qpStart();xredisclusterclient* C=cfCreate(F,NULL);xrediscommandcatalog* K=qpCatalogue(1,NULL);qpproducer P[4];xthread* T[4];for(unsigned I=0;I<4;++I){P[I]=(qpproducer){C,K,NULL};T[I]=xrtThreadCreate(qpProduce,&P[I],0);testRequire(T[I]!=NULL,"command producer");}for(unsigned I=0;I<4;++I){testRequire(xrtThreadWaitFor(T[I],15000)==XWAIT_OK && !xrtThreadExitCode(T[I]),"command producer joined");xrtThreadDestroy(T[I]);}xredisCommandCatalogRelease(K);cfStop(F,C);for(unsigned I=0;I<4;++I){qpValue(P[I].Saved);xrtFutureDestroy(P[I].Saved);}}
#if defined(XREDIS_CLUSTER_COMMAND_WHITEBOX)
typedef struct qpdeadline {rffixture* Fixture;xredisclusterclient* Client;double Entered;int64 Timeout;xatomic32 Done;} qpdeadline;
static void qpInspectDeadline(xnetworker* Worker,ptr Data)
{(void)Worker;qpdeadline* P=Data;xredisclient* Native=xrtAtomicPtrLoad(&P->Fixture->Nodes[0].Client,XMEMORY_ACQUIRE);xredisrequest* Q=Native->SentHead;xrtMutexLock(&P->Client->Gate);xredisclusteroperation* O=P->Client->All;
 testRequire(Q && O && O->Entered==P->Entered && O->Route->Entered==P->Entered && Q->Deadline==xredisClientDeadline(P->Entered,P->Timeout) && O->Deadline==Q->Deadline,"metadata/queue changed exact native deadline");xrtMutexUnlock(&P->Client->Gate);xrtAtomic32Store(&P->Done,1,XMEMORY_RELEASE);}
static void qpDeadline(void)
{
    for(unsigned Default=0;Default<2;++Default){rffixture* F=qpStart();xredisclusterclientconfig Config;xredisClusterClientConfigInit(&Config);Config.Route.Timeout=5007;xredisclusterclient* C=cfCreate(F,&Config);cfReady(C);xrediscommandcatalog* K=qpCatalogue(0,NULL);qpbarrier Gate={0};testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,1,qpBlock,&Gate),"deadline barrier");redisPeerWait(&Gate.Entered,1);xrtAtomic32Store(&F->Base->HoldBusiness,1,XMEMORY_RELEASE);
        qpClockBase=xrtTimer();qpClockCalls=0;qpClockArmed=true;xfuture* R=qpGet(C,K,Default?-1:10001);qpClockArmed=false;
        xrtMutexLock(&C->Gate);testRequire(C->All && C->All->Entered==qpClockBase && C->All->Slot==1,"call entry was recaptured after metadata");xrtMutexUnlock(&C->Gate);xredisCommandCatalogRelease(K);xrtAtomic32Store(&Gate.Continue,1,XMEMORY_RELEASE);redisPeerWait(&F->Base->Business,1);qpdeadline P={F,C,qpClockBase,Default?5007:10001,{0}};
        testRequire(xrtNetEnginePost(F->Base->Peers[0].Engine,0,qpInspectDeadline,&P),"native deadline probe");redisPeerWait(&P.Done,1);cfStop(F,C);xrtFutureDestroy(R);}
}
#endif
int main(void)
{qpBasic();qpRefusals();qpRedirectsAndStops();qpAllocationsAndOOM();qpConcurrent();
#if defined(XREDIS_CLUSTER_COMMAND_WHITEBOX)
qpDeadline();
#endif
puts("[PASS] Cluster command routing: full key metadata preflight, explicit keyless policy, copied/retired catalogue, native redirects/stops, exact deadlines, equal allocation bound, OOM and concurrent outcomes");return 0;}
