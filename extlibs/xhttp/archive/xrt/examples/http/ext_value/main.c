#include <stdio.h>

#include <xrt.h>



/* 编码并读取可直接用于 HTTP 扩展参数的 UTF-8 文件名。 */
int main(void)
{
	static const uint8 Name[] = {
		0xE4, 0xB8, 0xAD, 0xE6, 0x96, 0x87, '.', 't', 'x', 't'
	};
	xhttpextvalue Value;
	char Encoded[64];
	char Decoded[32];
	size_t iSize;

	if ( !xrtHttpExtValueWrite(
		XRT_STR_LITERAL(""), (xstrview){ NULL, 0 },
		(xbytesview){ Name, sizeof(Name) },
		Encoded, sizeof(Encoded), &iSize
	) ) {
		return 1;
	}
	printf("encoded = %.*s\n", (int)iSize, Encoded);
	if ( !xrtHttpExtValueParse(
		(xstrview){ Encoded, iSize }, &Value
	) || !xrtHttpExtValueRead(
		&Value, Decoded, sizeof(Decoded), &iSize
	) ) {
		return 2;
	}
	printf("decoded = %.*s\n", (int)iSize, Decoded);
	return 0;
}
