#include "../internal/xrt_websocket_stream.h"
#include "../internal/xrt_tcp.h"
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
#include "../internal/xrt_tls_stream.h"
#endif
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_WRITE_PROGRESS)
static bool __xrtWsProgressLive(xwsstream* S)
{
    return xrtWsStreamState(S)==XWS_STREAM_OPEN && !S->CloseEmitted &&
        !S->TransportClosing && xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE);
}
static bool __xrtWsProgressTcp(xwsstream* S,xnetstream* T)
{
    const xnetstreamevents* E=&xrtWsStreamTcpEventsOwnershipV1Get()->Events;
    return T && xrtAtomicPtrLoad(&T->Data,XMEMORY_RELAXED)==S &&
        T->Events.Read==E->Read && T->Events.Close==E->Close &&
        !xrtAtomic32Load(&T->AbortGate,XMEMORY_ACQUIRE) &&
        !xrtAtomic32Load(&T->CloseGate,XMEMORY_ACQUIRE) &&
        xrtNetStreamState(T)==XNET_STREAM_OPEN;
}
static void __xrtWsProgressWritten(xnetstream* T,size_t N,ptr Data)
{
    (void)T;xwsstream* S=Data;
    if((uint64)N>UINT64_MAX-S->WriteCompleted)S->WriteProgressOverflow=true;
    else S->WriteCompleted+=(uint64)N;
    if(S->WriteProgress){S->WriteProgressPending=true;(void)__xrtWsStreamDrivePrepare(S);}
}
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
static void __xrtWsProgressTlsWritten(xtlsstream* T,ptr Data)
{
    (void)T;xwsstream* S=Data;
    if(S->WriteProgress){S->WriteProgressPending=true;(void)__xrtWsStreamDrivePrepare(S);}
}
#endif
static bool __xrtWsProgressSet(xwsstream* S,xwsstreamwriteproc Proc)
{
    if(!__xrtWsProgressLive(S)){__xrtErrorSetInvalidState();return false;}
    ptr Native=xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE);
    if(S->TransportKind==__XRT_WS_TRANSPORT_TLS){
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
        if(!xrtTlsStreamSetWriteProgress(Native,Proc?__xrtWsProgressTlsWritten:NULL))return false;
#else
        __xrtWsStreamReject(XERR_UNSUPPORTED,XWS_STREAM_ERROR_STATE,"set-websocket-write-progress","WSS write progress selection is required",NULL);return false;
#endif
    }else{
        xnetstream* T=Native;
        if(!__xrtWsProgressTcp(S,T) || (S->WriteProgressEnabled &&
           (T->WriteProgress!=__xrtWsProgressWritten||T->WriteProgressData!=S)) ||
           (!S->WriteProgressEnabled&&T->WriteProgress)){__xrtErrorSetInvalidState();return false;}
        if(!S->WriteProgressEnabled){T->WriteProgressData=S;T->WriteProgress=__xrtWsProgressWritten;}
    }
    S->WriteProgressEnabled=true;S->WriteProgress=Proc;S->WriteProgressPending=false;return true;
}
XRT_API bool xrtWsStreamSetWriteProgress(xwsstream* S,xwsstreamwriteproc Proc)
{
    if(!__xrtWsStreamWorker(S,"set-websocket-write-progress"))return false;
    bool Owned=S->OwnershipPolicy!=NULL;
    if(Owned&&!__xrtWsStreamOwnershipEnter(S,true))return false;
    bool OK=__xrtWsProgressSet(S,Proc);
    if(Owned)__xrtWsStreamOwnershipLeave(S);
    return OK;
}
XRT_API xnetresult xrtWsStreamWriteProgress(xwsstream* S,xwswriteprogress* Out)
{
    if(!__xrtWsStreamWorker(S,"get-websocket-write-progress"))return XNET_RESULT_ERROR;
    size_t Bytes=0;ptr Native=xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE);
    if(!Out||!xrtMemRangeValid(Out,sizeof(*Out))||Out->Size!=sizeof(*Out)||
       !__xrtWsStreamStorageRange(S,&Bytes)||xrtMemRangesOverlap(Out,sizeof(*Out),S,Bytes)){
        __xrtErrorSetInvalidArgument();return XNET_RESULT_ERROR;}
    if(!__xrtWsProgressLive(S))return XNET_RESULT_CLOSED;
    if(!S->WriteProgressEnabled){__xrtErrorSetInvalidState();return XNET_RESULT_ERROR;}
    xwswriteprogress Value={.Size=sizeof(Value)};
    if(S->TransportKind==__XRT_WS_TRANSPORT_TLS){
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_WRITE_PROGRESS)
        xtlsstream* T=Native;xnetstream* TCP=xrtTlsStreamTransport(T);
        if(xrtMemRangesOverlap(Out,sizeof(*Out),T,sizeof(*T))||xrtMemRangesOverlap(Out,sizeof(*Out),T->Session,sizeof(*T->Session))||
           xrtMemRangesOverlap(Out,sizeof(*Out),TCP,sizeof(*TCP))){__xrtErrorSetInvalidArgument();return XNET_RESULT_ERROR;}
        if(xrtTlsStreamState(T)!=XTLS_STREAM_OPEN||T->CloseEmitted||xrtAtomic32Load(&T->AbortGate,XMEMORY_ACQUIRE)||!TCP||xrtNetStreamState(TCP)!=XNET_STREAM_OPEN||xrtAtomic32Load(&TCP->AbortGate,XMEMORY_ACQUIRE)||xrtAtomic32Load(&TCP->CloseGate,XMEMORY_ACQUIRE))return XNET_RESULT_CLOSED;
        Value.Staged=S->OutputHead!=NULL;
        xtlswriteprogress W={sizeof(W),0,0};if(!xrtTlsStreamWriteProgress(T,&W))return XNET_RESULT_ERROR;
        Value.Produced=W.Produced;Value.Completed=W.Completed;
#else
        __xrtWsStreamReject(XERR_UNSUPPORTED,XWS_STREAM_ERROR_STATE,"get-websocket-write-progress","WSS write progress selection is required",NULL);return XNET_RESULT_ERROR;
#endif
    }else{
        xnetstream* T=Native;
        if(xrtMemRangesOverlap(Out,sizeof(*Out),T,sizeof(*T))){__xrtErrorSetInvalidArgument();return XNET_RESULT_ERROR;}
        if(!__xrtWsProgressTcp(S,T))return XNET_RESULT_CLOSED;
        if(T->WriteProgress!=__xrtWsProgressWritten||T->WriteProgressData!=S){__xrtErrorSetInvalidState();return XNET_RESULT_ERROR;}
        uint64 Pending=(uint64)xrtNetStreamPending(T);
        if(S->WriteProgressOverflow||Pending>UINT64_MAX-S->WriteCompleted){
            __xrtWsStreamReject(XERR_RANGE,XWS_STREAM_ERROR_LIMIT,"get-websocket-write-progress","WebSocket write cursor overflow",NULL);return XNET_RESULT_ERROR;}
        Value.Produced=S->WriteCompleted+Pending;Value.Completed=S->WriteCompleted;
    }
    *Out=Value;return XNET_RESULT_OK;
}
void __xrtWsStreamPublishWriteProgress(xwsstream* S)
{
    if(!S->WriteProgressPending||!S->WriteProgress||!__xrtWsProgressLive(S))return;
    S->WriteProgressPending=false;S->WriteProgress(S,S->Data);
}
#endif
