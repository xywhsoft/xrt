#include "../test.h"
#include "../../src/internal/xrt_http.h"



/* 验证共享语言标签校验严格区分标签、范围、空值和非法分段。 */
int main(void)
{
	size_t iSubtags = 0;

	testRequire(__xrtHttpLanguageTextValid(
		XRT_STR_LITERAL("zh-Hans-CN"), false, false, &iSubtags
	) && (iSubtags == 3u), "language tag validation failed");
	iSubtags = SIZE_MAX;
	testRequire(__xrtHttpLanguageTextValid(
		XRT_STR_LITERAL("*"), true, false, &iSubtags
	) && (iSubtags == 0),
		"language wildcard range validation failed");
	testRequire(__xrtHttpLanguageTextValid(
		(xstrview){ NULL, 0 }, false, true, NULL
	), "empty extended-value language was rejected");
	testRequire(!__xrtHttpLanguageTextValid(
		XRT_STR_LITERAL("zh--CN"), false, false, NULL
	), "empty language subtag was accepted");
	testRequire(!__xrtHttpLanguageTextValid(
		XRT_STR_LITERAL("123-en"), false, false, NULL
	), "numeric primary language subtag was accepted");
	printf("[PASS] http_language_core\n");
	return 0;
}
