#define XHTTP_CLIENT_MODULE_HTTP_CLIENT
#include <xhttp-client.h>

int main(void)
{
	xhttpclientconfig Config; xrtHttpClientConfigInit(&Config); return Config.Timeout > 0 ? 0 : 1;
}
