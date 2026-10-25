/* Ordinary capability/config validation. The stopped engine never listens,
 * connects, resolves a name or sends traffic. */
#if defined(XRT_RESUME_OWNED_XLANG_CORE)
#include "release/include/xrt_decl.h"
#elif defined(XRT_RESUME_OWNED_XLANG_RUNTIME)
#include "release/include/xruntime_decl.h"
#elif defined(XRT_RESUME_OWNED_SINGLE)
#include "single/xrt_decl.h"
#else
#include <xrt.h>
#endif
#include <assert.h>
#include <stdio.h>
#if defined(XRT_FEATURE_TLS_SERVER_RESUME)
#error "This fixture must not select the resumption overlay"
#endif
static const xtlsresume* borrowed(ptr p,const xtlsserverresumerequest* r)
{(void)p;(void)r;assert(0);return NULL;}
static bool owned(ptr p,const xtlsserverresumerequest* r,xtlsresume** out)
{(void)p;(void)r;(void)out;assert(0);return false;}
static bool identity(ptr p,const xtlsserverrequest* r,xtlsserverchoice* out)
{(void)p;(void)r;(void)out;assert(0);return false;}
int main(void)
{
    assert(xrtMemDebugEnable(true));xrtSetErrorKind(XERR_STATE);xrtClearError();
    xmemdebugsnapshot before,after;xrtMemDebugSnapshot(&before);
    xtlsserverconfig c;xrtTlsServerConfigInit(&c);c.Select=identity;
    c.Resume=borrowed;
    assert(!xrtTlsServerCreate(&c,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&xrtErrorCode(xrtGetError())==XTLS_ERROR_RESUME);xrtClearError();
    c.Resume=NULL;c.ResumeOwnedV1=owned;
    assert(!xrtTlsServerCreate(&c,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&xrtErrorCode(xrtGetError())==XTLS_ERROR_RESUME);xrtClearError();
    c.Resume=borrowed;
    assert(!xrtTlsServerCreate(&c,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    xnetengineconfig ec;xrtNetEngineConfigInit(&ec);ec.Backend=XNET_PORT_SELECT;ec.Workers=1;
    xnetengine* engine=xrtNetEngineCreate(&ec);assert(engine);
    xtlslistenerconfig lc;xrtTlsListenerConfigInit(&lc);lc.Tls.Select=identity;
    assert(xrtNetAddrLoopback(&lc.Listen.Address,XNET_FAMILY_IPV4,0));
    lc.Tls.Resume=borrowed;
    assert(!xrtTlsListenerStart(engine,&lc,NULL,NULL,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&xrtErrorCode(xrtGetError())==XTLS_ERROR_RESUME);xrtClearError();
    lc.Tls.Resume=NULL;lc.Tls.ResumeOwnedV1=owned;
    assert(!xrtTlsListenerStart(engine,&lc,NULL,NULL,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_UNSUPPORTED&&xrtErrorCode(xrtGetError())==XTLS_ERROR_RESUME);xrtClearError();
    lc.Tls.Resume=borrowed;
    assert(!xrtTlsListenerStart(engine,&lc,NULL,NULL,NULL)&&xrtGetError()&&xrtErrorKind(xrtGetError())==XERR_ARGUMENT);xrtClearError();
    assert(xrtNetEngineDestroy(engine));xrtMemDebugSnapshot(&after);
    assert(before.LiveCount==after.LiveCount&&before.LiveBytes==after.LiveBytes);
    assert(after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount);
    assert(before.InvalidFreeCount==after.InvalidFreeCount&&before.DoubleFreeCount==after.DoubleFreeCount);
    puts("TLS resume disabled: borrowed/owned callbacks rejected, mixed configuration rejected, no network, balanced: ok");return 0;
}
