/* IP 身份校验和禁止 IP SNI 的契约必须在 Windows IOCP 后端保持一致。 */
#define TEST_HTTP_CLIENT_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_HTTPS_BACKEND_NAME "IOCP"
#define TEST_HTTP_CLIENT_HTTPS_IP 1
#include "test_http_client_https.c"
