#include "../test.h"
#include "../../../xhttp-server/src/internal/xrt_http_server_runtime.h"

static unsigned Completions;

static void optional_done(xhttpconn* http, xnetresult result, xwsconn* connection,
    const xerror* error, ptr data)
{
    (void)http; (void)result; (void)connection; (void)error; (void)data;
    Completions++;
}

static bool optional_case(bool valid, cstr message)
{
    printf("[%s] WebSocket optional deflate %s\n", valid ? "PASS" : "FAIL", message);
    xrtClearError();
    return valid;
}

int main(void)
{
    xwsserverconfig config;
    xwsserverhandshake handshake, before;
    xhttpconn http;
    xhttpreply* reply;
    bool valid = true, matched;

    /* This is the same valid permessage-deflate snapshot in every module
     * combination; public WebSocket layouts do not depend on feature flags. */
    memset(&handshake, 0, sizeof(handshake));
    memcpy(handshake.Accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", XWS_ACCEPT_CAPACITY);
    handshake.Deflate.ServerMaxWindowBits = 15;
    handshake.Deflate.ClientMaxWindowBits = 15;
    memcpy(handshake.Extensions, "permessage-deflate", sizeof("permessage-deflate"));
    handshake.DeflateEnabled = true;
    memcpy(&before, &handshake, sizeof(before));
    xrtWsServerConfigInit(&config);
    testRequire(xrtWsServerConfigValid(&config), "WebSocket default server Config is invalid");
    memset(&http, 0, sizeof(http));

    reply = xrtWsServerReply(&handshake);
#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
    const xhttpfield* extension = xrtHttpReplyHeader(reply,
        XRT_STR_LITERAL("Sec-WebSocket-Extensions"));
    matched = reply != NULL && extension != NULL &&
        extension->Value.Size == sizeof("permessage-deflate") - 1 &&
        memcmp(extension->Value.Data, "permessage-deflate", extension->Value.Size) == 0;
#else
    matched = reply == NULL && xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        xrtErrorCode(xrtGetError()) == XWS_HANDSHAKE_ERROR_EXTENSION;
#endif
    xrtHttpReplyDestroy(reply);
    if (!optional_case(matched && memcmp(&before, &handshake, sizeof(before)) == 0,
        "Reply honors selected modules")) valid = false;

    /* A compression-disabled Config must reject a compressed snapshot before
     * reaching any transport/Worker operation. The zero connection is never
     * an accepted producer; a missed preflight fails at the HTTP Worker gate. */
    matched = xrtWsUpgradeAccept(&http, &config, &handshake, NULL, NULL, NULL,
        optional_done, NULL) == XNET_RESULT_ERROR &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        xrtErrorCode(xrtGetError()) == XWS_HANDSHAKE_ERROR_EXTENSION &&
        Completions == 0 && memcmp(&before, &handshake, sizeof(before)) == 0;
    if (!optional_case(matched, "disabled Config rejects compressed snapshot")) valid = false;

    config.EnableDeflate = true;
#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
    matched = xrtWsServerConfigValid(&config);
#else
    matched = !xrtWsServerConfigValid(&config);
#endif
    if (!optional_case(matched, "Config honors selected modules")) valid = false;

    handshake.DeflateEnabled = false;
#if defined(XWS_SERVER_FEATURE_WEBSOCKET_SERVER_DEFLATE)
    config.RequireDeflate = true;
    matched = xrtWsUpgradeAccept(&http, &config, &handshake, NULL, NULL, NULL,
        optional_done, NULL) == XNET_RESULT_ERROR &&
        xrtErrorKind(xrtGetError()) == XERR_VALUE &&
        xrtErrorCode(xrtGetError()) == XWS_HANDSHAKE_ERROR_EXTENSION &&
        Completions == 0;
    if (!optional_case(matched, "required Config rejects uncompressed snapshot")) valid = false;
#endif
    reply = xrtWsServerReply(&handshake);
    matched = reply != NULL && xrtHttpReplyStatus(reply) == XHTTP_STATUS_SWITCHING_PROTOCOLS &&
        xrtHttpReplyHeader(reply, XRT_STR_LITERAL("Sec-WebSocket-Extensions")) == NULL;
    xrtHttpReplyDestroy(reply);
    if (!optional_case(matched, "uncompressed Reply remains available")) valid = false;
    testMemoryDebugDrain("WebSocket optional deflate checks leaked resources");
    return valid ? 0 : 1;
}
