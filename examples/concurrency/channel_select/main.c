#include <stdio.h>

#include <xrt.h>



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
