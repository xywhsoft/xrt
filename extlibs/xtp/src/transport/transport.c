#include <xtp/transport.h>
#include "../internal/xtp_codec.h"
#if defined(XTP_FEATURE_XTP_TRANSPORT)
XTP_API xtpresult xtpDrain(xtpsession* s,uint32_t lane,xtpwriteproc write,void* user,
    const xtpwritebudget* input,uint64_t now,xtpwriteprogress* progress)
{
    if(!s||!write||!progress||!xtp__SessionAddress(s,(uintptr_t)progress,sizeof(*progress))||
        (input&&(!xrtMemRangeValid(input,sizeof(*input))||xtp__Overlap(input,sizeof(*input),progress,sizeof(*progress)))))return XTP_E_ARGUMENT;
    xtpwritebudget b={262144,64,16384};if(input)b=*input;
    if(!b.Bytes||!b.Frames||!b.ChunkBytes||now<xtp__SessionTime(s))return XTP_E_ARGUMENT;
    xtpresult observed=xtp__SessionObserve(s,now);if(observed!=XTP_OK)return observed;
    xtpwriteprogress p={0};xtpresult result=XTP_AGAIN;
    while(p.Frames<b.Frames&&p.Bytes<b.Bytes){xtpoutput out;result=xtpOutput(s,lane,&out);
        if(result==XTP_AGAIN){result=XTP_OK;break;}if(result!=XTP_OK)break;
        size_t remaining=b.Bytes-p.Bytes,whole=out.Data.Size;
        if(out.Mode==XTP_INPUT_RECORD){if(whole>remaining){result=p.Calls?XTP_AGAIN:XTP_E_LIMIT;break;}}
        else {if(out.Data.Size>remaining)out.Data.Size=remaining;if(out.Data.Size>b.ChunkBytes)out.Data.Size=b.ChunkBytes;}
        size_t n=0;xtpresult sent=xtp__TransportWrite(s,&out,write,user,&n);++p.Calls;
        if(n>out.Data.Size||(out.Mode==XTP_INPUT_RECORD&&n&&n!=whole)){result=XTP_E_STATE;break;}
        if(n){uint64_t latest=xtp__SessionTime(s);result=xtpAdvance(s,out.Ticket,n,now<latest?latest:now);p.Bytes+=n;if(n==whole)++p.Frames;
            if(result!=XTP_OK)break;}
        if(!n&&!out.Reliable&&out.Mode==XTP_INPUT_RECORD&&(sent==XTP_E_LIMIT||sent==XTP_E_DEADLINE)){
            uint64_t latest=xtp__SessionTime(s);result=xtp__DropNotify(s,&out.Ticket,sent,now<latest?latest:now);
            if(result==XTP_OK){++p.Frames;++p.Dropped;result=XTP_DROPPED;}break;}
        if(sent!=XTP_OK){result=sent;break;}if(!n){result=XTP_AGAIN;break;}
        result=XTP_AGAIN;
    }
    *progress=p;return result;
}
#endif
