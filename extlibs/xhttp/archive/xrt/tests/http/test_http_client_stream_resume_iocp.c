/* 使用同一组正文恢复契约验证 Windows IOCP 后端。 */
#define TEST_HTTP_CLIENT_STREAM_BACKEND XNET_PORT_IOCP
#define TEST_HTTP_CLIENT_STREAM_BACKEND_NAME "IOCP"
#include "test_http_client_stream_resume.c"
