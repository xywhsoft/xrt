#include <stdio.h>

#include <xregex.h>



/* 演示一次编译多个分类规则并读取全部命中索引。 */
int main(void)
{
	const xstrview arrPattern[] = {
		{ "error", 5u },
		{ "timeout", 7u },
		{ "disk", 4u }
	};
	xregexset* pSet = xrtRegexSetCompile(arrPattern, 3u);
	xregexsetmatcher* pMatcher;
	xregexresult Result;

	if ( pSet == NULL ) {
		return 1;
	}
	pMatcher = xrtRegexSetMatcherCreate(pSet);
	xrtRegexSetRelease(pSet);
	if ( pMatcher == NULL ) {
		return 2;
	}
	Result = xrtRegexSetMatcherMatch(
		pMatcher,
		XRT_STR_LITERAL("disk timeout"),
		0
	);
	if ( Result == XREGEX_MATCH ) {
		for ( size_t i = 0; i < xrtRegexSetMatcherCount(pMatcher); i++ ) {
			printf("matched rule %zu\n", xrtRegexSetMatcherIndex(pMatcher, i));
		}
	}
	xrtRegexSetMatcherFree(pMatcher);
	return Result == XREGEX_ERROR ? 3 : 0;
}
