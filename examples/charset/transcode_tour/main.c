/*
 * 范例：charset/transcode_tour —— 流式校验 + 标量编解码 + BOM 三件套
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtUtf8Valid               严格校验（失败给首错字节位置）
 *   xrtUtf8StateInit / StateFeed / StateError   流式校验状态机
 *   xrtUtf8Decode / Encode     单标量解码/编码
 *   xrtUnicodeScalar           数值是否可编码标量
 *   xrtEncodingBom             识别 BOM
 *   xrtEncodingWriteBom        写出 BOM
 *   xrtEncodingUnitSize        编码单元字节数
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/charset/transcode_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   valid=1 state-feed=OK error-pos=NPOS（无错误）
 *   （首块返回 MORE——跨块汉字的前缀挂起）
 *   decode=U+4F60 encode=3 bytes scalar=1
 *   bom-size=3 unit=2
 *
 * Valid 一次性校验完整缓冲；State 族流式跨块校验
 *   （网络分块到达时用）——StateError 给绝对错误偏移。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xutf8state State;
	uint32 iScalar = 0;
	size_t iRead = 0;
	size_t iError = 0;
	size_t iSize = 0;
	unsigned char Bom[4];

	/* 一次性校验：合法文本。 */
	printf("valid=%d", xrtUtf8Valid(SV("你好"), &iError) ? 1 : 0);

	/* 流式校验：分两块喂，跨块汉字"你"（3 字节拆成 2+1）。 */
	xrtUtf8StateInit(&State);
	{
		static const char A[] = { (char)0xE4, (char)0xBD };
		static const char B[] = { (char)0xA0, 'x' };

		printf(" state-feed=%s",
			(xrtUtf8StateFeed(&State, (xstrview){ A, 2u }, false) ==
			XUTF_MORE &&
			xrtUtf8StateFeed(&State, (xstrview){ B, 2u }, true) ==
			XUTF_OK) ? "OK" : "FAIL");
		printf(" error-pos=%zu\n", xrtUtf8StateError(&State));
	}

	/* 单标量编解码往返。 */
	{
		char Out[4];
		size_t iWrote = 0;

		(void)xrtUtf8Decode(SV("你"), &iScalar, &iRead);
		printf("decode=U+%X", iScalar);
		iWrote = xrtUtf8Encode(iScalar, Out);
		printf(" encode=%zu bytes", iWrote);
		printf(" scalar=%d\n", xrtUnicodeScalar(0x4F60) ? 1 : 0);
	}

	/* BOM 三件套：写 UTF-8 BOM → 识别回读 + 单元大小。 */
	iSize = xrtEncodingWriteBom(XENCODING_UTF8, Bom, sizeof(Bom));
	printf("bom-size=%zu", iSize);
	{
		size_t iBom = 0;
		xencoding Enc = xrtEncodingBom((xbytesview){ Bom, iSize }, &iBom);

		printf(" bom-encoding=%d", (int)Enc);
	}
	printf(" unit=%zu\n", xrtEncodingUnitSize(XENCODING_UTF16_LE));
	return 0;
}
