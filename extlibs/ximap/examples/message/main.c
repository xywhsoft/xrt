#include <ximap.h>

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



/* 把已选中邮箱中的一封 IMAP 消息解析为拥有型 MIME 树。 */
bool fetchMessage(
	ximapclient* pClient,
	uint32 iUid,
	xmailtree* pTree,
	double iDeadline
)
{
	xmailtreelimits Limits;

	xrtMailTreeLimitsInit(&Limits);
	Limits.MaxSourceBytes = 16u * 1024u * 1024u;
	Limits.MaxDecodedBytes = 32u * 1024u * 1024u;
	return xrtImapClientMessageTree(
		pClient,
		iUid,
		true,
		true,
		&Limits,
		pTree,exampleTimerRemaining(iDeadline),
		NULL
	);
}



/* 示例由宿主提供已经认证并选中邮箱的 Client。 */
int main(void)
{
	return 0;
}
