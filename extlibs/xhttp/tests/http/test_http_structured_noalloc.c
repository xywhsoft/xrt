#include "../test_allocator.h"

#include <xrt/http_structured.h>



/* Structured Fields 的解析、查找和缓冲解码必须保持零堆分配。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Example"),
			XRT_STR_INIT("a;v=1, (\"b\" :YQ:)")
		},
		{
			XRT_STR_INIT("example"),
			XRT_STR_INIT("%\"%c3%bc\"")
		}
	};
	xhttpstructuredfieldcursor Cursor;
	xhttpstructuredmember Member;
	xhttpstructuredparameter Parameter;
	char arrOutput[8];
	size_t iSize;

	testRequire(
		testInstallFailAllocator(),
		"structured failure allocator install failed"
	);
	xrtHttpStructuredFieldCursorInit(&Cursor);
	testRequire(
		(xrtHttpStructuredListFieldNext(
			Fields, 2, XRT_STR_LITERAL("Example"),
			&Cursor, &Member
		) == XHTTP_NEXT_ITEM) &&
		(xrtHttpStructuredParameterFind(
			Member.Parameters, XRT_STR_LITERAL("v"),
			&Parameter
		) == XHTTP_NEXT_ITEM) &&
		(Parameter.Value.Number == 1),
		"structured parameter parsing allocated"
	);
	while ( xrtHttpStructuredListFieldNext(
		Fields, 2, XRT_STR_LITERAL("Example"),
		&Cursor, &Member
	) == XHTTP_NEXT_ITEM ) {
		if ( Member.Kind == XHTTP_STRUCTURED_MEMBER_ITEM &&
			(Member.Bare.Type == XHTTP_STRUCTURED_DISPLAY) ) {
			testRequire(
				xrtHttpStructuredDisplayDecode(
					&Member.Bare, arrOutput,
					sizeof(arrOutput), &iSize
				) && (iSize == 2u),
				"structured Display String decode allocated"
			);
		}
	}
	testRequire(
		xrtGetError() == NULL,
		"structured no-allocation iteration failed"
	);
	printf("[PASS] http_structured_noalloc\n");
	return 0;
}
