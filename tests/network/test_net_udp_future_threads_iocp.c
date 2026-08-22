/* 使用同一组取消和关闭竞态验证 Windows IOCP 后端。 */
#define TEST_UDP_FUTURE_THREADS_BACKEND XNET_PORT_IOCP
#define TEST_UDP_FUTURE_THREADS_BACKEND_NAME "IOCP"
#include "test_net_udp_future_threads.c"
