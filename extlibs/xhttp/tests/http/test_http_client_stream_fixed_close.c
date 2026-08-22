/* 验证固定长度正文和 FIN 同批到达时不会对已完成 Exchange 重复提交 EOF。 */
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length: 2\r\n" \
	"Connection: close\r\n" \
	"\r\n" \
	"OK"
#define TEST_HTTP_CLIENT_STREAM_STATUS 200
#define TEST_HTTP_CLIENT_STREAM_BODY "OK"
#define TEST_HTTP_CLIENT_STREAM_REMAINDER ""
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 0
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#define TEST_HTTP_CLIENT_STREAM_SERVER_CLOSE 1
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "fixed response with immediate close"
#include "test_http_client_stream.c"
