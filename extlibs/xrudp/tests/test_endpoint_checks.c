#include <xrudp.h>
#include <xrt/atomic.h>
#include <xrt/crypto.h>
#include <xrt/time.h>
#include <xrt/thread.h>
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"check test %d: %s\n",__LINE__,#c);return 1;}}while(0)
#ifndef TEST_RUDP_BACKEND
#define TEST_RUDP_BACKEND XNET_PORT_SELECT
#endif
typedef struct test test;
typedef struct side {test* T;unsigned Id;} side;
struct test {
    xnetengine* Engine; xrudpendpoint* E[2]; xrudppeer* Peer[2]; xnetudp* G[3];
    xnetaddr Local[2],Public[3]; xrudpcheckstate State[2]; side S[5];
    unsigned char Private[2][32],Key[2][32],Scope[16],Context[32];
    xatomic32 Ready,Done,Closed,Failure; xnetfamily Family;
    unsigned Dialer,Mode,DropCount; bool Begun,Checking; double Start;
    unsigned char Captured[72]; bool CapturedReply;
};
static void expect(test* t,bool value,const char* reason)
{if(!value){fprintf(stderr,"endpoint check mode=%u: %s\n",t->Mode,reason);xrtAtomic32Store(&t->Failure,1,XMEMORY_RELEASE);}}
static bool await(test* t,xatomic32* word,unsigned mask)
{double end=xrtTimer()+6;while((xrtAtomic32Load(word,XMEMORY_ACQUIRE)&mask)!=mask){if(xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE)||xrtTimer()>end)return false;xrtSleep(1);}return true;}
static void opened(xrudpendpoint* e,void* context)
{side* s=context;xrudpendpointinfo i;expect(s->T,xrtRudpEndpointInfo(e,&i)==XRUDP_OK,"info");s->T->Local[s->Id]=i.Local;(void)xrtAtomic32FetchOr(&s->T->Ready,1u<<s->Id,XMEMORY_RELEASE);}
static void closed(xrudpendpoint* e,xrudpresult reason,const xerror* error,void* context)
{side* s=context;(void)e;(void)reason;(void)error;(void)xrtAtomic32FetchOr(&s->T->Closed,1u<<s->Id,XMEMORY_RELEASE);}
static void confirmed(xrudpendpoint* e,xrudppeer* p,void* context)
{
    side* s=context;test* t=s->T;(void)e;expect(t,t->Mode==0,"forged/wrong-context probe admitted Noise");
    if(!t->Peer[s->Id])t->Peer[s->Id]=xrtRudpPeerRef(p);
    size_t n=0;expect(t,xrtRudpPeerWrite(p,"proof",5,&n)==XRUDP_OK && n==5,"encrypted data admission");
}
static void changed(xrudpendpoint* e,xrudppeer* p,uint32_t events,void* context)
{
    side* s=context;test* t=s->T;(void)e;
    if(events&XRUDP_PEER_READ_READY){unsigned char b[16];size_t n=0;xrudpresult r=xrtRudpPeerRead(p,b,sizeof(b),&n);
        expect(t,r==XRUDP_OK && n==5 && !memcmp(b,"proof",5),"encrypted data integrity");
        (void)xrtAtomic32FetchOr(&t->Done,1u<<s->Id,XMEMORY_RELEASE);}
}
static void gateway_open(xnetudp* u,void* context)
{side* s=context;expect(s->T,xrtNetUdpLocal(u,&s->T->Public[s->Id-2]),"gateway address");(void)xrtAtomic32FetchOr(&s->T->Ready,1u<<s->Id,XMEMORY_RELEASE);}
static void gateway_close(xnetudp* u,xnetresult result,const xerror* error,void* context)
{side* s=context;(void)u;(void)result;(void)error;(void)xrtAtomic32FetchOr(&s->T->Closed,1u<<s->Id,XMEMORY_RELEASE);}
/* Side 0's advertised mapping is a blackhole (G2). Its traffic to public
 * side 1 actually originates at G1. Thus an authenticated peer-reflexive
 * address and an independent native dialer role are both exercised. */
static void gateway_input(xnetudp* u,const xnetudpmessage* m,void* context)
{
    side* s=context;test* t=s->T;unsigned g=s->Id-2;(void)u;
    if(g==2){++t->DropCount;return;}
    unsigned from=g==0?0u:1u,to=1u-from;
    expect(t,xrtNetAddrEqual(&m->Remote,&t->Local[from]),"unexpected gateway sender");
    unsigned char corrupt[72];const void* bytes=m->Data;
    bool check=m->Size==72 && !memcmp(m->Data,"XRC1",4);
    if(check && g==1 && m->Data[4]==2){memcpy(t->Captured,m->Data,72);t->CapturedReply=true;}
    if(check && t->Mode==4 && g==0){expect(t,xrtNetUdpSendTo(t->G[0],&t->Local[0],m->Data,m->Size)==XNET_RESULT_OK,"reflection fixture");return;}
    if(t->Mode==1 && m->Size==72 && !memcmp(m->Data,"XRC1",4)) {memcpy(corrupt,bytes,72);corrupt[71]^=1;bytes=corrupt;}
    xnetudp* output=t->G[(t->Mode==3 && check && g==0 && m->Data[4]==3)?2:1-g];
    expect(t,xrtNetUdpSendTo(output,&t->Local[to],bytes,m->Size)==XNET_RESULT_OK,"NAT forwarding");
}
static void bind_all(xnetworker* worker,void* context)
{
    test* t=context;
    for(unsigned i=0;i<2;++i){xrudpendpointconfig c;xrtRudpEndpointConfigInit(&c);
        c.StaticPrivate=t->Private[i];c.PeerPublic=t->Key[1-i];c.Scope=t->Scope;c.ApplicationContext=t->Context;
        unsigned char wrong[32];memcpy(wrong,t->Context,32);wrong[0]^=1;if(t->Mode==2 && i==1)c.ApplicationContext=wrong;
        c.Listen=true;c.ConnectivityChecks=true;c.MaxPeers=c.MaxHalfOpen=1;c.HandshakeTimeoutUs=1000000;
        if(t->Mode==5)c.ConnectivityTimeoutMs=50;
        xrudpendpointevents v={opened,confirmed,NULL,closed,changed,NULL};xnetaddr local;xrtNetAddrLoopback(&local,t->Family,0);
        expect(t,xrtRudpEndpointBind(worker,&local,&c,&v,&t->S[i],&t->E[i])==XRUDP_OK,"bind");}
    for(unsigned i=0;i<3;++i){xnetudpconfig c;xrtNetUdpConfigInit(&c);c.IPv6Only=true;xnetaddr local;xrtNetAddrLoopback(&local,t->Family,0);
        xnetudpevents v={0};v.Open=gateway_open;v.Receive=gateway_input;v.Close=gateway_close;
        t->G[i]=xrtNetUdpBind(t->Engine,&local,0,&c,&v,&t->S[i+2]);expect(t,t->G[i]!=NULL,"gateway bind");}
    if(t->Mode==5)xrtSleep(250); /* Open callbacks cannot renew admission. */
}
static void tick(xnetworker* worker,uint64 id,xnetresult result,void* context)
{
    test* t=context;(void)id;if(result!=XNET_RESULT_OK)return;
    for(unsigned i=0;i<2;++i)expect(t,xrtRudpEndpointCheckInfo(t->E[i],&t->State[i])==XRUDP_OK,"check snapshot");
    if(!t->Begun && t->Mode==0 && t->State[0].Replies && t->State[1].Confirmations){
        expect(t,xrtNetAddrEqual(&t->State[0].Reply[0],&t->Public[0]),"returned address");
        expect(t,xrtNetAddrEqual(&t->State[1].Confirmed[0],&t->Public[1]) && !xrtNetAddrEqual(&t->Public[1],&t->Public[2]),"peer-reflexive source");
        unsigned d=t->Dialer;xrudpconnectconfig c={t->Key[1-d],t->Scope,t->Context};
        expect(t,xrtRudpEndpointConnect(t->E[d],d==0?&t->Public[0]:&t->State[1].Confirmed[0],&c,&t->Peer[d])==XRUDP_OK,"independent handshake role");t->Begun=true;
    }
    if(t->Mode && xrtTimer()-t->Start>.7){
        expect(t,!t->State[0].Confirmations&&!t->State[1].Replies&&!t->State[1].Confirmations,"unauthenticated address learned");
        expect(t,t->Mode==3?t->State[0].Replies>0:!t->State[0].Replies,"response authentication");
        expect(t,t->State[t->Mode==4?0:1].Rejected>0,"invalid packet not rejected");xrtAtomic32Store(&t->Done,3,XMEMORY_RELEASE);return;}
    if(xrtAtomic32Load(&t->Done,XMEMORY_ACQUIRE)!=3 && !xrtAtomic32Load(&t->Failure,XMEMORY_ACQUIRE))
        expect(t,xrtNetEngineAfter(t->Engine,xrtNetWorkerIndex(worker),5,tick,t)!=0,"poll timer");
}
static void start(xnetworker* worker,void* context)
{
    test* t=context;xnetaddr duplicate[2]={t->Public[0],t->Public[0]};
    if(t->Mode==5){
        for(unsigned i=0;i<2;++i){xrudpcheckstate state;
            expect(t,xrtRudpEndpointCheck(t->E[i],&t->Public[0],1,1000)==XRUDP_TIMEOUT,"late Open restarted check lifetime");
            expect(t,xrtRudpEndpointCheckInfo(t->E[i],&state)==XRUDP_OK && !state.Active && !state.Sent && state.Reason==XRUDP_TIMEOUT,"expired check emitted packets");}
        xrtAtomic32Store(&t->Done,3,XMEMORY_RELEASE);return;
    }
    expect(t,xrtRudpEndpointCheck(t->E[0],duplicate,2,1000)==XRUDP_ARGUMENT,"duplicate targets");
    expect(t,xrtRudpEndpointCheck(t->E[0],duplicate,0,1000)==XRUDP_ARGUMENT,"empty targets");
    expect(t,xrtRudpEndpointCheck(t->E[0],duplicate,9,1000)==XRUDP_ARGUMENT,"capacity");
    expect(t,xrtRudpEndpointCheck(t->E[0],duplicate,1,10001)==XRUDP_ARGUMENT,"deadline cap");
    expect(t,xrtRudpEndpointCheck(t->E[0],&t->Public[0],1,2000)==XRUDP_OK,"start client check");
    expect(t,xrtRudpEndpointCheck(t->E[1],&t->Public[2],1,2000)==XRUDP_OK,"start unreachable advertised check");
    xrudpcheckstate before,after;expect(t,xrtRudpEndpointCheckInfo(t->E[0],&before)==XRUDP_OK,"deadline before");
    expect(t,xrtRudpEndpointCheck(t->E[0],&t->Public[1],1,10000)==XRUDP_DONE,"cannot renew/retarget");
    expect(t,xrtRudpEndpointCheckInfo(t->E[0],&after)==XRUDP_OK && before.DeadlineUs==after.DeadlineUs,"deadline immutable");
    t->Start=xrtTimer();tick(worker,0,XNET_RESULT_OK,t);
}
static void canceled_snapshot(xnetworker* worker,uint64 id,xnetresult result,void* context)
{
    test* t=context;(void)worker;(void)id;if(result!=XNET_RESULT_OK)return;
    for(unsigned i=0;i<2;++i){xrudpcheckstate state;
        expect(t,xrtRudpEndpointCheckInfo(t->E[i],&state)==XRUDP_OK && !state.Active &&
            state.Replies==t->State[i].Replies && state.Confirmations==t->State[i].Confirmations,"late replay revived canceled check");}
    xrtAtomic32Store(&t->Ready,63,XMEMORY_RELEASE);
}
static void cancel(xnetworker* worker,void* context)
{
    test* t=context;(void)worker;
    for(unsigned i=0;i<2;++i){expect(t,xrtRudpEndpointCheckCancel(t->E[i])==(t->Mode==5?XRUDP_DONE:XRUDP_OK) && xrtRudpEndpointCheckCancel(t->E[i])==XRUDP_DONE,"idempotent cancel");
        expect(t,xrtRudpEndpointCheckInfo(t->E[i],&t->State[i])==XRUDP_OK && !t->State[i].Active,"credentials canceled");}
    if(t->CapturedReply)expect(t,xrtNetUdpSendTo(t->G[2],&t->Local[0],t->Captured,72)==XNET_RESULT_OK,"post-cancel authenticated replay");
    expect(t,xrtNetEngineAfter(t->Engine,xrtNetWorkerIndex(worker),50,canceled_snapshot,t)!=0,"cancel snapshot timer");
}
static int run(xnetfamily family,unsigned dialer,unsigned mode)
{
    test t;memset(&t,0,sizeof(t));t.Family=family;t.Dialer=dialer;t.Mode=mode;memset(t.Scope,7,16);memset(t.Context,9,32);
    for(unsigned i=0;i<5;++i){t.S[i].T=&t;t.S[i].Id=i;}
    for(unsigned i=0;i<2;++i){for(unsigned k=0;k<32;++k)t.Private[i][k]=(unsigned char)(19+i*61+k);CHECK(xrtX25519Public(t.Private[i],t.Key[i]));}
    xnetengineconfig c;xrtNetEngineConfigInit(&c);c.Workers=1;c.Backend=TEST_RUDP_BACKEND;
    t.Engine=xrtNetEngineCreate(&c);CHECK(t.Engine && xrtNetEngineStart(t.Engine));
    CHECK(xrtNetEnginePost(t.Engine,0,bind_all,&t) && await(&t,&t.Ready,31));
    CHECK(xrtNetEnginePost(t.Engine,0,start,&t) && await(&t,&t.Done,3));
    CHECK(xrtNetEnginePost(t.Engine,0,cancel,&t) && await(&t,&t.Ready,63));
    for(unsigned i=0;i<2;++i)CHECK(xrtRudpEndpointClose(t.E[i])==XRUDP_OK);
    for(unsigned i=0;i<3;++i)CHECK(xrtNetUdpAbort(t.G[i]));
    CHECK(await(&t,&t.Closed,31) && xrtNetEngineStop(t.Engine) && xrtNetEngineDestroy(t.Engine));
    for(unsigned i=0;i<2;++i){xrudpcheckstate state;CHECK(xrtRudpEndpointCheckInfo(t.E[i],&state)==XRUDP_OK && !state.Active);
        CHECK(xrtRudpEndpointCheckCancel(t.E[i])==XRUDP_DONE);if(mode==5)CHECK(state.Reason==XRUDP_TIMEOUT);
        xrtRudpPeerDestroy(t.Peer[i]);xrtRudpEndpointDestroy(t.E[i]);}
    for(unsigned i=0;i<3;++i)xrtNetUdpDestroy(t.G[i]);CHECK((mode==5?!t.DropCount:t.DropCount>0) && !xrtAtomic32Load(&t.Failure,XMEMORY_ACQUIRE));
    printf("PASS authenticated checks family=%u dialer=%u mode=%u APDM, return path, encrypted data and close\n",(unsigned)family,dialer,mode);return 0;
}
int main(void)
{
    CHECK(run(XNET_FAMILY_IPV4,0,0)==0);CHECK(run(XNET_FAMILY_IPV4,1,0)==0);
    CHECK(run(XNET_FAMILY_IPV6,0,0)==0);CHECK(run(XNET_FAMILY_IPV6,1,0)==0);
    CHECK(run(XNET_FAMILY_IPV4,0,1)==0);CHECK(run(XNET_FAMILY_IPV4,0,2)==0);
    CHECK(run(XNET_FAMILY_IPV4,0,3)==0);CHECK(run(XNET_FAMILY_IPV4,0,4)==0);
    CHECK(run(XNET_FAMILY_IPV4,0,5)==0);return 0;
}
