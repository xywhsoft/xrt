#include <xtp/xrudp.h>
#if defined(XTP_FEATURE_XTP_XRUDP)
static xtpresult xtpXrudpResult(xrudpresult r)
{
    return r==XRUDP_OK?XTP_OK:r==XRUDP_BACKPRESSURE?XTP_AGAIN:r==XRUDP_LIMIT?XTP_E_LIMIT:
        r==XRUDP_NO_MEMORY?XTP_E_MEMORY:r==XRUDP_CLOSED||r==XRUDP_CANCELLED||r==XRUDP_DONE?XTP_CLOSED:XTP_E_STATE;
}
XTP_API xtpresult xtpXrudpWrite(void* user,const xtpoutput* out,size_t* accepted)
{
    xtpxrudpwriter* w=user;
    if(!w||!w->Session||!out||!accepted)return XTP_E_ARGUMENT;
    *accepted=0;
    if(out->Mode==XTP_INPUT_STREAM&&out->Reliable)
        return xtpXrudpResult(xrtRudpSessionStreamWrite(w->Session,w->StreamID?w->StreamID:1,out->Data.Data,out->Data.Size,accepted));
    if(out->Mode!=XTP_INPUT_RECORD||out->Reliable||!w->ChannelID||out->Data.Size<32||out->Data.Data[16]!=XTP_NOTIFY)return XTP_E_ARGUMENT;
    if(out->DeadlineUs<=w->NowUs)return XTP_E_DEADLINE;
    if(out->DeadlineUs-w->NowUs>XRUDP_DATAGRAM_LIFETIME_MAX_US)return XTP_E_LIMIT;
    xrudpsessionstate state;xrudpresult r=xrtRudpSessionState(w->Session,&state);
    if(r!=XRUDP_OK)return xtpXrudpResult(r);
    if(state.Reason!=XRUDP_OK)return XTP_CLOSED;
    if(out->Data.Size>state.DatagramSendLimit)return XTP_E_LIMIT;
    uint64_t sequence;r=xrtRudpSessionDatagramWrite(w->Session,w->ChannelID,out->Data.Data,out->Data.Size,w->NowUs,out->DeadlineUs,&sequence);
    if(r==XRUDP_OK)*accepted=out->Data.Size;
    return xtpXrudpResult(r);
}
#endif
