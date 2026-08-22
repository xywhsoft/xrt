/* 同一代理恢复契约只切换 IOCP 后端，不复制测试状态机。 */
#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_PROXY_HTTPS_BACKEND_NAME "IOCP"
#include "test_http_client_resume_proxy.c"
