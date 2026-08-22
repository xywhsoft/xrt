/* 上下文 span 成功后，强制 continuation 或输出 Future 建立失败。 */
#define TEST_HTTP_SERVER_FUTURE_OOM_ALLOW 1
#define TEST_HTTP_SERVER_FUTURE_OOM_NAME "continuation"
#include "test_http_server_future_oom.c"
