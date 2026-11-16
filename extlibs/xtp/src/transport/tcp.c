#include <xtp/tcp.h>
#if defined(XTP_FEATURE_XTP_TCP)
XTP_API xtpresult xtpTcpWrite(void* stream,const xtpoutput* out,size_t* n)
{
    if(!stream||!out||!n||out->Mode!=XTP_INPUT_STREAM||!out->Reliable)return XTP_E_ARGUMENT;
    *n=0;size_t limit=xrtNetStreamWriteLimit(stream);
    size_t take=out->Data.Size<limit?out->Data.Size:limit;if(!take)return XTP_E_LIMIT;
    xnetresult r=xrtNetStreamSend(stream,out->Data.Data,take);
    if(r==XNET_RESULT_OK){*n=take;return XTP_OK;}
    return r==XNET_RESULT_AGAIN?XTP_AGAIN:r==XNET_RESULT_CLOSED?XTP_CLOSED:XTP_E_STATE;
}
#endif
