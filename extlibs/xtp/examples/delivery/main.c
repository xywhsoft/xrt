#include <xtp.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
static xtpdecision received(xtpsession* s,const xtpmessageview* m,void* user)
{(void)s;(void)user;printf("accepted push %llu\n",(unsigned long long)m->MessageID);return xtpAccept();}
static void completed(xtpsession* s,const xtpcompletion* c,void* user)
{(void)s;(void)user;printf("completed %llu: knowledge=%d\n",(unsigned long long)c->MessageID,(int)c->Knowledge);}
static void move(xtpsession* a,xtpsession* b,uint64_t binding)
{xtpoutput out;while(xtpOutput(a,0,&out)==XTP_OK){unsigned char copy[128];size_t n=out.Data.Size;assert(n<=sizeof(copy));
    memcpy(copy,out.Data.Data,n);assert(xtpAdvance(a,out.Ticket,n,0)==XTP_OK);xtpreadprogress p;
    assert(xtpInput(b,binding,1,(xbytesview){copy,n},false,0,&p)>=0&&p.Used==n);}}
int main(void)
{
    xtpconfig c;xtpConfigInit(&c);xtpDeliveryConfigInit(&c.Delivery);xtpevents e={received,completed,NULL};
    xtpsession* a;xtpsession* b;assert(xtpCreate(&c,&e,NULL,&a)==XTP_OK&&xtpCreate(&c,&e,NULL,&b)==XTP_OK);
    /* In production the host authenticates peers and exchanges a random shared
     * SessionID. Fixed public test identities are used only in this example. */
    xtpdeliveryproof proof={0};proof.Identity.SessionID[0]=1;proof.Identity.Domain[0]=2;
    proof.Identity.Local[0]=3;proof.Identity.Peer[0]=4;proof.UntilUs=100000000;
    proof.LifetimeUs=60000000;proof.PeerReceiveFloor=proof.PeerResultFloor=1;proof.StateRetained=true;
    xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpbinding bind={&lane,1,true,false,&proof};uint64_t ab,bb;
    assert(xtpBind(a,&bind,0,&ab)==XTP_OK);proof.Identity.Local[0]=4;proof.Identity.Peer[0]=3;
    assert(xtpBind(b,&bind,0,&bb)==XTP_OK);xtpmessage m={0};m.Type=XTP_PUSH;m.Command=xtpCmdID(7);
    xtpsubmitoptions opt={0};opt.Replay=true;assert(xtpSubmit(a,&m,&opt,0,NULL)==XTP_OK);move(a,b,bb);
    /* Simulate a lost ACK by detaching both transports before transmitting it. */
    assert(xtpUnbind(a,ab,0)==XTP_OK&&xtpUnbind(b,bb,0)==XTP_OK);
    proof.Resume=proof.OldBindingDetached=true;assert(xtpBind(b,&bind,0,&bb)==XTP_OK);
    proof.Identity.Local[0]=3;proof.Identity.Peer[0]=4;assert(xtpBind(a,&bind,0,&ab)==XTP_OK);
    move(a,b,bb);move(b,a,ab);assert(xtpClose(a,0)==XTP_CLOSED&&xtpClose(b,0)==XTP_CLOSED);
    assert(xtpDestroy(a)==XTP_OK&&xtpDestroy(b)==XTP_OK);return 0;
}
