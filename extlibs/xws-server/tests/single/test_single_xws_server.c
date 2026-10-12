#define XWS_SERVER_MODULE_ALL
#include "../../../xws-server/include/xws-server/features.h"
#include "../../../xhttp-server/include/xhttp-server/features.h"
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
#define XHTTP_SERVER_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp-server.h"
#define XWS_SERVER_IMPLEMENTATION
#include "../../../../single/extlibs/xws-server.h"

int main(void) { xwsserverconfig Config; xrtWsServerConfigInit(&Config); return 0; }
