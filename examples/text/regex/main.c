#include <stdio.h>

#include <xrt.h>



/* 演示不可变表达式、可复用 matcher 和命名捕获。 */
int main(void)
{
	xregex* pRegex = xrtRegexCompile(
		XRT_STR_LITERAL("(?<name>[A-Za-z]+)=(?<value>\\d+)")
	);
	xregexmatcher* pMatcher;
	xregexcapture Name;
	xregexcapture Value;
	xstrview Text = XRT_STR_LITERAL("width=128 height=72");
	str sLiteral;
	size_t iLiteralSize;

	if ( pRegex == NULL ) {
		return 1;
	}
	pMatcher = xrtRegexMatcherCreate(pRegex);
	xrtRegexRelease(pRegex);
	if ( pMatcher == NULL ) {
		return 2;
	}
	for ( xregexresult Result = xrtRegexMatcherFind(pMatcher, Text, 0);
		 Result == XREGEX_MATCH;
		 Result = xrtRegexMatcherNext(pMatcher) ) {
		if ( !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("name"), &Name) ||
			 !xrtRegexMatcherCaptureNamed(pMatcher, XRT_STR_LITERAL("value"), &Value) ) {
			xrtRegexMatcherFree(pMatcher);
			return 3;
		}
		printf(
			"%.*s = %.*s\n",
			(int)Name.Text.Size,
			Name.Text.Data,
			(int)Value.Text.Size,
			Value.Text.Data
		);
	}
	xrtRegexMatcherFree(pMatcher);

	sLiteral = xrtRegexEscape(XRT_STR_LITERAL("file[1].txt"), &iLiteralSize);
	if ( sLiteral == NULL ) {
		return 4;
	}
	pRegex = xrtRegexCompile(xrtStrViewN(sLiteral, iLiteralSize));
	xrtFree(sLiteral);
	if ( pRegex == NULL ) {
		return 5;
	}
	if ( xrtRegexFullTest(pRegex, XRT_STR_LITERAL("file[1].txt")) != XREGEX_MATCH ) {
		xrtRegexRelease(pRegex);
		return 6;
	}
	xrtRegexRelease(pRegex);
	return 0;
}
