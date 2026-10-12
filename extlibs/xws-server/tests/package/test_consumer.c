#define XWS_SERVER_MODULE_ALL
#include <xws-server.h>

int main(void)
{
    xwsserverconfig Config;
    xrtWsServerConfigInit(&Config);
    return !xrtWsServerConfigValid(&Config);
}
