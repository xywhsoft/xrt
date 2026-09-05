#include <stdio.h>

#include <xrt.h>



/*
 * 范例：concurrency/channel_cancel —— 可取消的通道接收
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChannelRecvCancel   接收 + 取消令牌二合一等待
 *   XWAIT_CANCELLED        取消触发时的等待结果
 * 模块宏：XRT_MODULE_CHANNEL（依赖 CANCEL）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/channel_cancel/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   cancelled: yes
 *
 * 死等通道的解法：RecvCancel 把"等数据"与"等取消"
 *   合成一次等待，先到者胜——停机排空的标准姿势。
 */


/* 演示用一个取消令牌中断无限 Channel 接收。 */
int main(void)
{
	xchannel tChannel;
	xcancel* pCancel;
	ptr pItem = NULL;
	xwaitresult iResult;

	if ( !xrtChannelInit(&tChannel, 1u) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( pCancel == NULL ) {
		(void)xrtChannelUnit(&tChannel);
		return 2;
	}
	if ( !xrtCancelRequest(pCancel) ) {
		xrtCancelDestroy(pCancel);
		(void)xrtChannelUnit(&tChannel);
		return 3;
	}

	iResult = xrtChannelRecvCancel(&tChannel, &pItem, pCancel);
	printf("cancelled: %s\n", iResult == XWAIT_CANCELLED ? "yes" : "no");

	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&tChannel) ) {
		return 4;
	}
	return iResult == XWAIT_CANCELLED ? 0 : 5;
}
