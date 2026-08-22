#include "../test.h"

#include <xrt/http_structured.h>



/* 对合法种子逐字节变异，验证所有严格入口保持一致且不会越界。 */
static void testStructuredMutateSeed(cstr sSeed)
{
	char arrValue[256];
	xhttpstructureddictionarymember Dictionary;
	xhttpstructureditem Item;
	xhttpstructuredmember Member;
	xstrview Value;
	size_t iLength = strlen(sSeed);
	size_t iOffset;
	size_t i;
	unsigned int j;

	testRequire(
		iLength < sizeof(arrValue),
		"structured mutation seed is too long"
	);
	for ( i = 0; i < iLength; i++ ) {
		for ( j = 0; j < 128u; j++ ) {
			memcpy(arrValue, sSeed, iLength);
			arrValue[i] = (char)j;
			Value = (xstrview){ arrValue, iLength };

			xrtClearError();
			if ( xrtHttpStructuredListValid(Value) ) {
				iOffset = 0;
				while ( xrtHttpStructuredListNext(
					Value, &iOffset, &Member
				) == XHTTP_NEXT_ITEM ) {
				}
				testRequire(
					iOffset == Value.Size,
					"structured valid List failed iteration"
				);
			}

			xrtClearError();
			if ( xrtHttpStructuredDictionaryValid(Value) ) {
				iOffset = 0;
				while ( xrtHttpStructuredDictionaryNext(
					Value, &iOffset, &Dictionary
				) == XHTTP_NEXT_ITEM ) {
				}
				testRequire(
					iOffset == Value.Size,
					"structured valid Dictionary failed iteration"
				);
			}

			xrtClearError();
			(void)xrtHttpStructuredItemParse(Value, &Item);
		}
	}
}



/* 运行包含全部结构和转义类型的确定性变异测试。 */
int main(void)
{
	static const cstr Seeds[] = {
		"a, (\"foo\";x=1 :YQ=:);lvl=5, %\"%c3%bc\"",
		"a=?0, b, c;foo=bar, d=(5 6);valid",
		"@1659578233;when, -12.34;name=\"x\\\"y\""
	};
	size_t i;

	for ( i = 0; i <
		(sizeof(Seeds) / sizeof(Seeds[0])); i++ ) {
		testStructuredMutateSeed(Seeds[i]);
	}
	printf("[PASS] http_structured_mutation\n");
	return 0;
}
