#define TEST_NATIVE_MULTI_MEMORY
#define main xrudp_native_stop_helpers_main
#include "test_endpoint_multi.c"
#undef main
int main(void)
{
	CHECK(run(XNET_FAMILY_IPV4,7)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(run(XNET_FAMILY_IPV4,7)==0 && run(XNET_FAMILY_IPV6,7)==0 && run(XNET_FAMILY_IPV4,10)==0);
	CHECK(run(XNET_FAMILY_IPV4,8)==0 && run(XNET_FAMILY_IPV4,9)==0);
	xrtClearError();xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes && before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);
	puts("xrudp native stop: default/extra receive cancellation, reverse/other streams, final/delivery hints and callback lifetimes balance");return 0;
}
