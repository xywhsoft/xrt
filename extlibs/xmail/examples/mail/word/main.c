#include <stdio.h>
#include <xmail.h>



/* 展示 UTF-8 主题的编码与解码。 */
int main(void)
{
	str sEncoded;
	str sDecoded;
	size_t iSize;
	xstrview Encoded;

	sEncoded = xrtMailWordEncode(
		XRT_STR_LITERAL("发布说明：网络底座已更新"),
		XMAIL_WORD_BASE64,
		&iSize
	);
	if ( sEncoded == NULL ) {
		return 1;
	}
	printf("%s\n", sEncoded);
	Encoded.Data = sEncoded;
	Encoded.Size = iSize;
	sDecoded = xrtMailWordDecode(
		Encoded,
		XMAIL_WORD_STRICT,
		NULL
	);
	xrtFree(sEncoded);
	if ( sDecoded == NULL ) {
		return 2;
	}
	printf("%s\n", sDecoded);
	xrtFree(sDecoded);
	return 0;
}
