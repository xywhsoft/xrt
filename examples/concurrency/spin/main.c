#include <xrt.h>

#include <stdio.h>



/*
 * 范例：concurrency/spin —— 自旋锁：纳秒级超短临界区
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtSpinInit / Lock / Unlock / Unit   自旋锁四件套
 * 模块宏：XRT_MODULE_SPIN
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/spin/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   counter=1
 *
 * 自旋 vs 互斥：等待时不睡眠而是忙等——临界区只有
 *   几条指令时比"睡眠-唤醒"的上下文切换便宜一个
 *   数量级；临界区稍长就烧 CPU。适用：计数器、
 *   指针发布的瞬时保护。
 */


/* 展示栈上短临界区锁的生命周期。 */
int main(void)
{
	xspinlock Spin;
	uint64 iCounter = 0u;

	if ( !xrtSpinInit(&Spin) ) {
		return 1;
	}
	if ( !xrtSpinLock(&Spin) ) {
		return 2;
	}
	iCounter++;
	if ( !xrtSpinUnlock(&Spin) || !xrtSpinUnit(&Spin) ) {
		return 3;
	}
	printf("counter=%llu\n", (unsigned long long)iCounter);
	return 0;
}
