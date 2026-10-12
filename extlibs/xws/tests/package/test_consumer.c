#define XWS_MODULE_ALL
#include <xws.h>

int main(void)
{
    xwsconnconfig Config;
    xrtWsConnConfigInit(&Config);
    return !xrtWsConnConfigValid(&Config);
}
