#include <stdio.h>

#include <xrt.h>



/* 演示检测结果同时返回编码、BOM 长度和置信度。 */
int main(void)
{
	static const unsigned char arrInput[] = {
		0xEFu, 0xBBu, 0xBFu, 'X', 'R', 'T'
	};
	xencodingguess Guess = xrtEncodingGuess(
		(xbytesview){ arrInput, sizeof(arrInput) });

	printf("encoding=%d bom=%llu confidence=%u\n", (int)Guess.Encoding,
		(unsigned long long)Guess.BomSize, (unsigned int)Guess.Confidence);
	return Guess.Encoding == XENCODING_UTF8 ? 0 : 1;
}
