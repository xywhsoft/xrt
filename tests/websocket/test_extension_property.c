#include "../test.h"



/* 验证长扩展列表保持顺序，并能逐项迭代全部参数。 */
static void testExtensionLongList(void)
{
	char Text[32768];
	xwsextension Extension;
	xhttpparam Param;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iText = 0;
	size_t iCount;

	for ( size_t i = 0; i < 256u; i++ ) {
		int iWritten = snprintf(
			Text + iText,
			sizeof(Text) - iText,
			"%s, ext%u; p=v%u; q=\"v\\%u\"",
			i == 0 ? "" : ",",
			(unsigned int)i,
			(unsigned int)i,
			(unsigned int)i
		);

		testRequire(
			(iWritten > 0) &&
			((size_t)iWritten < (sizeof(Text) - iText)),
			"WebSocket long extension fixture overflow"
		);
		iText += (size_t)iWritten;
	}
	testRequire(
		xrtWsExtensionCount(
			(xstrview){ Text, iText },
			&iCount
		) &&
		(iCount == 256u),
		"WebSocket long extension count mismatch"
	);

	for ( size_t i = 0; i < 256u; i++ ) {
		char Name[16];
		int iName;
		size_t iParam = 0;
		size_t iParamCount = 0;

		iName = snprintf(
			Name,
			sizeof(Name),
			"ext%u",
			(unsigned int)i
		);
		Next = xrtWsExtensionNext(
			(xstrview){ Text, iText },
			&iOffset,
			&Extension
		);
		testRequire(
			(Next == XHTTP_NEXT_ITEM) &&
			(iName > 0) &&
			(Extension.Name.Size == (size_t)iName) &&
			(memcmp(
				Extension.Name.Data,
				Name,
				(size_t)iName
			) == 0),
			"WebSocket long extension order mismatch"
		);
		for ( ;; ) {
			Next = xrtWsExtensionParamNext(
				&Extension,
				&iParam,
				&Param
			);
			if ( Next == XHTTP_NEXT_END ) {
				break;
			}
			testRequire(
				(Next == XHTTP_NEXT_ITEM) &&
				(((Param.Flags & XHTTP_PARAM_HAS_VALUE) == 0) ||
				 xrtHttpParamTokenValid(&Param)),
				"WebSocket long extension parameter mismatch"
			);
			iParamCount++;
		}
		testRequire(
			iParamCount == 2u,
			"WebSocket long extension parameter count mismatch"
		);
	}
	testRequire(
		xrtWsExtensionNext(
			(xstrview){ Text, iText },
			&iOffset,
			&Extension
		) == XHTTP_NEXT_END,
		"WebSocket long extension list did not end"
	);
}



/* 验证写出结果能够被同一严格解析器无损读取。 */
static void testExtensionWriteRoundTrip(void)
{
	char Output[128];

	for ( size_t i = 0; i < 10000u; i++ ) {
		char Name[24];
		char Parameters[64];
		xwsextension Extension;
		size_t iName;
		size_t iParameters;
		size_t iOffset = 0;
		size_t iSize;
		int iWritten;

		iWritten = snprintf(
			Name,
			sizeof(Name),
			"x-ext-%u",
			(unsigned int)i
		);
		testRequire(iWritten > 0,
			"WebSocket property name write failed");
		iName = (size_t)iWritten;
		iWritten = snprintf(
			Parameters,
			sizeof(Parameters),
			"mode=v%u; escaped=\"v\\%u\"",
			(unsigned int)i,
			(unsigned int)i
		);
		testRequire(iWritten > 0,
			"WebSocket property parameter write failed");
		iParameters = (size_t)iWritten;
		testRequire(
			xrtWsExtensionWrite(
				(xstrview){ Name, iName },
				(xstrview){ Parameters, iParameters },
				Output,
				sizeof(Output),
				&iSize
			) &&
			(xrtWsExtensionNext(
				(xstrview){ Output, iSize },
				&iOffset,
				&Extension
			) == XHTTP_NEXT_ITEM) &&
			(Extension.Name.Size == iName) &&
			(memcmp(Extension.Name.Data, Name, iName) == 0) &&
			(xrtWsExtensionNext(
				(xstrview){ Output, iSize },
				&iOffset,
				&Extension
			) == XHTTP_NEXT_END),
			"WebSocket extension write round-trip failed"
		);
	}
}



/* 执行 WebSocket 扩展性质测试。 */
int main(void)
{
	testExtensionLongList();
	testExtensionWriteRoundTrip();
	printf("[PASS] websocket_extension_property\n");
	return 0;
}
