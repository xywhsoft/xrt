#include <stdio.h>

#include <xregex.h>



/* 演示按多个分隔符流式拆分一行文本。 */
int main(void)
{
	xregex* pRegex = xrtRegexCompile(XRT_STR_LITERAL("[,;]\\s*"));
	xregexsplitconfig Config;
	xregexsplitter* pSplitter;
	xregexsplitpart Part;
	xregexresult Result;

	if ( pRegex == NULL ) {
		return 1;
	}
	xrtRegexSplitConfigInit(&Config);
	Config.Flags = XREGEX_SPLIT_SKIP_EMPTY;
	pSplitter = xrtRegexSplitterCreate(
		pRegex,
		XRT_STR_LITERAL("alpha, beta; gamma"),
		&Config
	);
	xrtRegexRelease(pRegex);
	if ( pSplitter == NULL ) {
		return 2;
	}
	while ( (Result = xrtRegexSplitterNext(pSplitter, &Part)) == XREGEX_MATCH ) {
		printf("%.*s\n", (int)Part.Text.Size, Part.Text.Data);
	}
	xrtRegexSplitterFree(pSplitter);
	return Result == XREGEX_ERROR ? 3 : 0;
}
