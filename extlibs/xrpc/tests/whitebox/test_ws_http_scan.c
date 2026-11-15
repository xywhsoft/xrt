/* Real production TU, public native IO and parser calls. Instrumentation only
 * counts parser input and paces the next real send after actual consumption. */
#include "../test.h"
#include <xrt/http1.h>
static xatomic32 ScanCalls,ScanBytes;
static xhttp1status scanRequest(xbytesview B,xhttp1head* H,const xhttp1limits* L,xhttp1errorinfo* E)
{xrtAtomic32FetchAdd(&ScanCalls,1,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&ScanBytes,(int32)B.Size,XMEMORY_RELAXED);return xrtHttp1RequestParse(B,H,L,E);}
static xhttp1status scanResponse(xbytesview B,xhttp1head* H,const xhttp1limits* L,xhttp1errorinfo* E)
{xrtAtomic32FetchAdd(&ScanCalls,1,XMEMORY_RELAXED);xrtAtomic32FetchAdd(&ScanBytes,(int32)B.Size,XMEMORY_RELAXED);return xrtHttp1ResponseParse(B,H,L,E);}
static size_t scanConsume(xnetstream*,size_t);
#define xrtHttp1RequestParse scanRequest
#define xrtHttp1ResponseParse scanResponse
#define xrtNetStreamConsume scanConsume
#ifndef XRPC_WS_SCAN_SOURCE
#define XRPC_WS_SCAN_SOURCE "../../src/ws/ws.c"
#endif
#include XRPC_WS_SCAN_SOURCE
#undef xrtHttp1RequestParse
#undef xrtHttp1ResponseParse
#undef xrtNetStreamConsume

#if !defined(XRPC_SCAN_BASELINE)
static void scanPrefixes(bytes Data,size_t Size,xhttp1limits Limits)
{
    xrpcwsupgrade H={0};H.Config.Head=Limits;H.Cache=Data;
    for(size_t N=0;N<=Size;++N){
        H.CacheUsed=N;size_t End=N;bool Parse=xrpcWsHttpProbeHeader(&H,&End);
        xhttpfield Fields[128];xhttp1head Head={.Fields=Fields,.FieldCapacity=128};xhttp1errorinfo Error={0};
        xhttp1status R=xrtHttp1RequestParse((xbytesview){Data,N},&Head,&Limits,&Error);xrtClearError();
        testRequire(Parse || R==XHTTP1_MORE,"incremental probe never skips an authoritative parser decision");
        if(Parse){testRequire(R!=XHTTP1_MORE || N==Limits.MaxHead,"probe requests Core only for a decision or the exact retained cap");if(R==XHTTP1_READY)testRequire(End==Head.Bytes,"probe preserves the exact first-header boundary");break;}
    }
}
static void scanDifferential(void)
{
    xhttp1limits L;xrtHttp1LimitsInit(&L);L.MaxHead=128;L.MaxStartLine=24;L.MaxFieldLine=16;L.MaxFields=4;
    static const cstr Inputs[]={"GET / HTTP/1.1\r\nHost: x\r\n\r\nSUFFIX","GET / HTTP/1.1\r\nX: \r\n\r\n","GET / HTTP/1.1\n","GET / HTTP/1.1\rX","GET /abcdefghijklmnop HTTP/1.1","GET / HTTP/1.1\r\nX: 123456789012345\r\n\r\n","\r\n\r\n","GET / HTTP/1.1\r\nX: a\n","GET / HTTP/1.1\r\nX: a\rX"};
    for(size_t I=0;I<sizeof(Inputs)/sizeof(*Inputs);++I)scanPrefixes((bytes)Inputs[I],strlen(Inputs[I]),L);
    unsigned char Data[132];uint32 State=89721;
    for(unsigned Trial=0;Trial<4000;++Trial){
        size_t N=24+Trial%109;for(size_t I=0;I<N;++I){State=State*1664525u+1013904223u;static const char Alphabet[]="GET /HTTP1.1:XYZ\r\n\t abc123";Data[I]=(unsigned char)Alphabet[(State>>16)%(sizeof(Alphabet)-1)];}
        if(Trial&1u){memcpy(Data,"GET / HTTP/1.1\r\n",16);}
        L.MaxHead=32+Trial%97;L.MaxStartLine=1+Trial%24;L.MaxFieldLine=1+Trial%16;scanPrefixes(Data,N,L);
    }
    puts("[PASS] all prefixes of 4009 adversarial heads agree with authoritative Core MORE/error/boundary decisions");
}
#endif

typedef struct scanScene {
    xnetengine* Engine;xnetlistener* Listener;xnetstream* Client;xrpcws* Peer;
    xatomicptr ServerRaw,ClientRaw;xatomic32 Bound,Done;xnetpost Next;
    char* Header;size_t HeaderBytes,Sent,Chunk;bool HeaderRead;
    double Deadline;
} scanScene;
static scanScene* ScanScene;
static void scanNext(xnetworker* W,ptr Data)
{
    (void)W;scanScene* S=Data;if(S->Sent==S->HeaderBytes)return;
    size_t N=S->HeaderBytes-S->Sent;if(N>S->Chunk)N=S->Chunk;
    testRequire(xrtNetStreamSend(S->Client,S->Header+S->Sent,N)==XNET_RESULT_OK,"actual paced native header offer");S->Sent+=N;
    if(S->Sent==S->HeaderBytes){static const unsigned char Ping[]={0x89,0x80,0x12,0x34,0x56,0x78};testRequire(xrtNetStreamSend(S->Client,Ping,sizeof(Ping))==XNET_RESULT_OK,"masked Ping immediately after final header bytes");}
}
static size_t scanConsume(xnetstream* Native,size_t N)
{
    size_t Used=xrtNetStreamConsume(Native,N);scanScene* S=ScanScene;
    if(S&&Used&&Native==xrtAtomicPtrLoad(&S->ServerRaw,XMEMORY_ACQUIRE)){
        /* A whole large offer can arrive in several native receive blocks.
         * No further application send is needed after the last offer. */
        if(!xrtNetPostPending(&S->Next))testRequire(xrtNetPost(xrtNetStreamWorker(xrtAtomicPtrLoad(&S->ClientRaw,XMEMORY_ACQUIRE)),&S->Next,scanNext,S),"pace next fragment after actual native consume");
    }
    return Used;
}
static void scanOpen(xnetstream* Native,ptr Data)
{scanScene* S=Data;S->Client=Native;xrtAtomicPtrStore(&S->ClientRaw,Native,XMEMORY_RELEASE);scanNext(xrtNetStreamWorker(Native),S);}
static void scanRead(xnetstream* Native,xnetbuf* B,ptr Data)
{
    scanScene* S=Data;
    if(!S->HeaderRead){
        xnetspan Span={0};if(!xrtNetBufFront(B,&Span))return;
        xhttpfield Fields[32];xhttp1head H={.Fields=Fields,.FieldCapacity=32};xhttp1status R=xrtHttp1ResponseParse((xbytesview){Span.Data,Span.Size},&H,NULL,NULL);
        if(R==XHTTP1_MORE)return;
        testRequire(R==XHTTP1_READY&&H.Status==101,"paced native request receives101");testRequire(xrtNetStreamConsume(Native,H.Bytes)==H.Bytes,"consume exactly101");S->HeaderRead=true;
    }
    unsigned char Pong[2];if(xrtNetBufPeek(B,0,Pong,2)!=2)return;
    testRequire(Pong[0]==0x8a&&!Pong[1],"first WS suffix survives HTTP scanning and produces Pong");testRequire(xrtNetStreamConsume(Native,2)==2,"consume actual native Pong");xrtAtomic32Store(&S->Done,1,XMEMORY_RELEASE);
}
static bool scanAccept(xnetlistener* L,xnetstream* Native,ptr Data)
{
    (void)L;scanScene* S=Data;xrpcwsconfig C;xrpcWsConfigInit(&C);xrpcwshttpconfig H;xrpcWsHttpConfigInit(&H);H.Host=XRT_STR_LITERAL("rpc.fixture");H.SetupTimeout=120000;
    xrtAtomicPtrStore(&S->ServerRaw,Native,XMEMORY_RELEASE);testRequire(xrpcWsHttpServerTcp(Native,&C,&H,&S->Peer),"actual native HTTP server factory");xrtAtomic32Store(&S->Bound,1,XMEMORY_RELEASE);return true;
}
static void scanNetwork(size_t Padding,size_t Chunk,xnetportbackend Backend)
{
    static const char Prefix[]="GET /rpc HTTP/1.1\r\nHost: rpc.fixture\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: jsonrpc\r\nX-Padding: ";
    scanScene S={.Chunk=Chunk};S.HeaderBytes=sizeof(Prefix)-1+Padding+4;S.Header=malloc(S.HeaderBytes);testRequire(S.Header!=NULL,"header fixture");memcpy(S.Header,Prefix,sizeof(Prefix)-1);memset(S.Header+sizeof(Prefix)-1,'a',Padding);memcpy(S.Header+S.HeaderBytes-4,"\r\n\r\n",4);
    xrtAtomicPtrInit(&S.ServerRaw,NULL);xrtAtomicPtrInit(&S.ClientRaw,NULL);xrtAtomic32Init(&S.Bound,0);xrtAtomic32Init(&S.Done,0);xrtAtomic32Init(&ScanCalls,0);xrtAtomic32Init(&ScanBytes,0);testRequire(xrtNetPostInit(&S.Next),"native pacing node");ScanScene=&S;
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;E.Backend=Backend;S.Engine=xrtNetEngineCreate(&E);testRequire(S.Engine&&xrtNetEngineStart(S.Engine),"native scan Engine");xnetlistenconfig L;xrtNetListenConfigInit(&L);xrtNetAddrLoopback(&L.Address,XNET_FAMILY_IPV4,0);xnetlistenerevents Accept={scanAccept,NULL,NULL};S.Listener=xrtNetListen(S.Engine,&L,&Accept,NULL,&S);xnetaddr Address;testRequire(S.Listener&&xrtNetListenerLocal(S.Listener,&Address),"native scan listener");
    xnetstreamevents Events={.Open=scanOpen,.Read=scanRead};double Start=xrtTimer();S.Deadline=Start+120;testRequire(xrtNetStreamConnect(S.Engine,&Address,1,NULL,&Events,&S)!=NULL,"raw paced client");
    while(!xrtAtomic32Load(&S.Done,XMEMORY_ACQUIRE)){testRequire(xrtTimer()<S.Deadline,"paced native101/Pong deadline");xrtSleep(1);}
    testRequire(xrtAtomic32Load(&S.Bound,XMEMORY_ACQUIRE),"published actual peer");xrpcwshttpinfo H={.Size=sizeof(H)};testRequire(xrpcWsHttpInfo(S.Peer,&H)&&H.Outcome==XRPC_WS_HTTP_OPEN&&H.HeaderBytes==S.HeaderBytes,"validated full header before WS handoff");
    testRequire(xrpcWsClose(S.Peer)&&xrpcWsWaitFor(S.Peer,10000)==XWAIT_OK&&xrpcWsDestroy(S.Peer),"physical server retirement");testRequire(xrtNetStreamAbort(S.Client)&&xrtNetStreamWaitRetired(S.Client,10000)==XWAIT_OK,"physical raw client retirement");xrtNetStreamDestroy(S.Client);testRequire(xrtNetListenerClose(S.Listener),"listener close");xrtNetListenerDestroy(S.Listener);
    xnetretireresult R;while((R=xrtNetEngineTryDestroy(S.Engine))==XNET_RETIRE_BUSY){testRequire(xrtTimer()<S.Deadline,"Engine physical join");xrtSleep(1);}testRequire(R==XNET_RETIRE_READY,"Engine destroyed");
    size_t Calls=(uint32)xrtAtomic32Load(&ScanCalls,XMEMORY_RELAXED),Bytes=(uint32)xrtAtomic32Load(&ScanBytes,XMEMORY_RELAXED);
    printf("[scan] header=%zu chunk=%zu parser_calls=%zu parser_input=%zu seconds=%.6f copy=%zu\n",S.HeaderBytes,Chunk,Calls,Bytes,xrtTimer()-Start,H.HeaderCopyBytes);
    #if !defined(XRPC_SCAN_BASELINE)
    testRequire(Bytes<=S.HeaderBytes+Chunk&&Calls<=4,"actual fragmented cache path has linear authoritative parser input");
    #else
    if(Chunk==1)testRequire(Bytes>S.HeaderBytes*S.HeaderBytes/3,"original production path reproduces quadratic parser input");
    #endif
    ScanScene=NULL;free(S.Header);
}
int main(int Argc,char** Argv)
{
    setbuf(stdout,NULL);
    #if !defined(XRPC_SCAN_BASELINE)
    scanDifferential();
    #endif
    xnetportbackend B=Argc>1&&!strcmp(Argv[1],"iocp")?XNET_PORT_IOCP:Argc>1&&!strcmp(Argv[1],"epoll")?XNET_PORT_EPOLL:XNET_PORT_SELECT;
    for(size_t Padding=512;Padding<=4096;Padding*=2)scanNetwork(Padding,1,B);
    scanNetwork(8000,7,B);scanNetwork(8000,65536,B);
    puts("[PASS] paced real IO, actual parser input,101 and preserved WS suffix, native physical joins");return 0;
}
