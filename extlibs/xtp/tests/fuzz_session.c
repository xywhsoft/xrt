#include <xtp.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct fuzz_session {
    uint64_t Now,Completed[256];size_t Count,Next;unsigned char Decision;
    xtprequesttoken Held[8];
} fuzz_session;
static xtpdecision fuzz_message(xtpsession* s,const xtpmessageview* m,void* user)
{
    fuzz_session* f=user;
    if(m->Type==XTP_REQUEST){f->Held[f->Next++%8]=m->Request;
        if(f->Decision&8){xtpresponse r={0};r.Body=m->Body;
            xtpresult result=xtpReply(s,m->Request,&r,f->Now);if(result==XTP_OK)return xtpAccept();}}
    switch(f->Decision&3){case 1:return xtpBusy();case 2:return xtpReject(11);case 3:return xtpPause();default:return xtpAccept();}
}
static void fuzz_complete(xtpsession* s,const xtpcompletion* c,void* user)
{
    fuzz_session* f=user;(void)s;
    for(size_t i=0;i<f->Count;++i)assert(f->Completed[i]!=c->Operation._Opaque[1]);
    assert(f->Count<256);f->Completed[f->Count++]=c->Operation._Opaque[1];
    if(c->Reason==XTP_FINISH_RESPONSE){xtpframeview view;
        assert(xtpDecode(c->Packet,NULL,&view)==XTP_OK&&view.MessageID==c->MessageID);}
}
int LLVMFuzzerTestOneInput(const unsigned char* data,size_t size)
{
    if(!size)return 0;
    fuzz_session f={0};f.Decision=*data++;--size;xtpconfig c;xtpConfigInit(&c);
    c.Reader.Codec.MaxFrameBytes=512;c.Reader.Codec.MaxMetadataBytes=128;c.Reader.Codec.MaxCommandBytes=32;
    c.Reader.Codec.MaxParams=8;c.Reader.Codec.MaxKeyBytes=16;c.Reader.Codec.MaxBatch=4;
    c.Reader.MaxWorkBytes=512;c.Reader.MaxFrames=4;c.Reader.MaxStorageBytes=4096;
    c.Reader.ReceiveTimeoutUs=20;c.MaxPending=8;c.MaxIncomingRequests=4;c.MaxLanes=2;
    c.MaxTxFrames=16;c.MaxTxBytes=2048;c.ResponseReserveBytes=512;c.ResponseReserveFrames=2;
    c.MaxResponseBytes=128;c.MaxSessionBytes=32768;c.DedupWindow=16;
    c.DefaultOperationTimeoutUs=50;c.IncomingRequestTimeoutUs=30;
    xtpevents e={fuzz_message,fuzz_complete,NULL};xtpsession* s=NULL;
    if(xtpCreate(&c,&e,&f,&s)!=XTP_OK)return 0;
    xtplane lanes[]={{1,XTP_INPUT_STREAM,true,0},{2,XTP_INPUT_RECORD,false,512}};
    xtpbinding b={lanes,2,true,false};uint64_t binding;
    assert(xtpBind(s,&b,0,&binding)==XTP_OK);
    for(size_t turn=0;size&&turn<128;++turn){unsigned char op=*data++;--size;uint64_t due=0;xtpinfo in;
        assert(xtpInfo(s,&in)==XTP_OK&&in.StorageBytes<=c.MaxSessionBytes&&in.PeakReservedBytes<=c.MaxSessionBytes);
        if(in.Closed)break;
        if(!in.Bound&&!in.Closing){xtpresult r=xtpDrive(s,f.Now,&due);
            if(r!=XTP_AGAIN)assert(xtpBind(s,&b,f.Now,&binding)==XTP_OK);continue;}
        switch(op&7){
        case 0:{size_t n=size;if(n>(op>>3)+1u)n=(op>>3)+1u;xtpreadprogress p;
            (void)xtpInput(s,binding,(op&8)?2:1,(xbytesview){data,n},(op&24)==24,f.Now,&p);data+=n;size-=n;break;}
        case 1:{xtpmessage m={0};m.Type=(op&8)?XTP_REQUEST:(op&16)?XTP_PUSH:XTP_NOTIFY;
            m.Command=xtpCmdID(op);size_t n=size<32?size:32;m.Body=(xbytesview){data,n};
            xtpsubmitoptions o={0};o.LaneID=(op&32)?2:1;(void)xtpSubmit(s,&m,&o,f.Now,NULL);break;}
        case 2:{xtpoutput out;if(xtpOutput(s,0,&out)==XTP_OK){size_t accepted=(op&8)?out.Data.Size:out.Data.Size/2;
            (void)xtpAdvance(s,out.Ticket,accepted,f.Now);}break;}
        case 3:f.Now+=op>>3;(void)xtpDrive(s,f.Now,&due);break;
        case 4:{xtpresponse r={0};r.Status=(op&8)?0:123;(void)xtpReply(s,f.Held[(op>>4)%8],&r,f.Now);break;}
        case 5:(void)xtpUnbind(s,binding,f.Now);break;
        case 6:(void)xtpResume(s,binding,1,f.Now);(void)xtpEnd(s,binding,1,f.Now);break;
        default:if(op&8)(void)xtpClose(s,f.Now);else f.Decision=op;break;}
    }
    xtpresult result=xtpClose(s,f.Now);
    for(size_t i=0;result==XTP_AGAIN&&i<32;++i){uint64_t due;result=xtpDrive(s,f.Now,&due);}
    assert(result==XTP_CLOSED&&xtpDestroy(s)==XTP_OK);return 0;
}
