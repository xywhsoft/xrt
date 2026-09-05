#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/channel_select —— 多路复用：哪个先来收哪个
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChannelCaseRecv   构造"接收候选"分支
 *   xrtChannelSelect     多通道一次等待（就绪分支胜出）
 * 模块宏：XRT_MODULE_CHANNEL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/channel_select/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   20
 *
 * Go select 的 C 版：两通道只有 second 有数据——Select
 *   就绪即返回该分支（20）。事件循环式工作分发不用
 *   轮询的关键原语。
 */


/* 从两个 Channel 中接收先到达的消息。 */
int main(void)
{
	xchannel tFirst;
	xchannel tSecond;
	xchannelcase arrCase[2];
	xchannelselectresult tResult;
	ptr pFirst = NULL;
	ptr pSecond = NULL;
	int iExit = 0;

	if (
		!xrtChannelInit(&tFirst, 1u) ||
		!xrtChannelInit(&tSecond, 1u)
	) {
		return 1;
	}
	if (
		xrtChannelTrySend(
			&tSecond,
			(ptr)(uintptr_t)20u
		) != XCHANNEL_OK
	) {
		iExit = 2;
	} else {
		arrCase[0] = xrtChannelCaseRecv(&tFirst, &pFirst);
		arrCase[1] = xrtChannelCaseRecv(&tSecond, &pSecond);
		tResult = xrtChannelSelect(arrCase, 2u);
		if (
			(tResult.Wait != XWAIT_OK) ||
			(tResult.Index != 1u)
		) {
			iExit = 3;
		} else {
			printf("%llu\n", (unsigned long long)(uintptr_t)pSecond);
		}
	}
	(void)xrtChannelUnit(&tFirst);
	(void)xrtChannelUnit(&tSecond);
	return iExit;
}
