/* Reuse the actual immutable-batch baseline, implementation, wait and heap
 * assertions. Only own literal-loopback normal traffic and allocation faults. */
#define main immutableBatchRegressionMain
#include "test_net_tcp_bytes_vec.c"
#undef main
#ifdef TEST_PINNED_TIMING
typedef xdeadline chain_deadline;
#define CHAIN_AFTER() xrtDeadlineAfter(UINT64_C(5000000))
#define CHAIN_EXPIRED(v) xrtDeadlineExpired(v)
#else
typedef double chain_deadline;
#define CHAIN_AFTER() __xrtWaitAfter(INT64_C(5000))
#define CHAIN_EXPIRED(v) __xrtWaitExpired(v)
#endif
typedef struct ChainObservation {
    xnetbuf* source;xnetbytes* bytes;xnetstream* stream;const uint8* first;
    size_t nodes;unsigned byteEdges,streamEdges;xatomic32 observed;bool abort,plain;
} ChainObservation;
static unsigned chainAccepted,chainRejected,chainObserved,chainDeferred,chainReentries;
static bool chainEdge(xrtownershipref ref,ptr raw)
{
    ChainObservation* test=raw;
    if(ref.Data==test->bytes)++test->byteEdges;
    if(ref.Data==test->stream)++test->streamEdges;
    return true;
}
static void chainHighWater(xnetstream* stream,size_t queued,ptr raw)
{
    ChainObservation* test=raw;test->stream=stream;
    if(test->plain){CHECK(queued==4&&xrtNetStreamAbort(stream));xrtAtomic32Store(&test->observed,1,XMEMORY_RELEASE);return;}
    CHECK(queued==(test->nodes==2?4:test->nodes*2));
    CHECK(!test->source->Head&&!test->source->Tail&&!test->source->Size&&!test->source->Blocks);
    CHECK(__xrtNetStreamBuffersOwnership(stream,chainEdge,test));
    CHECK(test->byteEdges==(test->nodes==2?1:test->nodes)&&test->streamEdges==1);
    const xnetblock* view=stream->WriteBuffer.Head;CHECK(view&&view->External==test->first);
    for(size_t i=0;i<test->nodes;++i){CHECK(view&&view->Flags==XRT_NET_BLOCK_OWN_TCP_BUFFER);view=view->Next;}
    CHECK(!view);xrtAtomic32Store(&test->observed,1,XMEMORY_RELEASE);
    if(test->abort)CHECK(xrtNetStreamAbort(stream));
}
typedef struct ChainPair {xnetengine* engine;xnetlistener* listener;xnetstream *client,*peer;} ChainPair;
static ChainPair chainPair(xnetportbackend backend,ChainObservation* observation)
{
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    ChainPair pair={0};pair.engine=xrtNetEngineCreate(&config);CHECK(pair.engine&&xrtNetEngineStart(pair.engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    listen.Stream.WriteHighWater=4;listen.Stream.WriteLowWater=2;listen.Stream.WriteLimit=observation?512:8;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    pair.listener=xrtNetListen(pair.engine,&listen,NULL,NULL,NULL);CHECK(pair.listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(pair.listener,&address));
    xfuture* accepted=xrtNetListenerAcceptAsync(pair.listener);CHECK(accepted);
    xnetstreamevents events={0};events.HighWater=chainHighWater;
    pair.client=xrtNetStreamConnect(pair.engine,&address,0,&listen.Stream,observation?&events:NULL,observation);CHECK(pair.client);
    complete(accepted);pair.peer=xrtNetStreamRef(xrtFutureValue(accepted));CHECK(pair.peer);xrtFutureDestroy(accepted);
    xfuture* opened=xrtNetStreamWaitAsync(pair.client,XNET_STREAM_WAIT_OPEN);complete(opened);xrtFutureDestroy(opened);return pair;
}
static void chainPairDrop(ChainPair pair)
{
    if(pair.client){CHECK(xrtNetStreamAbort(pair.client));xrtNetStreamDestroy(pair.client);}
    CHECK(xrtNetStreamAbort(pair.peer)&&xrtNetListenerClose(pair.listener));
    xrtNetStreamDestroy(pair.peer);xrtNetListenerDestroy(pair.listener);
    for(unsigned retry=0;!xrtNetEngineStop(pair.engine);++retry){CHECK(retry<1000000);xrtClearError();xrtThreadYield();}
    CHECK(xrtNetEngineDestroy(pair.engine));xrtClearError();
}
static void chainBalanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtClearError();xrtMemDebugSnapshot(&after);
    CHECK(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes&&after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    CHECK(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&before->UseAfterFreeCount==after.UseAfterFreeCount&&
        before->OverflowCount==after.OverflowCount&&before->UnderflowCount==after.UnderflowCount);
}
typedef struct TakeCall {xnetstream* stream;xnetbuf* source;unsigned budget;xnetresult result;bool triggered;xatomic32 done;} TakeCall;
static void workerTake(xnetworker* worker,ptr raw)
{
    TakeCall* call=raw;(void)worker;xrtClearError();CHECK(xrtMemDebugFailAfter(call->budget));
    call->result=xrtNetStreamSendBuffer(call->stream,call->source);call->triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    xrtClearError();xrtAtomic32Store(&call->done,1,XMEMORY_RELEASE);
}
static void chainRun(xnetportbackend backend,unsigned budget,bool worker,bool large)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetbuf source;CHECK(xrtNetBufInit(&source,NULL));xnetbytes* bytes=xrtNetBytesCopy("XbcY",4);CHECK(bytes);
    ChainObservation observation={0};observation.source=&source;observation.bytes=bytes;observation.nodes=large?65:2;
    xrtAtomic32Init(&observation.observed,0);ChainPair pair=chainPair(backend,&observation);
    if(!large){CHECK(xrtNetBufAppend(&source,"Xa\0",3)&&xrtNetBufConsume(&source,1)==1);}
    for(size_t i=large?0:1;i<observation.nodes;++i)CHECK(xrtNetBufAppendBytes(&source,bytes,1,2));
    observation.first=large?xrtNetBytesView(bytes).Data+1:source.Head->Data+source.Head->Begin;
    xnetbuf original=source;TakeCall call={0};call.stream=pair.client;call.source=&source;call.budget=budget;xrtAtomic32Init(&call.done,0);
    if(worker){
        CHECK(xrtNetEnginePost(pair.engine,0,workerTake,&call));
        chain_deadline due=CHAIN_AFTER();while(!xrtAtomic32Load(&call.done,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    }else workerTake(NULL,&call);
    if(call.result==XNET_RESULT_ERROR){
        CHECK(call.triggered&&source.Head==original.Head&&source.Tail==original.Tail&&source.Size==original.Size&&source.Blocks==original.Blocks);
        ++chainRejected;
    }else{
        CHECK(call.result==XNET_RESULT_OK&&!source.Head&&!source.Size&&!source.Blocks);++chainAccepted;
        CHECK(xrtNetBufAppend(&source,"reuse",5));xrtNetBufClear(&source);xrtNetBytesDestroy(bytes);bytes=NULL;
        if(call.triggered){CHECK(worker&&xrtErrorKind(pair.client->Error)==XERR_MEMORY);++chainDeferred;}
        size_t count=call.triggered?0:large?130:4,offset=0;
        while(offset<count){xfuture* read=xrtNetStreamRecvAsync(pair.peer,count-offset);complete(read);
            xbytesview view=xrtNetBytesView(xrtFutureValue(read));CHECK(view.Size&&view.Size<=count-offset);
            for(size_t i=0;i<view.Size;++i)CHECK(view.Data[i]==(large?(const uint8*)"bc":(const uint8*)"a\0bc")[(offset+i)%(large?2:4)]);
            offset+=view.Size;xrtFutureDestroy(read);
        }
        CHECK(xrtAtomic32Load(&observation.observed,XMEMORY_ACQUIRE));++chainObserved;
    }
    xrtNetBufClear(&source);xrtNetBytesDestroy(bytes);chainPairDrop(pair);chainBalanced(&before);
}
typedef struct ChainGate {xatomic32 entered,open;} ChainGate;
static void chainGate(xnetworker* worker,ptr raw)
{
    (void)worker;ChainGate* gate=raw;xrtAtomic32Store(&gate->entered,1,XMEMORY_RELEASE);
    chain_deadline due=CHAIN_AFTER();while(!xrtAtomic32Load(&gate->open,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
}
static void chainStops(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);ChainPair pair=chainPair(backend,NULL);
    xnetbuf source;CHECK(xrtNetBufInit(&source,NULL)&&xrtNetBufAppend(&source,"a\0bc",4));
    xnetbufpool* foreignPool=xrtNetBufPoolCreate(NULL);CHECK(foreignPool);
    xnetbuf foreign;CHECK(xrtNetBufInit(&foreign,foreignPool)&&xrtNetBufAppend(&foreign,"pool",4));
    xnetblock* foreignHead=foreign.Head;
    CHECK(xrtNetStreamSendBuffer(pair.client,&foreign)==XNET_RESULT_ERROR&&foreign.Head==foreignHead&&foreign.Size==4);
    xrtClearError();xrtNetBufClear(&foreign);CHECK(xrtNetBufPoolDestroy(foreignPool));
    xnetwspan writable;CHECK(xrtNetBufReserve(&source,4,&writable));
    xnetblock* reserved=source.Reserved;CHECK(xrtNetStreamSendBuffer(pair.client,&source)==XNET_RESULT_ERROR&&source.Reserved==reserved&&source.Size==4);
    xrtClearError();xrtNetBufCancel(&source);
    CHECK(xrtNetBufAppend(&source,"overflow",8));
    xnetblock* head=source.Head;CHECK(xrtNetStreamSendBuffer(pair.client,&source)==XNET_RESULT_AGAIN&&source.Head==head&&source.Size==12);
    xrtNetBufClear(&source);CHECK(xrtNetBufAppend(&source,"a\0bc",4));
    ChainGate gate={0};xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);CHECK(xrtNetEnginePost(pair.engine,0,chainGate,&gate));
    chain_deadline due=CHAIN_AFTER();while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    CHECK(xrtNetStreamSendBuffer(pair.client,&source)==XNET_RESULT_OK&&!source.Size&&!source.Head);
    CHECK(xrtNetStreamAbort(pair.client));xrtNetStreamDestroy(pair.client);pair.client=NULL;
    xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);xrtNetBufClear(&source);chainPairDrop(pair);chainBalanced(&before);
}
static void chainReenter(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetbuf source;CHECK(xrtNetBufInit(&source,NULL)&&xrtNetBufAppend(&source,"Xa\0",3)&&xrtNetBufConsume(&source,1)==1);
    xnetbytes* bytes=xrtNetBytesCopy("XbcY",4);CHECK(bytes&&xrtNetBufAppendBytes(&source,bytes,1,2));
    ChainObservation observation={0};observation.source=&source;observation.bytes=bytes;observation.nodes=2;
    observation.first=source.Head->Data+source.Head->Begin;observation.abort=true;xrtAtomic32Init(&observation.observed,0);
    ChainPair pair=chainPair(backend,&observation);ChainGate gate={0};xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);
    CHECK(xrtNetEnginePost(pair.engine,0,chainGate,&gate));chain_deadline due=CHAIN_AFTER();
    while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    CHECK(xrtNetStreamSendBuffer(pair.client,&source)==XNET_RESULT_OK&&!source.Head&&!source.Size);
    xrtNetStreamDestroy(pair.client);pair.client=NULL;xrtNetBytesDestroy(bytes);
    xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&observation.observed,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    xrtNetBufClear(&source);chainPairDrop(pair);chainBalanced(&before);++chainReentries;
}
static void commandReenter(xnetportbackend backend,unsigned kind)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    ChainObservation observation={0};observation.plain=true;xrtAtomic32Init(&observation.observed,0);
    ChainPair pair=chainPair(backend,&observation);
    if(kind==3&&!(xrtNetPortCapabilities(pair.client->Worker->Port)&XNET_PORT_CAP_SEND_FILE)){
        chainPairDrop(pair);chainBalanced(&before);return;
    }
    xnetbytes* bytes=xrtNetBytesCopy("a\0bc",4);CHECK(bytes);xfile file=NULL;str path=NULL;
    if(kind==3){file=xrtFileTemp(NULL,"xrt-chain-",".bin",&path);size_t written=0;
        CHECK(file&&path&&xrtWriteFull(file,"a\0bc",4,&written)&&written==4);}
    ChainGate gate={0};xrtAtomic32Init(&gate.entered,0);xrtAtomic32Init(&gate.open,0);
    CHECK(xrtNetEnginePost(pair.engine,0,chainGate,&gate));chain_deadline due=CHAIN_AFTER();
    while(!xrtAtomic32Load(&gate.entered,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    xnetbytesref refs[2]={{bytes,0,2},{bytes,2,2}};
    xnetresult result=kind==0?xrtNetStreamSend(pair.client,"a\0bc",4):kind==1?
        xrtNetStreamSendBytes(pair.client,bytes,0,4):kind==2?xrtNetStreamSendBytesVec(pair.client,refs,2):
        xrtNetStreamSendFile(pair.client,file,0,4);
    CHECK(result==XNET_RESULT_OK);if(file)CHECK(xrtClose(file));xrtNetBytesDestroy(bytes);
    xrtNetStreamDestroy(pair.client);pair.client=NULL;xrtAtomic32Store(&gate.open,1,XMEMORY_RELEASE);
    while(!xrtAtomic32Load(&observation.observed,XMEMORY_ACQUIRE)){CHECK(!CHAIN_EXPIRED(due));xrtThreadYield();}
    chainPairDrop(pair);if(path){CHECK(xrtFileDelete(path));xrtFree(path);}chainBalanced(&before);++chainReentries;
}
int main(void)
{
    CHECK(immutableBatchRegressionMain()==0&&xrtMemDebugEnable(true));
    for(unsigned choice=0;choice<2;++choice){
        xnetportbackend backend=XNET_PORT_SELECT;
        if(choice){
            #ifdef _WIN32
            backend=XNET_PORT_IOCP;
            #else
            backend=XNET_PORT_EPOLL;
            #endif
        }
        for(unsigned worker=0;worker<2;++worker){for(unsigned budget=0;budget<12;++budget)chainRun(backend,budget,worker!=0,false);
            chainRun(backend,128,worker!=0,true);}
        chainStops(backend);chainReenter(backend);
        for(unsigned kind=0;kind<4;++kind)commandReenter(backend,kind);
    }
    CHECK(chainAccepted&&chainRejected&&chainObserved==chainAccepted&&xrtMemDebugReset());
    printf("TCP chain take: budgets=48 accepted=%u rejected=%u observed=%u deferred-IO-failures=%u reentries=%u; exact payload addresses, Worker/off-Worker, 65 refs, rollback/reuse, reservation/backpressure/abort, balanced\n",chainAccepted,chainRejected,chainObserved,chainDeferred,chainReentries);return 0;
}
