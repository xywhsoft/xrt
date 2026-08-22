/* 使用 IOCP 验证空闲 TLS 连接上的晚到 ticket。 */
#define TEST_HTTP_CLIENT_RESUME_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_RESUME_BACKEND_NAME "IOCP"
#define TEST_HTTP_CLIENT_RESUME_LATE_TICKET 1
#include "test_http_client_resume.c"
