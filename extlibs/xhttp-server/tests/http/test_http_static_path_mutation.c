#include "../test.h"



/* 把一个字节写成大写 percent 转义。 */
static void testHttpStaticPathEscape(
	uint8 iValue,
	char sEscape[4]
)
{
	static const char Hex[] = "0123456789ABCDEF";

	sEscape[0] = '%';
	sEscape[1] = Hex[iValue >> 4u];
	sEscape[2] = Hex[iValue & 0x0Fu];
	sEscape[3] = 0;
}



/* 穷举单字节 percent 解码，锁定结构字节、控制字节和 UTF-8 边界。 */
int main(void)
{
	xhttpstaticpathconfig Config;
	char Raw[5] = "/";
	char Output[8];
	size_t iSize;
	bool bTrailingSlash;
	uint32 i;

	xrtHttpStaticPathConfigInit(&Config);
	Config.Flags = XHTTP_STATIC_PATH_ALLOW_HIDDEN;
	for ( i = 0; i <= UINT8_MAX; i++ ) {
		bool bExpected = (i >= 0x20u) && (i <= 0x7Eu) &&
			(i != (uint32)'.') &&
			(i != (uint32)'/') &&
			(i != (uint32)'\\');
		xhttpstaticpathstatus Status;

		testHttpStaticPathEscape((uint8)i, Raw + 1);
		Status = xrtHttpStaticPathWrite(
			(xstrview){ Raw, 4u },
			&Config,
			Output,
			sizeof(Output),
			&iSize,
			&bTrailingSlash
		);
		testRequire(
			(Status == XHTTP_STATIC_PATH_MATCH) == bExpected,
			"HTTP static percent byte classification mismatch"
		);
		if ( bExpected ) {
			testRequire((iSize == 1u) &&
				((uint8)Output[0] == (uint8)i) &&
				(Output[1] == 0) &&
				!bTrailingSlash,
				"HTTP static percent byte output mismatch");
		} else {
			xrtClearError();
		}
	}
	printf("[PASS] http_static_path_mutation\n");
	return 0;
}
