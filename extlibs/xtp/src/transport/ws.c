#include <xtp/ws.h>
#include "../internal/xtp_codec.h"
#if defined(XTP_FEATURE_XTP_WS)
XTP_API xtpresult xtpWsBegin(xtpwsinput* in,xwsopcode opcode)
{if(!in||!in->Session||in->Active||!xtp__RecordLane(in->Session,in->Binding,in->Lane))return XTP_E_STATE;
    if(opcode!=XWS_OPCODE_BINARY)return XTP_E_TYPE;
    in->Active=true;return XTP_OK;}
XTP_API xtpresult xtpWsData(xtpwsinput* in,xbytesview bytes,uint64_t now,xtpreadprogress* p)
{if(!in||!in->Active)return XTP_E_STATE;return xtpInput(in->Session,in->Binding,in->Lane,bytes,false,now,p);}
XTP_API xtpresult xtpWsEnd(xtpwsinput* in,uint64_t now,xtpreadprogress* p)
{
    if(!in||!in->Active)return XTP_E_STATE;
    xtpresult r=xtpInput(in->Session,in->Binding,in->Lane,(xbytesview){NULL,0},true,now,p);
    if(r!=XTP_AGAIN&&r!=XTP_PAUSED&&r>=0)in->Active=false;
    return r;
}
XTP_API xtpresult xtpWsWrite(void* connection,const xtpoutput* out,size_t* n)
{
    if(!connection||!out||!n||out->Mode!=XTP_INPUT_RECORD||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;xstrview protocol=xrtWsConnProtocol(connection);
    if(protocol.Size!=6||memcmp(protocol.Data,"xtp.v3",6))return XTP_E_STATE;
    xnetresult r=xrtWsConnBinary(connection,out->Data);
    if(r==XNET_RESULT_OK){*n=out->Data.Size;return XTP_OK;}
    return r==XNET_RESULT_AGAIN?XTP_AGAIN:r==XNET_RESULT_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
