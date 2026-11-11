#include "test.h"
#include <xredis/pubsub.h>
#include <xrt/memory.h>
#include <string.h>

static xredisreply* pubsubParse(const void* Data,size_t Bytes,size_t Fragment)
{
    xredisreplyreader* Reader=NULL;testRequire(xredisReplyReaderCreate(NULL,&Reader),"Pub/Sub reader");
    xredisreply* R=NULL;size_t Offset=0;
    while(Offset<Bytes){size_t N=Bytes-Offset;if(N>Fragment)N=Fragment;size_t Used=0;
        xredisreplystatus S=xredisReplyReaderFeed(Reader,(xbytesview){(cbytes)Data+Offset,N},&Used,&R);
        testRequire(S!=XREDIS_REPLY_ERROR && Used<=N && (Used || S==XREDIS_REPLY_YIELD),"fragmented semantic frame");Offset+=Used;
        if(S==XREDIS_REPLY_READY){testRequire(Offset==Bytes,"exact one frame");break;}
    }
    testRequire(R!=NULL && xredisReplyReaderFinish(Reader),"complete owned frame");xredisReplyReaderDestroy(Reader);return R;
}
static void pubsubCases(void)
{
    struct Case {cstr Wire;xredispubsubstatus Status;xredispubsubkind Kind;uint32 Flags;uint64 Count;} Cases[]={
        {"*3\r\n+message\r\n$3\r\na\0b\r\n$4\r\nv\0\r\n\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_MESSAGE,0,0},
        {">4\r\n+pmessage\r\n$2\r\np*\r\n$0\r\n\r\n$0\r\n\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_PMESSAGE,0,0},
        {">3\r\n+smessage\r\n$0\r\n\r\n$0\r\n\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_SMESSAGE,0,0},
        {"*3\r\n+subscribe\r\n$0\r\n\r\n:1\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_SUBSCRIBE,0,1},
        {">3\r\n+psubscribe\r\n+p*\r\n:9223372036854775807\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_PSUBSCRIBE,0,INT64_MAX},
        {">3\r\n+ssubscribe\r\n+s\r\n:1\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_SSUBSCRIBE,0,1},
        {"*3\r\n+unsubscribe\r\n$-1\r\n:2\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_UNSUBSCRIBE,XREDIS_PUBSUB_NAME_ABSENT,2},
        {">3\r\n+punsubscribe\r\n_\r\n:0\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_PUNSUBSCRIBE,XREDIS_PUBSUB_NAME_ABSENT,0},
        {">3\r\n+sunsubscribe\r\n_\r\n:0\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_SUNSUBSCRIBE,XREDIS_PUBSUB_NAME_ABSENT,0},
        {"*2\r\n+pong\r\n$0\r\n\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_PONG,0,0},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n*2\r\n$0\r\n\r\n$1\r\nx\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_MESSAGE,XREDIS_PUBSUB_STRUCTURED,0},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n*-1\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_MESSAGE,XREDIS_PUBSUB_STRUCTURED,0},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n$-1\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_MESSAGE,XREDIS_PUBSUB_STRUCTURED,0},
        {">3\r\n+unsubscribe\r\n$0\r\n\r\n:0\r\n",XREDIS_PUBSUB_EVENT,XREDIS_PUBSUB_UNSUBSCRIBE,0,0},
        {">3\r\n+message\r\n+c\r\n:1\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*4\r\n+pmessage\r\n_\r\n+c\r\n+v\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+smessage\r\n+c\r\n$-1\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+subscribe\r\n_\r\n:1\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+ssubscribe\r\n+c\r\n:0\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+subscribe\r\n+c\r\n:-1\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+unsubscribe\r\n*-1\r\n:0\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+punsubscribe\r\n+c\r\n+0\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*1\r\n+subscribe\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {">4\r\n+sunsubscribe\r\n+c\r\n:1\r\n+x\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*1\r\n+pong\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*2\r\n+pong\r\n_\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+message\r\n+__redis__:invalidate\r\n*1\r\n+x\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {">3\r\n+message\r\n+__redis__:invalidate\r\n*0\r\n",XREDIS_PUBSUB_MALFORMED,0,0,0},
        {"*3\r\n+other\r\n+c\r\n+v\r\n",XREDIS_PUBSUB_OTHER,0,0,0},
        {"*0\r\n",XREDIS_PUBSUB_OTHER,0,0,0},{"*-1\r\n",XREDIS_PUBSUB_OTHER,0,0,0},
        {"+PONG\r\n",XREDIS_PUBSUB_OTHER,0,0,0},{"-ERR request\r\n",XREDIS_PUBSUB_OTHER,0,0,0},
        {"~1\r\n+subscribe\r\n",XREDIS_PUBSUB_OTHER,0,0,0}
    };
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){
        /* Only the first binary literal has embedded zero bytes. */
        size_t Bytes=N?strlen(Cases[N].Wire):sizeof("*3\r\n+message\r\n$3\r\na\0b\r\n$4\r\nv\0\r\n\r\n")-1;
        for(size_t Fragment=1;Fragment<=Bytes;Fragment=Fragment==1?Bytes:Bytes+1){
            xredisreply* R=pubsubParse(Cases[N].Wire,Bytes,Fragment);xredispubsubevent E;memset(&E,0x55,sizeof(E));xredispubsubevent Saved=E;
            xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();xredispubsubstatus S=xredisPubSubEventRead(R,&E);
            testRequire(S==Cases[N].Status && xrtGetError()==Error,"exact event classification preserves ambient error");
            if(S==XREDIS_PUBSUB_EVENT){testRequire(E.Reply==R && E.Kind==Cases[N].Kind && E.Flags==Cases[N].Flags && E.Subscriptions==Cases[N].Count,"semantic fields and reported count");
                if(!N)testRequire(E.Channel.Size==3 && !memcmp(E.Channel.Data,"a\0b",3) && E.Payload.Size==4 && !memcmp(E.Payload.Data,"v\0\r\n",4),"binary views retain no terminator requirement");}
            else testRequire(!memcmp(&E,&Saved,sizeof(E)),"OTHER/malformed preserves complete output");
            xrtClearError();xredisReplyRelease(R);
        }
    }
}
static void pubsubPongs(void)
{
    static const char Binary[]="a\0\r\n";xbytesview Payload={(cbytes)Binary,sizeof(Binary)-1},Empty={NULL,0};
    struct {cstr Wire;size_t Bytes;const xbytesview* Expected;xredispubsubstatus Status;} Cases[]={
        {"+PONG\r\n",7,NULL,XREDIS_PUBSUB_EVENT},{"$0\r\n\r\n",6,&Empty,XREDIS_PUBSUB_EVENT},
        {"$4\r\na\0\r\n\r\n",10,&Payload,XREDIS_PUBSUB_EVENT},
        {"*2\r\n+pong\r\n$4\r\na\0\r\n\r\n",sizeof("*2\r\n+pong\r\n$4\r\na\0\r\n\r\n")-1,&Payload,XREDIS_PUBSUB_EVENT},
        {"*2\r\n+pong\r\n$0\r\n\r\n",18,NULL,XREDIS_PUBSUB_EVENT},
        {"+PONG\r\n",7,&Empty,XREDIS_PUBSUB_MALFORMED},{"$0\r\n\r\n",6,NULL,XREDIS_PUBSUB_MALFORMED},
        {"+wrong\r\n",8,NULL,XREDIS_PUBSUB_MALFORMED},{":1\r\n",4,NULL,XREDIS_PUBSUB_MALFORMED},
        {"$3\r\nabc\r\n",9,&Payload,XREDIS_PUBSUB_MALFORMED},
        {"*3\r\n+subscribe\r\n+c\r\n:1\r\n",26,NULL,XREDIS_PUBSUB_OTHER},
        {"-ERR denied\r\n",13,NULL,XREDIS_PUBSUB_OTHER}
    };
    for(size_t N=0;N<sizeof(Cases)/sizeof(*Cases);++N){
        size_t Bytes=Cases[N].Bytes;if(N!=2 && N!=3)Bytes=strlen(Cases[N].Wire);
        xredisreply* R=pubsubParse(Cases[N].Wire,Bytes,1);xredispubsubevent E;memset(&E,0x7a,sizeof(E));xredispubsubevent Saved=E;
        xrtSetErrorKind(XERR_MEMORY);const xerror* Error=xrtGetError();
        testRequire(xredisPubSubPongRead(R,Cases[N].Expected,&E)==Cases[N].Status && xrtGetError()==Error,"contextual scalar/aggregate binary PING");
        if(Cases[N].Status!=XREDIS_PUBSUB_EVENT)testRequire(!memcmp(&E,&Saved,sizeof(E)),"bad nonce/shape preserves output");
        xrtClearError();xredisReplyRelease(R);
    }
}
static void pubsubCommands(void)
{
    const cstr Words[]={"SUBSCRIBE","PSUBSCRIBE","SSUBSCRIBE","UNSUBSCRIBE","PUNSUBSCRIBE","SUNSUBSCRIBE","PING"};
    unsigned char Binary[]={'a',0,'\r','\n'};xbytesview Names[]={{Binary,sizeof(Binary)},{NULL,0},XRT_BYTES_LITERAL("same"),XRT_BYTES_LITERAL("same")};
    for(unsigned Op=0;Op<7;++Op)for(size_t N=0;N<=4;++N){
        unsigned char Actual[256],Expected[256];memset(Actual,0x66,sizeof(Actual));size_t Bytes=123,Written=456;
        bool Valid=(Op>=3 || N!=0) && (Op!=6 || N<=1);
        if(!Valid){testRequire(!xredisPubSubCommandSize((xredispubsubcommand)Op,Names,N,&Bytes) && Bytes==123,"typed command cardinality refusal");xrtClearError();continue;}
        xbytesview Args[5];Args[0]=(xbytesview){(cbytes)Words[Op],strlen(Words[Op])};memcpy(Args+1,Names,N*sizeof(*Names));size_t Reference=0;
        testRequire(xredisCommandWrite(Expected,sizeof(Expected),Args,N+1,&Reference) && xredisPubSubCommandSize((xredispubsubcommand)Op,Names,N,&Bytes) && Bytes==Reference,"typed command measurement matches full RESP frame");
        testRequire(!xredisPubSubCommandWrite(Actual,Bytes-1,(xredispubsubcommand)Op,Names,N,&Written) && Written==456 && Actual[0]==0x66,"capacity failure is atomic");xrtClearError();
        testRequire(xredisPubSubCommandWrite(Actual,Bytes,(xredispubsubcommand)Op,Names,N,&Written) && Written==Bytes && !memcmp(Actual,Expected,Bytes) && Actual[Bytes]==0x66,"exact frame and untouched tail");
    }
    size_t Bytes=17;testRequire(!xredisPubSubCommandSize((xredispubsubcommand)-1,Names,1,&Bytes) && Bytes==17,"invalid command enum");xrtClearError();
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,SIZE_MAX,&Bytes) && Bytes==17,"count overflow before input access");xrtClearError();
    xbytesview Bad={NULL,1};testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,&Bad,1,&Bytes) && Bytes==17,"invalid name span");xrtClearError();
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,2,(size_t*)&Names[1]),"size cannot overwrite descriptors");xrtClearError();
    testRequire(!xredisPubSubCommandWrite(Names,sizeof(Names),XREDIS_PUBSUB_CMD_UNSUBSCRIBE,NULL,0,(size_t*)Names),"output/result overlap");xrtClearError();
    testRequire(!xredisPubSubCommandWrite(Names,sizeof(Names),XREDIS_PUBSUB_CMD_SUBSCRIBE,Names,2,NULL),"wire cannot overwrite name descriptors");xrtClearError();
    xbytesview Many[XREDIS_PUBSUB_NAMES_MAX],Args[XREDIS_PUBSUB_NAMES_MAX+1];unsigned char A[8192],B[8192];
    for(size_t N=0;N<XREDIS_PUBSUB_NAMES_MAX;++N)Many[N]=(xbytesview){NULL,0};
    Args[0]=(xbytesview)XRT_BYTES_LITERAL("SUBSCRIBE");memcpy(Args+1,Many,sizeof(Many));
    const size_t Widths[]={1,9,10,99,100,999,1000,1024};
    for(size_t N=0;N<sizeof(Widths)/sizeof(*Widths);++N){size_t Expected=0,Actual=0;
        testRequire(xredisCommandWrite(A,sizeof(A),Args,Widths[N]+1,&Expected) && xredisPubSubCommandWrite(B,sizeof(B),XREDIS_PUBSUB_CMD_SUBSCRIBE,Many,Widths[N],&Actual) && Actual==Expected && !memcmp(A,B,Actual),"maximum count and decimal-width boundaries");}
}
static void pubsubOwnership(void)
{
    static const char Wire[]="|1\r\n+meta\r\n+value\r\n>3\r\n+message\r\n$3\r\na\0b\r\n$0\r\n\r\n";
    xredisreply* R=pubsubParse(Wire,sizeof(Wire)-1,1);xredispubsubevent E;testRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT,"attribute-preserving view");
    xredisreplyinfo Info={0};Info.Size=sizeof(Info);xredisReplyInfo(R,&Info);xredisreplyvalue Root;xredisReplyGet(R,Info.Root,&Root);testRequire(Root.Attributes!=XREDIS_REPLY_NONE,"root attributes stay attached");
    testRequire(xredisPubSubEventRead(R,(xredispubsubevent*)E.Channel.Data)==XREDIS_PUBSUB_ERROR,"output cannot alias reply payload");xrtClearError();
    testRequire(xredisPubSubEventRead(R,(xredispubsubevent*)R)==XREDIS_PUBSUB_ERROR,"output cannot alias reply metadata");xrtClearError();
    testRequire(xredisPubSubPongRead(R,(const xbytesview*)&E,&E)==XREDIS_PUBSUB_ERROR,"expected/output descriptor overlap");xrtClearError();
    xallocationbudget* B=NULL;xallocationscope Scope={0};testRequire(xrtAllocationBudgetCreate(1,&B) && xrtAllocationBudgetEnter(B,&Scope),"minimal decode/encode budget");
    xbytesview Name=XRT_BYTES_LITERAL("binary\0name");unsigned char Out[128];size_t Written;
#if defined(XRT_FEATURE_MEMORY_STATS)
    xmemstats Before,After;xrtMemStatsEnable(true);xrtMemStatsGet(&Before);
#endif
    for(unsigned N=0;N<4096;++N)testRequire(xredisPubSubEventRead(R,&E)==XREDIS_PUBSUB_EVENT && xredisPubSubCommandWrite(Out,sizeof(Out),XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Written),"warmed views/commands allocate nothing");
#if defined(XRT_FEATURE_MEMORY_STATS)
    xrtMemStatsGet(&After);xrtMemStatsEnable(false);
    testRequire(Before.MallocCalls==After.MallocCalls && Before.CallocCalls==After.CallocCalls && Before.ReallocCalls==After.ReallocCalls && Before.BlockAllocCalls==After.BlockAllocCalls,"no allocation API/backing requests on warmed operations");
#endif
    testRequire(xrtAllocationBudgetLeave(&Scope),"minimal budget leave");xallocationbudgetinfo I={0};I.Size=sizeof(I);testRequire(xrtAllocationBudgetInfo(B,&I) && !I.LiveCount && !I.LiveBytes,"no retained allocation duties");xrtAllocationBudgetFree(B);
    testRequire(xredisReplyRetain(R),"retain view owner");xredisReplyRelease(R);testRequire(E.Channel.Size==3 && !memcmp(E.Channel.Data,"a\0b",3),"view after parser and original owner release");xredisReplyRelease(R);
}
static void pubsubBounds(void)
{
    size_t Out=77;
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,NULL,1,&Out) && Out==77 && xrtErrorCode(xrtGetError())==XREDIS_COMMAND_ARGUMENT,"missing names preserve command input diagnostic");xrtClearError();
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,(const xbytesview*)(uintptr_t)(UINTPTR_MAX-1u),1,&Out) && Out==77 && xrtErrorCode(xrtGetError())==XREDIS_COMMAND_OVERFLOW,"wrapped descriptors rejected before access");xrtClearError();
    xbytesview Name={(cbytes)(uintptr_t)(UINTPTR_MAX-1u),4};
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Out) && Out==77 && xrtErrorCode(xrtGetError())==XREDIS_COMMAND_OVERFLOW,"wrapped payload span diagnostic");xrtClearError();
    if(SIZE_MAX>(uint64)INT64_MAX){Name=(xbytesview){(cbytes)1,(size_t)INT64_MAX+1u};
        testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,&Name,1,&Out) && Out==77 && xrtErrorCode(xrtGetError())==XREDIS_COMMAND_OVERFLOW,"RESP signed length bound before data access");xrtClearError();}
    xbytesview Huge[]={{(cbytes)1,SIZE_MAX/2u},{(cbytes)1,SIZE_MAX/2u}};
    testRequire(!xredisPubSubCommandSize(XREDIS_PUBSUB_CMD_SUBSCRIBE,Huge,2,&Out) && Out==77 && xrtErrorCode(xrtGetError())==XREDIS_PUBSUB_LIMIT,"encoded total overflows atomically");xrtClearError();
}
#ifndef XREDIS_PUBSUB_TEST_MAIN
#define XREDIS_PUBSUB_TEST_MAIN main
#endif
int XREDIS_PUBSUB_TEST_MAIN(void)
{
    pubsubCases();pubsubPongs();pubsubCommands();pubsubOwnership();pubsubBounds();
    puts("[PASS] Pub/Sub wire: all ACK/message families, binary/empty/null/counts, contextual PONG, exact atomic commands, fragmentation, attributes, aliases and zero-heap ownership");return 0;
}
