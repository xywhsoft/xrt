#include "../test.h"



/* 无效调用不会隐式创建客户端或执行网络操作。 */
static bool testImapMessageDiscard(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}



/* 验证消息便利层的无副作用参数边界。 */
int main(void)
{
	testRequire(!xrtImapClientBodyWrite(
		NULL,
		1u,
		XRT_STR_LITERAL(""),
		false,
		true,
		1024u,
		testImapMessageDiscard,
		NULL,
		NULL,
		0,
		NULL
	) && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"IMAP BODY accepted a missing client");
	xrtClearError();
	testRequire(xrtImapClientBodyBytes(
		NULL,
		0,
		XRT_STR_LITERAL("HEADER] UID FETCH 1 BODY["),
		true,
		true,
		1024u,
		NULL,
		0,
		NULL
	) == NULL && (xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"IMAP BODY accepted an invalid message or section");
	xrtClearError();
	return 0;
}
