/* 使用同一组 Upgrade 契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "IOCP"
#include "test_http_client_stream_upgrade.c"
