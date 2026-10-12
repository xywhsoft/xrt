#define XHTTP_POLICY_MODULE_HTTP_RETRY_POLICY
#define XHTTP_POLICY_IMPLEMENTATION
#define XHTTP_IMPLEMENTATION
#include "../../include/xhttp-policy/features.h"
#include <xhttp/features.h>
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xhttp-policy.h"
int main(void) { uint64 Delay; return !(xrtHttpRetryStatusDefault(503) && xrtHttpRetryBackoff(1000,4000,8,&Delay) && Delay == 4000); }
