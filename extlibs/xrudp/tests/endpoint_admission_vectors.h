/* Internal, deterministic checks of the actual generated endpoint rate gate.
 * Include after the single-header implementation. This does not simulate UDP. */
#include <xrt/memory_debug.h>
#include <stdio.h>
#include <string.h>
#define ADMIT_CHECK(v) do { if (!(v)) { fprintf(stderr,"admission:%d: %s\n",__LINE__,#v); return 1; } } while (0)

static void admission_init(xrudpendpoint* e, uint32_t total)
{
	memset(e,0,sizeof(*e)); e->RateSecond=UINT64_MAX;
	e->Config.HandshakesPerSecond=total; e->Config.AddressHandshakesPerSecond=8;
}
static xrudpaddress admission_address(unsigned int id, bool ipv6, uint32_t zone)
{
	xrudpaddress a; memset(&a,0,sizeof(a)); a.Family=ipv6?6:4; a.Port=1000; a.Zone=zone;
	if (ipv6) { a.Address[0]=0xfe; a.Address[1]=0x80; a.Address[15]=(unsigned char)id; }
	else { a.Address[0]=127; a.Address[3]=(unsigned char)id; }
	return a;
}
static int admission_port_and_scope(void)
{
	xrudpendpoint e; admission_init(&e,128); xrudpaddress a=admission_address(1,false,0);
	for (unsigned int i=0;i<8;++i) { a.Port=(uint16_t)(1000+i); ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,999999)); }
	for (unsigned int i=0;i<128;++i) { a.Port=(uint16_t)(2000+i); ADMIT_CHECK(!xrudp__endpoint_rate(&e,&a,999999)); }
	ADMIT_CHECK(e.RateCount==8);
	ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,1000000) && e.RateCount==1);
	/* Equal link-local bytes on distinct interface scopes are distinct hosts. */
	for (uint32_t zone=1;zone<=2;++zone) {
		a=admission_address(1,true,zone);
		for (unsigned int i=0;i<8;++i) { ++a.Port; ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,1000000)); }
		ADMIT_CHECK(!xrudp__endpoint_rate(&e,&a,1000000));
	}
	ADMIT_CHECK(e.RateCount==17);
	return 0;
}
static int admission_full_table(void)
{
	xrudpendpoint e; admission_init(&e,512);
	for (unsigned int i=1;i<=XRUDP_ENDPOINT_ADDRESS_BUCKETS;++i) {
		xrudpaddress a=admission_address(i,false,0); ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,7));
	}
	xrudp__addressrate before[XRUDP_ENDPOINT_ADDRESS_BUCKETS]; memcpy(before,e.Rates,sizeof(before));
	xrudpaddress unknown=admission_address(65,false,0);
	for (unsigned int i=0;i<10000;++i) { unknown.Port=(uint16_t)(i+1); ADMIT_CHECK(!xrudp__endpoint_rate(&e,&unknown,7)); }
	ADMIT_CHECK(e.RateCount==XRUDP_ENDPOINT_ADDRESS_BUCKETS && memcmp(before,e.Rates,sizeof(before))==0);
	xrudpaddress known=admission_address(1,false,0);
	for (unsigned int i=1;i<8;++i) ADMIT_CHECK(xrudp__endpoint_rate(&e,&known,7));
	ADMIT_CHECK(!xrudp__endpoint_rate(&e,&known,7));
	ADMIT_CHECK(e.RateCount==XRUDP_ENDPOINT_ADDRESS_BUCKETS+7);
	/* Expired entries may be reclaimed, but current-window entries never are. */
	ADMIT_CHECK(xrudp__endpoint_rate(&e,&unknown,1000000) && e.RateCount==1);
	for (uint64_t second=2;second<258;++second) {
		for (unsigned int i=1;i<=XRUDP_ENDPOINT_ADDRESS_BUCKETS;++i) {
			xrudpaddress a=admission_address(i,true,1); ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,second*1000000));
		}
		xrudpaddress extra=admission_address(65,true,1); ADMIT_CHECK(!xrudp__endpoint_rate(&e,&extra,second*1000000));
		ADMIT_CHECK(e.RateCount==XRUDP_ENDPOINT_ADDRESS_BUCKETS);
	}
	return 0;
}
static int admission_global_limit(void)
{
	xrudpendpoint e; admission_init(&e,128);
	for (unsigned int repeat=0;repeat<2;++repeat) for (unsigned int id=1;id<=64;++id) {
		xrudpaddress a=admission_address(id,false,0); ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,999999));
	}
	xrudp__addressrate before[XRUDP_ENDPOINT_ADDRESS_BUCKETS]; memcpy(before,e.Rates,sizeof(before));
	xrudpaddress a=admission_address(1,false,0); ADMIT_CHECK(!xrudp__endpoint_rate(&e,&a,999999));
	ADMIT_CHECK(e.RateCount==128 && memcmp(before,e.Rates,sizeof(before))==0);
	ADMIT_CHECK(xrudp__endpoint_rate(&e,&a,1000000) && e.RateCount==1);
	return 0;
}
int main(void)
{
	ADMIT_CHECK(xrtMemDebugFailAfter(0));
	ADMIT_CHECK(admission_port_and_scope()==0);
	ADMIT_CHECK(admission_full_table()==0);
	ADMIT_CHECK(admission_global_limit()==0);
	ADMIT_CHECK(!xrtMemDebugFailTriggered()); xrtMemDebugFailClear();
	puts("xrudp admission vectors: port/scope identity, 64 slots, 10000 full-table drops, 256 windows, global limit, zero XRT allocations passed");
	return 0;
}
#undef ADMIT_CHECK
