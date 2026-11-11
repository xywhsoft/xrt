#include "test.h"
#include <xredis/subscriber.h>
#include "support/client_peer.h"
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
#include "support/tls_peer.h"
#endif
typedef struct subkey {unsigned Family;size_t Bytes;bool Present;unsigned char Data[64];} subkey;
typedef struct subpeer {subkey Keys[32];bool Resp3,Messages,Hold,CheckSync,Large;unsigned Malformed;} subpeer;
typedef struct subhold {xatomic32 Entered,Released,Exited;} subhold;
static void subHoldWorker(xnetworker* Worker,ptr Data)
{(void)Worker;subhold* H=Data;xrtAtomic32Store(&H->Entered,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&H->Released,XMEMORY_ACQUIRE))xrtThreadYield();xrtAtomic32Store(&H->Exited,1,XMEMORY_RELEASE);}
static void subHoldInit(subhold* H)
{xrtAtomic32Init(&H->Entered,0);xrtAtomic32Init(&H->Released,0);xrtAtomic32Init(&H->Exited,0);}
static size_t subCount(subpeer* S,unsigned Family)
{size_t N=0;for(size_t I=0;I<32;++I)if(S->Keys[I].Present && (Family==2?S->Keys[I].Family==2:S->Keys[I].Family<2))++N;return N;}
static subkey* subFind(subpeer* S,unsigned Family,xbytesview Name,bool Create)
{
    subkey* Empty=NULL;for(size_t N=0;N<32;++N){subkey* K=S->Keys+N;if(!K->Present){if(!Empty)Empty=K;continue;}
        if(K->Family==Family && K->Bytes==Name.Size && (!Name.Size || !memcmp(K->Data,Name.Data,Name.Size)))return K;}
    if(!Create)return NULL;
    testRequire(Empty && Name.Size<=sizeof(Empty->Data),"fixture finite keys");
    Empty->Family=Family;Empty->Bytes=Name.Size;Empty->Present=true;if(Name.Size)memcpy(Empty->Data,Name.Data,Name.Size);return Empty;
}
static void subBulk(redispeer* P,xbytesview V)
{char H[40];int N=snprintf(H,sizeof(H),"$%zu\r\n",V.Size);redisPeerSend(P,H,(size_t)N);if(V.Size)redisPeerSend(P,V.Data,V.Size);redisPeerSend(P,"\r\n",2);}
static void subAck(redispeer* P,subpeer* S,unsigned Op,const xbytesview* Name)
{
    static const cstr Tags[]={"subscribe","psubscribe","ssubscribe","unsubscribe","punsubscribe","sunsubscribe"};
    unsigned Family=Op%3;redisPeerSend(P,S->Resp3?">3\r\n":"*3\r\n",4);subBulk(P,(xbytesview){(cbytes)Tags[Op],strlen(Tags[Op])});
    if(S->Malformed==2)subBulk(P,(xbytesview)XRT_BYTES_LITERAL("wrong"));
    else if(Name)subBulk(P,*Name);else redisPeerSend(P,S->Resp3?"_\r\n":"$-1\r\n",S->Resp3?3:5);
    char Count[40];size_t Value=subCount(S,Family);int N=snprintf(Count,sizeof(Count),":%zu\r\n",S->Malformed==1?Value+1:Value);redisPeerSend(P,Count,(size_t)N);
    if(S->Messages && Op<3){
        redisPeerSend(P,Family==1?(S->Resp3?">4\r\n":"*4\r\n"):(S->Resp3?">3\r\n":"*3\r\n"),4);
        cstr Tag=Family==1?"pmessage":Family==2?"smessage":"message";subBulk(P,(xbytesview){(cbytes)Tag,strlen(Tag)});
        subBulk(P,*Name);if(Family==1)subBulk(P,(xbytesview)XRT_BYTES_LITERAL("chan\0nel"));subBulk(P,(xbytesview)XRT_BYTES_LITERAL("body\0\r\n"));
    }
}
static bool subCommand(redispeer* P,xredisreply* R,ptr Data)
{
    subpeer* S=Data;xredisreplyinfo I={0};I.Size=sizeof(I);xredisReplyInfo(R,&I);xredisreplyvalue Root,Word;xredisReplyGet(R,I.Root,&Root);xredisReplyGet(R,Root.FirstChild,&Word);
    if(redisPeerName(Word,"HELLO")){static const char Hello[]="%3\r\n+server\r\n+redis\r\n+version\r\n+fixture\r\n+proto\r\n:3\r\n";redisPeerSend(P,Hello,sizeof(Hello)-1);return true;}
    static const cstr Words[]={"SUBSCRIBE","PSUBSCRIBE","SSUBSCRIBE","UNSUBSCRIBE","PUNSUBSCRIBE","SUNSUBSCRIBE","PING"};unsigned Op;
    for(Op=0;Op<7;++Op)if(redisPeerName(Word,Words[Op]))break;
    if(Op==7)return false;
    if(S->Large && Op==0){xredisreplyvalue Arg;testRequire(Root.ChildCount==2 && xredisReplyGet(R,Word.Next,&Arg),"single large TLS name");S->Keys[0].Present=true;S->Keys[0].Family=0;subAck(P,S,0,&Arg.Data);return true;}
    if(S->CheckSync){xfuture* F=(xfuture*)(uintptr_t)1;testRequire(xredisSubscriberCommand(P->Client,XREDIS_PUBSUB_CMD_PING,NULL,0,100,&F)==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"sync cannot block another worker on same Engine");xrtClearError();}
    if(S->Hold)P->Blocked=true;
    if(S->Malformed==3){redisPeerSend(P,"-ERR denied\r\n",13);return true;}
    if(Op==6){
        xredisreplyvalue Arg;bool Given=Root.ChildCount==2;if(Given)xredisReplyGet(R,Word.Next,&Arg);
        if(!S->Resp3 && (subCount(S,0) || subCount(S,2))){redisPeerSend(P,"*2\r\n$4\r\npong\r\n",14);subBulk(P,Given?Arg.Data:(xbytesview){NULL,0});}
        else if(Given)subBulk(P,Arg.Data);else redisPeerSend(P,"+PONG\r\n",7);
        return true;
    }
    unsigned Family=Op%3;
    if(Root.ChildCount==1){
        bool Any=false;for(size_t N=32;N>0;--N){subkey* K=S->Keys+N-1;if(K->Present && K->Family==Family){K->Present=false;xbytesview Name={K->Data,K->Bytes};subAck(P,S,Op,&Name);Any=true;}}
        if(!Any)subAck(P,S,Op,NULL);
    }else for(size_t Node=Word.Next;Node!=XREDIS_REPLY_NONE;){xredisreplyvalue Arg;xredisReplyGet(R,Node,&Arg);subkey* K=subFind(S,Family,Arg.Data,Op<3);if(Op>=3 && K)K->Present=false;subAck(P,S,Op,&Arg.Data);Node=Arg.Next;if(S->Malformed==5)break;}
    return true;
}
static xredisclient* subConnect(redispeer* P,subpeer* S,xredisclientconfig* Config,const xredissubscriberconfig* Registry)
{
    xredisclient* C=NULL;testRequire(xredisClientConnect(P->Engine,&P->Address,0,Config,&C),"subscriber TCP connect");P->Client=C;
    xfuture* F=xredisClientReady(C);testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK,"subscriber transport ready");xrtFutureDestroy(F);
    testRequire(xredisSubscriberStart(C,Registry)==XREDIS_ADMISSION_ACCEPTED,"idle client adoption");(void)S;return C;
}
static const xredissubscriberresult* subResult(xfuture* F)
{
    testRequire(F && xrtFutureWaitFor(F,10000)==XWAIT_OK,"subscriber result wait");xfutureresult R;testRequire(xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"typed subscriber outcome");return R.Value;
}
static void subIdle(xredisclient* C)
{double End=xrtTimer()+10;for(;;){xredisclientinfo I={0};I.Size=sizeof(I);xredisClientInfo(C,&I);if(!I.Requests)return;testRequire(xrtTimer()<End,"physical request tails");xrtThreadYield();}}
static void subClose(redispeer* P,xredisclient* C)
{
    xfuture* F=xredisClientClosed(C);testRequire(xredisClientClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"subscriber physical close");
    xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C,&I) && !I.Entries && !I.NameBytes && !I.Subscriptions && !I.ShardSubscriptions,"registry duties retire at close");
    xredisclientinfo K={0};K.Size=sizeof(K);testRequire(xredisClientInfo(C,&K) && !K.Requests && !K.CommandBytes && K.Accepted==K.Completed,"request duties retire");
    xrtFutureDestroy(F);xredisClientRelease(C);redisPeerStop(P);
}
static void subPipeline(bool Resp3)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=Resp3;S.Messages=true;S.Hold=true;S.CheckSync=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.ReadBytes=1;K.ReadFrames=1;K.WorkRequests=1;K.MaxInFlight=16;
    xredisclient* C=subConnect(&P,&S,&K,NULL);xfuture* Refused=(xfuture*)(uintptr_t)1;xbytesview Ping=XRT_BYTES_LITERAL("PING");
    testRequire(xredisClientCallAsync(C,&Ping,1,100,&Refused)==XREDIS_ADMISSION_OVERLOADED && Refused==(xfuture*)(uintptr_t)1,"ordinary calls cannot enter subscriber mode");
    testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_OVERLOADED,"adoption is exclusive");
#if defined(XREDIS_FEATURE_XREDIS_TRACKING)
    testRequire(xredisClientTrackingObserve(C,0)==XREDIS_ADMISSION_OVERLOADED,"tracking cannot enable observation after subscriber adoption");
#endif
    xbytesview A[]={XRT_BYTES_LITERAL("a\0x"),XRT_BYTES_LITERAL("a\0x"),{NULL,0}},Patterns[]={XRT_BYTES_LITERAL("p*"),{NULL,0}},Shards[]={XRT_BYTES_LITERAL("a\0x"),XRT_BYTES_LITERAL("s2")};
    xbytesview Remove[]={XRT_BYTES_LITERAL("a\0x"),XRT_BYTES_LITERAL("a\0x"),XRT_BYTES_LITERAL("absent")},Payload=XRT_BYTES_LITERAL("echo\0\r\n");
    xfuture* F[10]={0};
    const xredispubsubcommand Ops[]={XREDIS_PUBSUB_CMD_SUBSCRIBE,XREDIS_PUBSUB_CMD_PSUBSCRIBE,XREDIS_PUBSUB_CMD_SSUBSCRIBE,XREDIS_PUBSUB_CMD_PING,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PUNSUBSCRIBE,XREDIS_PUBSUB_CMD_SUNSUBSCRIBE,XREDIS_PUBSUB_CMD_UNSUBSCRIBE,XREDIS_PUBSUB_CMD_PING};
    const xbytesview* Names[]={A,Patterns,Shards,&Payload,Remove,NULL,NULL,NULL,NULL,NULL};size_t Counts[]={3,2,2,1,3,0,0,0,0,0},Acks[]={3,2,2,0,3,1,2,2,1,0};
    for(size_t N=0;N<10;++N)testRequire(xredisSubscriberCommandAsync(C,Ops[N],Names[N],Counts[N],5000,&F[N])==XREDIS_ADMISSION_ACCEPTED,"pipelined subscription admission");
    redisPeerWait(&P.Commands,10);redisPeerReleaseBlocked(&P);
    for(size_t N=0;N<10;++N){const xredissubscriberresult* R=subResult(F[N]);testRequire(R->Request.Status==XREDIS_REQUEST_OK && R->Request.Delivery==XREDIS_REPLIED && R->Acknowledged==Acks[N],"exact ordered ACK cardinality and PING outcomes");xrtFutureDestroy(F[N]);}
    size_t Messages=0;xredisreply* R=NULL;while(xredisClientTakePush(C,&R)){xredispubsubevent E;testRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && E.Payload.Size==7 && !memcmp(E.Payload.Data,"body\0\r\n",7),"binary message sideband separate from ACKs");++Messages;xredisReplyRelease(R);}
    testRequire(Messages==7,"all channel/pattern/shard messages delivered");subIdle(C);
    xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C,&I) && !I.Entries && !I.Subscriptions && !I.ShardSubscriptions && I.Acknowledgements==16 && I.Messages==7,"registry returns to empty after all unordered ACKs");
    subClose(&P,C);
}
static void subAbandon(bool Cancel)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Hold=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=subConnect(&P,&S,NULL,NULL);xbytesview Name=XRT_BYTES_LITERAL("late\0ack");xfuture* F=NULL;
    testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,Cancel?5000:100,&F)==XREDIS_ADMISSION_ACCEPTED,"sent abandoned subscription");redisPeerWait(&P.Commands,1);
    if(Cancel)testRequire(xrtFutureCancel(F),"cancel sent subscription");
    const xredissubscriberresult* Result=subResult(F);testRequire(Result->Request.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_TIMEOUT) && Result->Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && !Result->Acknowledged && !Result->Subscriptions,"logical stop before ACK");
    xredissubscriberresult Saved=*Result;S.Hold=false;redisPeerReleaseBlocked(&P);xfuture* Next=NULL;
    testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,5000,&Next)==XREDIS_ADMISSION_ACCEPTED && subResult(Next)->Request.Status==XREDIS_REQUEST_OK,"late ACK drained before next PONG");
    testRequire(!memcmp(Result,&Saved,sizeof(Saved)),"late confirmation never mutates earlier outcome");
    xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C,&I) && I.Subscriptions==1 && I.Entries==1 && I.Acknowledgements==1,"cancelled sent subscription still becomes confirmed");
    xrtFutureDestroy(Next);xrtFutureDestroy(F);subClose(&P,C);
}
static void subFailure(unsigned Mode)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Malformed=Mode;S.Messages=Mode==4 || Mode==6;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.MaxPushes=1;if(Mode==6){K.MaxPushes=128;K.MaxPushBytes=1;}xredisclient* C=subConnect(&P,&S,&K,NULL);
    xbytesview Names[]={XRT_BYTES_LITERAL("one"),XRT_BYTES_LITERAL("two")};xfuture* F=NULL;
    testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,Mode==4?2:1,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"malformed/error/overflow control");
    const xredissubscriberresult* R=subResult(F);
    if(Mode==3){testRequire(R->Request.Status==XREDIS_REQUEST_SERVER_ERROR && R->Request.Delivery==XREDIS_REPLIED,"whole-command server rejection");subIdle(C);xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C,&I) && !I.Entries,"rejected subscription releases reservations");}
    else{
        if(Mode<3)testRequire(R->Request.Status==XREDIS_REQUEST_PROTOCOL,"wrong ACK name/count retires connection");
        xfuture* Closed=xredisClientClosed(C);testRequire(xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"automatic error/overflow close");xrtFutureDestroy(Closed);
        xredisclientinfo I={0};I.Size=sizeof(I);xredisClientInfo(C,&I);testRequire(I.CloseStatus==(Mode>=4?XREDIS_REQUEST_LIMIT:XREDIS_REQUEST_PROTOCOL),"precise connection retirement cause");
    }
    xredisreply* Retained=NULL;if(Mode==4)testRequire(xredisClientTakePush(C,&Retained),"queued delivery retained across overflow close");
    xrtFutureDestroy(F);subClose(&P,C);
    if(Retained){xredispubsubevent E;testRequire(xredisPubSubEventRead(Retained,&E)==XREDIS_PUBSUB_EVENT && E.Channel.Size==3,"owned message after Client and Engine destruction");xredisReplyRelease(Retained);}
}
static void subPartialGrace(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Malformed=5;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclientconfig K;xredisClientConfigInit(&K);K.MaxInFlight=1;K.DrainTimeout=30;xredisclient* C=subConnect(&P,&S,&K,NULL);xbytesview Names[]={XRT_BYTES_LITERAL("one"),XRT_BYTES_LITERAL("two")};xfuture* F=NULL;
    testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,2,XRT_WAIT_FOREVER,&F)==XREDIS_ADMISSION_ACCEPTED,"partial multi-ACK control");
    double End=xrtTimer()+10;xredissubscriberinfo I={0};I.Size=sizeof(I);do{testRequire(xredisSubscriberInfo(C,&I) && xrtTimer()<End,"first ACK observation");xrtThreadYield();}while(I.Acknowledgements!=1);
    testRequire(xrtFutureCancel(F),"cancel after one ACK");const xredissubscriberresult* R=subResult(F);xredissubscriberresult Saved=*R;
    testRequire(R->Request.Status==XREDIS_REQUEST_CANCELLED && R->Request.Delivery==XREDIS_MAY_HAVE_EXECUTED && R->Acknowledged==1 && R->Subscriptions==1,"partial confirmation knowledge survives caller stop");
    xfuture* Next=NULL;testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_PING,NULL,0,5000,&Next)==XREDIS_ADMISSION_ACCEPTED,"successor remains behind unfilled ACK obligation");
    const xredissubscriberresult* N=subResult(Next);testRequire(N->Request.Status==XREDIS_REQUEST_DISCONNECTED && N->Request.Delivery==XREDIS_NOT_SENT && !memcmp(R,&Saved,sizeof(Saved)),"bounded drain closes rather than shifting successor slot");
    xrtFutureDestroy(Next);xrtFutureDestroy(F);subClose(&P,C);
}
static void subConstructionOOM(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredisclient* C=subConnect(&P,&S,NULL,NULL);size_t Accepted=0,Failed=0;
    for(size_t Bytes=1;Bytes<=1536;++Bytes){xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(Bytes,&B) && xrtAllocationBudgetEnter(B,&Scope),"constructor budget");
        xfuture* F=(xfuture*)(uintptr_t)1;xredisadmission A=xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_PING,NULL,0,5000,&F);
        testRequire(xrtAllocationBudgetLeave(&Scope),"constructor budget leave");
        if(A==XREDIS_ADMISSION_ACCEPTED){++Accepted;testRequire(subResult(F)->Request.Status==XREDIS_REQUEST_OK,"constructed bounded PING");xrtFutureDestroy(F);}
        else{++Failed;testRequire(A==XREDIS_ADMISSION_ERROR && F==(xfuture*)(uintptr_t)1,"OOM preserves Future output");xrtClearError();}
        subIdle(C);xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"no retained constructor quota duties");xrtAllocationBudgetFree(B);
    }
    testRequire(Accepted && Failed,"all constructor quota prefixes cover failure and success");subClose(&P,C);
}
typedef struct subbudget {xallocationbudget* Budget;xallocationscope Scope;xatomic32 Armed,Left;} subbudget;
static void subBudgetArm(xnetworker* Worker,ptr Data)
{(void)Worker;subbudget* B=Data;testRequire(xrtAllocationBudgetEnter(B->Budget,&B->Scope),"worker budget enter");xrtAtomic32Store(&B->Armed,1,XMEMORY_RELEASE);}
static void subBudgetLeave(xnetworker* Worker,ptr Data)
{(void)Worker;subbudget* B=Data;testRequire(xrtAllocationBudgetLeave(&B->Scope),"worker budget leave");xrtAtomic32Store(&B->Left,1,XMEMORY_RELEASE);}
static void subRegistryOOM(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);
    subbudget B={0};xrtAtomic32Init(&B.Armed,0);xrtAtomic32Init(&B.Left,0);testRequire(xrtAllocationBudgetCreate(1,&B.Budget) && xrtNetEnginePost(P.Engine,0,subBudgetArm,&B),"arm allocation failure on IO owner");redisPeerWait(&B.Armed,1);
    xbytesview Name=XRT_BYTES_LITERAL("worker-name");xfuture* F=NULL;testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,XRT_WAIT_FOREVER,&F)==XREDIS_ADMISSION_ACCEPTED,"worker registry allocation admission");
    const xredissubscriberresult* R=subResult(F);testRequire(R->Request.Status==XREDIS_REQUEST_MEMORY && R->Request.Delivery==XREDIS_NOT_SENT && !xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"entry OOM before any bytes sent");
    testRequire(xrtNetEnginePost(P.Engine,0,subBudgetLeave,&B),"leave owner budget");redisPeerWait(&B.Left,1);xrtFutureDestroy(F);subIdle(C);
    xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B.Budget,&I) && !I.LiveCount && !I.LiveBytes,"worker allocation duties released");xrtAllocationBudgetFree(B.Budget);
    F=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED && subResult(F)->Request.Status==XREDIS_REQUEST_OK,"same connection succeeds after entry OOM");xrtFutureDestroy(F);subClose(&P,C);
}
static void subClosedOwners(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);
    xbytesview Name=XRT_BYTES_LITERAL("retained");xfuture* F=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED && subResult(F)->Acknowledged==1,"retained control result");
    xfuture* Closed=xredisClientClosed(C);testRequire(xredisClientClose(C) && xrtFutureWaitFor(Closed,10000)==XWAIT_OK,"retained Client physical close");redisPeerStop(&P);
    xfuture* Sentinel=(xfuture*)(uintptr_t)1;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,5000,&Sentinel)==XREDIS_ADMISSION_CLOSED && Sentinel==(xfuture*)(uintptr_t)1,"sync closed admission never dereferences a destroyed Engine");
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"closed adoption allocation budget");
    testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_CLOSED && xrtAllocationBudgetLeave(&Scope),"closed adoption rejects before cold allocation");xrtAllocationBudgetFree(B);
    xredissubscriberinfo I={0};I.Size=sizeof(I);testRequire(xredisSubscriberInfo(C,&I) && !I.Entries && !I.Subscriptions && I.RegistryBytes,"closed Client retains only observable cold mode storage");
    testRequire(subResult(F)->Request.Status==XREDIS_REQUEST_OK,"control result outlives physical owners");xrtFutureDestroy(F);xrtFutureDestroy(Closed);xredisClientRelease(C);
}
static void subSnapshots(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};S.Messages=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=subConnect(&P,&S,NULL,NULL);
    subhold H;subHoldInit(&H);testRequire(xrtNetEnginePost(P.Engine,0,subHoldWorker,&H),"hold snapshot owner");redisPeerWait(&H.Entered,1);
    unsigned char* Bytes=xrtMalloc(6);testRequire(Bytes!=NULL,"caller name storage");memcpy(Bytes,"keep\0x",6);xbytesview Name={Bytes,6};xfuture* F=NULL;
    testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"snapshot before owner can read");memset(Bytes,0xcc,6);xrtFree(Bytes);Name=(xbytesview){NULL,SIZE_MAX};xrtAtomic32Store(&H.Released,1,XMEMORY_RELEASE);
    testRequire(subResult(F)->Request.Status==XREDIS_REQUEST_OK,"encoded input survives caller mutation/free");xredisreply* Message=NULL;double End=xrtTimer()+10;while(!xredisClientTakePush(C,&Message)){testRequire(xrtTimer()<End,"snapshot message");xrtThreadYield();}
    subClose(&P,C);xredispubsubevent E;testRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && E.Channel.Size==6 && !memcmp(E.Channel.Data,"keep\0x",6),"persistent name is copied from owned wire");xredisReplyRelease(Message);xrtFutureDestroy(F);
}
static void subAdoptionOOM(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);xredisclient* C=NULL;testRequire(xredisClientConnect(P.Engine,&P.Address,0,NULL,&C),"cold adoption client");P.Client=C;
    xfuture* Ready=xredisClientReady(C);testRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK,"cold adoption ready");xrtFutureDestroy(Ready);
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"cold adoption budget");
    testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ERROR && xrtAllocationBudgetLeave(&Scope),"failed cold mode publication consumes nothing");xrtClearError();
    xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"cold mode allocation leaves no duties");xrtAllocationBudgetFree(B);
    xbytesview Ping=XRT_BYTES_LITERAL("PING");xfuture* F=NULL;testRequire(xredisClientCall(C,&Ping,1,5000,&F)==XREDIS_ADMISSION_ACCEPTED && ((const xredisrequestresult*)subResult(F))->Status==XREDIS_REQUEST_OK,"ordinary Client remains usable after failed adoption");xrtFutureDestroy(F);subIdle(C);
    testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"later adoption succeeds");subClose(&P,C);
}
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
static void subTLS(void)
{
#if defined(XREDIS_SUBSCRIBER_WHITEBOX)
    for(unsigned Cancel=0;Cancel<2;++Cancel){
#else
    for(unsigned Cancel=0;Cancel<1;++Cancel){
#endif
        redispeer P;redisPeerStart(&P);subpeer S={0};S.Resp3=true;S.Messages=true;S.Large=true;redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
        redistlspeer TLS={0};xrtAtomic32Init(&TLS.Verified,0);redisTlsPeerCredentials(&TLS,&P,false,false);xredisclienttlsconfig Secure;redisTlsPeerConfig(&TLS,&Secure);Secure.WriteBytes=3;
        xredisclientconfig K;xredisClientConfigInit(&K);K.WorkRequests=1;xredisclient* C=NULL;testRequire(xredisClientTlsConnect(P.Engine,&P.Address,0,&K,NULL,&Secure,&C),"native TLS subscriber factory");P.Client=C;
        xfuture* Ready=xredisClientReady(C);testRequire(xrtFutureWaitFor(Ready,10000)==XWAIT_OK,"TLS ready wait");xfutureresult Value;testRequire(xrtFutureResult(Ready,&Value) && ((const xredisreadyresult*)Value.Value)->Status==XREDIS_READY_OK,"verified TLS plus HELLO");xrtFutureDestroy(Ready);
        testRequire(xredisSubscriberStart(C,NULL)==XREDIS_ADMISSION_ACCEPTED,"managed TLS adoption");
        xfuture* Prime=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,10000,&Prime)==XREDIS_ADMISSION_ACCEPTED && subResult(Prime)->Request.Status==XREDIS_REQUEST_OK,"TLS priming PONG");xrtFutureDestroy(Prime);subIdle(C);
        subhold Before,Inside;subHoldInit(&Before);subHoldInit(&Inside);
        if(Cancel){testRequire(xrtNetEnginePost(P.Engine,0,subHoldWorker,&Before),"hold before submission");redisPeerWait(&Before.Entered,1);}
        unsigned char NameBytes[16384];memset(NameBytes,'n',sizeof(NameBytes));NameBytes[0]='t';NameBytes[1]=0;NameBytes[sizeof(NameBytes)-1]='z';xbytesview Name={NameBytes,sizeof(NameBytes)};xfuture* F=NULL;
        testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,10000,&F)==XREDIS_ADMISSION_ACCEPTED,"TLS subscription control");
        if(Cancel){testRequire(xrtNetEnginePost(P.Engine,0,subHoldWorker,&Inside),"hold after first write turn");xrtAtomic32Store(&Before.Released,1,XMEMORY_RELEASE);redisPeerWait(&Inside.Entered,1);
#if defined(XREDIS_SUBSCRIBER_WHITEBOX)
            bool Seen=false;
            for(unsigned Attempt=0;Attempt<128;++Attempt){
                Seen=C->TlsWrite && C->TlsWrite->Observe>=5 && C->TlsWritten>0 && C->TlsWritten<C->TlsWrite->WireBytes;
                if(Seen)break;
                xrtAtomic32Store(&Inside.Released,1,XMEMORY_RELEASE);redisPeerWait(&Inside.Exited,1);subHoldInit(&Inside);
                testRequire(xrtNetEnginePost(P.Engine,0,subHoldWorker,&Inside),"observe another bounded TLS write turn");redisPeerWait(&Inside.Entered,1);
            }
            testRequire(Seen,"actual partial TLS subscription before cancellation");
#endif
            testRequire(xrtFutureCancel(F),"cancel partial TLS control");xrtAtomic32Store(&Inside.Released,1,XMEMORY_RELEASE);
        }
        const xredissubscriberresult* R=subResult(F);xredissubscriberresult Saved=*R;
        if(R->Request.Status!=(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_OK) || R->Request.Delivery!=(Cancel?XREDIS_MAY_HAVE_EXECUTED:XREDIS_REPLIED))
            fprintf(stderr,"TLS control: cancel=%u status=%d delivery=%d acknowledgements=%zu\n",Cancel,R->Request.Status,R->Request.Delivery,R->Acknowledged);
        testRequire(R->Request.Status==(Cancel?XREDIS_REQUEST_CANCELLED:XREDIS_REQUEST_OK) && R->Request.Delivery==(Cancel?XREDIS_MAY_HAVE_EXECUTED:XREDIS_REPLIED),"partial TLS outcome knowledge");
        xfuture* Ping=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,10000,&Ping)==XREDIS_ADMISSION_ACCEPTED && subResult(Ping)->Request.Status==XREDIS_REQUEST_OK,"TLS cancelled control finishes original wire and ACK before successor");
        testRequire(!memcmp(&Saved,R,sizeof(Saved)),"TLS late ACK cannot change outcome");xrtFutureDestroy(Ping);
        xredisreply* Message=NULL;testRequire(xredisClientTakePush(C,&Message),"TLS confirmed subscription message");subClose(&P,C);redisTlsPeerCredentialsDrop(&TLS);
        xredispubsubevent E;testRequire(xredisPubSubEventRead(Message,&E)==XREDIS_PUBSUB_EVENT && E.Channel.Size==Name.Size && !memcmp(E.Channel.Data,Name.Data,Name.Size),"retained TLS message after all transport owners");xredisReplyRelease(Message);xrtFutureDestroy(F);
    }
}
#endif
static void subLimit(void)
{
    redispeer P;redisPeerStart(&P);subpeer S={0};redispeerhook Hook={subCommand,&S};xrtAtomicPtrStore(&P.Hook,&Hook,XMEMORY_RELEASE);
    xredissubscriberconfig R;xredisSubscriberConfigInit(&R);R.MaxSubscriptions=1;R.MaxNameBytes=8;xredisclient* C=subConnect(&P,&S,NULL,&R);
    xbytesview Names[]={XRT_BYTES_LITERAL("one"),XRT_BYTES_LITERAL("two")};xfuture* F=NULL;testRequire(xredisSubscriberCommandAsync(C,XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,2,5000,&F)==XREDIS_ADMISSION_ACCEPTED,"bounded registry admission");
    const xredissubscriberresult* Result=subResult(F);testRequire(Result->Request.Status==XREDIS_REQUEST_LIMIT && Result->Request.Delivery==XREDIS_NOT_SENT && !xrtAtomic32Load(&P.Commands,XMEMORY_ACQUIRE),"registry failure precedes wire acceptance and rolls back");xrtFutureDestroy(F);subIdle(C);
    F=NULL;testRequire(xredisSubscriberCommand(C,XREDIS_PUBSUB_CMD_PING,NULL,0,5000,&F)==XREDIS_ADMISSION_ACCEPTED && subResult(F)->Request.Status==XREDIS_REQUEST_OK,"connection usable after registry limit");xrtFutureDestroy(F);subClose(&P,C);
}
#ifndef XREDIS_SUBSCRIBER_TEST_MAIN
#define XREDIS_SUBSCRIBER_TEST_MAIN main
#endif
int XREDIS_SUBSCRIBER_TEST_MAIN(void)
{subPipeline(false);subPipeline(true);subLimit();subAbandon(false);subAbandon(true);for(unsigned N=1;N<=4;++N)subFailure(N);subFailure(6);subPartialGrace();subConstructionOOM();subRegistryOOM();subClosedOwners();subSnapshots();subAdoptionOOM();
#if defined(XREDIS_FEATURE_XREDIS_TLS) && defined(XRT_FEATURE_TLS_IDENTITY_RSA)
subTLS();
#endif
puts("[PASS] subscriber native: pipelined ACK correlation, binary/duplicate/empty names, separate counts, unordered unsubscribe-all, messages, late cancel/timeout, malformed ACKs, server error, overflow/lifetime and constructor/owner OOM rollback");return 0;}
