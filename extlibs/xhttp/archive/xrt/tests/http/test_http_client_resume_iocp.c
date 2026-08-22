/* 使用同一 TLS 1.3 恢复契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_RESUME_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_RESUME_BACKEND_NAME "IOCP"
#include "test_http_client_resume.c"
