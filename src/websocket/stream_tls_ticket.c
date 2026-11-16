#include "../internal/xrt_websocket_stream.h"
#if defined(XRT_FEATURE_WEBSOCKET_STREAM_TLS_TICKET)
#include "../internal/xrt_tls_stream.h"
static bool __xrtWsTicketLive(xwsstream* S)
{
    return xrtWsStreamState(S)==XWS_STREAM_OPEN && !S->CloseEmitted &&
        !S->TransportClosing && S->TransportKind==__XRT_WS_TRANSPORT_TLS;
}
static bool __xrtWsTicketSet(xwsstream* S,xwsstreamtlsticketproc Proc)
{
    if(!__xrtWsTicketLive(S)){__xrtErrorSetInvalidState();return false;}
    xtlsstream* T=xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE);
    const xtlsstreamevents* E=&xrtWsStreamTlsEventsOwnershipV1Get()->Events;
    if(!T || xrtTlsStreamState(T)!=XTLS_STREAM_OPEN ||
       xrtTlsSessionRole(xrtTlsStreamSession(T))!=XTLS_CLIENT ||
       (T->EventPolicy ? (T->EventPolicy!=xrtWsStreamTlsEventsOwnershipV1Get() ||
        xrtAtomicPtrLoad(&T->EventData,XMEMORY_ACQUIRE)!=S) : xrtTlsStreamData(T)!=S) ||
       T->CloseEmitted ||
       xrtAtomic32Load(&T->AbortGate,XMEMORY_ACQUIRE) ||
       T->Events.Open!=E->Open || T->Events.Read!=E->Read ||
       T->Events.End!=E->End || T->Events.Writable!=E->Writable ||
       T->Events.Drain!=E->Drain || T->Events.Close!=E->Close ||
       T->Events.Ticket!=E->Ticket){__xrtErrorSetInvalidState();return false;}
    S->TlsTicket=Proc;return true;
}
XRT_API bool xrtWsStreamSetTlsTicket(xwsstream* S,xwsstreamtlsticketproc Proc)
{
    if(!__xrtWsStreamWorker(S,"set-websocket-tls-ticket"))return false;
    bool Owned=S->OwnershipPolicy!=NULL;
    if(Owned&&!__xrtWsStreamOwnershipEnter(S,true))return false;
    bool OK=__xrtWsTicketSet(S,Proc);
    if(Owned)__xrtWsStreamOwnershipLeave(S);
    return OK;
}
void __xrtWsStreamTlsTicket(xtlsstream* T,ptr Data)
{
    xwsstream* S=Data;
    if(__xrtWsTicketLive(S) && S->TlsTicket &&
       xrtAtomicPtrLoad(&S->Transport,XMEMORY_ACQUIRE)==T)
        S->TlsTicket(S,S->Data);
}
#endif
