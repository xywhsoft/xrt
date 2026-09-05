#include <stdio.h>
#include <xrt.h>



/*
 * 范例：concurrency/channel_select_cancel —— 可取消的多路等待
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtChannelSelectCancel   Select + 取消令牌
 * 模块宏：XRT_MODULE_CHANNEL（依赖 CANCEL）
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c $BS}
 *       examples/concurrency/channel_select_cancel/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   select wait result: 2
 *
 * 结果 2 = XWAIT_CANCELLED：两通道皆空时只有取消能唤醒
 *   Select——多路等待同样可被打断，与 channel_cancel
 *   单路版对称。
 */


/* 用取消令牌中止一个尚未就绪的 Channel Select。 */
int main(void)
{
	xchannel Channel;
	xchannelcase Case;
	xchannelselectresult Result;
	xcancel* pCancel;
	ptr pValue = NULL;
	int iResult = 1;

	if ( !xrtChannelInit(&Channel, 0) ) {
		return 1;
	}
	pCancel = xrtCancelCreate();
	if ( (pCancel != NULL) && xrtCancelRequest(pCancel) ) {
		Case = xrtChannelCaseRecv(&Channel, &pValue);
		Result = xrtChannelSelectUntilCancel(
			&Case,
			1u,
			XRT_DEADLINE_NEVER,
			pCancel
		);
		printf("select wait result: %d\n", (int)Result.Wait);
		iResult = (Result.Wait == XWAIT_CANCELLED) ? 0 : 2;
	}
	xrtCancelDestroy(pCancel);
	if ( !xrtChannelUnit(&Channel) && (iResult == 0) ) {
		iResult = 3;
	}
	return iResult;
}
