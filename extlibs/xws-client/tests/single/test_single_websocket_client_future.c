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

/* 单头发布复用客户端 Future 结果取得和早到帧交接回归。 */
#include "../websocket/test_http_client_future.c"
