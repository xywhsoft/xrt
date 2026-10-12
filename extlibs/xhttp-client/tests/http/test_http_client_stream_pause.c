/* 验证 Body 回调内暂停、跨线程恢复和底层 TCP 读取背压。 */
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
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 1
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#define TEST_HTTP_CLIENT_STREAM_RESUME 0
#define TEST_HTTP_CLIENT_STREAM_INPUT_PAUSE 1
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "paused response input"
#include "test_http_client_stream.c"
