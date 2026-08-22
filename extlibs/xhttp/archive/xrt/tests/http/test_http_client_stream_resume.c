/* 验证异步请求正文暂停后由 Future 完成自动恢复。 */
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
#define TEST_HTTP_CLIENT_STREAM_RESUME 1
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "body resume"
#include "test_http_client_stream.c"
