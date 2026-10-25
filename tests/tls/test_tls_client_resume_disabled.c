/* Valid credential/config capability checks only: no handshakes or traffic. */
#if defined(XRT_CLIENT_RESUME_XLANG_CORE)
#include "release/include/xrt_decl.h"
#elif defined(XRT_CLIENT_RESUME_XLANG_RUNTIME)
#include "release/include/xruntime_decl.h"
#elif defined(XRT_CLIENT_RESUME_SINGLE)
#include "single/xrt_decl.h"
#else
#include <xrt.h>
#endif
#include <assert.h>
#include <stdio.h>
#if defined(XRT_FEATURE_TLS_CLIENT_RESUME)
#error "This fixture must not enable the client resumption overlay"
#endif
static xtlsverifydecision never(const xtlspeer* peer,ptr context)
{(void)peer;(void)context;assert(0);return XTLS_VERIFY_ERROR;}
static void rejected(const xtlsclientconfig* config)
{
    assert(!xrtTlsClientCreate(config,NULL));
    assert(xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&xrtErrorCode(xrtGetError())==XTLS_ERROR_RESUME);
    xrtClearError();
}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xtlsverifierconfig vc;xrtTlsVerifierConfigInit(&vc);vc.Verify=never;
    xtlsverifier* verifier=xrtTlsVerifierCreate(&vc);assert(verifier);
    xtlsresumeconfig rc;xrtTlsResumeConfigInit(&rc);rc.Cipher=XTLS_AES_128_GCM_SHA256;
    rc.Ticket=(xbytesview){(const uint8*)"ticket",6};rc.Secret=(xbytesview){(const uint8*)"01234567890123456789012345678901",32};
    rc.ServerName=(xstrview){"localhost",9};rc.Lifetime=120;
    xtlsresume* resume=xrtTlsResumeCreate(&rc);assert(resume);
    xtlsclientconfig c;xrtTlsClientConfigInit(&c);c.Verifier=verifier;c.ServerName=c.VerifyName=rc.ServerName;
    xtlssession* normal=xrtTlsClientCreate(&c,NULL);assert(normal);xrtTlsSessionDestroy(normal);
    c.Resume=resume;rejected(&c);
    c.ResumeOnly=true;rejected(&c);
    c.Resume=NULL;rejected(&c);
    c.ResumeOnly=false;normal=xrtTlsClientCreate(&c,NULL);assert(normal);xrtTlsSessionDestroy(normal);
    xrtTlsResumeRelease(resume);xrtTlsVerifierRelease(verifier);xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    puts("TLS client resume disabled: valid Resume/ResumeOnly explicitly rejected, default verified config unchanged, no traffic, balanced: ok");return 0;
}
