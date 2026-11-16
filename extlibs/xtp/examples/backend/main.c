#include <xtp.h>
#include <stdio.h>
typedef struct backend_state {size_t Messages,Drops;} backend_state;
static xtpflow backend_frame(xtpreader* reader,const xtpframeview* frame,void* user)
{
    backend_state* state=user;xbytesview value;(void)reader;
    if(xtpParamAt(&frame->Params,0,&value)!=XTP_PARAM_FOUND||value.Size!=1||value.Data[0]!=7)return XTP_FLOW_STOP;
    ++state->Messages;return XTP_FLOW_ACCEPT;
}
static void backend_drop(xtpreader* reader,xtpresult reason,void* user)
{(void)reader;(void)reason;++((backend_state*)user)->Drops;}
int main(void)
{
    xtpfield fields[]={{XRT_STR_INIT("z"),XRT_BYTES_INIT("\7")},{XRT_STR_INIT("a"),XRT_BYTES_INIT("x")}};
    xtppacket packets[3];for(size_t i=0;i<3;++i){packets[i]=(xtppacket){0};packets[i].Type=XTP_NOTIFY;
        packets[i].Command=xtpCmdID(1001);packets[i].Params=xtpTable(fields,2);}
    unsigned char wire[256];size_t n;
    if(xtpEncode(packets,3,NULL,wire,sizeof(wire),&n)!=XTP_OK)return 1;
    wire[n/3+17]|=0x80; /* Intentionally invalid inner flags; framing stays valid. */
    backend_state state={0};xtpreaderevents events={backend_frame,backend_drop};xtpreader* reader=NULL;
    if(xtpReaderCreate(NULL,&events,&state,&reader)!=XTP_OK)return 2;
    size_t offset=0;uint64_t now=0;
    while(offset<n){size_t take=n-offset;if(take>7)take=7;xtpreadprogress p;
        xtpresult r=xtpReaderInput(reader,(xbytesview){wire+offset,take},false,now++,&p);
        offset+=p.Used;
        if(r<0||r==XTP_CLOSED||r==XTP_PAUSED){(void)xtpReaderDestroy(reader);return 3;}}
    uint64_t due;xtpresult r=xtpReaderDrive(reader,now++,&due);
    if(r<0||xtpReaderEnd(reader,now)!=XTP_END||xtpReaderDestroy(reader)!=XTP_OK)return 4;
    printf("XTP backend: messages=%zu dropped=%zu (7-byte fragments)\n",state.Messages,state.Drops);
    return state.Messages!=2||state.Drops!=1;
}
