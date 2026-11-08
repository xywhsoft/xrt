/* One unmodified fixed certificate and routine view/ABI checks only.
 * No network activity, malformed certificates or security test campaign. */
#include "../test.h"
#include "../fixtures/x509_path_legacy.h"

int main(void)
{
    xmemdebugsnapshot before;xrtMemDebugSnapshot(&before);
    xx509cert cert;
    testRequire(xrtX509Parse(X509_PATH_LEAF,sizeof(X509_PATH_LEAF),&cert),"parse normal fixed certificate");
    xx509genname name={0};
    xstrview host=XRT_STR_LITERAL("sha1-root.test.xrt");
    testRequire(xrtX509MatchHost(&cert,host,&name)==X509_VALUE && name.Type==X509_NAME_DNS &&
        name.Value.Size==host.Size && !memcmp(name.Value.Data,host.Data,host.Size),"normal DNS SAN match");
    xx509genname saved=name;
    testRequire(xrtMemDebugFailAfter(0),"observe allocation-free ordinary DNS matching");
    bool same=xrtX509MatchDns(host,host)==X509_VALUE && xrtX509MatchHost(&cert,host,NULL)==X509_VALUE;
    bool hit=xrtMemDebugFailTriggered();xrtMemDebugFailClear();
    testRequire(same && !hit && !xrtGetError(),"DNS boolean match does no heap work");
    testRequire(xrtX509MatchDns(XRT_STR_LITERAL("other.test.xrt"),host)==X509_DONE && !xrtGetError(),"ordinary different name");
    const char ip[3]={':',':','1'};
    testRequire(xrtX509MatchHost(&cert,(xstrview){ip,sizeof(ip)},&name)==X509_DONE && !xrtGetError() &&
        !memcmp(&name,&saved,sizeof(name)),"exact nonterminated numeric view; no-match preserves output");
    /* A common C ABI error: sizeof includes the terminator. Do not silently
     * reinterpret a four-byte view as the three-byte numeric literal. */
    testRequire(xrtX509MatchHost(&cert,(xstrview){"::1",sizeof("::1")},&name)==X509_ERROR &&
        xrtErrorKind(xrtGetError())==XERR_VALUE && !memcmp(&name,&saved,sizeof(name)),"C terminator is not logical host data");
    xrtClearError();
    xmemdebugsnapshot after;xrtMemDebugSnapshot(&after);
    testRequire(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes &&
        after.AllocCount-before.AllocCount==after.FreeCount-before.FreeCount &&
        before.InvalidFreeCount==after.InvalidFreeCount && before.DoubleFreeCount==after.DoubleFreeCount &&
        before.UseAfterFreeCount==after.UseAfterFreeCount,"identity input memory balance");
    testRequire(xrtMemDebugReset(),"identity input memory drain");
    printf("[PASS] x509_identity_span: normal DNS; exact 3-byte numeric view; C terminator rejected; no heap on DNS match; balanced\n");
    return 0;
}
