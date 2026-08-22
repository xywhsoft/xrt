#include <xmail.h>



/* 把一封 POP3 邮件直接解析为拥有型 MIME 树。 */
bool fetchMessage(
	xpop3client* pClient,
	uint64 iMessage,
	xmailtree* pTree,
	xdeadline iDeadline
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
		pTree,
		iDeadline,
		NULL
	);
}



/* 示例由宿主提供已经认证并处于 TRANSACTION 状态的 Client。 */
int main(void)
{
	return 0;
}
