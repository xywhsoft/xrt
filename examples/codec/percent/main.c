/*
 * 范例：codec/percent —— URL path segment 的百分号编解码
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPercentEncode      按白名单编码到调用方缓冲（unsafe 字符 → %XX）
 *   xrtPercentDecodeNew   解码 %XX 序列为拥有式字节缓冲
 * 模块宏：XRT_MODULE_CODEC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/codec/percent/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   encoded: reports%2FJuly%202026
 *   decoded: reports/July 2026
 *
 * 白名单语义：字母数字默认原样保留，第三参数是"额外保留"字符集；
 *   传空视图 = 除字母数字外全部转义（本例：/ 与空格被转义）；
 *   需要更宽松时可传 XRT_STR_LITERAL("-._~") 这类安全字符集。
 */

#include <stdio.h>

#include <xrt.h>



int main(void)
{
	/* 含路径分隔符与空格的原始 segment——两者都必须被转义。 */
	static const char Segment[] = "reports/July 2026";
	char Encoded[64];        /* 最坏情况 3n：17 字节 → 51 字符，64 够用 */
	bytes pDecoded;
	size_t iEncodedSize;
	size_t iDecodedSize;

	/*
	 * 编码：白名单传空视图——字母数字保留，其余（含 / 与空格）转成 %XX。
	 * 容量不足时原子失败（不写半个结果），与 Base64 Encode 同一约定。
	 */
	if ( !xrtPercentEncode(
		Segment, sizeof(Segment) - 1u, XRT_STR_LITERAL(""),
		Encoded, sizeof(Encoded), &iEncodedSize
	) ) {
		return 1;
	}

	/* 分配型解码：%XX 还原为字节；非法转义（如 %G0）会失败返回 NULL。 */
	pDecoded = xrtPercentDecodeNew(
		(xstrview){ Encoded, iEncodedSize }, &iDecodedSize
	);
	if ( pDecoded == NULL ) {
		return 2;
	}

	/* 解码产物是二进制缓冲：打印时用显式长度，不依赖结尾零。 */
	printf("encoded: %s\ndecoded: %.*s\n",
		Encoded, (int)iDecodedSize, (const char*)pDecoded);
	xrtFree(pDecoded);
	return 0;
}
