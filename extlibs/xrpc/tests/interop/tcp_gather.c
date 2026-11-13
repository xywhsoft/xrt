/* Production-TU probe; omit ordinary RPC TCP and native Core TCP objects.
 * Only native send APIs are interposed; no hooks enter production code. */
#include "../test.h"
#include <xrpc/tcp.h>
#include <xrt/atomic.h>
#include "../../../../src/internal/xrt_tcp.h"
static xnetresult tcpGatherSend(xnetstream*,const void*,size_t);
#if !defined(XRPC_TCP_GATHER_BASELINE)
static xnetresult tcpGatherSendVec(xnetstream*,const xnetspan*,size_t);
#endif
static size_t tcpGatherWritable(xnetstream*);
static size_t tcpGatherPending(const xnetstream*);
#define xrtNetStreamSend tcpGatherSend
#define xrtNetStreamSendVec tcpGatherSendVec
#define xrtNetStreamWritable tcpGatherWritable
#define xrtNetStreamPending tcpGatherPending
#ifndef XRPC_TCP_PROBE_SOURCE
#define XRPC_TCP_PROBE_SOURCE "../../src/tcp/tcp.c"
#endif
#include XRPC_TCP_PROBE_SOURCE
#undef xrtNetStreamSend
#undef xrtNetStreamSendVec
#undef xrtNetStreamWritable
#undef xrtNetStreamPending
#define main tcpGatherOrdinaryMain
#include "../test_tcp.c"
#undef main
static xatomic64 TcpGatherOffers,TcpGatherVectors,TcpGatherNative;
static xatomic32 TcpGatherBlockWrites;
typedef struct tcpGatherState {
    bool Active,Again,Memory,Pending;
    size_t Credit,Bytes,Offers,Vectors,Largest;
    unsigned char Wire[4096];
} tcpGatherState;
static tcpGatherState TcpGather;
static xnetresult tcpGatherOffer(const xnetspan* Spans,size_t Count)
{
    testRequire(Count && Count<=2,"gather span bound");size_t Total=0;
    for(size_t I=0;I<Count;++I){testRequire(Spans[I].Data && Spans[I].Size,"gather empty span");Total+=Spans[I].Size;}
    testRequire(Total<=TcpGather.Credit && Total<=sizeof(TcpGather.Wire)-TcpGather.Bytes,"gather credit/buffer bound");
    ++TcpGather.Offers;if(Count==2)++TcpGather.Vectors;if(Total>TcpGather.Largest)TcpGather.Largest=Total;
    if(TcpGather.Again){TcpGather.Again=false;return XNET_RESULT_AGAIN;}
    if(TcpGather.Memory){xrtSetErrorInfo(XERR_MEMORY,"xrt.test",1,"injected atomic native send refusal");return XNET_RESULT_ERROR;}
    for(size_t I=0;I<Count;++I){memcpy(TcpGather.Wire+TcpGather.Bytes,Spans[I].Data,Spans[I].Size);TcpGather.Bytes+=Spans[I].Size;}return XNET_RESULT_OK;
}
static xnetresult tcpGatherSend(xnetstream* S,const void* Bytes,size_t Count)
{
    if(TcpGather.Active){xnetspan Span={Bytes,Count};return tcpGatherOffer(&Span,1);}
    xrtAtomic64FetchAdd(&TcpGatherOffers,1,XMEMORY_RELAXED);return xrtNetStreamSend(S,Bytes,Count);
}
#if !defined(XRPC_TCP_GATHER_BASELINE)
static xnetresult tcpGatherSendVec(xnetstream* S,const xnetspan* Spans,size_t Count)
{
    if(TcpGather.Active)return tcpGatherOffer(Spans,Count);
    xrtAtomic64FetchAdd(&TcpGatherOffers,1,XMEMORY_RELAXED);xrtAtomic64FetchAdd(&TcpGatherVectors,1,XMEMORY_RELAXED);return xrtNetStreamSendVec(S,Spans,Count);
}
#endif
static size_t tcpGatherWritable(xnetstream* S)
{return TcpGather.Active?TcpGather.Credit:xrtNetStreamWritable(S);}
static size_t tcpGatherPending(const xnetstream* S)
{return TcpGather.Active?(TcpGather.Pending?1:0):xrtNetStreamPending(S);}
/* These delegates replace only the corresponding calls in a separately
 * compiled native tcp.c. They count kernel-facing public send offers, not
 * an operating-system syscall trace or a general syscall-count guarantee. */
xnetresult tcpGatherSocketSendVec(xnetsocket S,const xnetspan* P,size_t N,size_t* Used)
{if(xrtAtomic32Load(&TcpGatherBlockWrites,XMEMORY_ACQUIRE)){*Used=0;return XNET_RESULT_AGAIN;}xrtAtomic64FetchAdd(&TcpGatherNative,1,XMEMORY_RELAXED);return xrtNetSocketSendVec(S,P,N,Used);}
bool tcpGatherPortSendVec(xnetport* Port,xnetsocket S,const xnetspan* P,size_t N,uint64 Id,ptr User)
{xrtAtomic64FetchAdd(&TcpGatherNative,1,XMEMORY_RELAXED);return xrtNetPortSendVec(Port,S,P,N,Id,User);}
static void tcpGatherCleanup(xrpctcp* P,xfuture* F,xrpccallstatus Status)
{
    xrpcTcpOutputClear(P);testRequire(xrpcDuplexClose(P->Duplex,Status==XRPC_CALL_NOTIFY_DONE?XRPC_CALL_CLOSED:Status) && xrpcDuplexStep(P->Duplex),"gather endpoint close");
    const xrpccallresult* R=xrtFutureValue(F);testRequire(R && R->Status==Status && R->Delivery==XRPC_MAY_HAVE_EXECUTED,"gather delivery after offer");
    testRequire(xrpcDuplexDestroy(P->Duplex),"gather endpoint retirement");xrtFutureDestroy(F);xrtMutexUnit(&P->Gate);xrtClearError();
}
static void tcpGatherBoundaries(void)
{
    static const size_t Limits[]={1,3,4,5,19,20,21,22,23,24,32,64,4096};
    for(unsigned Format=1;Format<=2;++Format)for(unsigned Pending=0;Pending<2;++Pending)for(unsigned Mode=0;Mode<3;++Mode)for(size_t K=0;K<sizeof(Limits)/sizeof(*Limits);++K){
        xrpctcp P;memset(&P,0,sizeof(P));testRequire(xrtMutexInit(&P.Gate) && xrpcTcpConfigInit(&P.Config) && xrpcDuplexCreate(NULL,&P.Duplex),"gather probe init");
        P.Info.State=XRPC_TCP_OPEN;P.Stream=(xnetstream*)(uintptr_t)1;P.Config.Frame.Format=(xrpcframeformat)Format;P.Config.WorkFrames=1;
        P.Config.WriteChunkBytes=Mode==0?Limits[K]:4096;P.Config.WriteBytes=Mode==1?Limits[K]:4096;
        xfuture* F=NULL;testRequire(xrpcDuplexNotify(P.Duplex,XRT_STR_LITERAL("probe"),XRT_STR_LITERAL("[1,2,3]"),XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(P.Duplex),"gather probe notification");
        memset(&TcpGather,0,sizeof(TcpGather));TcpGather.Active=true;TcpGather.Pending=Pending!=0;TcpGather.Again=true;TcpGather.Credit=Mode==2?Limits[K]:4096;
        xrpcTcpWrite(&P);testRequire(TcpGather.Offers==1 && !TcpGather.Bytes && !P.HeaderOffset && !P.BodyOffset && !P.Info.WriteBytes && P.Offered && P.Output,"gather AGAIN changed input offsets/credit");
        size_t Turns=0;while(!xrtFutureDone(F)){size_t Before=TcpGather.Bytes,Offers=TcpGather.Offers;xrpcTcpWrite(&P);testRequire(TcpGather.Bytes-Before<=P.Config.WriteBytes && TcpGather.Bytes-Before<=P.Config.WriteChunkBytes && TcpGather.Bytes-Before<=TcpGather.Credit && TcpGather.Offers-Offers<=1,"gather per-turn credit");testRequire(++Turns<1000,"gather writer stalled");}
        testRequire(P.Info.WriteFrames==1 && P.Info.WriteBytes==TcpGather.Bytes && !P.Output && (!Pending || !TcpGather.Vectors),"gather complete-message/queued-tail accounting");
        xrpcframereader* Reader=NULL;testRequire(xrpcFrameReaderCreate(&P.Config.Frame,&Reader),"gather frame proof");size_t Used=0;xrpcframeview Frame;
        testRequire(xrpcFrameReaderFeed(Reader,(xbytesview){TcpGather.Wire,TcpGather.Bytes},&Used,&Frame)==XRPC_FRAME_READY && Used==TcpGather.Bytes && Frame.Body.Size && xrpcFrameReaderEnd(Reader)==XRPC_FRAME_END,"gather accepted wire duplicated/truncated");
        xrpcmessage M;xrpcdocument Doc;testRequire(xrpcDecode((xstrview){(cstr)Frame.Body.Data,Frame.Body.Size},NULL,&M,1,&Doc)==XRPC_DECODE_DONE && M.Kind==XRPC_MESSAGE_NOTIFICATION && M.Params.Raw.Size==7 && !memcmp(M.Params.Raw.Data,"[1,2,3]",7),"gather protocol/body proof");
        xrpcFrameReaderDestroy(Reader);tcpGatherCleanup(&P,F,XRPC_CALL_NOTIFY_DONE);TcpGather.Active=false;
    }
    for(unsigned Format=1;Format<=2;++Format){xrpctcp P;memset(&P,0,sizeof(P));testRequire(xrtMutexInit(&P.Gate) && xrpcTcpConfigInit(&P.Config) && xrpcDuplexCreate(NULL,&P.Duplex),"gather refusal init");P.Info.State=XRPC_TCP_OPEN;P.Stream=(xnetstream*)(uintptr_t)1;P.Config.Frame.Format=(xrpcframeformat)Format;
        xfuture* F=NULL;testRequire(xrpcDuplexNotify(P.Duplex,XRT_STR_LITERAL("probe"),(xstrview){0},XRT_WAIT_FOREVER,&F)==XRPC_DUPLEX_ACCEPTED && xrpcDuplexStep(P.Duplex),"gather refusal call");memset(&TcpGather,0,sizeof(TcpGather));TcpGather.Active=TcpGather.Memory=true;TcpGather.Credit=4096;
        xrpcTcpWrite(&P);testRequire(P.Info.State==XRPC_TCP_CLOSING && P.Info.Reason==XRPC_TCP_MEMORY && !P.HeaderOffset && !P.BodyOffset && !P.Info.WriteBytes && !TcpGather.Bytes,"gather memory refusal advanced offsets");tcpGatherCleanup(&P,F,XRPC_CALL_MEMORY);TcpGather.Active=false;
    }
}
static void tcpGatherNativeProof(void)
{
    tcpTest T;tcpTestStart(&T,1,false);xrtAtomic64Store(&TcpGatherOffers,0,XMEMORY_RELAXED);xrtAtomic64Store(&TcpGatherVectors,0,XMEMORY_RELAXED);xrtAtomic64Store(&TcpGatherNative,0,XMEMORY_RELAXED);
    xfuture* F=NULL;testRequire(xrpcTcpCall(T.Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&F)==XRPC_DUPLEX_ACCEPTED,"gather native proof call");tcpTestResult(F,XRPC_CALL_OK,XRPC_REPLIED);
    uint64 Offers=xrtAtomic64Load(&TcpGatherOffers,XMEMORY_RELAXED),Vectors=xrtAtomic64Load(&TcpGatherVectors,XMEMORY_RELAXED),Native=xrtAtomic64Load(&TcpGatherNative,XMEMORY_RELAXED);
#if defined(XRPC_TCP_GATHER_BASELINE)
    testRequire(Offers==4 && !Vectors && Native==4,"baseline native two-frame send proof");
#else
    testRequire(Offers==2 && Vectors==2 && Native==2,"gather native two-frame send proof");
#endif
    printf("native_offer_proof: stream_offers=%llu vector_offers=%llu kernel_facing_offers=%llu\n",(unsigned long long)Offers,(unsigned long long)Vectors,(unsigned long long)Native);tcpTestFinish(&T);xrtFutureDestroy(F);
}
typedef struct tcpGatherBurst {
    xrpctcp* Peer;xnetpost Post;xatomic32 Ready,Permit,Done;size_t Bytes,Blocks,Capacity,BlockSize;
} tcpGatherBurst;
static void tcpGatherBurstWork(xnetworker* W,ptr Data)
{
    (void)W;tcpGatherBurst* B=Data;B->Peer->Config.WorkFrames=64;xrtAtomic32Store(&B->Ready,1,XMEMORY_RELEASE);double End=xrtTimer()+10;
    while(!xrtAtomic32Load(&B->Permit,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"gather burst caller did not publish");xrtSleep(0);}
    testRequire(xrpcDuplexStep(B->Peer->Duplex),"gather burst owner step");xrtAtomic32Store(&TcpGatherBlockWrites,1,XMEMORY_RELEASE);xrpcTcpWrite(B->Peer);
    const xnetbuf* Buffer=&B->Peer->Stream->WriteBuffer;B->Bytes=Buffer->Size;B->Blocks=Buffer->Blocks;B->BlockSize=Buffer->Pool->Config.BlockSize[0];
    for(const xnetblock* P=Buffer->Head;P;P=P->Next)B->Capacity+=P->Capacity;
    xrtAtomic32Store(&TcpGatherBlockWrites,0,XMEMORY_RELEASE);xrtAtomic32Store(&B->Done,1,XMEMORY_RELEASE);
}
static void tcpGatherQueuePacking(void)
{
    tcpTest T;tcpTestStart(&T,1,false);xrpctcpinfo I={0};I.Size=sizeof(I);double End=xrtTimer()+10;
    do{testRequire(xrpcTcpInfo(T.Left,&I) && xrtTimer()<End,"gather burst left open");xrtSleep(1);}while(I.State!=XRPC_TCP_OPEN);
    tcpGatherBurst B;memset(&B,0,sizeof(B));B.Peer=T.Left;xrtAtomic32Init(&B.Ready,0);xrtAtomic32Init(&B.Permit,0);xrtAtomic32Init(&B.Done,0);
    testRequire(xrtNetPostInit(&B.Post) && xrtNetPost(xrtNetEngineWorker(T.Engine,1),&B.Post,tcpGatherBurstWork,&B),"gather burst owner post");
    while(!xrtAtomic32Load(&B.Ready,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"gather burst ready");xrtSleep(1);}xfuture* F[16];
    for(unsigned N=0;N<16;++N)testRequire(xrpcTcpNotify(T.Left,XRT_STR_LITERAL("note"),XRT_STR_LITERAL("[1,2,3]"),10000,F+N)==XRPC_DUPLEX_ACCEPTED,"gather burst notify");
    xrtAtomic32Store(&B.Permit,1,XMEMORY_RELEASE);while(!xrtAtomic32Load(&B.Done,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<End,"gather burst owner stalled");xrtSleep(1);}
    for(unsigned N=0;N<16;++N){tcpTestResult(F[N],XRPC_CALL_NOTIFY_DONE,XRPC_MAY_HAVE_EXECUTED);xrtFutureDestroy(F[N]);}
    xfuture* Echo=NULL;testRequire(xrpcTcpCall(T.Left,XRT_STR_LITERAL("echo"),XRT_STR_LITERAL("[1,2,3]"),5000,&Echo)==XRPC_DUPLEX_ACCEPTED,"gather burst echo");tcpTestResult(Echo,XRPC_CALL_OK,XRPC_REPLIED);xrtFutureDestroy(Echo);
    printf("native_queue_packing: messages=16 queued_bytes=%zu owned_blocks=%zu payload_capacity=%zu block_class=%zu\n",B.Bytes,B.Blocks,B.Capacity,B.BlockSize);tcpTestFinish(&T);
    testRequire(B.Bytes && B.Capacity<=B.Bytes+2*B.BlockSize,"gather queued buffer packing regressed");
}
int main(void)
{
    xrtAtomic64Init(&TcpGatherOffers,0);xrtAtomic64Init(&TcpGatherVectors,0);xrtAtomic64Init(&TcpGatherNative,0);xrtAtomic32Init(&TcpGatherBlockWrites,0);(void)tcpGatherPending;tcpGatherNativeProof();tcpGatherQueuePacking();tcpGatherBoundaries();testRequire(tcpGatherOrdinaryMain()==0,"gather full real TCP suite");puts("[PASS] production-TU TCP gather: 156 empty/queued framing/budget/chunk/writable boundaries, AGAIN/no-advance, memory refusal, native send offer proof/actual buffer packing, full real TCP suite");return 0;
}
