/* 验证调用对象 OOM 不转移 Stream 或 Exchange 所有权。 */
#define TEST_HTTP_CLIENT_STREAM_OOM 1
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
	"Content-Length: 2\r\n" \
	"Connection: keep-alive\r\n" \
	"\r\n" \
	"OK"
#define TEST_HTTP_CLIENT_STREAM_STATUS 200
#define TEST_HTTP_CLIENT_STREAM_BODY "OK"
#define TEST_HTTP_CLIENT_STREAM_REMAINDER ""
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 1
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "constructor OOM recovery"
#include "test_http_client_stream.c"
