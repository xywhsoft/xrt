#define XHTTP_SERVER_MODULE_ALL
#include <xhttp-server/features.h>
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_SERVER_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-server.h"

int main(void)
{
	xhttpserverconfig Config; xrtHttpServerConfigInit(&Config); return Config.WriteSize > 0 ? 0 : 1;
}
