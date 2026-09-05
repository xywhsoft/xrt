#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/rwlock —— 读写锁：升级/降级的完整路径
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtRWLockRead / ReadUnlock     共享读
 *   xrtRWLockUpgrade / Downgrade   读→写原子升级 / 写→读降级
 *   xrtRWLockWrite / WriteUnlock   排他写
 *   xrtRWLockTryRead               非阻塞尝试
 * 模块宏：XRT_MODULE_RWLOCK
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/concurrency/rwlock/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   value=15
 *
 * 升级路径解决"先读检查、条件满足再写"的窗口竞态：
 *   手工"放读锁→拿写锁"之间数据可能被改——Upgrade
 *   原子完成切换；Downgrade 反向同理（写完转读继续用）。
 *   7+5+3=15 印证三条路径各执行一次。
 */


/* 展示读写锁的普通路径、升级路径和原子降级路径。 */
int main(void)
{
	xrwlock Lock;
	int iValue = 7;
	bool bOkay = false;

	if ( !xrtRWLockInit(&Lock) ) {
		return 1;
	}
	if ( !xrtRWLockRead(&Lock) ||
		!xrtRWLockUpgrade(&Lock) ) {
		goto cleanup;
	}
	iValue += 5;
	if ( !xrtRWLockDowngrade(&Lock) ||
		!xrtRWLockReadUnlock(&Lock) ||
		!xrtRWLockWrite(&Lock) ) {
		goto cleanup;
	}
	iValue += 3;
	if ( !xrtRWLockWriteUnlock(&Lock) ||
		!xrtRWLockTryRead(&Lock) ) {
		goto cleanup;
	}
	bOkay = xrtRWLockReadUnlock(&Lock);

cleanup:
	printf("value=%d\n", iValue);
	return xrtRWLockUnit(&Lock) && bOkay && (iValue == 15) ? 0 : 2;
}
