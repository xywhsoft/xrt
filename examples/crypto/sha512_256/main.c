#include <stdio.h>

#include <xrt.h>



/* 计算并输出一段文本的 SHA-512/256 摘要。 */
int main(void)
{
	static const char Hex[] = "0123456789abcdef";
	uint8 Digest[XRT_SHA512_256_SIZE];

	if ( !xrtSha512_256("abc", 3u, Digest) ) {
		return 1;
	}
	for ( size_t i = 0; i < sizeof(Digest); i++ ) {
		putchar(Hex[Digest[i] >> 4u]);
		putchar(Hex[Digest[i] & 0x0Fu]);
	}
	putchar('\n');
	return 0;
}
