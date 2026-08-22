/* 复用同一夹具验证 Windows IOCP 下的 Cookie 重定向策略。 */
#define TEST_HTTP_COOKIE_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_COOKIE_BACKEND_NAME "IOCP"
#define TEST_HTTP_COOKIE_REDIRECT_ONLY
#include "test_http_client_cookie.c"
