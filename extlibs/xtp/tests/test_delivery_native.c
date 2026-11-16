/* Real TLS disconnect with a partial RESPONSE, followed by authenticated
 * host-controlled state continuation. The bearer exchange is test frontend
 * code over pinned TLS, not an XTP wire command or a production login service. */
#include "../examples/support/native_loopback.h"
#include <xrt/random.h>
typedef struct recovery recovery;
typedef struct recovery_peer {
    recovery* Test;unsigned Index;xtpsession* Session;xtlsstream* Tls;uint64_t Binding;
    size_t AuthBytes,Requests,Pushes,Responses,Acks,Unknown;unsigned char Auth[32];
    bool Reading,Flushing,Authenticated,Blocked;size_t Prefix;
} recovery_peer;
struct recovery {
    native_test Native;recovery_peer Peers[2];xatomic32 Ready,Received,Finished,Stopped,Denied,Drained;
    xnetlistener* Listener;unsigned Failure;bool Resume,Cutting,OldTicketValid;xtpoutputticket OldTicket;
    uint64_t Until;unsigned char Token[32],SessionID[16];
};
static void recovery_drain(recovery_peer* p)
{
    if(!p->Authenticated||p->Blocked||p->Flushing)return;
    p->Flushing=true;xtpwritebudget b={p->Prefix?p->Prefix:1048576,64,p->Prefix?p->Prefix:16384};xtpwriteprogress progress;
    xtpresult r=xtpDrain(p->Session,1,xtpTlsWrite,p->Tls,&b,clock_us(),&progress);assert(r==XTP_OK||r==XTP_AGAIN);
    if(p->Prefix&&progress.Bytes){assert(progress.Bytes==p->Prefix);p->Prefix=0;p->Blocked=true;}
    p->Flushing=false;
}
static xtpdecision recovery_message(xtpsession* session,const xtpmessageview* message,void* user)
{
    recovery_peer* p=user;assert(p->Index==1&&message->Body.Size==5&&!memcmp(message->Body.Data,"hello",5));
    if(message->Type==XTP_REQUEST){++p->Requests;xtpresponse reply={0};reply.Body=message->Body;assert(xtpReply(session,message->Request,&reply,clock_us())==XTP_OK);}
    else {assert(message->Type==XTP_PUSH);++p->Pushes;}
    if(p->Requests==p->Pushes)xrtAtomic32Store(&p->Test->Received,(uint32)p->Requests,XMEMORY_RELEASE);
    return xtpAccept();
}
static void recovery_complete(xtpsession* session,const xtpcompletion* completion,void* user)
{
    recovery_peer* p=user;(void)session;assert(!p->Index);
    if(completion->Reason==XTP_FINISH_CLOSED){assert(completion->Knowledge==XTP_KNOW_UNKNOWN);++p->Unknown;return;}
    if(completion->Reason==XTP_FINISH_RESPONSE){assert(completion->Knowledge==XTP_KNOW_REPLIED&&completion->Body.Size==5);++p->Responses;}
    else {assert(completion->Reason==XTP_FINISH_ACK&&completion->Knowledge==XTP_KNOW_ACCEPTED);++p->Acks;}
    if(p->Responses==p->Acks)xrtAtomic32Store(&p->Test->Finished,(uint32)p->Responses,XMEMORY_RELEASE);
}
static void recovery_fault(xtpsession* session,const xtpfault* fault,void* user)
{(void)session;(void)user;(void)fault;assert(!"unexpected recovery protocol fault");}
static void recovery_create_session(recovery_peer* p)
{
    xtpconfig config;xtpConfigInit(&config);xtpDeliveryConfigInit(&config.Delivery);config.Delivery.AllowRequests=true;
    config.Delivery.MaxLifetimeUs=30000000;config.MaxResponseBytes=256;config.DedupWindow=4;
    xtpevents events={recovery_message,recovery_complete,recovery_fault};assert(xtpCreate(&config,&events,p,&p->Session)==XTP_OK);
}
static void recovery_bind(recovery_peer* p)
{
    recovery* t=p->Test;xtpinfo peer;assert(xtpInfo(t->Peers[1-p->Index].Session,&peer)==XTP_OK);
    xtpdeliveryproof proof={0};memcpy(proof.Identity.SessionID,t->SessionID,16);proof.Identity.Domain[0]=11;
    proof.Identity.Local[0]=(unsigned char)(p->Index+1);proof.Identity.Peer[0]=(unsigned char)(2-p->Index);
    proof.UntilUs=t->Until;proof.LifetimeUs=30000000;proof.PeerReceiveFloor=peer.ReceiveFloor;proof.PeerResultFloor=peer.ResultFloor;
    proof.AllowRequests=true;proof.Resume=proof.OldBindingDetached=t->Resume;proof.StateRetained=!(t->Resume&&t->Failure==2);
    if(t->Resume&&t->Failure==3&&!p->Index)proof.Identity.Peer[1]=1;
    xtplane lane={1,XTP_INPUT_STREAM,true,0};xtpbinding binding={&lane,1,true,false,&proof};
    xtpresult r=xtpBind(p->Session,&binding,clock_us(),&p->Binding);
    if(r!=XTP_OK){assert(t->Resume&&t->Failure);p->Authenticated=false;xrtAtomic32Store(&t->Denied,1,XMEMORY_RELEASE);return;}
    if(p->Index&&t->OldTicketValid)assert(xtpAdvance(p->Session,t->OldTicket,1,clock_us())==XTP_E_STATE);
    p->Authenticated=true;(void)xrtAtomic32FetchOr(&t->Ready,1u<<p->Index,XMEMORY_RELEASE);recovery_drain(p);
}
static void recovery_auth_send(recovery_peer* p,const unsigned char* bytes)
{size_t n;assert(xrtTlsStreamSend(p->Tls,bytes,32,&n)==XTLS_OK&&n==32);}
static void recovery_read(xtlsstream* stream,const xnetbuf* buffer,void* user)
{
    recovery_peer* p=user;(void)buffer;if(p->Reading)return;p->Reading=true;
    size_t n;
    if(!p->Authenticated){
        if(xrtAtomic32Load(&p->Test->Denied,XMEMORY_ACQUIRE)){p->Reading=false;return;}
        while(p->AuthBytes<32&&xrtTlsStreamRead(stream,p->Auth+p->AuthBytes,32-p->AuthBytes,&n)==XTLS_OK&&n)p->AuthBytes+=n;
        if(p->AuthBytes<32){p->Reading=false;return;}
        unsigned char expected[32];memcpy(expected,p->Test->Token,32);if(!p->Index)for(size_t i=0;i<32;++i)expected[i]^=0xa5;
        if(!xrtConstTimeEqual(expected,p->Auth,32)){assert(p->Test->Failure==1);xrtAtomic32Store(&p->Test->Denied,1,XMEMORY_RELEASE);p->Reading=false;return;}
        if(p->Index){for(size_t i=0;i<32;++i)expected[i]^=0xa5;recovery_auth_send(p,expected);}
        recovery_bind(p);if(!p->Authenticated){p->Reading=false;return;}
    }
    unsigned char bytes[512];while(xrtTlsStreamRead(stream,bytes,sizeof(bytes),&n)==XTLS_OK&&n){
        size_t offset=0;while(offset<n){xtpreadprogress progress;xtpresult r=xtpInput(p->Session,p->Binding,1,(xbytesview){bytes+offset,n-offset},false,clock_us(),&progress);
            assert(r>=0&&progress.Used&&r!=XTP_PAUSED&&r!=XTP_CLOSED);offset+=progress.Used;}}
    p->Reading=false;recovery_drain(p);
}
static void recovery_open(xtlsstream* stream,void* user)
{
    recovery_peer* p=user;p->Tls=stream;assert(xrtTlsSessionVersion(xrtTlsStreamSession(stream))==XTLS_VERSION_13);
    if(!p->Index){unsigned char token[32];memcpy(token,p->Test->Token,32);if(p->Test->Resume&&p->Test->Failure==1)token[0]^=1;recovery_auth_send(p,token);}
}
static void recovery_writable(xtlsstream* stream,void* user){(void)stream;recovery_drain(user);}
static const xtlsstreamevents recovery_events={.Open=recovery_open,.Read=recovery_read,.Writable=recovery_writable,.Drain=recovery_writable};
static bool recovery_accept(xnetlistener* listener,xnetstream* stream,void* user)
{recovery* t=user;(void)listener;return xrtTlsStreamAccept(stream,&t->Native.Server,&t->Native.TlsConfig,&recovery_events,&t->Peers[1],&t->Peers[1].Tls);}
static void recovery_wait(recovery* t,xatomic32* value,uint32 expected)
{double due=xrtTimer()+15;while(xrtAtomic32Load(value,XMEMORY_ACQUIRE)!=expected){assert(!xrtAtomic32Load(&t->Denied,XMEMORY_ACQUIRE));assert(xrtTimer()<due);xrtSleep(1);}}
static void recovery_connect(recovery* t)
{
    xrtAtomic32Store(&t->Ready,0,XMEMORY_RELEASE);xrtAtomic32Store(&t->Denied,0,XMEMORY_RELEASE);
    for(size_t i=0;i<2;++i){t->Peers[i].AuthBytes=0;t->Peers[i].Authenticated=t->Peers[i].Reading=t->Peers[i].Flushing=false;}
    xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=t->Native.Context;client.Verifier=t->Native.Verifier;client.ServerName=XRT_STR_LITERAL("xtp.fixture");
    static const xstrview protocols[]={XRT_STR_INIT("xtp/3")};client.Protocols=protocols;client.ProtocolCount=1;
    xnetaddr address;assert(xrtNetListenerLocal(t->Listener,&address));xnetstreamconfig config;xrtNetStreamConfigInit(&config);
    t->Peers[0].Tls=xrtTlsStreamConnect(t->Native.Engine,&address,0,&config,&client,&t->Native.TlsConfig,&recovery_events,&t->Peers[0]);assert(t->Peers[0].Tls);
    if(t->Resume&&t->Failure){double due=xrtTimer()+15;while(!xrtAtomic32Load(&t->Denied,XMEMORY_ACQUIRE)){assert(xrtTimer()<due);xrtSleep(1);}}
    else recovery_wait(t,&t->Ready,3);
}
static void recovery_submit(xnetworker* worker,void* user)
{
    recovery* t=user;(void)worker;t->Peers[1].Blocked=false;t->Peers[1].Prefix=7;
    xtpmessage messages[2]={{0},{0}};xtpsubmitoptions options[2]={{0},{0}};
    for(size_t i=0;i<2;++i){messages[i].Type=i?XTP_PUSH:XTP_REQUEST;messages[i].Command=xtpCmdID(1001);messages[i].Body=XRT_BYTES_LITERAL("hello");options[i].Replay=true;}
    assert(xtpSubmitBatch(t->Peers[0].Session,messages,2,options,clock_us(),NULL)==XTP_OK);recovery_drain(&t->Peers[0]);
}
static void recovery_stop(xnetworker* worker,void* user)
{
    recovery* t=user;(void)worker;
    if(t->Cutting){xtpoutput out;assert(t->Peers[1].Blocked&&!t->Peers[1].Prefix);
        assert(xtpOutput(t->Peers[1].Session,1,&out)==XTP_OK&&out.Data.Size==62);t->OldTicket=out.Ticket;t->OldTicketValid=true;}
    for(size_t i=0;i<2;++i){recovery_peer* p=&t->Peers[i];p->Authenticated=false;xtpinfo info;assert(xtpInfo(p->Session,&info)==XTP_OK);
        if(info.Bound){xtpresult r=xtpUnbind(p->Session,p->Binding,clock_us());while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(p->Session,clock_us(),&due);}assert(r==XTP_OK);}
        if(p->Tls)(void)xrtTlsStreamAbort(p->Tls);}
    xrtAtomic32Store(&t->Stopped,1,XMEMORY_RELEASE);
}
static void recovery_disconnect(recovery* t,bool cutting)
{
    t->Cutting=cutting;
    xrtAtomic32Store(&t->Stopped,0,XMEMORY_RELEASE);assert(xrtNetEnginePost(t->Native.Engine,0,recovery_stop,t));
    double due=xrtTimer()+15;while(!xrtAtomic32Load(&t->Stopped,XMEMORY_ACQUIRE)){assert(xrtTimer()<due);xrtSleep(1);}
    for(size_t i=0;i<2;++i){assert(xrtTlsStreamWaitRetired(t->Peers[i].Tls,10000)==XWAIT_OK);xrtTlsStreamDestroy(t->Peers[i].Tls);t->Peers[i].Tls=NULL;}
}
static void recovery_drained(xnetworker* worker,void* user)
{recovery* t=user;(void)worker;xtpinfo info;assert(xtpInfo(t->Peers[1].Session,&info)==XTP_OK);if(!info.Retained)xrtAtomic32Store(&t->Drained,1,XMEMORY_RELEASE);}
static void recovery_run(unsigned failure)
{
    recovery t;memset(&t,0,sizeof(t));t.Failure=failure;t.Native.Mode=1;tls_setup(&t.Native);
    assert(xrtSecureRandom(t.Token,sizeof(t.Token))&&xrtSecureRandom(t.SessionID,sizeof(t.SessionID)));t.Until=clock_us()+120000000;
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Workers=1;t.Native.Engine=xrtNetEngineCreate(&ec);assert(t.Native.Engine&&xrtNetEngineStart(t.Native.Engine));
    xrtAtomic32Init(&t.Ready,0);xrtAtomic32Init(&t.Received,0);xrtAtomic32Init(&t.Finished,0);xrtAtomic32Init(&t.Stopped,0);xrtAtomic32Init(&t.Denied,0);xrtAtomic32Init(&t.Drained,0);
    for(unsigned i=0;i<2;++i){t.Peers[i].Test=&t;t.Peers[i].Index=i;recovery_create_session(&t.Peers[i]);}
    xnetlistenconfig listen;xrtNetListenConfigInit(&listen);assert(xrtNetAddrParse(&listen.Address,"127.0.0.1",0));xnetlistenerevents events={recovery_accept,NULL,NULL};
    t.Listener=xrtNetListen(t.Native.Engine,&listen,&events,NULL,&t);assert(t.Listener);
    unsigned cycles=failure?1:20;
    for(unsigned cycle=0;cycle<cycles;++cycle){recovery_connect(&t);assert(xrtNetEnginePost(t.Native.Engine,0,recovery_submit,&t));recovery_wait(&t,&t.Received,cycle+1);
        recovery_disconnect(&t,true);assert(t.Peers[0].Responses==cycle&&t.Peers[0].Acks==cycle);
        if(failure==2){xtpresult r=xtpClose(t.Peers[1].Session,clock_us());while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(t.Peers[1].Session,clock_us(),&due);}
            assert(r==XTP_CLOSED&&xtpDestroy(t.Peers[1].Session)==XTP_OK);recovery_create_session(&t.Peers[1]);}
        t.Resume=true;t.Peers[1].Blocked=false;t.Peers[1].Prefix=0;recovery_connect(&t);
        if(!failure){recovery_wait(&t,&t.Finished,cycle+1);xrtAtomic32Store(&t.Drained,0,XMEMORY_RELEASE);
            double due=xrtTimer()+15;while(!xrtAtomic32Load(&t.Drained,XMEMORY_ACQUIRE)){assert(xrtTimer()<due);assert(xrtNetEnginePost(t.Native.Engine,0,recovery_drained,&t));xrtSleep(1);}}
        recovery_disconnect(&t,false);assert(t.Peers[1].Requests==cycle+1&&t.Peers[1].Pushes==cycle+1);
    }
    assert(xrtNetListenerClose(t.Listener));xrtNetListenerDestroy(t.Listener);
    double deadline=xrtTimer()+10;xnetretireresult retired;while((retired=xrtNetEngineTryDestroy(t.Native.Engine))==XNET_RETIRE_BUSY){assert(xrtTimer()<deadline);xrtSleep(1);}assert(retired==XNET_RETIRE_READY);
    for(size_t i=0;i<2;++i){xtpresult r=xtpClose(t.Peers[i].Session,clock_us());while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(t.Peers[i].Session,clock_us(),&due);}assert(r==XTP_CLOSED&&xtpDestroy(t.Peers[i].Session)==XTP_OK);}
    if(failure)assert(t.Peers[0].Unknown==2&&!t.Peers[0].Responses&&!t.Peers[0].Acks);
    else assert(t.Peers[0].Responses==20&&t.Peers[0].Acks==20&&!t.Peers[0].Unknown);
    xrtTlsVerifierRelease(t.Native.Verifier);xrtTlsIdentityRelease(t.Native.Identity);xrtTlsContextRelease(t.Native.Context);
    printf("XTP real TLS recovery: scenario=%u cycles=%u UNKNOWN=%zu no duplicate business PASS\n",failure,cycles,t.Peers[0].Unknown);
}
int main(void){(void)run;for(unsigned failure=0;failure<4;++failure)recovery_run(failure);return 0;}
