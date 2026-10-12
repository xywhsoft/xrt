#define XHTTP_SERVER_MODULE_HTTP_SERVER
#include <xhttp-server.h>

int main(void)
{
	xhttpserverconfig Config; xrtHttpServerConfigInit(&Config); return Config.WriteSize > 0 ? 0 : 1;
}
