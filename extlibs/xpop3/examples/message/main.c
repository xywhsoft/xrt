#include <xpop3.h>

#include <math.h>
static inline double exampleTimerLimit(int64 Timeout)
{
    return Timeout == XRT_WAIT_FOREVER ? INFINITY : xrtTimer() + (double)Timeout / 1000.0;
}
static inline bool exampleTimerExpired(double Limit)
{
    return xrtTimer() >= Limit;
}
static inline int64 exampleTimerRemaining(double Limit)
{
    double Ms;
    if (Limit == INFINITY) return XRT_WAIT_FOREVER;
    Ms = ceil((Limit - xrtTimer()) * 1000.0);
    return Ms <= 0 ? 0 : Ms >= 0x1p63 ? INT64_MAX : (int64)Ms;
}



/* 把一封 POP3 邮件直接解析为拥有型 MIME 树。 */
bool fetchMessage(
	xpop3client* pClient,
	uint64 iMessage,
	xmailtree* pTree,
	double iDeadline
)
{
	xmailtreelimits Limits;

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxSourceBytes = 16u * 1024u * 1024u;
	Limits.MaxDecodedBytes = 32u * 1024u * 1024u;
	return xrtPop3ClientRetrTree(
		pClient,
		iMessage,
		&Limits,
		pTree,exampleTimerRemaining(iDeadline),
		NULL
	);
}



/* 示例由宿主提供已经认证并处于 TRANSACTION 状态的 Client。 */
int main(void)
{
	return 0;
}
