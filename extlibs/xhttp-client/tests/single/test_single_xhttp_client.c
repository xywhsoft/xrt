#define XHTTP_CLIENT_MODULE_ALL
#include <xhttp-client/features.h>
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_POLICY_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-policy.h"
#define XHTTP_CLIENT_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-client.h"

int main(void)
{
	xhttpclientconfig Config; xrtHttpClientConfigInit(&Config); return Config.Timeout > 0 ? 0 : 1;
}
