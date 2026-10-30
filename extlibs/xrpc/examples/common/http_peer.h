#ifndef XRPC_EXAMPLE_HTTP_PEER_H
#define XRPC_EXAMPLE_HTTP_PEER_H
/* Deterministic local TCP fixture; not an independent interoperability peer or
 * production benchmark. Test-only command behavior is intentionally explicit. */
#include <xrt.h>
#include <xrpc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xrpc/http_client.h>
#include <xrt/thread.h>
#include <xrt/sync.h>
static inline void rpcHttpPeerRequire(bool OK,cstr Message)
{if(!OK){fprintf(stderr,"[FAIL] local HTTP peer: %s\n",Message);exit(1);}}
typedef struct rpcHttpPeer {
    xnetengine* Engine;xnetlistener* Listener;xatomicptr Stream;xnetaddr Address;
    xatomic32 Commands,Closed,ListenerClosed;
    unsigned char Input[65536];size_t Used;
} rpcHttpPeer;
static inline void rpcHttpWait(xatomic32* Value,uint32 Minimum)
{double End=xrtTimer()+10;while(xrtAtomic32Load(Value,XMEMORY_ACQUIRE)<Minimum){rpcHttpPeerRequire(xrtTimer()<End,"peer wait expired");xrtSleep(1);}}
static inline xnetstream* rpcHttpPeerStream(rpcHttpPeer* P)
{return (xnetstream*)xrtAtomicPtrLoad(&P->Stream,XMEMORY_ACQUIRE);}
static inline void rpcHttpPeerSend(rpcHttpPeer* P,cstr Text,size_t Size)
{
    xnetstream* S=rpcHttpPeerStream(P);xnetresult R=xrtNetStreamSend(S,Text,Size);
    /* Limit/protocol tests intentionally make the client abort while this
     * fixture is still sending. Healthy paths still assert their full reply. */
    rpcHttpPeerRequire(R==XNET_RESULT_OK || R==XNET_RESULT_CLOSED ||
        (R==XNET_RESULT_ERROR && xrtNetStreamState(S)>=XNET_STREAM_CLOSING),"peer send");
}
static inline void rpcHttpPeerReply(rpcHttpPeer* P,xstrview Body)
{
    xrpcmessage M;xrpcdocument D={0};xrpcdecodeconfig K;xrpcDecodeConfigInit(&K);K.Mode=XRPC_DECODE_REQUESTS;
    rpcHttpPeerRequire(xrpcDecode(Body,&K,&M,1,&D)==XRPC_DECODE_DONE && M.Kind==XRPC_MESSAGE_CALL,"peer request envelope");
    char Reply[65536],Head[256];int N;
    if(M.Method.Raw.Size==6 && !memcmp(M.Method.Raw.Data,"\"hold\"",6)){xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);return;}
    if(M.Method.Raw.Size==6 && !memcmp(M.Method.Raw.Data,"\"drop\"",6)){xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);xrtNetStreamAbort(rpcHttpPeerStream(P));return;}
    if(M.Method.Raw.Size==8 && !memcmp(M.Method.Raw.Data,"\"broken\"",8)){
        cstr H="HTTP/1.1 200 OK\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n";
        rpcHttpPeerSend(P,H,strlen(H));xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);return;
    }
    if(M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"large\"",7)){
        cstr H="HTTP/1.1 200 OK\r\nContent-Length: 2000000\r\n\r\n";rpcHttpPeerSend(P,H,strlen(H));xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);return;
    }
    if(M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"trunc\"",7)){
        cstr H="HTTP/1.1 200 OK\r\nContent-Length: 100\r\n\r\n{";rpcHttpPeerSend(P,H,strlen(H));rpcHttpPeerRequire(xrtNetStreamShutdownWrite(rpcHttpPeerStream(P)),"peer truncated EOF");return;
    }
    if(M.Method.Raw.Size==9 && !memcmp(M.Method.Raw.Data,"\"bighead\"",9)){
        char H[600];memset(H,'a',sizeof(H));memcpy(H,"HTTP/1.1 200 OK\r\nX-Large: ",26);memcpy(H+sizeof(H)-4,"\r\n\r\n",4);
        rpcHttpPeerSend(P,H,sizeof(H));return;
    }
    bool Bad=M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"badid\"",7);
    bool Error=M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"error\"",7);
    bool Chunk=M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"chunk\"",7);
    bool Close=M.Method.Raw.Size==7 && !memcmp(M.Method.Raw.Data,"\"close\"",7);
    bool Encoded=M.Method.Raw.Size==10 && !memcmp(M.Method.Raw.Data,"\"encoding\"",10);
    if(Error)N=snprintf(Reply,sizeof(Reply),"{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"error\":{\"code\":-1,\"message\":\"failure\"}}",(int)M.Id.Raw.Size,M.Id.Raw.Data);
    else N=snprintf(Reply,sizeof(Reply),"{\"jsonrpc\":\"2.0\",\"id\":%.*s,\"result\":%.*s}",(int)(Bad?5:M.Id.Raw.Size),Bad?"\"bad\"":M.Id.Raw.Data,
        (int)(M.Params.Raw.Size?M.Params.Raw.Size:4),M.Params.Raw.Size?M.Params.Raw.Data:"null");
    rpcHttpPeerRequire(N>0 && (size_t)N<sizeof(Reply),"peer reply size");
    if(Chunk){
        rpcHttpPeerSend(P,"HTTP/1.1 100 Continue\r\n\r\n",25);
        cstr H="HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n";rpcHttpPeerSend(P,H,strlen(H));
        for(int I=0;I<N;++I){rpcHttpPeerSend(P,"1\r\n",3);rpcHttpPeerSend(P,Reply+I,1);rpcHttpPeerSend(P,"\r\n",2);}
        rpcHttpPeerSend(P,"0\r\nX-Note: tail\r\n\r\n",19);
    }else if(Close){
        cstr H="HTTP/1.0 200 OK\r\n\r\n";rpcHttpPeerSend(P,H,strlen(H));rpcHttpPeerSend(P,Reply,(size_t)N);rpcHttpPeerRequire(xrtNetStreamShutdownWrite(rpcHttpPeerStream(P)),"peer shutdown");
    }else{
        int H=snprintf(Head,sizeof(Head),"HTTP/1.1 %s\r\nContent-Length: %d\r\n%s\r\n",Error?"500 Failure":"200 OK",N,
            Encoded?"Content-Encoding: identity\r\nContent-Encoding: gzip\r\n":"");
        rpcHttpPeerSend(P,Head,(size_t)H);rpcHttpPeerSend(P,Reply,(size_t)N);
    }
    xrtAtomic32FetchAdd(&P->Commands,1,XMEMORY_RELEASE);
}
static inline void rpcHttpPeerRead(xnetstream* S,xnetbuf* Buffer,ptr Data)
{
    (void)S;rpcHttpPeer* P=Data;size_t N=xrtNetBufSize(Buffer);rpcHttpPeerRequire(N<=sizeof(P->Input)-P->Used,"peer input limit");
    P->Used+=xrtNetBufRead(Buffer,P->Input+P->Used,N);
    for(;;){xhttpfield Fields[32];xhttp1head M;xrtHttp1HeadInit(&M,Fields,32);
        xhttp1status Status=xrtHttp1RequestParse((xbytesview){P->Input,P->Used},&M,NULL,NULL);
        rpcHttpPeerRequire(Status==XHTTP1_MORE || Status==XHTTP1_READY,"peer HTTP framing");if(Status==XHTTP1_MORE)break;
        xhttp1bodyplan Plan;rpcHttpPeerRequire(xrtHttp1RequestBodyPlan(&M,&Plan) && Plan.Mode==XHTTP1_BODY_FIXED && Plan.Length<=sizeof(P->Input)-M.Bytes,"peer fixed request body");
        if(Plan.Length>P->Used-M.Bytes)break;
        rpcHttpPeerRequire(M.MethodCode==XHTTP_METHOD_POST && M.Target.Size==4 && !memcmp(M.Target.Data,"/rpc",4),"peer method/target");
        const xhttpfield* Encoding=xrtHttp1Field(&M,XRT_STR_LITERAL("Accept-Encoding"));
        rpcHttpPeerRequire(Encoding && xrtHttpTokenEqual(Encoding->Value,XRT_STR_LITERAL("identity")),"peer explicit supported response encoding");
        rpcHttpPeerReply(P,(xstrview){(cstr)P->Input+M.Bytes,(size_t)Plan.Length});
        size_t Used=M.Bytes+(size_t)Plan.Length;memmove(P->Input,P->Input+Used,P->Used-Used);P->Used-=Used;if(!P->Used)break;
    }
}
static inline void rpcHttpPeerEnd(xnetstream* S,ptr Data){(void)Data;(void)xrtNetStreamAbort(S);}
static inline void rpcHttpPeerClosed(xnetstream* S,xnetresult Result,const xerror* Error,ptr Data)
{(void)S;(void)Result;(void)Error;xrtAtomic32Store(&((rpcHttpPeer*)Data)->Closed,1,XMEMORY_RELEASE);}
static inline bool rpcHttpPeerAccept(xnetlistener* L,xnetstream* S,ptr Data)
{(void)L;rpcHttpPeer* P=Data;rpcHttpPeerRequire(!rpcHttpPeerStream(P),"peer unexpected second connection/replay");
 static const xnetstreamevents Events={NULL,rpcHttpPeerRead,rpcHttpPeerEnd,NULL,NULL,NULL,rpcHttpPeerClosed};
 rpcHttpPeerRequire(xrtNetStreamSetEvents(S,&Events,P),"peer accepted event context");xrtAtomicPtrStore(&P->Stream,S,XMEMORY_RELEASE);return true;}
static inline void rpcHttpPeerListenerClosed(xnetlistener* L,ptr Data)
{(void)L;xrtAtomic32Store(&((rpcHttpPeer*)Data)->ListenerClosed,1,XMEMORY_RELEASE);}
static inline void rpcHttpPeerStart(rpcHttpPeer* P)
{
    memset(P,0,sizeof(*P));xrtAtomicPtrInit(&P->Stream,NULL);xrtAtomic32Init(&P->Commands,0);xrtAtomic32Init(&P->Closed,0);xrtAtomic32Init(&P->ListenerClosed,0);
    xnetengineconfig E;xrtNetEngineConfigInit(&E);E.Workers=2;P->Engine=xrtNetEngineCreate(&E);rpcHttpPeerRequire(P->Engine && xrtNetEngineStart(P->Engine),"peer engine");
    xnetlistenconfig K;xrtNetListenConfigInit(&K);xrtNetAddrParse(&K.Address,"127.0.0.1",0);K.Affinity=0;K.Distribution=XNET_ACCEPT_LOCAL;
    static const xnetlistenerevents L={rpcHttpPeerAccept,NULL,rpcHttpPeerListenerClosed};
    P->Listener=xrtNetListen(P->Engine,&K,&L,NULL,P);rpcHttpPeerRequire(P->Listener && xrtNetListenerLocal(P->Listener,&P->Address),"peer listen");
}
static inline void rpcHttpPeerStop(rpcHttpPeer* P)
{
    rpcHttpPeerRequire(xrtNetListenerClose(P->Listener),"peer listener close");rpcHttpWait(&P->ListenerClosed,1);
    xnetstream* S=rpcHttpPeerStream(P);if(S)rpcHttpWait(&P->Closed,1);
    xrtNetStreamDestroy(S);xrtNetListenerDestroy(P->Listener);
    rpcHttpPeerRequire(xrtNetEngineStop(P->Engine) && xrtNetEngineDestroy(P->Engine),"engine physical duties leaked");
}
static inline xrpchttpclient* rpcHttpConnect(rpcHttpPeer* P,xrpchttpconfig* K)
{xrpchttpclient* C=NULL;rpcHttpPeerRequire(xrpcHttpConnect(P->Engine,&P->Address,1,XRT_STR_LITERAL("localhost"),XRT_STR_LITERAL("/rpc"),K,&C),"RPC HTTP connect");return C;}
static inline xfuture* rpcHttpSubmit(xrpchttpclient* C,cstr Method,cstr Params,int64 Timeout)
{xfuture* F=NULL;xrpcadmission A=xrpcHttpCallAsync(C,(xstrview){Method,strlen(Method)},(xstrview){Params,Params?strlen(Params):0},Timeout,&F);
 if(A!=XRPC_ADMISSION_ACCEPTED)fprintf(stderr,"admission=%d error=%d %s\n",(int)A,xrtErrorCode(xrtGetError()),xrtErrorMessage(xrtGetError()));
 rpcHttpPeerRequire(A==XRPC_ADMISSION_ACCEPTED,"RPC HTTP admission");return F;}
static inline const xrpccallresult* rpcHttpResult(xfuture* F)
{xfutureresult R;rpcHttpPeerRequire(xrtFutureWaitFor(F,10000)==XWAIT_OK && xrtFutureResult(F,&R) && R.State==XFUTURE_RESOLVED && R.Value,"RPC HTTP typed result wait");return R.Value;}
static inline void rpcHttpClose(rpcHttpPeer* P,xrpchttpclient* C)
{
    xfuture* F=xrpcHttpClosed(C);rpcHttpPeerRequire(F && xrpcHttpClose(C) && xrtFutureWaitFor(F,10000)==XWAIT_OK,"RPC HTTP physical close wait");
    xrpchttpinfo I={0};I.Size=sizeof(I);rpcHttpPeerRequire(xrpcHttpInfo(C,&I) && I.State==XRPC_HTTP_CLOSED && !I.Requests && !I.RequestBytes && I.Accepted==I.Completed,"RPC HTTP retired accounting");
    rpcHttpPeerRequire(xrpcHttpClose(C),"idempotent HTTP close");xrpcHttpRelease(C);xrtFutureDestroy(F);rpcHttpPeerStop(P);
}
#endif
