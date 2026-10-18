#define TEST_NATIVE_DG_TIMEOUT
#define main xrudp_native_datagram_base_main
#include "test_single_endpoint_datagrams.c"
#undef main
int main(void)
{
	CHECK(run(XNET_FAMILY_IPV4,0)==0);xmemdebugsnapshot before,after;xrtClearError();xrtMemDebugSnapshot(&before);
	CHECK(run(XNET_FAMILY_IPV4,6)==0);xrtClearError();xrtMemDebugSnapshot(&after);
	CHECK(before.LiveCount==after.LiveCount && before.LiveBytes==after.LiveBytes);
	CHECK(before.DoubleFreeCount==after.DoubleFreeCount && before.UseAfterFreeCount==after.UseAfterFreeCount);return 0;
}
