#define XWS_CLIENT_MODULE_ALL
#include <xws-client.h>

int main(void)
{
    xwsclientconfig Config;
    char Key[XWS_KEY_CAPACITY];
    xrtWsClientConfigInit(&Config);
    xhttprequest* Request = xrtWsClientRequestCreate(XRT_STR_LITERAL("ws://localhost/test"), &Config, Key);
    if (Request == NULL) return 1;
    xrtHttpRequestDestroy(Request);
    return !xrtWsKeyValid((xstrview) {Key, XWS_KEY_SIZE});
}
