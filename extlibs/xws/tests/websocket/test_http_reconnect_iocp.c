#if defined(_WIN32)
	#define TEST_WS_RECONNECT_BACKEND XNET_PORT_IOCP
	#define TEST_WS_RECONNECT_BACKEND_NAME "iocp"
	#include "test_http_reconnect.c"
#else
	#include <stdio.h>



	/* IOCP 压力门禁只在 Windows 构建机执行。 */
	int main(void)
	{
		printf("[PASS] WebSocket HTTP reconnect IOCP skipped\n");
		return 0;
	}
#endif
