#ifndef XRT_XNET2_BENCH_COMMON_H
#define XRT_XNET2_BENCH_COMMON_H

#if defined(_WIN32) || defined(_WIN64)
	#include <winsock2.h>
#endif

#include "../bench_common.h"



/* 初始化网络基准所需的平台网络环境和可选 CPU 绑定。 */
static bool xbenchNetInit(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		WSADATA WSAData;

		if ( WSAStartup(MAKEWORD(2, 2), &WSAData) != 0 ) {
			return false;
		}
	#endif

	xbenchApplyCpuPinFromEnv();
	return true;
}



/* 释放网络基准的平台网络环境。 */
static void xbenchNetUnit(void)
{
	#if defined(_WIN32) || defined(_WIN64)
		WSACleanup();
	#endif
}

#endif
