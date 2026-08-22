#include <xmail.h>



/* 把已选中邮箱中的一封 IMAP 消息解析为拥有型 MIME 树。 */
bool fetchMessage(
	ximapclient* pClient,
	uint32 iUid,
	xmailtree* pTree,
	xdeadline iDeadline
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
		pTree,
		iDeadline,
		NULL
	);
}



/* 示例由宿主提供已经认证并选中邮箱的 Client。 */
int main(void)
{
	return 0;
}
