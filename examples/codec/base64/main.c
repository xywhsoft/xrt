/*
 * 范例：codec/base64 —— 标准与分配型两种 Base64 编解码姿势
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtBase64Encode      编码到调用方缓冲（容量不足不写半个结果）
 *   xrtBase64DecodeNew   解码为新的拥有式字节缓冲
 *   （同族还有 EncodeNew / Decode / URL-safe 与自定义字母表变体）
 * 模块宏：XRT_MODULE_CODEC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/codec/base64/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xrt codec -> eHJ0IGNvZGVj
 *
 * 容量规则：4 × ceil(n/3)。9 字节 → 12 字符（无填充）。
 * Encode 是"原子"的：缓冲不够时返回 false 且不写半个结果，
 * 不会留下半截可误用的输出。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	/* sizeof 含结尾零，编码长度用 sizeof-1 显式排除。 */
	static const char Message[] = "xrt codec";
	char Encoded[32];         /* 调用方缓冲：12 字符 + 零足够 */
	bytes pDecoded;           /* 拥有式：解码产物 */
	size_t iEncodedSize;
	size_t iDecodedSize;

	/*
	 * 编码到自有缓冲：
	 *   参数依次 源/源长/出缓冲/容量/出长度/出错码（可 NULL）。
	 * 返回 false 的两种情况：容量不足或参数非法——都能安全重试。
	 */
	if ( !xrtBase64Encode(
		Message, sizeof(Message) - 1u, Encoded, sizeof(Encoded),
		&iEncodedSize, NULL
	) ) {
		return 1;
	}

	/*
	 * 分配型解码：直接返回拥有式缓冲，长度写入出参。
	 * 校验三件事：非空 / 长度等于原文 / 内容逐字节一致——
	 * 证明编码往返无损（Base64 只改表示不改内容）。
	 */
	pDecoded = xrtBase64DecodeNew(
		Encoded, iEncodedSize, &iDecodedSize, NULL
	);
	if ( (pDecoded == NULL) || (iDecodedSize != sizeof(Message) - 1u) ||
		(memcmp(pDecoded, Message, iDecodedSize) != 0) ) {
		xrtFree(pDecoded);
		return 2;
	}
	printf("%s -> %s\n", Message, Encoded);
	xrtFree(pDecoded);
	return 0;
}
