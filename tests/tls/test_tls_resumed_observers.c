/* Ordinary role creation/observation only: no transport or handshake input. */
#if defined(XRT_OBSERVER_XLANG_CORE)
    #include "release/include/xrt_decl.h"
#elif defined(XRT_OBSERVER_XLANG_RUNTIME)
    #include "release/include/xruntime_decl.h"
#elif defined(XRT_OBSERVER_SINGLE)
    #include "single/xrt_decl.h"
#else
    #include <xrt.h>
#endif
#include <assert.h>
#include <stdio.h>

static int decisions;
static xtlsverifydecision verify(const xtlspeer* peer,ptr data)
{(void)peer;(void)data;decisions++;return XTLS_VERIFY_ACCEPT;}
static bool selectIdentity(ptr data,const xtlsserverrequest* request,xtlsserverchoice* choice)
{(void)data;(void)request;(void)choice;decisions++;return false;}
static void exercise(void)
{
    static const xtlsversion versions[]={XTLS_VERSION_13};
    static const xtlscipher ciphers[]={XTLS_AES_128_GCM_SHA256};
    static const uint16 groups[]={XTLS_GROUP_X25519};
    static const xtlssignature signatures[]={XTLS_SIGNATURE_ED25519};
    xtlspolicy policy;xrtTlsPolicyInit(&policy);
    policy.Versions=versions;policy.VersionCount=1;
    policy.Ciphers=ciphers;policy.CipherCount=1;
    policy.Groups=groups;policy.GroupCount=1;
    policy.Signatures=signatures;policy.SignatureCount=1;
    xtlscontextconfig config;xrtTlsContextConfigInit(&config);config.Policy=&policy;
    xtlscontext* context=xrtTlsContextCreate(&config);assert(context);
    xtlsverifierconfig validation;xrtTlsVerifierConfigInit(&validation);validation.Verify=verify;
    xtlsverifier* verifier=xrtTlsVerifierCreate(&validation);assert(verifier);
    xtlsclientconfig clientConfig;xrtTlsClientConfigInit(&clientConfig);
    clientConfig.Context=context;clientConfig.Verifier=verifier;clientConfig.ServerName=XRT_STR_LITERAL("localhost");
    xtlssession* client=xrtTlsClientCreate(&clientConfig,NULL);assert(client);
    xtlsserverconfig serverConfig;xrtTlsServerConfigInit(&serverConfig);
    serverConfig.Context=context;serverConfig.Select=selectIdentity;
    xtlssession* server=xrtTlsServerCreate(&serverConfig,NULL);assert(server);
    for(int i=0;i<64;i++){
        assert(!xrtTlsClientResumed(client)&&!xrtGetError());
        assert(!xrtTlsServerResumed(server)&&!xrtGetError());
    }
    assert(!xrtTlsClientResumed(server)&&xrtGetError());xrtClearError();
    assert(!xrtTlsServerResumed(client)&&xrtGetError());xrtClearError();
    assert(!xrtTlsClientResumed(NULL)&&xrtGetError());xrtClearError();
    assert(!xrtTlsServerResumed(NULL)&&xrtGetError());xrtClearError();
    xrtTlsSessionDestroy(server);xrtTlsSessionDestroy(client);
    xrtTlsVerifierRelease(verifier);xrtTlsContextRelease(context);
    assert(decisions==0);
}
int main(void)
{
    assert(xrtMemDebugEnable(true));exercise();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    exercise();xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    puts("TLS resumed observers: real role factories; 256 fresh observations; checked receivers; balanced owners; no transport or decisions");
    return 0;
}
