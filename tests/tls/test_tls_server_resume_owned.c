/* Ordinary valid, fixed-material in-memory TLS handshakes and lifetime/OOM.
 * No sockets, malformed records, external endpoints or crypto security tests. */
#if defined(XRT_RESUME_OWNED_XLANG_CORE)
#include "release/include/xrt_decl.h"
#elif defined(XRT_RESUME_OWNED_XLANG_RUNTIME)
#include "release/include/xruntime_decl.h"
#elif defined(XRT_RESUME_OWNED_SINGLE)
#include "single/xrt_decl.h"
#else
#include <xrt.h>
#endif
#include "../fixtures/tls_server.h"
#include <assert.h>

typedef struct lookup {xtlsresume* seed;unsigned mode,calls;const xerror* primary;} lookup;
static const xstrview protocols[]={XRT_STR_INIT("http/1.1")};
static void error(lookup* p)
{
    xerrordesc d={0};d.Kind=XERR_CANCELLED;d.Domain="test.resume.owned";
    d.Code=190173;d.Message="ordinary resume lookup failure";
    xrtSetErrorTake(xrtErrorBuild(&d));p->primary=xrtGetError();assert(p->primary);
}
static const xtlsresume* borrowed(ptr raw,const xtlsserverresumerequest* request)
{
    lookup* p=raw;assert(request);++p->calls;
    if(p->mode==6){error(p);return NULL;}
    return p->seed;
}
static bool owned(ptr raw,const xtlsserverresumerequest* request,xtlsresume** output)
{
    lookup* p=raw;assert(request&&output&&!*output);++p->calls;
    xtlsresumeinfo i;assert(xrtTlsResumeInfo(p->seed,&i));
    assert(testTlsServerViewEqual(request->Ticket,i.Ticket));
    assert(testTlsServerViewEqual(request->ServerName,(xbytesview){(const uint8*)"example.com",11}));
    if(p->mode==2)return true; /* Explicit cache miss: full certificate flight. */
    if(p->mode==3)return false; /* Failed lookup, no diagnostic or result. */
    xtlsresumeconfig c={0};c.Version=i.Version;c.Cipher=i.Cipher;c.Ticket=i.Ticket;c.Secret=i.Secret;
    c.ServerName=i.ServerName;c.Protocol=i.Protocol;c.PeerIdentity=i.PeerIdentity;c.Lifetime=i.Lifetime;
    c.AgeAdd=i.AgeAdd;c.MaxEarlyData=i.MaxEarlyData;c.IssuedAt=i.IssuedAt;
    *output=xrtTlsResumeCreate(&c);if(!*output)return false;
    if(p->mode==4||p->mode==5)error(p);
    return p->mode!=4&&p->mode!=7;
}
static xtlsresume* seed(void)
{
    static const uint8 ticket[32]={1,0,3,4,5};static const uint8 secret[32]={7,6,5,0,3,2,1};
    xtlsresumeconfig c;xrtTlsResumeConfigInit(&c);c.Cipher=XTLS_AES_128_GCM_SHA256;
    c.Ticket=(xbytesview){ticket,sizeof(ticket)};c.Secret=(xbytesview){secret,sizeof(secret)};
    c.ServerName=XRT_STR_LITERAL("example.com");c.Protocol=(xbytesview){(const uint8*)"http/1.1",8};
    c.Lifetime=3600;c.AgeAdd=UINT32_C(0x31415926);return xrtTlsResumeCreate(&c);
}
static bool handshake(xtlssession* client,xtlssession* server,test_tls_server_rng* rng)
{
    for(unsigned i=0;i<8192;++i){
        xtlsresult a=xrtTlsClientDrive(client),b=xrtTlsServerDrive(server);
        if((a!=XTLS_OK&&a!=XTLS_AGAIN)||(b!=XTLS_OK&&b!=XTLS_AGAIN))return false;
        bool progress=false;
        if(!testTlsServerMove(client,server,rng,37,&progress)||!testTlsServerMove(server,client,rng,53,&progress))return false;
        if(xrtTlsSessionState(client)==XTLS_STATE_READY&&xrtTlsSessionState(server)==XTLS_STATE_READY)return true;
    }
    assert(0);return false;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount);
}
static void exercise(unsigned mode,size_t prefix,bool fault)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);lookup state={0};state.mode=mode;
    state.seed=seed();assert(state.seed);
    xtlscontext* context=testTlsServerContext();xtlsidentity* identity=testTlsServerIdentity();assert(context&&identity);
    xtlsverifierconfig vc;xrtTlsVerifierConfigInit(&vc);vc.Verify=testTlsServerAccept;
    xtlsverifier* verifier=xrtTlsVerifierCreate(&vc);assert(verifier);
    xtlsclientconfig cc;xrtTlsClientConfigInit(&cc);cc.Context=context;cc.Verifier=verifier;cc.Resume=state.seed;
    cc.ServerName=XRT_STR_LITERAL("example.com");cc.Protocols=protocols;cc.ProtocolCount=1;
    xtlsserverconfig sc;xrtTlsServerConfigInit(&sc);sc.Context=context;sc.Identity=identity;
    sc.Protocols=protocols;sc.ProtocolCount=1;sc.RequireProtocol=true;sc.ResumeContext=&state;
    if(mode==0||mode==6)sc.Resume=borrowed;else sc.ResumeOwnedV1=owned;
    xtlssession* client=xrtTlsClientCreate(&cc,NULL);xtlssession* server=xrtTlsServerCreate(&sc,NULL);assert(client&&server);
    if(fault)assert(xrtMemDebugFailAfter(prefix));
    test_tls_server_rng rng={UINT32_C(0x53AC2081)};bool ok=handshake(client,server,&rng);
    bool hit=fault&&xrtMemDebugFailTriggered();if(fault)xrtMemDebugFailClear();
    if(fault&&!prefix)assert(hit);
    if(hit)assert(!ok);
    else if(mode<3){
        assert(ok&&state.calls==1);
        assert(xrtTlsServerResumed(server)==(mode!=2)&&xrtTlsClientResumed(client)==(mode!=2));
        const uint8 text[]={0,1,2,0,255};assert(testTlsServerTransfer(client,server,true,text,sizeof(text),&rng));
    }else{
        assert(!ok&&state.calls==1&&xrtTlsSessionState(server)==XTLS_STATE_FAILED);
        /* The lookup preserves an absent diagnostic through its rollback.
         * The existing Session boundary then supplies its documented generic
         * TLS root error. It must still fail, never become a cache miss. */
        if(mode==3||mode==7)assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_INTERNAL&&
            xrtErrorCode(xrtGetError())==XTLS_ERROR_INTERNAL&&!strcmp(xrtErrorOperation(xrtGetError()),"fail-tls-session"));
        else assert(xrtGetError()==state.primary&&xrtErrorCode(xrtGetError())==190173);
    }
    xrtClearError();xrtTlsSessionDestroy(server);xrtTlsSessionDestroy(client);
    xrtTlsVerifierRelease(verifier);xrtTlsIdentityRelease(identity);xrtTlsContextRelease(context);xrtTlsResumeRelease(state.seed);
    assert(!xrtGetError());balanced(&before);
}
static void exclusive(void)
{
    xtlsserverconfig c;xrtTlsServerConfigInit(&c);assert(!c.Resume&&!c.ResumeOwnedV1);
    c.Resume=borrowed;c.ResumeOwnedV1=owned;
    assert(!xrtTlsServerCreate(&c,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();exclusive();
    for(unsigned mode=0;mode<8;++mode)exercise(mode,0,false);
    for(size_t point=0;point<96;++point)exercise(1,point,true);
    puts("TLS owned resume: borrowed/temporary results, miss, diagnostic/no-diagnostic failures, true-with-error, 96 OOM prefixes balanced");return 0;
}
