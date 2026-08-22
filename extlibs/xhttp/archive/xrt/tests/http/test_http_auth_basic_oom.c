#include "../test_allocator.h"

#include <xrt/codec.h>
#include <xrt/http_auth.h>



/* 验证 Basic 写出分配失败时不修改调用方输出和长度。 */
static void testHttpBasicWriteOom(void)
{
	char User[180];
	char Password[100];
	unsigned char Output[512];
	unsigned char Before[512];
	size_t iSize = 71u;

	memset(User, 'u', sizeof(User));
	memset(Password, 'p', sizeof(Password));
	memset(Output, 0xA5, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	testRequire(!xrtHttpBasicWrite(
		(xstrview){ User, sizeof(User) },
		(xstrview){ Password, sizeof(Password) },
		Output,
		sizeof(Output),
		&iSize
	) && (iSize == 71u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Basic writer OOM was not atomic");
	xrtClearError();
}



/* 验证 Basic 解码分配失败时不泄露部分结果。 */
static void testHttpBasicReadOom(void)
{
	char Plain[281];
	char Value[512];
	unsigned char Output[300];
	unsigned char Before[300];
	xhttpbasicauth Basic;
	size_t iEncoded;
	size_t iSize = 73u;

	memset(Plain, 'u', 180u);
	Plain[180] = ':';
	memset(Plain + 181u, 'p', 100u);
	memcpy(Value, "Basic ", 6u);
	testRequire(xrtBase64Encode(
		Plain, sizeof(Plain),
		Value + 6u, sizeof(Value) - 6u, &iEncoded, NULL
	), "HTTP Basic OOM fixture encoding failed");
	memset(Output, 0x5A, sizeof(Output));
	memcpy(Before, Output, sizeof(Output));
	memset(&Basic, 0xA5, sizeof(Basic));
	testRequire(!xrtHttpBasicRead(
		(xstrview){ Value, iEncoded + 6u },
		Output,
		sizeof(Output),
		&iSize,
		&Basic
	) && (iSize == 73u) &&
		(Basic.User.Data == NULL) &&
		(Basic.User.Size == 0u) &&
		(Basic.Password.Data == NULL) &&
		(Basic.Password.Size == 0u) &&
		(memcmp(Output, Before, sizeof(Output)) == 0) &&
		(xrtErrorKind(xrtGetError()) == XERR_MEMORY),
		"HTTP Basic reader OOM was not atomic");
}



int main(void)
{
	testRequire(testInstallFailAllocator(),
		"HTTP Basic failure allocator install failed");
	testHttpBasicWriteOom();
	testHttpBasicReadOom();
	puts("[PASS] HTTP Basic authentication OOM");
	return 0;
}
