#ifndef XRT_NET_H
#define XRT_NET_H

#include "xrt_net_types.h"
#include "xrt_net_platform.h"
#include "xrt_net_tcp.h"
#include "xrt_net_udp.h"
#include "xrt_net_tls.h"

xrt_net_result xrt_net_init();
void xrt_net_cleanup();

#endif
