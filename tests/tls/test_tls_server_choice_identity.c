/* Ordinary fixed-certificate in-memory handshakes and ownership/OOM tests.
 * No sockets, public endpoints, malformed records or crypto security tests. */
#if defined(XRT_CHOICE_XLANG_CORE)
#include "release/include/xrt_decl.h"
#elif defined(XRT_CHOICE_XLANG_RUNTIME)
#include "release/include/xruntime_decl.h"
#elif defined(XRT_CHOICE_SINGLE)
#include "single/xrt_decl.h"
#else
#include <xrt.h>
#endif
#include "../fixtures/tls_server.h"
#include <assert.h>

typedef struct selection_state {
    xtlsidentity* signing;
    unsigned created,released,calls,mode;
    const xerror* primary;
} selection_state;
static void released(ptr context){((selection_state*)context)->released++;}
static bool sign(ptr context,xtlsversion version,xtlssignature signature,
    xbytesview message,void* output,size_t capacity,size_t* size)
{
    return xrtTlsIdentitySign(((selection_state*)context)->signing,
        version,signature,message,output,capacity,size);
}
static xtlsidentity* identity(selection_state* state)
{
    xbytesview certificate={X509_LEGACY_RSA_CERT,sizeof(X509_LEGACY_RSA_CERT)};
    xtlsidentityconfig config={0};config.Certificates=&certificate;config.CertificateCount=1;
    config.Type=XTLS_IDENTITY_RSA;config.Sign=sign;config.Release=released;config.Context=state;
    xtlsidentity* result=xrtTlsIdentityCreate(&config);if(result)state->created++;
    return result;
}
static bool selectIdentity(ptr context,const xtlsserverrequest* request,xtlsserverchoice* choice)
{
    selection_state* state=context;state->calls++;
    assert(request&&choice&&request->ServerName.Size==11);
    assert(!memcmp(request->ServerName.Data,"example.com",11));
    assert(xrtTlsProtocolFind(request->Protocols,XRT_BYTES_LITERAL("http/1.1"))==XTLS_ITEM_VALUE);
    choice->Cookie=UINT64_C(0xD15EA5E5C001C0DE);choice->Protocol=0;
    if(!state->mode)return true; /* Existing borrowed/static contract. */
    for(unsigned i=0;i<2;++i){
        xtlsidentity* temporary=identity(state);if(!temporary)return false;
        bool ok=xrtTlsServerChoiceSetIdentityV1(choice,temporary);
        xrtTlsIdentityRelease(temporary); /* Nothing outside Choice owns it. */
        if(!ok)return false;
        assert(state->released==state->created-1);
    }
    if(state->mode==2||state->mode==4){
        xerrordesc error={0};error.Kind=XERR_CANCELLED;error.Domain="test.selector";
        error.Code=190172;error.Message="ordinary checked callback failure";
        xrtSetErrorTake(xrtErrorBuild(&error));state->primary=xrtGetError();assert(state->primary);
    }
    return state->mode!=2&&state->mode!=3;
}
static bool handshake(xtlssession* client,xtlssession* server,test_tls_server_rng* rng)
{
    for(unsigned i=0;i<8192;++i){
        xtlsresult left=xrtTlsClientDrive(client),right=xrtTlsServerDrive(server);
        if((left!=XTLS_OK&&left!=XTLS_AGAIN)||(right!=XTLS_OK&&right!=XTLS_AGAIN))return false;
        bool progress=false;
        if(!testTlsServerMove(client,server,rng,37,&progress)||
            !testTlsServerMove(server,client,rng,53,&progress))return false;
        if(xrtTlsSessionState(client)==XTLS_STATE_READY&&xrtTlsSessionState(server)==XTLS_STATE_READY)return true;
    }
    assert(false);return false;
}
static void balanced(const xmemdebugsnapshot* before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    assert(before->LiveCount==after.LiveCount&&before->LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before->AllocCount==after.FreeCount-before->FreeCount);
    assert(before->InvalidFreeCount==after.InvalidFreeCount&&before->DoubleFreeCount==after.DoubleFreeCount&&before->UseAfterFreeCount==after.UseAfterFreeCount);
}
static void exercise(unsigned mode,size_t prefix,bool fault)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    selection_state state={0};state.mode=mode;state.signing=testTlsServerIdentity();assert(state.signing);
    xtlscontext* context=testTlsServerContext();assert(context);
    xtlsverifierconfig verification;xrtTlsVerifierConfigInit(&verification);verification.Verify=testTlsServerAccept;
    xtlsverifier* verifier=xrtTlsVerifierCreate(&verification);assert(verifier);
    static const xstrview protocols[]={XRT_STR_INIT("http/1.1")};
    xtlsclientconfig cc;xrtTlsClientConfigInit(&cc);cc.Context=context;cc.ServerName=XRT_STR_LITERAL("example.com");
    cc.Protocols=protocols;cc.ProtocolCount=1;cc.Verifier=verifier;
    xtlsserverconfig sc;xrtTlsServerConfigInit(&sc);sc.Context=context;
    sc.Identity=mode?NULL:state.signing;sc.Protocols=protocols;sc.ProtocolCount=1;
    sc.RequireProtocol=true;sc.Select=selectIdentity;sc.SelectContext=&state;
    xtlssession* client=xrtTlsClientCreate(&cc,NULL);xtlssession* server=xrtTlsServerCreate(&sc,NULL);assert(client&&server);
    if(fault)assert(xrtMemDebugFailAfter(prefix));
    test_tls_server_rng rng={UINT32_C(0x12345678)};bool ok=handshake(client,server,&rng);
    bool hit=fault&&xrtMemDebugFailTriggered();if(fault)xrtMemDebugFailClear();
    if(fault&&!prefix)assert(hit);
    if(hit){assert(!ok&&xrtGetError());}
    else if(mode<2){
        assert(ok&&state.calls==1&&state.released==state.created-(mode?1:0));
        uint64 cookie=0;assert(xrtTlsServerCookie(server,&cookie)&&cookie==UINT64_C(0xD15EA5E5C001C0DE));
        const char data[]="ordinary application data";
        assert(testTlsServerTransfer(client,server,true,data,sizeof(data),&rng));
    }else{
        assert(!ok&&state.calls==1&&state.released==state.created&&xrtGetError());
        if(mode==2||mode==4){
            assert(xrtGetError()==state.primary);
            assert(xrtErrorKind(xrtGetError())==XERR_CANCELLED&&xrtErrorCode(xrtGetError())==190172);
        }
    }
    xrtClearError();xrtTlsSessionDestroy(server);xrtTlsSessionDestroy(client);
    assert(state.created==state.released);
    xrtTlsVerifierRelease(verifier);xrtTlsContextRelease(context);xrtTlsIdentityRelease(state.signing);
    assert(!xrtGetError());balanced(&before);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));
    assert(!xrtTlsServerChoiceSetIdentityV1(NULL,NULL)&&xrtGetError());xrtClearError();
    xtlsserverchoice choice={0};
    assert(!xrtTlsServerChoiceSetIdentityV1(&choice,NULL)&&!choice.Identity&&!choice.IdentityOwnerV1&&xrtGetError());xrtClearError();
    xrtSetErrorKind(XERR_STATE);const xerror* primary=xrtGetError();
    assert(!xrtTlsServerChoiceSetIdentityV1(NULL,NULL)&&xrtGetError()==primary);xrtClearError();
    for(unsigned mode=0;mode<5;++mode)exercise(mode,0,false);
    for(size_t point=0;point<64;++point)exercise(1,point,true);
    puts("TLS selector identity: borrowed and temporary results; replacement/rejection/checked errors; 64 OOM prefixes balanced");
    return 0;
}
