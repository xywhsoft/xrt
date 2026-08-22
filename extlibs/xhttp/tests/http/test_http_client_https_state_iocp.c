/* 在 Windows IOCP 后端验证 TLS 握手状态和协作取消。 */
#define TEST_HTTP_CLIENT_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME "IOCP"
#include "test_http_client_https_state.c"
