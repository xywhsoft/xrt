#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* Bearer 谓词、读写和借用结果不得依赖堆分配。 */
int main(void)
{
	char Output[64];
	xstrview Token;
	size_t iSize;

	testRequire(testInstallFailAllocator(),
		"HTTP Bearer failure allocator install failed");
	testRequire(xrtHttpBearerTokenValid(
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM")
	), "HTTP Bearer token predicate allocated");
	testRequire(xrtHttpBearerWrite(
		XRT_STR_LITERAL("mF_9.B5f-4.1JqM"),
		Output, sizeof(Output), &iSize
	), "HTTP Bearer writer allocated");
	testRequire(xrtHttpBearerRead(
		(xstrview){ Output, iSize }, &Token
	) && (Token.Data == (Output + 7u)) &&
		(Token.Size == 15u),
		"HTTP Bearer reader allocated or copied its token");
	puts("[PASS] HTTP Bearer authentication no allocation");
	return 0;
}
