/*
 * 范例：charset/detect —— 编码探测：同时给出编码、BOM 与置信度
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtEncodingGuess  对字节序列做编码猜测，一次返回三项结论
 *   xencodingguess    结果结构：Encoding / BomSize / Confidence
 *   XENCODING_UTF8    编码枚举（=1；另有 UTF16_LE/BE、UTF32、LATIN1 等）
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/charset/detect/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   encoding=1 bom=3 confidence=100
 *
 * 判定策略（charset.h）：
 *   - BOM 存在时直接定性（本例 UTF-8 BOM = EF BB BF，3 字节，置信 100）；
 *   - 无 BOM 时按 UTF-8 合法性/零字节分布等启发式给出较低置信度，
 *     调用方应把 Confidence 当作"建议"而非承诺。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* UTF-8 BOM（EF BB BF）后跟 ASCII "XRT"：最典型的带签名 UTF-8。 */
	static const unsigned char arrInput[] = {
		0xEFu, 0xBBu, 0xBFu, 'X', 'R', 'T'
	};

	/*
	 * 一次调用返回完整结论结构（按值返回，零分配）：
	 *   Encoding   —— 最可能的编码枚举；
	 *   BomSize    —— 检测到的 BOM 字节数（无 BOM 为 0），
	 *                 解码前可用它跳过签名；
	 *   Confidence —— 0..100，BOM 定性时为 100。
	 */
	xencodingguess Guess = xrtEncodingGuess(
		(xbytesview){ arrInput, sizeof(arrInput) });

	printf("encoding=%d bom=%llu confidence=%u\n", (int)Guess.Encoding,
		(unsigned long long)Guess.BomSize, (unsigned int)Guess.Confidence);

	/* 退出码即断言：探测结论必须是 UTF-8 才算范例成功。 */
	return Guess.Encoding == XENCODING_UTF8 ? 0 : 1;
}
