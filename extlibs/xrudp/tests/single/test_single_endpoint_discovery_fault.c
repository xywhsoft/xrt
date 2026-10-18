#ifndef XRUDP_MODULE_RUDP_ENDPOINT
#define XRUDP_MODULE_RUDP_ENDPOINT
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../../include/xrudp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
#include "../../include/xrudp/endpoint.h"
static uint32_t discovery_fault_mode,discovery_fault_left;
static xrudpendpoint* discovery_fault_endpoint;
static void discovery_native_fault_reset(uint32_t mode) {discovery_fault_mode=mode;discovery_fault_left=1;}
static void discovery_native_fault_endpoint(xrudpendpoint* e) {discovery_fault_endpoint=e;}
static xnetresult discovery_native_send(xnetudp* udp,const xnetaddr* address,const void* data,size_t size)
{
	const unsigned char* p=(const unsigned char*)data;
	if(size>=20 && p[0]==0 && p[1]==1 && p[4]==0x21 && p[5]==0x12 && p[6]==0xa4 && p[7]==0x42 && discovery_fault_left) {
		--discovery_fault_left;
		if(discovery_fault_mode==14) {(void)xrtRudpEndpointClose(discovery_fault_endpoint);xrtSetErrorKind(XERR_IO);return XNET_RESULT_ERROR;}
		if(discovery_fault_mode==12) {xrtSetErrorInfo(XERR_IO,"discovery-test",19,"injected route refusal");return XNET_RESULT_ERROR;}
		if(discovery_fault_mode==13) {xrtSetErrorKind(XERR_MEMORY);return XNET_RESULT_ERROR;}
		return XNET_RESULT_AGAIN;
	}
	return xrtNetUdpSendTo(udp,address,data,size);
}
/* Fault only xrudp's transport call sites after the real XRT implementation.
 * Actual server sockets and all non-STUN peer traffic keep their real path. */
#define xrtNetUdpSendTo discovery_native_send
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#undef xrtNetUdpSendTo
#define TEST_NATIVE_DISCOVERY_SEND_FAULTS
#include "../test_endpoint_discovery.c"
