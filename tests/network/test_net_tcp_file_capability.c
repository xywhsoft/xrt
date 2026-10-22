/* Standalone normal loopback and capability admission; no security traffic.
 * The verifier selects implementation/features and TEST_PINNED_TIMING. */
#include "xrt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(C) do{if(!(C)){fprintf(stderr,"line %u: %s\n",(unsigned)__LINE__,#C);abort();}}while(0)
#ifdef TEST_PINNED_TIMING
#define FILE_WAIT UINT64_C(5000000)
#else
#define FILE_WAIT INT64_C(5000)
#endif
static void completed(xfuture* future,cstr stage)
{
    xwaitresult waited=future?xrtFutureWaitFor(future,FILE_WAIT):XWAIT_ERROR;
    xfuturestate state=future?xrtFutureState(future):XFUTURE_FAILED;
    if(waited!=XWAIT_OK||state!=XFUTURE_RESOLVED)fprintf(stderr,"stage=%s wait=%d state=%d\n",stage,(int)waited,(int)state);
    CHECK(future&&waited==XWAIT_OK&&state==XFUTURE_RESOLVED);
}
static void exercise(xnetportbackend backend)
{
    xmemdebugsnapshot before;xrtClearError();xrtMemDebugSnapshot(&before);
    xnetengineconfig config;xrtNetEngineConfigInit(&config);config.Backend=backend;config.Workers=1;
    xnetengine* engine=xrtNetEngineCreate(&config);CHECK(engine&&xrtNetEngineStart(engine));
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);listen.Stream.ReadMode=XNET_STREAM_READ_PROBE;
    CHECK(xrtNetAddrLoopback(&listen.Address,XNET_FAMILY_IPV4,0));
    xnetlistener* listener=xrtNetListen(engine,&listen,NULL,NULL,NULL);CHECK(listener);
    xnetaddr address;CHECK(xrtNetListenerLocal(listener,&address));
    xfuture* accepted=xrtNetListenerAcceptAsync(listener);CHECK(accepted);
    xnetstream* client=xrtNetStreamConnect(engine,&address,0,&listen.Stream,NULL,NULL);CHECK(client);
    completed(accepted,"accept");xnetstream* peer=xrtNetStreamRef(xrtFutureValue(accepted));CHECK(peer);xrtFutureDestroy(accepted);
    xfuture* ready=xrtNetStreamWaitAsync(client,XNET_STREAM_WAIT_OPEN);completed(ready,"open");xrtFutureDestroy(ready);
    str path=NULL;xfile file=xrtFileTemp(NULL,"xrt-file-cap-",".bin",&path);CHECK(file&&path);
    size_t written=0;CHECK(xrtWriteFull(file,"prefix:a\0b:suffix",17,&written)&&written==17);
    uint64 position=0,after=0;CHECK(xrtTell(file,&position));
    bool unsupported=false;
    #ifdef _WIN32
    unsupported=backend==XNET_PORT_SELECT;
    #endif
    xnetresult result=xrtNetStreamSendFile(client,file,7,3);
    if(unsupported){
        CHECK(result==XNET_RESULT_ERROR&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED);
        xrtClearError();CHECK(xrtNetStreamState(client)==XNET_STREAM_OPEN&&xrtNetStreamPending(client)==0);
        CHECK(xrtNetStreamSend(client,"a\0b",3)==XNET_RESULT_OK);
    }else CHECK(result==XNET_RESULT_OK);
    CHECK(xrtTell(file,&after)&&after==position&&xrtClose(file));
    size_t offset=0;while(offset<3){xfuture* read=xrtNetStreamRecvAsync(peer,3-offset);completed(read,"receive");
        xbytesview view=xrtNetBytesView(xrtFutureValue(read));CHECK(view.Size&&view.Size<=3-offset);
        CHECK(!memcmp(view.Data,&"a\0b"[offset],view.Size));offset+=view.Size;xrtFutureDestroy(read);}
    xfuture* closeClient=xrtNetStreamWaitAsync(client,XNET_STREAM_WAIT_CLOSE);
    xfuture* closePeer=xrtNetStreamWaitAsync(peer,XNET_STREAM_WAIT_CLOSE);
    CHECK(xrtNetStreamAbort(client)&&xrtNetStreamAbort(peer)&&xrtNetListenerClose(listener));
    /* Abort cancels even close-condition waiters; this is terminal transport,
     * not proof that the physical Worker tail has returned. Engine Stop below
     * must independently observe all actual live objects retiring. */
    CHECK(closeClient&&closePeer&&xrtFutureWaitFor(closeClient,FILE_WAIT)==XWAIT_OK&&
        xrtFutureWaitFor(closePeer,FILE_WAIT)==XWAIT_OK&&xrtFutureState(closeClient)==XFUTURE_CANCELLED&&
        xrtFutureState(closePeer)==XFUTURE_CANCELLED);
    xrtFutureDestroy(closeClient);xrtFutureDestroy(closePeer);
    xrtNetStreamDestroy(client);xrtNetStreamDestroy(peer);xrtNetListenerDestroy(listener);
    for(unsigned retry=0;!xrtNetEngineStop(engine);++retry){CHECK(retry<1000000);xrtClearError();xrtThreadYield();}
    CHECK(xrtNetEngineDestroy(engine));
    CHECK(xrtFileDelete(path));xrtFree(path);xrtClearError();
    xmemdebugsnapshot end;xrtMemDebugSnapshot(&end);
    CHECK(before.LiveCount==end.LiveCount&&before.LiveBytes==end.LiveBytes);
    CHECK(end.AllocCount-before.AllocCount==end.FreeCount-before.FreeCount);
    CHECK(before.InvalidFreeCount==end.InvalidFreeCount&&before.DoubleFreeCount==end.DoubleFreeCount);
    printf("TCP file capability: backend=%d unsupported=%d no cursor change, close-safe, NUL roundtrip and balanced\n",(int)backend,(int)unsupported);
}
int main(void)
{
    CHECK(xrtMemDebugEnable(true));exercise(XNET_PORT_SELECT);
    #ifdef _WIN32
    exercise(XNET_PORT_IOCP);
    #else
    exercise(XNET_PORT_EPOLL);
    #endif
    CHECK(xrtMemDebugReset());return 0;
}
