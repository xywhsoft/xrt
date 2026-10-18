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
static uint32_t punch_fault_mode,punch_fault_left;
static xrudpendpoint* punch_fault_endpoint;
static void punch_native_fault_reset(uint32_t mode) {punch_fault_mode=mode;punch_fault_left=mode>=8?1:0;}
static void punch_native_fault_endpoint(xrudpendpoint* e) {punch_fault_endpoint=e;}
static xnetresult punch_native_send(xnetudp* udp,const xnetaddr* address,const void* data,size_t size)
{
	const unsigned char* p=(const unsigned char*)data;
	if(size==28 && p[0]==0 && p[1]==0x11 && p[4]==0x21 && p[5]==0x12 && p[6]==0xa4 && p[7]==0x42 && punch_fault_left) {
		--punch_fault_left;
		if(punch_fault_mode==11){(void)xrtRudpEndpointClose(punch_fault_endpoint);xrtSetErrorKind(XERR_IO);return XNET_RESULT_ERROR;}
		if(punch_fault_mode==10){xrtSetErrorInfo(XERR_IO,"punch-test",19,"injected route refusal");return XNET_RESULT_ERROR;}
		if(punch_fault_mode==9){xrtSetErrorKind(XERR_MEMORY);return XNET_RESULT_ERROR;}
		return XNET_RESULT_AGAIN;
	}
	return xrtNetUdpSendTo(udp,address,data,size);
}
/* Substitute xrudp sites only; gateway sockets retain real UDP behavior. */
#define xrtNetUdpSendTo punch_native_send
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#undef xrtNetUdpSendTo
#define TEST_NATIVE_PUNCH_SEND_FAULTS
#include "../test_endpoint_punch.c"
