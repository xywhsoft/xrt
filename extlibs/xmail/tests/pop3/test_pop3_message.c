#include "../test.h"



/* 空客户端必须在任何输出或结果修改前失败。 */
static bool testPop3MessageSink(xbytesview Data, ptr pUserData)
{
	(void)Data;
	(void)pUserData;
	return true;
}



/* 验证参数前置检查和 MIME 树预算入口。 */
int main(void)
{
	xmailtreelimits Limits;
	xmailtree Tree;
	size_t iWritten = 99u;

	testRequire(!xrtPop3ClientRetrWrite(
		NULL,
		1u,
		1024u,
		testPop3MessageSink,
		NULL,
		&iWritten,
		0,
		NULL
	) && (iWritten == 99u),
		"POP3 message write modified output before validation");
	xrtClearError();

	xrtMailTreeLimitsInit(&Limits);
	Limits.Flags = UINT32_MAX;
	memset(&Tree, 0, sizeof(Tree));
	Tree.PartCount = 77u;
	testRequire(!xrtPop3ClientRetrTree(
		NULL,
		1u,
		&Limits,
		&Tree,
		0,
		NULL
	) && (Tree.PartCount == 77u) &&
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT),
		"POP3 message tree accepted invalid limits or modified output");
	return 0;
}
