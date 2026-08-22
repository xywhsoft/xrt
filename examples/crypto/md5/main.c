#include <stdio.h>

#include <xrt.h>



/* 为必须兼容 MD5 的历史协议计算摘要。 */
int main(void)
{
	static const char Hex[] = "0123456789abcdef";
	uint8 Digest[XRT_MD5_SIZE];

	if ( !xrtMd5("abc", 3u, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		putchar(Hex[Digest[i] >> 4u]);
		putchar(Hex[Digest[i] & 0x0Fu]);
	}
	putchar('\n');
	return 0;
}
