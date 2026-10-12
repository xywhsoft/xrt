/* 使用同一组所有权转移断言验证 Windows IOCP 后端。 */
#define TEST_HTTP_SERVER_UPGRADE_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_SERVER_UPGRADE_BACKEND_NAME "IOCP"
#include "test_http_server_upgrade.c"
#include "../test.h"
