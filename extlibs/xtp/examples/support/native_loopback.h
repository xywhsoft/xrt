/* Runnable loopback infrastructure. Test-only Ed25519 certificate pin; never a production trust policy. */
#include <xtp.h>
#include <xrt/thread.h>
#include <xrt/crypto.h>
#include <xrt/tls_verify.h>
#include <assert.h>
#include <stdio.h>
#if defined(XTP_TEST_WSS)
#include <xrt/websocket_upgrade.h>
#endif
#define TEST_TLS_IDENTITY_FIXTURE_ED25519
#include "../../../../tests/fixtures/tls_identity_ec_ed.h"
#if defined(XTP_BENCH_NATIVE)
#define XTP_NATIVE_IO_SIZE 16384
static size_t native_count=10000,native_window=32,native_body=64,native_rate;
static unsigned char native_payload[65536];
#else
#define XTP_NATIVE_IO_SIZE 7
#endif

typedef struct native_test native_test;
typedef struct native_peer {
    native_test* Test;unsigned Index;xtpsession* Xtp;uint64_t Binding;
    xnetstream* Tcp;xtlsstream* Tls;xwsconn* Ws;xtpwsinput WsInput;
    size_t Messages,Completed;bool Started,Flushing,Reading;
    unsigned char Received[512];size_t ReceivedBytes;
#if defined(XTP_TEST_WSS)
    char Head[2048];size_t HeadBytes;
#endif
#if defined(XTP_BENCH_NATIVE)
    size_t Submitted,Offered,Rejected,GeneratorTicks,AllocationCalls,PlaintextBytes,WriteCalls;
    uint64_t NativeSent,NativeReceived,ReadEvents,WriteEvents;
    double GeneratorLagUs;
    double* Begin;double* Latency;
#endif
#if defined(XTP_BENCH_PRODUCTION)
    size_t AllocLive,AllocPeak,AllocBlocks;
    uint64_t TotalPlaintext,TotalWrites;
    uint64_t TotalSubmitted;unsigned char* Payloads;
#endif
} native_peer;
struct native_test {
    unsigned Mode;bool Reject,Failure,Aes;native_peer Peers[2];xatomic32 Done,Failed,Closing;
    xnetengine* Engine;xtlscontext* Context;xtlsidentity* Identity;xtlsverifier* Verifier;
    xtlsserverconfig Server;xtlsstreamconfig TlsConfig;
#if defined(XTP_TEST_WSS)
    bool Wss;unsigned BadUpgrade;
#endif
#if defined(XTP_BENCH_NATIVE)
    double Begin,Seconds;
#endif
#if defined(XTP_BENCH_MULTI)
    xatomic32 Ready;
#endif
#if defined(XTP_BENCH_PRODUCTION)
    xatomic32 Inspected;size_t LiveBytes,LiveBlocks,ReservedPeak;
    uint64_t PairOrdinal;
#endif
};
static uint64_t clock_us(void){return (uint64_t)(xrtTimer()*1000000.0);}
static void drain(native_peer* p)
{
    if(p->Flushing||xrtAtomic32Load(&p->Test->Closing,XMEMORY_ACQUIRE))return;
    p->Flushing=true;
    xtpwriteproc writer=p->Test->Mode==0?xtpTcpWrite:p->Test->Mode==1?xtpTlsWrite:xtpWsWrite;
    void* transport=p->Test->Mode==0?(void*)p->Tcp:p->Test->Mode==1?(void*)p->Tls:(void*)p->Ws;
#if defined(XTP_TEST_WSS)
    if(p->Test->Wss){writer=xtpWsWrite;transport=p->Ws;}
#endif
    xtpwritebudget b={1048576,64,XTP_NATIVE_IO_SIZE};xtpwriteprogress progress;
    xtpresult r=xtpDrain(p->Xtp,1,writer,transport,&b,clock_us(),&progress);
    if(r!=XTP_OK&&r!=XTP_AGAIN)fprintf(stderr,"native drain mode=%u peer=%u result=%d\n",p->Test->Mode,p->Index,(int)r);
    assert(r==XTP_OK||r==XTP_AGAIN);p->Flushing=false;
#if defined(XTP_BENCH_NATIVE)
    p->PlaintextBytes+=progress.Bytes;p->WriteCalls+=progress.Calls;
#if defined(XTP_BENCH_PRODUCTION)
    p->TotalPlaintext+=progress.Bytes;p->TotalWrites+=progress.Calls;
#endif
#endif
}
#if defined(XTP_BENCH_NATIVE)
#if defined(XTP_BENCH_PRODUCTION)
static unsigned char native_pattern(uint64_t ordinal,uint64_t pair,unsigned side,size_t index)
{
    if(index<8)return (unsigned char)(ordinal>>(index*8));
    if(index<16)return (unsigned char)(pair>>((index-8)*8));
    if(index==16)return (unsigned char)side;
    return (unsigned char)((ordinal>>((index%8)*8))^(pair*29)^((uint64_t)side*71)^(index*37));
}
#endif
static void native_submit(native_peer* p,size_t n)
{
    while(n){size_t batch=n>32?32:n;xtpmessage messages[32];xtpsubmitoptions options[32];memset(options,0,sizeof(options));
        for(size_t i=0;i<batch;++i){size_t index=p->Submitted+i;p->Begin[index]=xrtTimer();messages[i]=(xtpmessage){0};
            messages[i].Type=XTP_REQUEST;messages[i].Command=xtpCmdID(1001);messages[i].Body=(xbytesview){native_payload,native_body};options[i].Tag=&p->Begin[index];
#if defined(XTP_BENCH_PRODUCTION)
            unsigned char* payload=p->Payloads+index*native_body;
            for(size_t j=0;j<native_body;++j)payload[j]=native_pattern(p->TotalSubmitted+i+1,p->Test->PairOrdinal,p->Index,j);
            messages[i].Body=(xbytesview){payload,native_body};
#endif
        }
        assert(xtpSubmitBatch(p->Xtp,messages,batch,options,clock_us(),NULL)==XTP_OK);p->Submitted+=batch;n-=batch;
#if defined(XTP_BENCH_PRODUCTION)
        p->TotalSubmitted+=batch;
#endif
    }
}
static void native_fill(native_peer* p)
{
    size_t pending=p->Submitted-p->Completed,n=native_window>pending?native_window-pending:0;
    if(n>native_count-p->Submitted)n=native_count-p->Submitted;
    native_submit(p,n);
}
static void native_done(native_test* t)
{
    for(size_t i=0;i<2;++i)if(t->Peers[i].Completed+t->Peers[i].Rejected!=native_count)return;
    t->Seconds=xrtTimer()-t->Begin;xrtAtomic32Store(&t->Done,1,XMEMORY_RELEASE);
}
#if !defined(XTP_BENCH_MULTI)
static void native_tick(xnetworker* worker,uint64_t id,xnetresult result,void* user)
{
    native_peer* p=user;(void)worker;(void)id;
    if(result!=XNET_RESULT_OK||xrtAtomic32Load(&p->Test->Closing,XMEMORY_ACQUIRE))return;
    ++p->GeneratorTicks;double elapsed=xrtTimer()-p->Test->Begin;
    size_t due=(size_t)(elapsed*native_rate)+1;if(due>native_count)due=native_count;
    if(due>p->Offered){double lag=(elapsed-(double)p->Offered/native_rate)*1e6;
        if(lag>p->GeneratorLagUs)p->GeneratorLagUs=lag;
        size_t offered=due-p->Offered,capacity=native_window-(p->Submitted-p->Completed);
        size_t accepted=offered<capacity?offered:capacity;p->Offered=due;p->Rejected+=offered-accepted;
        native_submit(p,accepted);drain(p);}
    native_done(p->Test);
    if(p->Offered<native_count)assert(xrtNetEngineAfter(p->Test->Engine,0,1,native_tick,p));
}
#endif
#if defined(XTP_BENCH_PRODUCTION)
/* Test-only allocator instrumentation; excluded from ordinary capacity probes. */
typedef union native_allocation {size_t Size;long double Align;void* Pointer;} native_allocation;
static void* native_alloc(void* user,void* memory,size_t size)
{
    native_peer* p=user;bool fresh=memory==NULL;native_allocation* old=memory?(native_allocation*)memory-1:NULL;size_t oldn=old?old->Size:0;
    if(!size){if(old){assert(p->AllocLive>=oldn&&p->AllocBlocks);p->AllocLive-=oldn;--p->AllocBlocks;xrtFree(old);}return NULL;}
    assert(size<=SIZE_MAX-sizeof(native_allocation));++p->AllocationCalls;
    native_allocation* grown=xrtRealloc(old,sizeof(*grown)+size);if(!grown)return NULL;
    grown->Size=size;p->AllocLive=p->AllocLive-oldn+size;if(fresh)++p->AllocBlocks;
    if(p->AllocLive>p->AllocPeak)p->AllocPeak=p->AllocLive;
    return grown+1;
}
#else
static void* native_alloc(void* user,void* memory,size_t size)
{native_peer* p=user;if(size)++p->AllocationCalls;else{xrtFree(memory);return NULL;}return xrtRealloc(memory,size);}
#endif
#endif
static xtpdecision business(xtpsession* s,const xtpmessageview* m,void* user)
{
    native_peer* p=user;++p->Messages;
    assert(m->Command.Kind==XTP_COMMAND_ID&&m->Command.As.ID==1001);
#if defined(XTP_BENCH_PRODUCTION)
    assert(m->Body.Size==native_body);
    for(size_t j=0;j<native_body;++j)assert(m->Body.Data[j]==native_pattern(m->MessageID,p->Test->PairOrdinal,1-p->Index,j));
#endif
    if(m->Type==XTP_REQUEST){xtpresponse response={0};response.Body=m->Body;
#if defined(XTP_PRODUCTION_TEST_CORRUPT)
        unsigned char wrong[64];assert(native_body==sizeof(wrong));memcpy(wrong,m->Body.Data,sizeof(wrong));wrong[0]^=1;response.Body=(xbytesview){wrong,sizeof(wrong)};
#endif
        assert(xtpReply(s,m->Request,&response,clock_us())==XTP_OK);}
    return xtpAccept();
}
static void complete(xtpsession* s,const xtpcompletion* c,void* user)
{
    native_peer* p=user;(void)s;
    if(c->Reason==XTP_FINISH_CLOSED)return;
#if defined(XTP_BENCH_NATIVE)
    assert(c->Reason==XTP_FINISH_RESPONSE&&c->Body.Size==native_body);
#if defined(XTP_BENCH_PRODUCTION)
    uintptr_t tag=(uintptr_t)c->Tag,begin=(uintptr_t)p->Begin;
    assert(tag>=begin&&tag-begin<native_count*sizeof(double)&&(tag-begin)%sizeof(double)==0);
    size_t index=(tag-begin)/sizeof(double);assert(!memcmp(c->Body.Data,p->Payloads+index*native_body,native_body));
#endif
    p->Latency[p->Completed++]=(xrtTimer()-*(double*)c->Tag)*1e6;
    if(!native_rate)native_fill(p);
    native_done(p->Test);
#else
    assert(c->Reason==XTP_FINISH_RESPONSE||c->Reason==XTP_FINISH_ACK||c->Reason==XTP_FINISH_SENT);
    if(c->Reason==XTP_FINISH_RESPONSE)assert(c->Body.Size==5&&!memcmp(c->Body.Data,"hello",5));
    ++p->Completed;if(p->Completed==3&&p->Test->Peers[1-p->Index].Completed==3)xrtAtomic32Store(&p->Test->Done,1,XMEMORY_RELEASE);
#endif
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
    xtpevents e={business,complete,fault};xtpconfig config;xtpConfigInit(&config);
#if defined(XTP_BENCH_NATIVE)
    config.MaxTxBytes=16777216;config.MaxSessionBytes=33554432;config.MaxResponseBytes=131072;
    config.Realloc=native_alloc;config.AllocatorUser=p;
#endif
    assert(xtpCreate(&config,&e,p,&p->Xtp)==XTP_OK);
    xtplane lane={1,p->Test->Mode==2?XTP_INPUT_RECORD:XTP_INPUT_STREAM,true,1048576};
#if defined(XTP_TEST_WSS)
    if(p->Test->Wss)lane.Mode=XTP_INPUT_RECORD;
#endif
    xtpbinding binding={&lane,1,p->Test->Mode==1,p->Test->Mode!=1,NULL};
    assert(xtpBind(p->Xtp,&binding,clock_us(),&p->Binding)==XTP_OK);
    p->WsInput=(xtpwsinput){p->Xtp,p->Binding,1,false};
}
static void start(native_peer* p)
{
    if(p->Started)return;
    p->Started=true;
    if(!p->Xtp)initialize(p);
#if defined(XTP_BENCH_NATIVE)
#if defined(XTP_BENCH_MULTI)
    xrtAtomic32FetchAdd(&p->Test->Ready,1,XMEMORY_RELEASE);
#else
    if(native_rate){
        if(p->Test->Peers[1-p->Index].Started){p->Test->Begin=xrtTimer();
            for(size_t i=0;i<2;++i)assert(xrtNetEngineAfter(p->Test->Engine,0,1,native_tick,&p->Test->Peers[i]));}
    }else {if(!p->Test->Begin)p->Test->Begin=xrtTimer();native_fill(p);drain(p);}
#endif
#else
    xtpmessage m[3];for(size_t i=0;i<3;++i){m[i]=(xtpmessage){0};m[i].Type=i==0?XTP_REQUEST:i==1?XTP_PUSH:XTP_NOTIFY;
        m[i].Command=xtpCmdID(1001);m[i].Body=XRT_BYTES_LITERAL("hello");}
    assert(xtpSubmitBatch(p->Xtp,m,3,NULL,clock_us(),NULL)==XTP_OK);drain(p);
#endif
}
static void input_bytes(native_peer* p,const unsigned char* bytes,size_t n)
{
#if !defined(XTP_BENCH_NATIVE)
    assert(n<=sizeof(p->Received)-p->ReceivedBytes);memcpy(p->Received+p->ReceivedBytes,bytes,n);p->ReceivedBytes+=n;
#endif
    size_t offset=0;while(offset<n){xtpreadprogress progress;size_t take=n-offset;if(take>XTP_NATIVE_IO_SIZE)take=XTP_NATIVE_IO_SIZE;
        bool record=p->Test->Mode==2;
#if defined(XTP_TEST_WSS)
        record=record||p->Test->Wss;
#endif
        xtpresult r=record?xtpWsData(&p->WsInput,(xbytesview){bytes+offset,take},clock_us(),&progress):
            xtpInput(p->Xtp,p->Binding,1,(xbytesview){bytes+offset,take},false,clock_us(),&progress);
        assert(r>=0&&r!=XTP_PAUSED&&r!=XTP_CLOSED&&progress.Used<=take);offset+=progress.Used;
        if(!progress.Used){uint64_t due;assert(r==XTP_AGAIN&&xtpDrive(p->Xtp,clock_us(),&due)>=0);}}
}
static void tcp_read(xnetstream* stream,xnetbuf* buffer,void* user)
{
    native_peer* p=user;(void)buffer;if(p->Reading)return;p->Reading=true;
    /* Read/Consume can drive another native Read on readiness backends.
     * Keep the copied prefix ahead of those later bytes. */
    unsigned char bytes[XTP_NATIVE_IO_SIZE];size_t n;while((n=xrtNetStreamRead(stream,bytes,sizeof(bytes)))!=0)input_bytes(p,bytes,n);
    p->Reading=false;drain(p);
}
static void tcp_drain(xnetstream* s,void* user){(void)s;native_peer* p=user;if(p->Xtp)drain(p);}
#if defined(XTP_TEST_WSS)
static void upgrade_read(native_peer* p);
#endif
static void tls_read(xtlsstream* s,const xnetbuf* buffer,void* user)
{native_peer* p=user;(void)buffer;if(p->Reading)return;p->Reading=true;
#if defined(XTP_TEST_WSS)
    if(p->Test->Wss){upgrade_read(p);p->Reading=false;return;}
#endif
    unsigned char bytes[XTP_NATIVE_IO_SIZE];size_t n;while(xrtTlsStreamRead(s,bytes,sizeof(bytes),&n)==XTLS_OK&&n)input_bytes(p,bytes,n);
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
#if defined(XTP_TEST_WSS)
/* Test-only HTTP frontend: read exactly through CRLFCRLF so AttachTls owns
 * every remaining plaintext byte, even when the first WS frame is coalesced. */
static void upgrade_send(native_peer* p,const char* bytes,size_t n)
{size_t written=0;assert(xrtTlsStreamSend(p->Tls,bytes,n,&written)==XTLS_OK&&written==n);}
static void upgrade_read(native_peer* p)
{
    size_t n;
    while(p->HeadBytes<sizeof(p->Head)&&xrtTlsStreamRead(p->Tls,p->Head+p->HeadBytes,1,&n)==XTLS_OK&&n){
        ++p->HeadBytes;
        if(p->HeadBytes<4||memcmp(p->Head+p->HeadBytes-4,"\r\n\r\n",4))continue;
        xhttpfield fields[16];xhttp1head head;xrtHttp1HeadInit(&head,fields,16);
        xbytesview bytes={(const unsigned char*)p->Head,p->HeadBytes};xwsupgrade upgrade;bool valid;
        static const xstrview key=XRT_STR_INIT("dGhlIHNhbXBsZSBub25jZQ==");
        if(p->Index){xwsupgradeserverconfig c;xrtWsUpgradeServerConfigInit(&c);c.Protocols=XRT_STR_LITERAL("xtp.v3");
            valid=xrtHttp1RequestParse(bytes,&head,NULL,NULL)==XHTTP1_READY&&xrtWsUpgradeRequestCheck(&head,&c,&upgrade);
        }else{xwsupgradeclientconfig c;xrtWsUpgradeClientConfigInit(&c);c.Protocols=XRT_STR_LITERAL("xtp.v3");
            valid=xrtHttp1ResponseParse(bytes,&head,NULL,NULL)==XHTTP1_READY&&xrtWsUpgradeResponseCheck(&head,key,&c,&upgrade);}
        valid=valid&&upgrade.Protocol.Size==6&&!memcmp(upgrade.Protocol.Data,"xtp.v3",6);
        if(!valid){assert(p->Test->BadUpgrade);xrtAtomic32Store(&p->Test->Failed,1,XMEMORY_RELEASE);return;}
        if(p->Index){char response[512];int length=snprintf(response,sizeof(response),
            "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n\r\n",upgrade.Accept,p->Test->BadUpgrade==2?"xtp.v2":"xtp.v3");
            assert(length>0&&(size_t)length<sizeof(response));upgrade_send(p,response,(size_t)length);
            if(p->Test->BadUpgrade)return;}
        initialize(p);xwsconnconfig c;xrtWsConnConfigInit(&c);c.Role=p->Index?XWS_ROLE_SERVER:XWS_ROLE_CLIENT;
        c.Protocol=upgrade.Protocol;c.ControlReserve=1024;c.SendLimit=2097152;
        p->Ws=xrtWsConnAttachTls(p->Tls,&c,&ws_events,p);assert(p->Ws);start(p);return;
    }
    assert(p->HeadBytes<sizeof(p->Head));
}
#endif
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
    assert(xrtTlsSessionCipher(xrtTlsStreamSession(s))==(p->Test->Aes?XTLS_AES_128_GCM_SHA256:XTLS_CHACHA20_POLY1305_SHA256));
#if defined(XTP_TEST_WSS)
    if(p->Test->Wss){if(!p->Index){char request[512];int n=snprintf(request,sizeof(request),
            "GET /xtp HTTP/1.1\r\nHost: xtp.fixture\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Protocol: %s\r\n\r\n",p->Test->BadUpgrade==1?"xtp.v2":"xtp.v3");
            assert(n>0&&(size_t)n<sizeof(request));upgrade_send(p,request,(size_t)n);}return;}
#endif
    start(p);}
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
#if defined(XTP_TEST_WSS)
    static const xstrview http[]={XRT_STR_INIT("http/1.1")};if(t->Wss)t->Server.Protocols=http;
#endif
}
static void run(unsigned mode,bool reject)
{
    native_test t;memset(&t,0,sizeof(t));t.Mode=mode>2?1:mode;t.Reject=reject;t.Failure=reject||mode==3;t.Aes=mode==4;
#if defined(XTP_TEST_WSS)
    t.Wss=mode>=5;t.BadUpgrade=mode>=6?mode-5:0;t.Failure=t.Failure||t.BadUpgrade;
#endif
    for(unsigned i=0;i<2;++i){t.Peers[i].Test=&t;t.Peers[i].Index=i;}
#if defined(XTP_BENCH_NATIVE)
    for(size_t i=0;i<2;++i){t.Peers[i].Begin=calloc(native_count,sizeof(double));t.Peers[i].Latency=calloc(native_count,sizeof(double));assert(t.Peers[i].Begin&&t.Peers[i].Latency);}
#endif
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
#if defined(XTP_TEST_WSS)
        static const xstrview http[]={XRT_STR_INIT("http/1.1")};if(t.Wss)client.Protocols=http;
#endif
        client_owner=xrtTlsStreamConnect(t.Engine,&address,0,&c.Stream,&client,&t.TlsConfig,&tls_events,&t.Peers[0]);assert(client_owner);
    }else {xnetstream* stream=xrtNetStreamConnect(t.Engine,&address,0,&c.Stream,&tcp_events,&t.Peers[0]);assert(stream);}
    double deadline=xrtTimer()+60;while(!xrtAtomic32Load(t.Failure?&t.Failed:&t.Done,XMEMORY_ACQUIRE)){assert(xrtTimer()<deadline);xrtSleep(1);}
    xrtAtomic32Store(&t.Closing,1,XMEMORY_RELEASE);
#if defined(XTP_BENCH_NATIVE)
    /* Snapshot both live transports before shutdown; do not add an alias
     * reference that would itself delay the native retirement barrier. */
    for(size_t i=0;i<2;++i){native_peer* p=&t.Peers[i];xnetstreamstats io;
        assert(xrtNetStreamStats(t.Mode==1?xrtTlsStreamTransport(i?p->Tls:client_owner):p->Tcp,&io));
        p->NativeSent=io.SentBytes;p->NativeReceived=io.ReceivedBytes;p->ReadEvents=io.ReadEvents;p->WriteEvents=io.WriteEvents;}
#endif
    for(unsigned i=0;i<2;++i){native_peer* p=&t.Peers[i];
#if defined(XTP_TEST_WSS)
        if(p->Ws&&t.Wss){(void)xrtWsConnAbort(p->Ws);xrtWsConnDestroy(p->Ws);continue;}
#endif
        if(t.Mode==1){xtlsstream* tls=i?p->Tls:client_owner;if(tls){(void)xrtTlsStreamAbort(tls);assert(xrtTlsStreamWaitRetired(tls,10000)==XWAIT_OK);xrtTlsStreamDestroy(tls);}}
        else {if(p->Ws)(void)xrtWsConnAbort(p->Ws);else assert(xrtNetStreamClose(p->Tcp));
            assert(xrtNetStreamWaitRetired(p->Tcp,10000)==XWAIT_OK);if(p->Ws)xrtWsConnDestroy(p->Ws);xrtNetStreamDestroy(p->Tcp);}
#if defined(XTP_BENCH_NATIVE)
        if(p->Xtp){
            xtpinfo state;assert(xtpInfo(p->Xtp,&state)==XTP_OK);
            native_report(t.Mode,p,t.Seconds,state.PeakReservedBytes);}
#endif
    }
    assert(xrtNetListenerClose(listener));xrtNetListenerDestroy(listener);
#if defined(XTP_BENCH_NATIVE)
    xnetenginestats stats;assert(xrtNetEngineStats(t.Engine,&stats));
    printf("engine_events: %llu\nengine_timers: %llu\n",(unsigned long long)stats.Events,(unsigned long long)stats.TimersFired);
#endif
    deadline=xrtTimer()+10;xnetretireresult retired;while((retired=xrtNetEngineTryDestroy(t.Engine))==XNET_RETIRE_BUSY){assert(xrtTimer()<deadline);xrtSleep(1);}assert(retired==XNET_RETIRE_READY);
    for(size_t i=0;i<2;++i){native_peer* p=&t.Peers[i];if(p->Xtp){xtpresult r=xtpClose(p->Xtp,clock_us());
        while(r==XTP_AGAIN){uint64_t due;r=xtpDrive(p->Xtp,clock_us(),&due);}assert(r==XTP_CLOSED&&xtpDestroy(p->Xtp)==XTP_OK);}}
    if(t.Mode==1){xrtTlsVerifierRelease(t.Verifier);xrtTlsIdentityRelease(t.Identity);xrtTlsContextRelease(t.Context);if(legacy_context)xrtTlsContextRelease(legacy_context);}
#if defined(XTP_BENCH_NATIVE)
    assert(t.Peers[0].Messages==t.Peers[1].Submitted&&t.Peers[1].Messages==t.Peers[0].Submitted);
    for(size_t i=0;i<2;++i){free(t.Peers[i].Begin);free(t.Peers[i].Latency);}
#else
    if(!t.Failure)assert(t.Peers[0].Messages==3&&t.Peers[1].Messages==3);else assert(!t.Peers[0].Messages&&!t.Peers[1].Messages);
#endif
}
