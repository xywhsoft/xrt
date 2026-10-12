#define XHTTP_POLICY_MODULE_ALL
#include <xhttp-policy/features.h>
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_POLICY_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-policy.h"

int main(void)
{
	xcookiejarconfig Config; xrtCookieJarConfigInit(&Config); return 0;
}
