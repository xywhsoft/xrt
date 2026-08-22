#include "../test.h"



/* BOM 是唯一可以给出确定编码结论的检测输入。 */
static void testBomDetection(void)
{
	static const unsigned char arrData[] = { 0x00u, 0x00u, 0xFEu, 0xFFu, 0, 0, 0, 'A' };
	xencodingguess Guess = xrtEncodingGuess(
		(xbytesview){ arrData, sizeof(arrData) });

	testRequire((Guess.Encoding == XENCODING_UTF32_BE) &&
		(Guess.BomSize == 4) && (Guess.Confidence == 100),
		"BOM detection is not certain");
}



/* 无 BOM 的宽编码仅在零字节分布和严格合法性同时成立时判断。 */
static void testWideHeuristics(void)
{
	static const unsigned char arrUtf16Le[] = { 'A', 0, 'B', 0, 'C', 0 };
	static const unsigned char arrUtf16Be[] = { 0, 'A', 0, 'B', 0, 'C' };
	static const unsigned char arrUtf32Le[] = { 'A', 0, 0, 0, 'B', 0, 0, 0 };
	xencodingguess Guess;

	Guess = xrtEncodingGuess((xbytesview){ arrUtf16Le, sizeof(arrUtf16Le) });
	testRequire((Guess.Encoding == XENCODING_UTF16_LE) &&
		(Guess.Confidence >= 80), "UTF-16 LE heuristic failed");
	Guess = xrtEncodingGuess((xbytesview){ arrUtf16Be, sizeof(arrUtf16Be) });
	testRequire((Guess.Encoding == XENCODING_UTF16_BE) &&
		(Guess.Confidence >= 80), "UTF-16 BE heuristic failed");
	Guess = xrtEncodingGuess((xbytesview){ arrUtf32Le, sizeof(arrUtf32Le) });
	testRequire((Guess.Encoding == XENCODING_UTF32_LE) &&
		(Guess.Confidence >= 80), "UTF-32 LE heuristic failed");
}



/* UTF-8 置信度区分明确多字节文本和编码不唯一的纯 ASCII。 */
static void testUtf8Confidence(void)
{
	static const unsigned char arrUtf8[] = { 0xE4u, 0xBDu, 0xA0u };
	static const unsigned char arrBinary[] = { 0xFFu, 0x01u, 0xFEu };
	xencodingguess Guess;

	Guess = xrtEncodingGuess((xbytesview){ arrUtf8, sizeof(arrUtf8) });
	testRequire((Guess.Encoding == XENCODING_UTF8) &&
		(Guess.Confidence == 90), "non-ASCII UTF-8 confidence is wrong");
	Guess = xrtEncodingGuess((xbytesview){ (cbytes)"ASCII", 5 });
	testRequire((Guess.Encoding == XENCODING_UTF8) &&
		(Guess.Confidence == 40), "ASCII ambiguity is not represented");
	Guess = xrtEncodingGuess((xbytesview){ arrBinary, sizeof(arrBinary) });
	testRequire((Guess.Encoding == XENCODING_UNKNOWN) &&
		(Guess.Confidence == 0), "unknown binary data was over-classified");
}



/* 极短的带零样本不具备足够统计信息，不能冒充高置信度宽编码。 */
static void testShortSampleAmbiguity(void)
{
	static const unsigned char arrOneUtf16[] = { 'A', 0 };
	static const unsigned char arrOneUtf32[] = { 'A', 0, 0, 0 };
	xencodingguess Guess;

	Guess = xrtEncodingGuess(
		(xbytesview){ arrOneUtf16, sizeof(arrOneUtf16) });
	testRequire((Guess.Encoding != XENCODING_UTF16_LE) &&
		(Guess.Encoding != XENCODING_UTF16_BE),
		"one code unit was over-classified as UTF-16");
	testRequire(Guess.Confidence <= 50u,
		"one UTF-16-like code unit received excessive confidence");

	Guess = xrtEncodingGuess(
		(xbytesview){ arrOneUtf32, sizeof(arrOneUtf32) });
	testRequire((Guess.Encoding != XENCODING_UTF32_LE) &&
		(Guess.Encoding != XENCODING_UTF32_BE),
		"one code unit was over-classified as UTF-32");
	testRequire(Guess.Confidence <= 50u,
		"one UTF-32-like code unit received excessive confidence");
}



/* 执行编码检测的确定结果和启发式结果测试。 */
int main(void)
{
	testBomDetection();
	testWideHeuristics();
	testUtf8Confidence();
	testShortSampleAmbiguity();
	return 0;
}
