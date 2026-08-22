#include <stdio.h>

#include <xmail.h>



/* 编码一个 UTF-8 文本正文并立即解码。 */
int main(void)
{
	static const char sText[] = "Hello, xlang.\n邮件正文。";
	size_t iEncodedSize;
	size_t iDecodedSize;
	str sEncoded = xrtMailQp(
		sText,
		sizeof(sText) - 1u,
		0,
		XMAIL_QP_TEXT,
		&iEncodedSize
	);
	bytes pDecoded;
	xstrview Encoded;

	if ( sEncoded == NULL ) {
		return 1;
	}
	Encoded.Data = sEncoded;
	Encoded.Size = iEncodedSize;
	pDecoded = xrtMailQpDecode(
		Encoded,
		0,
		&iDecodedSize
	);
	if ( pDecoded == NULL ) {
		xrtFree(sEncoded);
		return 2;
	}
	printf("%s\n", sEncoded);
	printf("decoded bytes: %zu\n", iDecodedSize);
	xrtFree(pDecoded);
	xrtFree(sEncoded);
	return 0;
}
