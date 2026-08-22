/* 使用同一组阻塞终态竞态验证 Windows IOCP。 */
#define TEST_NET_SYNC_THREADS_BACKEND XNET_PORT_IOCP
#define TEST_NET_SYNC_THREADS_BACKEND_NAME "IOCP"
#include "test_net_sync_threads.c"
