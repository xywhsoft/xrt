/* 使用同一组高层并发终态契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_RACE_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_RACE_BACKEND_NAME "IOCP"
#include "test_http_client_cancel_race.c"
