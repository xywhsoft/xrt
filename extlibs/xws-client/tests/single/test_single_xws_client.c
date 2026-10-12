#define XWS_CLIENT_MODULE_ALL
#include "../../../xws-client/include/xws-client/features.h"
#include "../../../xhttp-client/include/xhttp-client/features.h"
#include "../../../xhttp-policy/include/xhttp-policy/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#include "../../../xws/include/xws/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#define XWS_IMPLEMENTATION
#include "../../../../single/extlibs/xws.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#define XHTTP_POLICY_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-policy.h"
#define XHTTP_CLIENT_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-client.h"
#define XWS_CLIENT_IMPLEMENTATION
#include "../../../../single/extlibs/xws-client.h"

int main(void) { xwsclientconfig Config; xrtWsClientConfigInit(&Config); return 0; }
