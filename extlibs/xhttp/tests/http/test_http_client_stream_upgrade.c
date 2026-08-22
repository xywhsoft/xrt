/* 验证 HTTP Upgrade 后响应、传输和新协议余量同时移交。 */
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 101 Switching Protocols\r\n" \
	"Connection: Upgrade\r\n" \
	"Upgrade: websocket\r\n" \
	"\r\n" \
	"XYZ"
#define TEST_HTTP_CLIENT_STREAM_STATUS 101
#define TEST_HTTP_CLIENT_STREAM_BODY ""
#define TEST_HTTP_CLIENT_STREAM_REMAINDER "XYZ"
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 0
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 1
#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "upgrade"
#include "test_http_client_stream.c"
