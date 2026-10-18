#define TEST_NATIVE_MULTI_MEMORY
#define main xrudp_native_shutdown_helpers_main
#include "test_endpoint_multi.c"
#undef main
int main(void)
{
	CHECK(run(XNET_FAMILY_IPV4,11)==0); xmemdebugsnapshot before,after; xrtClearError(); xrtMemDebugSnapshot(&before);
	CHECK(run(XNET_FAMILY_IPV4,11)==0 && run(XNET_FAMILY_IPV6,11)==0);
	CHECK(run(XNET_FAMILY_IPV4,12)==0 && run(XNET_FAMILY_IPV4,13)==0);
	xrtClearError(); xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp native shutdown: reliable drain, peer request hints, abort/drop and endpoint-close callbacks balance"); return 0;
}
