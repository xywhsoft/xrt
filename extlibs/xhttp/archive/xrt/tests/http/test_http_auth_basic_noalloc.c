#include "../test_allocator.h"

#include <xrt/http_auth.h>



/* 常见 Basic 编码、解码、查询和 challenge 不得依赖堆分配。 */
int main(void)
{
	char User[120];
	char Password[60];
	char Value[256];
	char Decoded[192];
	char ChallengeValue[64];
	char Realm[16];
	xhttpbasicauth Basic;
	xhttpbasicchallenge Challenge;
	size_t iSize;
	size_t iDecoded;

	memset(User, 'u', sizeof(User));
	memset(Password, 'p', sizeof(Password));
	testRequire(testInstallFailAllocator(),
		"HTTP Basic failure allocator install failed");
	testRequire(xrtHttpBasicWrite(
		(xstrview){ User, sizeof(User) },
		(xstrview){ Password, sizeof(Password) },
		Value, sizeof(Value), &iSize
	), "HTTP Basic common writer allocated");
	testRequire(xrtHttpBasicRead(
		(xstrview){ Value, iSize },
		NULL, 0, &iDecoded, &Basic
	) && (iDecoded == 181u) &&
		(Basic.User.Data == NULL) &&
		(Basic.Password.Data == NULL),
		"HTTP Basic common query allocated");
	testRequire(xrtHttpBasicRead(
		(xstrview){ Value, iSize },
		Decoded, sizeof(Decoded), &iDecoded, &Basic
	) && (Basic.User.Size == sizeof(User)) &&
		(Basic.Password.Size == sizeof(Password)),
		"HTTP Basic common reader allocated");
	testRequire(xrtHttpBasicChallengeWrite(
		XRT_STR_LITERAL("api"), true,
		ChallengeValue, sizeof(ChallengeValue), &iSize
	), "HTTP Basic challenge writer allocated");
	testRequire(xrtHttpBasicChallengeRead(
		(xstrview){ ChallengeValue, iSize },
		NULL, 0, &iDecoded, &Challenge
	) && (iDecoded == 3u) && Challenge.Utf8 &&
		(Challenge.Realm.Data == NULL),
		"HTTP Basic challenge query allocated");
	testRequire(xrtHttpBasicChallengeRead(
		(xstrview){ ChallengeValue, iSize },
		Realm, sizeof(Realm), &iDecoded, &Challenge
	) && Challenge.Utf8 && (Challenge.Realm.Size == 3u) &&
		(memcmp(Challenge.Realm.Data, "api", 3u) == 0),
		"HTTP Basic challenge reader allocated");
	puts("[PASS] HTTP Basic authentication no allocation");
	return 0;
}
