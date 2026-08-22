#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须保留带置信度的编码检测。 */
int main(void)
{
	static const unsigned char arrText[] = { 'A', 0, 'B', 0 };
	xencodingguess Guess = xrtEncodingGuess(
		(xbytesview){ arrText, sizeof(arrText) });

	return Guess.Encoding == XENCODING_UTF16_LE ? 0 : 1;
}
