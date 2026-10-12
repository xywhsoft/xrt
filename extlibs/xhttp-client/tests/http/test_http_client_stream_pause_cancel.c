/* 验证暂停响应输入后取消会封闭暂停门，并且不再消费剩余正文。 */
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 200 OK\r\n" \
	"Transfer-Encoding: chunked\r\n" \
	"Connection: keep-alive\r\n" \
	"\r\n" \
	"3\r\nabc\r\n" \
	"3\r\ndef\r\n" \
	"0\r\n\r\n"
#define TEST_HTTP_CLIENT_STREAM_STATUS 200
#define TEST_HTTP_CLIENT_STREAM_BODY "abcdef"
#define TEST_HTTP_CLIENT_STREAM_REMAINDER ""
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 0
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
#define TEST_HTTP_CLIENT_STREAM_CANCEL 1
#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#define TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE 1
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "paused response cancellation"
#include "test_http_client_stream.c"
