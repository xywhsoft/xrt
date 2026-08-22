#include "../test.h"
#include "../test_allocator.h"



/* 所有分配型 Unicode 转换必须完整传播内存不足。 */
int main(void)
{
	static const uint16 arrText[] = { 't', 'e', 'x', 't' };
	uint16* pText;

	testRequire(testInstallFailAllocator(), "failed to install Unicode OOM allocator");
	pText = xrtUtf16DupView(xrtUtf16View(arrText, 4));
	testRequire(pText == NULL, "Unicode duplicate ignored allocation failure");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Unicode duplicate did not preserve OOM error");
	xrtClearError();
	pText = xrtUtf8To16("text", NULL);
	testRequire(pText == NULL, "Unicode conversion ignored allocation failure");
	testRequire((xrtGetError() != NULL) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"Unicode conversion did not preserve OOM error");
	return 0;
}
