/* 验证 readiness Future 在 waiter 注册前完成的竞态。 */
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
#define TEST_HTTP_CLIENT_STREAM_BODY_TERMINAL 0
#define TEST_HTTP_CLIENT_STREAM_BODY_IMMEDIATE 1
#define TEST_HTTP_CLIENT_STREAM_SCENARIO "immediate body Future"
#include "test_http_client_stream.c"
