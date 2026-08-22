#include <stdio.h>
#include <xrt.h>



/* 演示精确容量、非阻塞发送和关闭后排空。 */
int main(void)
{
	xchannel* pChannel = xrtChannelCreate(2u);
	ptr pItem = NULL;

	if ( pChannel == NULL ) {
		return 1;
	}
	if (
		(xrtChannelTrySend(pChannel, (ptr)(uintptr_t)10u) != XCHANNEL_OK) ||
		(xrtChannelTrySend(pChannel, (ptr)(uintptr_t)20u) != XCHANNEL_OK)
	) {
		(void)xrtChannelDestroy(pChannel);
		return 2;
	}
	xrtChannelClose(pChannel);
	while ( xrtChannelRecv(pChannel, &pItem) == XWAIT_OK ) {
		printf("%llu\n", (unsigned long long)(uintptr_t)pItem);
	}
	return xrtChannelDestroy(pChannel) ? 0 : 3;
}
