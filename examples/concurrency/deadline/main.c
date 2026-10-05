#include <xrt/detail/wait.h>
#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/deadline —— 截止时间：剩余量与过期判定
 * ----------------------------------------------------------------
 * 演示 API：
 *   __xrtWaitAfter     相对超时 → 绝对截止（单调钟基准）
 *   __xrtWaitRemaining 剩余微秒
 *   __xrtWaitExpired   是否已过期
 * 模块宏：XRT_MODULE_THREAD（deadline 同族）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/deadline/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   remaining: 50000 us
 *   expired: yes
 *
 * deadline vs 超时值：deadline 是绝对时刻，一处构造多处
 *   传递——重试循环里每次判断都不重置；全库等待类 API
 *   的超时统一收接这套"相对与绝对转换"数学。
 */


/* 展示相对超时与绝对截止时间之间的统一转换。 */
int main(void)
{
	double iDeadline = __xrtWaitAfter(UINT64_C(50000));

	printf("remaining: %llu us\n",
		(unsigned long long)__xrtWaitRemaining(iDeadline));
	xrtSleepUntil(iDeadline);
	printf("expired: %s\n", __xrtWaitExpired(iDeadline) ? "yes" : "no");
	return 0;
}
