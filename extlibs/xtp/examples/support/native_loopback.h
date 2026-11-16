/* Runnable loopback infrastructure. Test-only Ed25519 certificate pin; never a production trust policy. */
#include <xtp.h>
#include <xrt/thread.h>
#include <xrt/crypto.h>
#include <xrt/tls_verify.h>
#include <assert.h>
#include <stdio.h>
#define TEST_TLS_IDENTITY_FIXTURE_ED25519
#include "../../../../tests/fixtures/tls_identity_ec_ed.h"

typedef struct native_test native_test;
typedef struct native_peer {
    native_test* Test;unsigned Index;xtpsession* Xtp;uint64_t Binding;
    xnetstream* Tcp;xtlsstream* Tls;xwsconn* Ws;xtpwsinput WsInput;
    size_t Messages,Completed;bool Started,Flushing,Reading;
    unsigned char Received[512];size_t ReceivedBytes;
} native_peer;
struct native_test {
    unsigned Mode;bool Reject,Failure,Aes;native_peer Peers[2];xatomic32 Done,Failed,Closing;
    xnetengine* Engine;xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;
    xtlsserverconfig Server;xtlsstreamconfig TlsConfig;
};
static uint64_t clock_us(void){return (uint64_t)(xrtTimer()*1000000.0);}
static void drain(native_peer* p)
{
    if(p->Flushing||xrtAtomic32Load(&p->Test->Closing,XMEMORY_ACQUIRE))return;
    p->Flushing=true;
    xtpwriteproc writer=p->Test->Mode==0?xtpTcpWrite:p->Test->Mode==1?xtpTlsWrite:xtpWsWrite;
    void* transport=p->Test->Mode==0?(void*)p->Tcp:p->Test->Mode==1?(void*)p->Tls:(void*)p->Ws;
    xtpwritebudget b={1048576,64,7};xtpwriteprogress progress;
    xtpresult r=xtpDrain(p->Xtp,1,writer,transport,&b,clock_us(),&progress);
    if(r!=XTP_OK&&r!=XTP_AGAIN)fprintf(stderr,"native drain mode=%u peer=%u result=%d\n",p->Test->Mode,p->Index,(int)r);
    assert(r==XTP_OK||r==XTP_AGAIN);p->Flushing=false;
}
static xtpdecision business(xtpsession* s,const xtpmessageview* m,void* user)
{
    native_peer* p=user;++p->Messages;
    assert(m->Command.Kind==XTP_COMMAND_ID&&m->Command.As.ID==1001);
    if(m->Type==XTP_REQUEST){xtpresponse response={0};response.Body=m->Body;
        assert(xtpReply(s,m->Request,&response,clock_us())==XTP_OK);}
    return xtpAccept();
}
static void complete(xtpsession* s,const xtpcompletion* c,void* user)
{
    native_peer* p=user;(void)s;
    if(c->Reason==XTP_FINISH_CLOSED)return;
    assert(c->Reason==XTP_FINISH_RESPONSE||c->Reason==XTP_FINISH_ACK||c->Reason==XTP_FINISH_SENT);
    if(c->Reason==XTP_FINISH_RESPONSE)assert(c->Body.Size==5&&!memcmp(c->Body.Data,"hello",5));
    ++p->Completed;if(p->Completed==3&&p->Test->Peers[1-p->Index].Completed==3)xrtAtomic32Store(&p->Test->Done,1,XMEMORY_RELEASE);
}
static void fault(xtpsession* s,const xtpfault* f,void* user)
{
    native_peer* p=user;(void)s;
    fprintf(stderr,"native fault mode=%u peer=%u reason=%d fatal=%d bytes=%zu\n",p->Test->Mode,p->Index,(int)f->Reason,f->Fatal,p->ReceivedBytes);
    for(size_t i=0;i<p->ReceivedBytes;++i)fprintf(stderr,"%02x%s",p->Received[i],i%32==31?"\n":" ");
    fputc('\n',stderr);assert(!f->Fatal);
}
static void initialize(native_peer* p)
{
    xtpevents e={business,complete,fault};assert(xtpCreate(NULL,&e,p,&p->Xtp)==XTP_OK);
    xtplane lane={1,p->Test->Mode==2?XTP_INPUT_RECORD:XTP_INPUT_STREAM,true,1048576};
    xtpbinding binding={&lane,1,p->Test->Mode==1,p->Test->Mode!=1};
    assert(xtpBind(p->Xtp,&binding,clock_us(),&p->Binding)==XTP_OK);
    p->WsInput=(xtpwsinput){p->Xtp,p->Binding,1,false};
}
static void start(native_peer* p)
{
    if(p->Started)return;
    p->Started=true;
    if(!p->Xtp)initialize(p);
    xtpmessage m[3];for(size_t i=0;i<3;++i){m[i]=(xtpmessage){0};m[i].Type=i==0?XTP_REQUEST:i==1?XTP_PUSH:XTP_NOTIFY;
        m[i].Command=xtpCmdID(1001);m[i].Body=XRT_BYTES_LITERAL("hello");}
    assert(xtpSubmitBatch(p->Xtp,m,3,NULL,clock_us(),NULL)==XTP_OK);drain(p);
}
static void input_bytes(native_peer* p,const unsigned char* bytes,size_t n)
{
    assert(n<=sizeof(p->Received)-p->ReceivedBytes);memcpy(p->Received+p->ReceivedBytes,bytes,n);p->ReceivedBytes+=n;
    size_t offset=0;while(offset<n){xtpreadprogress progress;size_t take=n-offset;if(take>7)take=7;
        xtpresult r=p->Test->Mode==2?xtpWsData(&p->WsInput,(xbytesview){bytes+offset,take},clock_us(),&progress):
            xtpInput(p->Xtp,p->Binding,1,(xbytesview){bytes+offset,take},false,clock_us(),&progress);
        assert(r>=0&&r!=XTP_PAUSED&&r!=XTP_CLOSED&&progress.Used==take);offset+=take;}
}
static void tcp_read(xnetstream* stream,xnetbuf* buffer,void* user)
{
    native_peer* p=user;(void)buffer;if(p->Reading)return;p->Reading=true;
    /* Read/Consume can drive another native Read on readiness backends.
     * Keep the copied prefix ahead of those later bytes. */
    unsigned char bytes[7];size_t n;while((n=xrtNetStreamRead(stream,bytes,sizeof(bytes)))!=0)input_bytes(p,bytes,n);
    p->Reading=false;drain(p);
}
static void tcp_drain(xnetstream* s,void* user){(void)s;native_peer* p=user;if(p->Xtp)drain(p);}
static void tls_read(xtlsstream* s,const xnetbuf* buffer,void* user)
{native_peer* p=user;(void)buffer;if(p->Reading)return;p->Reading=true;
    unsigned char bytes[7];size_t n;while(xrtTlsStreamRead(s,bytes,sizeof(bytes),&n)==XTLS_OK&&n)input_bytes(p,bytes,n);
    p->Reading=false;drain(p);}
static void tls_drain(xtlsstream* s,void* user){(void)s;native_peer* p=user;if(p->Xtp)drain(p);}
static void tls_close(xtlsstream* s,xnetresult result,const xerror* error,void* user)
{native_peer* p=user;(void)s;(void)result;(void)error;if(p->Test->Failure)xrtAtomic32Store(&p->Test->Failed,1,XMEMORY_RELEASE);}
static void ws_begin(xwsconn* ws,const xwsmessageinfo* info,void* user)
{native_peer* p=user;(void)ws;assert(xtpWsBegin(&p->WsInput,info->Opcode)==XTP_OK);}
static void ws_data(xwsconn* ws,xbytesview bytes,void* user){(void)ws;input_bytes(user,bytes.Data,bytes.Size);}
static void ws_end(xwsconn* ws,void* user)
{native_peer* p=user;(void)ws;xtpreadprogress progress;assert(xtpWsEnd(&p->WsInput,clock_us(),&progress)==XTP_OK);drain(p);}
static void ws_drain(xwsconn* ws,void* user){(void)ws;native_peer* p=user;if(p->Xtp)drain(p);}
static const xwsconnevents ws_events={.MessageBegin=ws_begin,.MessageData=ws_data,.MessageEnd=ws_end,.Drain=ws_drain};
static void attach_ws(native_peer* p,xnetstream* stream)
{
    initialize(p);p->Tcp=xrtNetStreamRef(stream);assert(p->Tcp);
    xwsconnconfig c;xrtWsConnConfigInit(&c);c.Role=p->Index?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;c.Protocol=XRT_STR_LITERAL("xtp.v3");
    p->Ws=xrtWsConnAttach(stream,&c,&ws_events,p);assert(p->Ws);start(p);
}
static void tcp_open(xnetstream* s,void* user)
{native_peer* p=user;if(p->Test->Mode==2)attach_ws(p,s);else{p->Tcp=s;start(p);}}
static void tls_open(xtlsstream* s,void* user){native_peer* p=user;p->Tls=s;
    assert(xrtTlsSessionVersion(xrtTlsStreamSession(s))==XTLS_VERSION_13);
    assert(xrtTlsSessionCipher(xrtTlsStreamSession(s))==(p->Test->Aes?XTLS_AES_128_GCM_SHA256:XTLS_CHACHA20_POLY1305_SHA256));start(p);}
static const xnetstreamevents tcp_events={.Open=tcp_open,.Read=tcp_read,.Drain=tcp_drain};
static const xtlsstreamevents tls_events={.Open=tls_open,.Read=tls_read,.Writable=tls_drain,.Drain=tls_drain,.Close=tls_close};
static bool accept_peer(xnetlistener* listener,xnetstream* s,void* user)
{
    native_test* t=user;native_peer* p=&t->Peers[1];(void)listener;
    if(t->Mode==1)return xrtTlsStreamAccept(s,&t->Server,&t->TlsConfig,&tls_events,p,&p->Tls);
    if(t->Mode==2){attach_ws(p,s);return true;}
    p->Tcp=s;assert(xrtNetStreamSetEvents(s,&tcp_events,p));start(p);tcp_read(s,NULL,p);return true;
}
static xtlsverifydecision verify_pin(const xtlspeer* peer,void* user)
{
    native_test* t=user;xbytesview pin;
    if(t->Reject||!peer||!peer->CertificateCount||!xrtTlsIdentityCertificate(t->Identity,0,&pin)||
        peer->Name.Size!=11||memcmp(peer->Name.Data,"xtp.fixture",11))return XTLS_VERIFY_REJECT;
    return peer->Certificates[0].Raw.Size==pin.Size&&!memcmp(peer->Certificates[0].Raw.Data,pin.Data,pin.Size)?XTLS_VERIFY_ACCEPT:XTLS_VERIFY_REJECT;
}
static void tls_setup(native_test* t)
{
    unsigned char seed[32]={1},public_key[32],cert[512],pkcs8[64];size_t cn,pn;
    assert(xrtEd25519Public(seed,public_key)&&testTlsIdentityEdCertificate(public_key,cert,sizeof(cert),&cn)&&testTlsIdentityEdPkcs8(seed,pkcs8,sizeof(pkcs8),&pn));
    xbytesview chain={cert,cn};t->Identity=xrtTlsIdentityEd25519(&chain,1,(xbytesview){pkcs8,pn});assert(t->Identity);
    xtlspolicy policy;xtpTlsPolicyInit(&policy,!t->Aes);
    if(t->Aes){++policy.Ciphers;policy.CipherCount=1;}
    xtlscontextconfig context;xrtTlsContextConfigInit(&context);context.Policy=&policy;
    t->Context=xrtTlsContextCreate(&context);assert(t->Context);xtlsverifierconfig verify;xrtTlsVerifierConfigInit(&verify);verify.Verify=verify_pin;verify.Context=t;
    t->Verifier=xrtTlsVerifierCreate(&verify);assert(t->Verifier);xrtTlsServerConfigInit(&t->Server);t->Server.Context=t->Context;t->Server.Identity=t->Identity;
    static const xstrview protocols[]={XRT_STR_INIT("xtp/3")};t->Server.Protocols=protocols;t->Server.ProtocolCount=1;xrtTlsStreamConfigInit(&t->TlsConfig);
}
static void run(unsigned mode,bool reject)
{
    native_test t;memset(&t,0,sizeof(t));t.Mode=mode>2?1:mode;t.Reject=reject;t.Failure=reject||mode==3;t.Aes=mode==4;
    for(unsigned i=0;i<2;++i){t.Peers[i].Test=&t;t.Peers[i].Index=i;}
    xrtAtomic32Init(&t.Done,0);xrtAtomic32Init(&t.Failed,0);xrtAtomic32Init(&t.Closing,0);
    xnetengineconfig engine;xrtNetEngineConfigInit(&engine);engine.Workers=1;t.Engine=xrtNetEngineCreate(&engine);assert(t.Engine&&xrtNetEngineStart(t.Engine));
    if(t.Mode==1)tls_setup(&t);
    xnetlistenconfig c;xrtNetListenConfigInit(&c);assert(xrtNetAddrParse(&c.Address,"127.0.0.1",0));
    xnetlistenerevents events={accept_peer,NULL,NULL};xnetlistener* listener=xrtNetListen(t.Engine,&c,&events,NULL,&t);assert(listener);
    xnetaddr address;assert(xrtNetListenerLocal(listener,&address));
    xtlsstream* client_owner=NULL;xtlscontext* legacy_context=NULL;
    if(t.Mode==1){xtlsclientconfig client;xrtTlsClientConfigInit(&client);client.Context=t.Context;client.Verifier=t.Verifier;client.ServerName=XRT_STR_LITERAL("xtp.fixture");
        if(mode==3){xtlspolicy policy;xrtTlsPolicyInit(&policy);const xtlsversion version[]={XTLS_VERSION_12};const xtlscipher cipher[]={XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256};
            policy.Versions=version;policy.VersionCount=1;policy.Ciphers=cipher;policy.CipherCount=1;
            xtlscontextconfig config;xrtTlsContextConfigInit(&config);config.Policy=&policy;legacy_context=xrtTlsContextCreate(&config);assert(legacy_context);client.Context=legacy_context;}
        static const xstrview protocols[]={XRT_STR_INIT("xtp/3")};client.Protocols=protocols;client.ProtocolCount=1;
        client_owner=xrtTlsStreamConnect(t.Engine,&address,0,&c.Stream,&client,&t.TlsConfig,&tls_events,&t.Peers[0]);assert(client_owner);
    }else {xnetstream* stream=xrtNetStreamConnect(t.Engine,&address,0,&c.Stream,&tcp_events,&t.Peers[0]);assert(stream);}
    double deadline=xrtTimer()+10;while(!xrtAtomic32Load(t.Failure?&t.Failed:&t.Done,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtSleep(1);}
    xrtAtomic32Store(&t.Closing,1,XMEMORY_RELEASE);
    for(unsigned i=0;i<2;++i){native_peer* p=&t.Peers[i];
        if(t.Mode==1){xtlsstream* tls=i?p->Tls:client_owner;if(tls){(void)xrtTlsStreamAbort(tls);assert(xrtTlsStreamWaitRetired(tls,10000)==XWAIT_OK);xrtTlsStreamDestroy(tls);}}
        else {if(p->Ws)(void)xrtWsConnAbort(p->Ws);else assert(xrtNetStreamClose(p->Tcp));
            assert(xrtNetStreamWaitRetired(p->Tcp,10000)==XWAIT_OK);if(p->Ws)xrtWsConnDestroy(p->Ws);xrtNetStreamDestroy(p->Tcp);}
        if(p->Xtp){xtpresult r=xtpClose(p->Xtp,clock_us());while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(p->Xtp,clock_us(),&due);}assert(r==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);}}
    assert(xrtNetListenerClose(listener));xrtNetListenerDestroy(listener);
    deadline=xrtTimer()+10;xnetretireresult retired;while((retired=xrtNetEngineTryDestroy(t.Engine))==XNET_RETIRE_BUSY){assert(xrtTimer()<deadline);xrtSleep(1);}assert(retired==XNET_RETIRE_READY);
    if(t.Mode==1){xrtTlsVerifierRelease(t.Verifier);xrtTlsIdentityRelease(t.Identity);xrtTlsContextRelease(t.Context);if(legacy_context)xrtTlsContextRelease(legacy_context);}
    if(!t.Failure)assert(t.Peers[0].Messages==3&&t.Peers[1].Messages==3);else assert(!t.Peers[0].Messages&&!t.Peers[1].Messages);
}
