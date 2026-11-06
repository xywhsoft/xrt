#define XWS_SERVER_MODULE_ALL
#include <xws-server.h>
#include <string.h>

int main(void)
{
    xwsserverconfig Config;
    xwsserverhandshake Handshake;
    xhttpreply* Reply;
    bool Valid;

    xrtWsServerConfigInit(&Config);
    if (!xrtWsServerConfigValid(&Config)) return 1;
    memset(&Handshake, 0, sizeof(Handshake));
    memcpy(Handshake.Accept, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", XWS_ACCEPT_CAPACITY);
    Reply = xrtWsServerReply(&Handshake);
    Valid = Reply != NULL &&
        xrtHttpReplyStatus(Reply) == XHTTP_STATUS_SWITCHING_PROTOCOLS &&
        xrtHttpReplyHeaderCount(Reply) == 3;
    xrtHttpReplyDestroy(Reply);
    if (!Valid) return 2;

    /* Verify the extension reports through the consumer's same Core instance. */
    xrtClearError();
    Reply = xrtWsServerReply((const xwsserverhandshake*)(uintptr_t)(UINTPTR_MAX - 1u));
    Valid = Reply == NULL && xrtErrorKind(xrtGetError()) == XERR_ARGUMENT;
    xrtHttpReplyDestroy(Reply);
    xrtClearError();
    return Valid ? 0 : 3;
}
