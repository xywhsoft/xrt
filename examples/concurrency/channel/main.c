#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/channel —— 通道：容量、TrySend 与关闭排空
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChannelCreate(2)   精确容量创建
 *   xrtChannelTrySend     非阻塞发送（满即失败）
 *   xrtChannelClose       关闭（此后只可继续接收）
 *   xrtChannelRecv        阻塞接收（关且空后返回非 OK）
 * 模块宏：XRT_MODULE_CHANNEL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/channel/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   10
 *   20
 *
 * 关闭语义（Go 同款）：Close 后发送方报错、接收方把
 *   存量取完才结束——"排空"循环即本例写法。
 */


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
