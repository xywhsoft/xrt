/* 使用同一组异步正文契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_SERVER_BODY_ASYNC_BACKEND \
	XNET_PORT_IOCP
#define TEST_HTTP_SERVER_BODY_ASYNC_BACKEND_NAME \
	"IOCP"
#include "test_http_server_body_async.c"
