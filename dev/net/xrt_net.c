#include "xrt_net.h"

xrt_net_result xrt_net_init()
{
	xrt_net_result result;

	result = xrt_net_platform_init();
	if ( result != XRT_NET_OK ) {
		return result;
	}

	result = xrt_tls_init();
	if ( result != XRT_NET_OK ) {
		xrt_net_platform_cleanup();
		return result;
	}

	return XRT_NET_OK;
}

void xrt_net_cleanup()
{
	xrt_tls_cleanup();
	xrt_net_platform_cleanup();
}
