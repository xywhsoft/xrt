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

/* 复用完整单头 WebSocket HTTP CONNECT 组合契约验证 Windows IOCP。 */
#define TEST_SINGLE_WS_HTTP_PROXY_BACKEND XNET_PORT_IOCP
#define TEST_SINGLE_WS_HTTP_PROXY_BACKEND_NAME "iocp"
#include "test_single_websocket_http_proxy_transport.c"
