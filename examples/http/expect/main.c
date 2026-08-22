#include <xrt.h>

#include <stdio.h>



/* 迭代标准和扩展 expectation，并执行服务器支持分类。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("100-continue") },
		{ XRT_STR_INIT("Expect"), XRT_STR_INIT("feature=on") }
	};
	xhttpexpectfieldcursor Cursor;
	xhttpexpectation Expectation;

	xrtHttpExpectFieldCursorInit(&Cursor);
	while ( xrtHttpExpectFieldNext(
		Fields, 2u, &Cursor, &Expectation
	) == XHTTP_NEXT_ITEM ) {
		printf(
			"expectation: %.*s\n",
			(int)Expectation.Name.Size,
			Expectation.Name.Data
		);
	}
	printf(
		"supported: %s\n",
		xrtHttpExpectFields(Fields, 2u) ==
			XHTTP_EXPECT_CONTINUE ? "yes" : "no"
	);
	return 0;
}
