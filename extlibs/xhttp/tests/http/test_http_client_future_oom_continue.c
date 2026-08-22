/* Waiter span 成功后，在后续 Future 构造链中注入 OOM。 */
#define TEST_HTTP_CLIENT_FUTURE_OOM_ALLOW 1
#define TEST_HTTP_CLIENT_FUTURE_OOM_NAME "continuation"
#include "test_http_client_future_oom.c"
