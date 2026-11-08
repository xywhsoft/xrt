/* Ordinary offline construction and physical ownership; no socket/handshake,
 * altered certificate, trust bypass or network/security campaign. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"
#include "../fixtures/tls_identity_legacy.h"
/* Actual private base constructor, not a replacement provider. The public
 * role factories normally own a tail; this fixture also checks its absence. */
extern xtlssession* __xrtTlsSessionCreate(const xtlscontext*,xnetbufpool*,xtlsrole);
typedef struct verifier_context { volatile int32 Refs; unsigned Drops; } verifier_context;
static bool contextCount(const void* data,size_t* count)
{const verifier_context* c=data;if(!c||!count||c->Refs<=0)return false;*count=(size_t)c->Refs;return true;}
static bool contextTrace(const void* data,xrtownershipvisitor visit,ptr context)
{(void)context;return data&&visit;}
static const xrtownershipops ContextOps={contextCount,contextTrace};
static void contextDrop(ptr data)
{
    verifier_context* c=data;xrtownershipscope mutation={0};
    testRequire(xrtOwnershipMutationBegin(&mutation),"context drop mutation");
    testRequire(c->Refs==1,"one actual verifier context slot");c->Refs=0;c->Drops++;
    testRequire(xrtOwnershipScopeEnd(&mutation),"context drop end");
}
static const xtlsverifiercontextownershipv1 ContextPolicy={sizeof(ContextPolicy),NULL,NULL,NULL,contextDrop,&ContextOps};
typedef struct edges { const xtlsverifier* Verifier; unsigned Matches,Others; } edges;
static bool visit(xrtownershipref ref,ptr raw)
{
    edges* output=raw;
    testRequire(ref.Data&&ref.Ops&&ref.Ops->Count&&ref.Ops->Trace,"well-formed physical child");
    if(ref.Data==output->Verifier){
        testRequire(ref.Ops==xrtTlsVerifierOwnership(output->Verifier).Ops,"exact verifier identity");output->Matches++;
    }else output->Others++;
    return true;
}
static size_t references(const xtlsverifier* verifier)
{
    xrtownershipref ref=xrtTlsVerifierOwnership(verifier);size_t count=0;xrtownershipscope freeze={0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze)&&ref.Ops->Count(ref.Data,&count),"stable verifier reference count");
    testRequire(xrtOwnershipScopeEnd(&freeze),"reference count freeze end");return count;
}
static void traceSession(xtlssession* session,const xtlsverifier* verifier,unsigned expected)
{
    xrtownershipscope freeze={0};edges output={verifier,0,0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"session snapshot freeze");
    testRequire(xrtTlsSessionBufferOwnershipTrace(session,visit,&output)&&!output.Matches,"buffer-only API keeps its scope");
    output=(edges){verifier,0,0};
    testRequire(xrtTlsSessionOwnershipTrace(session,visit,&output)&&output.Matches==expected,"actual session verifier slot exactly once");
    testRequire(xrtOwnershipScopeEnd(&freeze),"session snapshot end");
}
static void balanced(xmemdebugsnapshot before)
{
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes&&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount&&
        before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount&&
        before.UseAfterFreeCount==after.UseAfterFreeCount,"exact allocation/ownership rollback");
}
typedef struct signer { xtlsidentity* Key;unsigned Signs,Drops; } signer;
static bool canSign(ptr raw,xtlsversion version,xtlssignature scheme)
{return xrtTlsIdentityCanSign(((signer*)raw)->Key,version,scheme);}
static bool signMessage(ptr raw,xtlsversion version,xtlssignature scheme,xbytesview message,
    void* output,size_t capacity,size_t* size)
{signer* s=raw;s->Signs++;return xrtTlsIdentitySign(s->Key,version,scheme,message,output,capacity,size);}
static void signerDrop(ptr raw)
{signer* s=raw;s->Drops++;xrtTlsIdentityRelease(s->Key);s->Key=NULL;}
static void signerBoundary(void)
{
    /* A real forwarding signer, not a fake signature provider. No signing or
     * handshake is performed; only construction, graph admission and release. */
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    uint8 material[2048];size_t length=0;
    testRequire(testTlsIdentityLegacyKey(material,sizeof(material),&length),"ordinary fixed RSA material");
    xbytesview certificate={X509_LEGACY_RSA_CERT,sizeof(X509_LEGACY_RSA_CERT)};
    xtlsidentity* key=xrtTlsIdentityRsa(&certificate,1,(xbytesview){material,length});
    xrtSecureZero(material,sizeof(material));testRequire(key!=NULL,"actual built-in identity");
    xtlsserverconfig config;xrtTlsServerConfigInit(&config);config.Identity=key;
    xtlssession* session=xrtTlsServerCreate(&config,NULL);testRequire(session!=NULL,"ordinary built-in server session");
    traceSession(session,NULL,0);xrtTlsSessionDestroy(session);
    signer state={key,0,0};xtlsidentityconfig external={0};external.Certificates=&certificate;external.CertificateCount=1;
    external.Type=xrtTlsIdentityType(key);external.Supports=canSign;external.Sign=signMessage;
    external.Release=signerDrop;external.Context=&state;
    xtlsidentity* identity=xrtTlsIdentityCreate(&external);testRequire(identity!=NULL,"real forwarding external identity");
    config.Identity=identity;session=xrtTlsServerCreate(&config,NULL);testRequire(session!=NULL,"external signer normal C construction");
    xrtownershipscope freeze={0};edges output={0};
    testRequire(xrtOwnershipFreezeTryBegin(&freeze),"external signer graph freeze");
    testRequire(!xrtTlsSessionOwnershipTrace(session,visit,&output)&&!xrtGetError(),"opaque owning signer is not a leaf");
    testRequire(xrtOwnershipScopeEnd(&freeze)&&state.Signs==0&&!state.Drops,"graph walk executes no signer or cleanup");
    xrtTlsIdentityRelease(identity);testRequire(!state.Drops,"session retains actual external identity");
    xrtTlsSessionDestroy(session);testRequire(state.Drops==1&&!state.Key&&!state.Signs,"external signer released exactly once without signing");
    balanced(before);
}
int main(void)
{
    xmemdebugsnapshot baseline;xrtMemDebugSnapshot(&baseline);
    xx509store* store=xrtX509StoreCreate();
    testRequire(store&&xrtX509StoreAdd(store,X509_PATH_ROOT,sizeof(X509_PATH_ROOT))==X509_VALUE,"normal immutable trust source");
    verifier_context context={1,0};xtlsverifierconfig verify;xrtTlsVerifierConfigInit(&verify);
    verify.Store=store;verify.Context=&context;verify.Release=contextDrop;
    xtlsverifylimits limits;xrtTlsVerifyLimitsInit(&limits);
    xtlsverifier* verifier=xrtTlsVerifierCreateOwnedV1(&verify,&limits,&ContextPolicy);
    testRequire(verifier!=NULL,"owned normal trust verifier");xrtX509StoreFree(store);
    xtlsclientconfig client;xrtTlsClientConfigInit(&client);
    client.Verifier=verifier;client.ServerName=XRT_STR_LITERAL("sha1-root.test.xrt");
    xtlssession* session=xrtTlsClientCreate(&client,NULL);
    if(!session&&xrtGetError())fprintf(stderr,"client creation: %s: %s\n",xrtErrorOperation(xrtGetError()),xrtErrorMessage(xrtGetError()));
    testRequire(session&&references(verifier)==2,"client owns a separate native verifier reference");
    traceSession(session,verifier,1);xrtTlsSessionDestroy(session);
    testRequire(references(verifier)==1&&!context.Drops,"session alone consumes only its own reference");
    unsigned failed=0;bool complete=false;
    for(size_t budget=0;budget<256&&!complete;budget++){
        xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
        testRequire(xrtMemDebugFailAfter(budget),"arm client construction allocation prefix");
        session=xrtTlsClientCreate(&client,NULL);bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
        if(hit){testRequire(!session&&xrtGetError(),"failed client is not published");failed++;}
        else{testRequire(session&&!xrtGetError(),"complete constructor allocation prefix");traceSession(session,verifier,1);complete=true;}
        xrtClearError();xrtTlsSessionDestroy(session);
        testRequire(references(verifier)==1&&!context.Drops,"client rollback releases exactly its verifier retain");
        balanced(before);
    }
    testRequire(complete&&failed,"complete client construction prefix sweep");
    session=xrtTlsClientCreate(&client,NULL);testRequire(session!=NULL,"last-consumer client");
    xrtTlsVerifierRelease(verifier);testRequire(references(verifier)==1&&!context.Drops,"session outlives creator owner");
    traceSession(session,verifier,1);xrtTlsSessionDestroy(session);
    testRequire(context.Drops==1&&context.Refs==0,"native session releases last context owner once");
    xtlscontext* parameters=xrtTlsContextCreate(NULL);testRequire(parameters!=NULL,"base session parameters");
    session=__xrtTlsSessionCreate(parameters,NULL,XTLS_CLIENT);xrtTlsContextRelease(parameters);
    testRequire(session!=NULL,"base client without role tail");
    traceSession(session,NULL,0);xrtTlsSessionDestroy(session);
    signerBoundary();xrtClearError();balanced(baseline);
    printf("TLS session verifier edge: exact native slot, creator/session lifetimes, %u constructor allocation failures, base session without tail; memory balanced\n",failed);
    return 0;
}
