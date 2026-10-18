#define XRUDP_MODULE_RUDP_DISCOVERY
#define XRT_MODULE_MEMORY_DEBUG
#include "../../include/xrudp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static bool discovery_fail;
static uint32_t discovery_random_calls;
static bool discovery_random(void* output,size_t size)
{
	++discovery_random_calls;
	if(discovery_fail) {memset(output,0xa5,size);return false;}
	return xrtSecureRandom(output,size);
}
/* Substitute only xrudp call sites after the actual XRT implementation. */
#define xrtSecureRandom discovery_random
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#undef xrtSecureRandom
#include <stdio.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
int main(void)
{
	xrudpaddress server={4,3478,0,{192,0,2,1}};xrudpdiscovery d;unsigned char before[sizeof(d)];
	memset(&d,0xa5,sizeof(d));memcpy(before,&d,sizeof(d));discovery_fail=true;
	CHECK(xrtRudpDiscoveryInit(&d,&server,0,10000,false)==XRUDP_IO_ERROR && discovery_random_calls==1 && !memcmp(before,&d,sizeof(d)));
	discovery_fail=false;CHECK(xrtRudpDiscoveryInit(&d,&server,0,10000,false)==XRUDP_OK && discovery_random_calls==2);
	memcpy(before,&d,sizeof(d));discovery_fail=true;CHECK(xrtRudpDiscoveryInit(&d,&server,1000,10000,true)==XRUDP_IO_ERROR && discovery_random_calls==3 && !memcmp(before,&d,sizeof(d)));
	discovery_fail=false;CHECK(xrtMemDebugFailAfter(0));unsigned char wire[512];size_t n;uint64_t due;
	CHECK(xrtRudpDiscoveryNext(&d,1000,wire,sizeof(wire),&n,&due)==XRUDP_OK && n==20 && xrtRudpDiscoverySent(&d,1000)==XRUDP_OK && xrtRudpDiscoveryCancel(&d)==XRUDP_CANCELLED);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();
	puts("xrudp discovery entropy: partial RNG failure preserves object and pending transaction, recoverable with no I/O heap");return 0;
}
