#ifndef XRUDP_MODULE_RUDP_PUNCH
#define XRUDP_MODULE_RUDP_PUNCH
#endif
#ifndef XRT_MODULE_MEMORY_DEBUG
#define XRT_MODULE_MEMORY_DEBUG
#endif
#include "../../include/xrudp/features.h"
#define XRT_IMPLEMENTATION
#include "../../../../single/xrt.h"
static bool punch_fail;
static uint32_t punch_calls;
static bool punch_random(void* output,size_t size)
{
	++punch_calls;if(punch_fail){memset(output,0xa5,size);return false;}
	return xrtSecureRandom(output,size);
}
/* Only xrudp call sites after real XRT; no product hooks or global RNG swap. */
#define xrtSecureRandom punch_random
#define XRUDP_IMPLEMENTATION
#include "../../../../single/extlibs/xrudp.h"
#undef xrtSecureRandom
#include <stdio.h>
#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
int main(void)
{
	xrudppunch p,before;xrudpaddress target={4,42000,0,{192,0,2,1}};memset(&p,0xa5,sizeof(p));before=p;punch_fail=true;
	CHECK(xrtRudpPunchInit(&p,&target,1,0,0)==XRUDP_IO_ERROR && punch_calls==1 && !memcmp(&p,&before,sizeof(p)));
	punch_fail=false;CHECK(xrtRudpPunchInit(&p,&target,1,0,0)==XRUDP_OK && punch_calls==2);
	unsigned char wire[28],saved[28];size_t n;uint32_t index;uint64_t due;
	CHECK(xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&due)==XRUDP_OK);memcpy(saved,wire,28);before=p;punch_fail=true;
	CHECK(xrtRudpPunchInit(&p,&target,1,0,0)==XRUDP_IO_ERROR && punch_calls==3 && !memcmp(&p,&before,sizeof(p)));
	CHECK(xrtMemDebugFailAfter(0));
	CHECK(xrtRudpPunchNext(&p,0,wire,28,&n,&target,&index,&due)==XRUDP_OK && !memcmp(saved,wire,28) && xrtRudpPunchSent(&p,index,0)==XRUDP_OK);
	CHECK(xrtRudpPunchNext(&p,200000,wire,28,&n,&target,&index,&due)==XRUDP_OK && memcmp(saved,wire,28) && xrtRudpPunchSent(&p,index,200000)==XRUDP_OK && xrtRudpPunchCancel(&p)==XRUDP_CANCELLED && punch_calls==3);
	CHECK(!xrtMemDebugFailTriggered());xrtMemDebugFailClear();puts("xrudp punch entropy: partial RNG failure preserves pending offer; all later emissions are independent and RNG/heap free");return 0;
}
