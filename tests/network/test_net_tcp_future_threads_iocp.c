/* 使用同一组取消和关闭竞态验证 Windows IOCP。 */
#define TEST_TCP_FUTURE_THREADS_BACKEND XNET_PORT_IOCP
#define TEST_TCP_FUTURE_THREADS_BACKEND_NAME "IOCP"
#include "test_net_tcp_future_threads.c"
