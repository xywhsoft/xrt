#include <stdio.h>

#include <xrt.h>



/* 应用取消回调只负责唤醒或标记对应工作。 */
static void stopWork(ptr pData)
{
	bool* pStopped = (bool*)pData;

	*pStopped = true;
}



/*
 * 范例：concurrency/cancel —— 取消令牌：父子传播 + 观察者回调
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtCancelCreate / Destroy    令牌生命周期
 *   xrtCancelChild               派生子令牌（父取消→子取消）
 *   xrtCancelWatch / Unwatch     注册取消观察者（回调式唤醒）
 *   xrtCancelRequest             发起取消
 *   xrtCancelRef                 增加令牌引用（多持有者）
 *   xrtCancelTriggered           监听是否已命中取消
 * 模块宏：XRT_MODULE_CANCEL
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/cancel/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   operation stopped: yes
 *   watch-triggered=1
 *
 * 协作取消模型：Request 只"举旗"不抢占——各工作点自查；
 *   回调的职责是唤醒沉睡的工作。父子树让"取消一组操作"
 *   一次完成：请求父令牌、全部子令牌同时置位。
 */


/* 用父令牌一次取消一组子操作。 */
int main(void)
{
	xcancel* pRequest = xrtCancelCreate();
	xcancel* pOperation;
	xcancelwatch* pWatch;
	bool bStopped = false;

	if ( pRequest == NULL ) {
		return 1;
	}
	pOperation = xrtCancelChild(pRequest);
	if ( pOperation == NULL ) {
		xrtCancelDestroy(pRequest);
		return 1;
	}
	pWatch = xrtCancelWatch(pOperation, stopWork, &bStopped);
	if ( pWatch == NULL ) {
		xrtCancelDestroy(pOperation);
		xrtCancelDestroy(pRequest);
		return 1;
	}

	/* Ref：增加令牌引用（多持有者共享同一取消源），用完各Destroy一次。 */
	xcancel* pExtra = xrtCancelRef(pOperation);

	(void)xrtCancelRequest(pRequest);
	printf("operation stopped: %s\n", bStopped ? "yes" : "no");
	printf("watch-triggered=%d\n", xrtCancelTriggered(pWatch) ? 1 : 0);
	xrtCancelDestroy(pExtra);

	xrtCancelUnwatch(pWatch);
	xrtCancelDestroy(pOperation);
	xrtCancelDestroy(pRequest);
	return bStopped ? 0 : 1;
}
