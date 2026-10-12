/* 验证提前最终响应停止异步请求正文并取消可读性等待。 */
#define TEST_HTTP_CLIENT_STREAM_RESPONSE \
	"HTTP/1.1 413 Content Too Large\r\n" \
	"Content-Length: 0\r\n" \
	"Connection: close\r\n" \
	"\r\n"
#define TEST_HTTP_CLIENT_STREAM_STATUS 413
#define TEST_HTTP_CLIENT_STREAM_BODY ""
#define TEST_HTTP_CLIENT_STREAM_REMAINDER ""
#define TEST_HTTP_CLIENT_STREAM_REUSABLE 0
#define TEST_HTTP_CLIENT_STREAM_UPGRADED 0
#define TEST_HTTP_CLIENT_STREAM_CANCEL 0
#define TEST_HTTP_CLIENT_STREAM_RESUME 1
#define TEST_HTTP_CLIENT_STREAM_EARLY_FINAL 1
#define TEST_HTTP_CLIENT_STREAM_REQUEST_DONE 0
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "early final response"
#include "test_http_client_stream.c"
