#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



/* 单头发布复用客户端响应拒绝、范围检查和传输回收矩阵。 */
#include "../websocket/test_http_client_invalid.c"
