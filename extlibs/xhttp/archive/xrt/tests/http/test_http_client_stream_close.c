/* 验证 close-delimited 响应只转移 Response 并由调用层关闭传输。 */
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
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
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "close-delimited response"
#include "test_http_client_stream.c"
