#include <xtp.h>
#include <stdio.h>
static xtpdecision receive(xtpsession* s,const xtpmessageview* m,void* user)
{
    uint64_t* now=user;
    printf("command=%u kind=%d\n",m->Command.As.ID,(int)m->Type);
    if(m->Type==XTP_REQUEST){xtpresponse r={0};r.Body=XRT_BYTES_LITERAL("hello");
        if(xtpReply(s,m->Request,&r,*now)!=XTP_OK)return xtpReject(XTP_STATUS_INTERNAL);}
    return xtpAccept();
}
static void complete(xtpsession* s,const xtpcompletion* c,void* user)
{(void)s;(void)user;printf("completed id=%llu reason=%d status=%d body=%.*s\n",
    (unsigned long long)c->MessageID,(int)c->Reason,c->Status,(int)c->Body.Size,c->Body.Data?(const char*)c->Body.Data:"");}
/* The transport must accept/copy this buffer before Advance; this in-memory
 * example does so explicitly. TCP/TLS short writes advance only their prefix. */
static int pump(xtpsession* from,xtpsession* to,uint64_t binding,uint64_t now)
{
    xtpoutput o;xtpresult r=xtpOutput(from,0,&o);if(r!=XTP_OK)return r==XTP_AGAIN?0:1;
    unsigned char wire[256];if(o.Data.Size>sizeof(wire))return 1;
    size_t n=o.Data.Size;memcpy(wire,o.Data.Data,n);
    if(xtpAdvance(from,o.Ticket,n,now)<0)return 1;
    xtpreadprogress p;r=xtpInput(to,binding,o.LaneID,(xbytesview){wire,n},false,now,&p);
    return r<0||p.Used!=n;
}
int main(void)
{
    uint64_t now=1,ab,bb;xtpsession *a=NULL,*b=NULL;xtpevents e={receive,complete,NULL};
    if(xtpCreate(NULL,&e,&now,&a)!=XTP_OK||xtpCreate(NULL,&e,&now,&b)!=XTP_OK)return 1;
    xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpbinding binding={&lane,1,false,true};
    if(xtpBind(a,&binding,now,&ab)!=XTP_OK||xtpBind(b,&binding,now,&bb)!=XTP_OK)return 2;
    xtpmessage m={0};m.Type=XTP_REQUEST;m.Command=xtpCmdID(1001);
    if(xtpSubmit(a,&m,NULL,now,NULL)!=XTP_OK||pump(a,b,bb,now)||pump(b,a,ab,now))return 3;
    if(xtpClose(a,now)!=XTP_CLOSED||xtpClose(b,now)!=XTP_CLOSED)return 4;
    return xtpDestroy(a)!=XTP_OK||xtpDestroy(b)!=XTP_OK;
}
