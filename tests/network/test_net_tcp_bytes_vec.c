/* Ordinary own literal-loopback batches, allocator failure and native edges.
 * The HighWater observation is on the actual owning Worker before DriveWrite;
 * it does not pretend an active callback is a whole-graph Freeze boundary. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
#ifdef TEST_PINNED_TIMING
#define WAIT UINT64_C(5000000)
#else
#define WAIT INT64_C(5000)
#endif
typedef struct Observation {xnetbytes* bytes;xnetstream* stream;unsigned edges,streams;xatomic32 observed;bool opaque;} Observation;
static unsigned acceptedCount,rejectedCount,observations;
static bool edge(xrtownershipref ref,ptr raw)
{
    Observation* test=raw;
    if(ref.Data==test->bytes)++test->edges;
    else if(ref.Data==test->stream)++test->streams;
    return true;
}
static void highWater(xnetstream* stream,size_t queued,ptr raw)
{
    Observation* test=raw;CHECK(queued==4);test->stream=stream;
    if(test->opaque)CHECK(!__xrtNetStreamBuffersOwnership(stream,NULL,NULL));
    else{
        CHECK(__xrtNetStreamBuffersOwnership(stream,edge,test)&&test->edges==2&&test->streams==1);
        xbytesview input=xrtNetBytesView(test->bytes);const xnetblock* first=stream->WriteBuffer.Head;
        CHECK(first&&first->External==input.Data&&first->Next&&first->Next->External==input.Data+2);
        /* A missing provenance tag must be rejected, not guessed from callbacks. */
        xnetblock* changed=stream->WriteBuffer.Head;uint32 tag=changed->Flags;changed->Flags=0;
        CHECK(!__xrtNetStreamBuffersOwnership(stream,NULL,NULL));changed->Flags=tag;
    }
    xrtAtomic32Store(&test->observed,1,XMEMORY_RELEASE);
}
static void complete(xfuture* future)
{CHECK(future&&xrtFutureWaitFor(future,WAIT)==XWAIT_OK&&xrtFutureState(future)==XFUTURE_RESOLVED);}
static void freeOpaque(ptr raw,cbytes data,size_t size)
{(void)data;(void)size;xrtNetBytesDestroy(raw);}
static void run(xnetportbackend backend,unsigned budget,bool opaque)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Workers=1;config.Backend=backend;config.NodeCacheBytes=0;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    listen.Stream.WriteLimit=8;listen.Stream.WriteHighWater=4;listen.Stream.WriteLowWater=2;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetlistener* listener=xrtNetListen(engine,&listen,NULL,NULL,NULL);CHECK(listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(listener,&address));
    xfuture* connection=xrtNetListenerAcceptAsync(listener);CHECK(connection);
    Observation test={0};xrtAtomic32Init(&test.observed,0);test.opaque=opaque;
    xnetstreamevents events={0};events.HighWater=highWater;
    xnetstream* client=xrtNetStreamConnect(engine,&address,0,&listen.Stream,&events,&test);CHECK(client);
    complete(connection);xnetstream* peer=xrtNetStreamRef(xrtFutureValue(connection));CHECK(peer);xrtFutureDestroy(connection);
    xfuture* ready=xrtNetStreamWaitAsync(client,XNET_STREAM_WAIT_OPEN);complete(ready);xrtFutureDestroy(ready);
    xnetbytes* bytes=xrtNetBytesCopy("a\0bc",4);CHECK(bytes);test.bytes=bytes;
    xnetbytesref refs[3]={{bytes,0,2},{bytes,2,2},{bytes,4,0}};
    xnetbytesref invalid={bytes,5,0};
    CHECK(xrtNetStreamSendBytesVec(client,&invalid,1)==XNET_RESULT_ERROR&&xrtErrorKind(xrtGetError())==XERR_RANGE);xrtClearError();
    xnetbytesref oversized[3]={{bytes,0,4},{bytes,0,4},{bytes,0,1}};
    CHECK(xrtNetStreamSendBytesVec(client,oversized,3)==XNET_RESULT_AGAIN&&xrtNetStreamPending(client)==0);xrtClearError();
    xnetresult result;
    CHECK(xrtMemDebugFailAfter(budget));
    if(opaque){
        CHECK(xrtNetBytesRef(bytes));xnetref raw={xrtNetBytesView(bytes).Data,4,freeOpaque,bytes};
        result=xrtNetStreamSendRefs(client,&raw,1);
        if(result!=XNET_RESULT_OK)xrtNetBytesDestroy(bytes);
    }else result=xrtNetStreamSendBytesVec(client,refs,3);
    bool triggered=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    if(result==XNET_RESULT_ERROR){CHECK(triggered);++rejectedCount;xrtClearError();}
    else{
        CHECK(result==XNET_RESULT_OK&&!triggered);++acceptedCount;
        /* Only native accepted references remain, never a retained language wrapper. */
        xrtNetBytesDestroy(bytes);bytes=NULL;
        size_t offset=0;while(offset<4){xfuture* input=xrtNetStreamRecvAsync(peer,4-offset);complete(input);
            xbytesview view=xrtNetBytesView(xrtFutureValue(input));CHECK(view.Size&&view.Size<=4-offset&&!memcmp(view.Data,"a\0bc"+offset,view.Size));
            offset+=view.Size;xrtFutureDestroy(input);}
        CHECK(xrtAtomic32Load(&test.observed,XMEMORY_ACQUIRE));++observations;
    }
    if(bytes){xrtownershipscope scope={0};while(!xrtOwnershipFreezeTryBegin(&scope))xrtThreadYield();
        size_t owners=0;xrtownershipref ref=xrtNetBytesOwnership(bytes);CHECK(ref.Ops->Count(ref.Data,&owners)&&owners==1);
        CHECK(xrtOwnershipScopeEnd(&scope));xrtNetBytesDestroy(bytes);}
    CHECK(xrtNetStreamAbort(client)&&xrtNetStreamAbort(peer)&&xrtNetListenerClose(listener));
    xrtNetStreamDestroy(client);xrtNetStreamDestroy(peer);xrtNetListenerDestroy(listener);
    for(unsigned retry=0;!xrtNetEngineStop(engine);++retry){CHECK(retry<1000000);xrtClearError();xrtThreadYield();}
    CHECK(xrtNetEngineDestroy(engine));xrtClearError();
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    CHECK(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&before.UseAfterFreeCount==after.UseAfterFreeCount&&
        before.OverflowCount==after.OverflowCount&&before.UnderflowCount==after.UnderflowCount);
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));
    for(unsigned backend=0;backend<2;++backend){
        xnetportbackend port=XNET_PORT_SELECT;
        if(backend){
            #ifdef _WIN32
            port=XNET_PORT_IOCP;
            #else
            port=XNET_PORT_EPOLL;
            #endif
        }
        for(unsigned budget=0;budget<12;++budget)run(port,budget,false);
        run(port,128,true);
    }
    CHECK(acceptedCount&&rejectedCount&&observations==acceptedCount&&xrtMemDebugReset());
    printf("TCP immutable batch: budgets=24 accepted=%u rejected=%u observed=%u, duplicate native edges, exact slices, opaque rejection, no caller-owner loss; balanced\n",acceptedCount,rejectedCount,observations);return 0;
}
