#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件复用完整 HTTPS Router、WSS Upgrade 与连接生命周期回归。 */
#define TEST_WS_SERVER_ROUTER_TLS 1
#define TEST_WS_SERVER_ROUTER_NAME "select/TLS"
#include "../websocket/test_server_router.c"
