#include <xtp.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
typedef struct delivery_fuzz {
    xtpsession* Session;uint64_t Binding,Now;
    uint64_t Accepted[256],Completed[256];size_t AcceptedCount,CompletedCount;
    unsigned Decision;xtpdeliveryproof Proof;xtpbinding BindingConfig;xtprequesttoken Held[8];size_t Next;
} delivery_fuzz;
static xtpdecision delivery_message(xtpsession* s,const xtpmessageview* m,void* user)
{
    delivery_fuzz* f=user;
    if(f->Decision&1)return xtpBusy();
    if(f->Decision&2)return xtpPause();
    if(m->Type!=XTP_NOTIFY){for(size_t i=0;i<f->AcceptedCount;++i)assert(f->Accepted[i]!=m->MessageID);
        assert(f->AcceptedCount<256);f->Accepted[f->AcceptedCount++]=m->MessageID;}
    if(m->Type==XTP_REQUEST){f->Held[f->Next++%8]=m->Request;
        if(f->Decision&4){xtpresponse r={0};r.Body=m->Body;(void)xtpReply(s,m->Request,&r,f->Now);}}
    return xtpAccept();
}
static void delivery_completed(xtpsession* s,const xtpcompletion* c,void* user)
{
    delivery_fuzz* f=user;(void)s;
    for(size_t i=0;i<f->CompletedCount;++i)assert(f->Completed[i]!=c->Operation._Opaque[1]);
    assert(f->CompletedCount<256);f->Completed[f->CompletedCount++]=c->Operation._Opaque[1];
}
int LLVMFuzzerTestOneInput(const unsigned char* data,size_t size)
{
    if(!size)return 0;
    xtpconfig c;xtpConfigInit(&c);xtpDeliveryConfigInit(&c.Delivery);c.Delivery.AllowRequests=true;
    c.Reader.Codec.MaxFrameBytes=512;c.Reader.Codec.MaxMetadataBytes=128;c.Reader.Codec.MaxCommandBytes=32;
    c.Reader.Codec.MaxParams=8;c.Reader.Codec.MaxKeyBytes=16;c.Reader.Codec.MaxBatch=4;
    c.Reader.MaxWorkBytes=512;c.Reader.MaxFrames=4;c.Reader.MaxStorageBytes=4096;c.Reader.ReceiveTimeoutUs=20;
    c.MaxPending=8;c.MaxIncomingRequests=4;c.MaxLanes=1;c.MaxTxFrames=16;c.MaxTxBytes=4096;
    c.ResponseReserveBytes=512;c.ResponseReserveFrames=2;c.MaxResponseBytes=128;c.MaxSessionBytes=65536;c.DedupWindow=16;
    c.DefaultOperationTimeoutUs=50;c.IncomingRequestTimeoutUs=30;c.Delivery.MaxReplayMessages=12;c.Delivery.MaxLifetimeUs=100;
    c.Delivery.RetryMinUs=2;c.Delivery.RetryMaxUs=16;
    delivery_fuzz f[2];memset(f,0,sizeof(f));xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpevents e={delivery_message,delivery_completed,NULL};
    for(size_t k=0;k<2;++k){delivery_fuzz* a=&f[k];a->Decision=4;
        a->Proof.Identity.SessionID[0]=1;a->Proof.Identity.Domain[0]=2;a->Proof.Identity.Local[0]=(unsigned char)(3+k);
        a->Proof.Identity.Peer[0]=(unsigned char)(4-k);a->Proof.UntilUs=10000;a->Proof.LifetimeUs=100;
        a->Proof.PeerReceiveFloor=a->Proof.PeerResultFloor=1;a->Proof.StateRetained=a->Proof.AllowRequests=true;
        a->BindingConfig=(xtpbinding){&lane,1,true,false,&a->Proof};assert(xtpCreate(&c,&e,a,&a->Session)==XTP_OK);
        assert(xtpBind(a->Session,&a->BindingConfig,0,&a->Binding)==XTP_OK);}
    uint64_t now=0;
    for(size_t turn=0;size&&turn<128;++turn){unsigned char op=*data++;--size;size_t k=(op>>3)&1;
        delivery_fuzz* a=&f[k];delivery_fuzz* b=&f[1-k];a->Now=b->Now=now;xtpinfo ai,bi;
        assert(xtpInfo(a->Session,&ai)==XTP_OK&&xtpInfo(b->Session,&bi)==XTP_OK);
        assert(ai.StorageBytes<=c.MaxSessionBytes&&ai.PeakReservedBytes<=c.MaxSessionBytes&&ai.Retained<=c.Delivery.MaxReplayMessages&&ai.Receipts<=c.DedupWindow);
        uint64_t due=0;
        switch(op&7){
        case 0:case 1:{xtpmessage m={0};m.Type=(op&16)?XTP_REQUEST:(op&32)?XTP_PUSH:XTP_NOTIFY;
            m.Command=xtpCmdID(op);size_t n=size<32?size:32;m.Body=(xbytesview){data,n};xtpsubmitoptions opt={0};
            opt.Replay=m.Type!=XTP_NOTIFY&&(op&64);(void)xtpSubmit(a->Session,&m,&opt,now,NULL);break;}
        case 2:{xtpoutput out;if(xtpOutput(a->Session,0,&out)==XTP_OK){unsigned char bytes[512];size_t n=out.Data.Size;assert(n<=512);
            memcpy(bytes,out.Data.Data,n);(void)xtpAdvance(a->Session,out.Ticket,n,now);
            if(!(op&16)&&bi.Bound){xtpreadprogress p;(void)xtpInput(b->Session,b->Binding,1,(xbytesview){bytes,n},false,now,&p);}}break;}
        case 3:now+=op>>4;f[0].Now=f[1].Now=now;(void)xtpDrive(a->Session,now,&due);(void)xtpDrive(b->Session,now,&due);break;
        case 4:if(ai.Bound)(void)xtpUnbind(a->Session,a->Binding,now);else if(ai.Suspended){
            a->Proof.Resume=a->Proof.OldBindingDetached=true;a->Proof.PeerReceiveFloor=bi.ReceiveFloor;a->Proof.PeerResultFloor=bi.ResultFloor;
            (void)xtpBind(a->Session,&a->BindingConfig,now,&a->Binding);}break;
        case 5:{xtpresponse r={0};r.Status=(op&16)?17:0;(void)xtpReply(a->Session,a->Held[(op>>4)%8],&r,now);break;}
        case 6:a->Decision=op>>4;(void)xtpResume(a->Session,a->Binding,1,now);break;
        default:if(op&16)(void)xtpClose(a->Session,now);else (void)xtpDrive(a->Session,now,&due);break;}
    }
    for(size_t k=0;k<2;++k){xtpresult r=xtpClose(f[k].Session,now);
        for(size_t i=0;r==XTP_AGAIN&&i<32;++i){uint64_t due;r=xtpDrive(f[k].Session,now,&due);}
        assert(r==XTP_CLOSED&&xtpDestroy(f[k].Session)==XTP_OK);}
    return 0;
}
