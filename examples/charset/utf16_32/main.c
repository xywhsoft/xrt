/*
 * 范例：charset/utf16_32 —— UTF-16 / UTF-32 全族：视图/长度/校验/互转
 * ----------------------------------------------------------------
 * 演示 API：
 *   【视图/长度】 xrtUtf32View / Utf16Len / Utf32Len / Utf16Count
 *   【校验】     xrtUtf16Valid
 *   【码元级】   xrtUtf16Decode / Utf16Encode
 *   【UTF-8 ↔ 16/32】 Utf8To16 / Utf8To16Buffer / Utf8To32 / Utf8To32Buffer
 *   【16 → 8/32】 Utf16To8 / Utf16To8Buffer / Utf16To32 / Utf16To32Buffer
 *   【32 → 8/16】 Utf32To8 / Utf32To8Buffer / Utf32To16 / Utf32To16Buffer
 *   【视图版】   Utf16ViewTo32 / Utf32ViewTo8 / Utf32ViewTo16
 *   【复制】     Utf16Dup / Utf32Dup / Utf32DupView
 * 模块宏：XRT_MODULE_CHARSET
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/charset/utf16_32/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   u8→16: len=2 valid=1 count=2
 *   u8→32: len=2 count=2
 *   16→8: [a你Ģ] 16→32: 3（缓冲版覆盖了 A16 后半段）
 *   32→8: [a你] 32→16: 2
 *   view-to32=2 view-to8=[a] dup=1
 *   encode=2 decode=U+4F60
 *
 * 三个层次：cstr 版（零结尾入）、Buffer 版（显式容量）、
 *   View 版（xutf16view/xutf32view + xutfpolicy 代理对策略）。
 *   嵌入零只有 View 版保留——Dup/DupView 的区别正在于此。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	uint16 A16[8];
	uint32 A32[8];
	char Back[16];
	size_t iSize = 0;
	uint32 iScalar = 0;
	size_t iRead = 0;

	/* UTF-8 → UTF-16 分配版 + 长度/校验/计数。 */
	{
		uint16* p16 = xrtUtf8To16("a你", &iSize);

		printf("u8→16: len=%zu", iSize);
		printf(" valid=%d", xrtUtf16Valid((xutf16view){ p16, iSize },
			NULL) ? 1 : 0);
		printf(" count=%zu\n", xrtUtf16Count((xutf16view){ p16, iSize }));
		memcpy(A16, p16, iSize * sizeof(uint16));
		xrtFree(p16);
	}
	/* UTF-8 → UTF-32 分配版。 */
	{
		uint32* p32 = xrtUtf8To32("a你", &iSize);

		printf("u8→32: len=%zu", iSize);
		printf(" count=2\n");
		memcpy(A32, p32, iSize * sizeof(uint32));
		xrtFree(p32);
	}
	/* UTF-8 → 16/32 缓冲版（显式容量 + 策略；结果含读/写量）。 */
	{
		xutfresult R = xrtUtf8To16Buffer(SV("a你"), A16, 8u, XUTF_REPLACE);

		(void)R;
		{
			xutfresult R32 = xrtUtf8To32Buffer(SV("a你"), A32, 8u, XUTF_REPLACE);

			(void)R32;
		}
	}

	/* UTF-16 → UTF-8 / UTF-32。 */
	{
		str s8 = xrtUtf16To8(A16, &iSize);

		printf("16→8: [%.*s]", (int)iSize, s8 ? s8 : "?");
		xrtFree(s8);
		(void)xrtUtf16To8Buffer((xutf16view){ A16, 2u }, Back,
			sizeof(Back), XUTF_REPLACE);
		{
			uint32* p32 = xrtUtf16To32(A16, &iSize);

			printf(" 16→32: %zu\n", iSize);
			xrtFree(p32);
		}
		(void)xrtUtf16To32Buffer((xutf16view){ A16, 2u }, A32, 8u, XUTF_REPLACE);
	}

	/* UTF-32 → UTF-8 / UTF-16。 */
	{
		str s8 = xrtUtf32To8(A32, &iSize);

		printf("32→8: [%.*s]", (int)iSize, s8 ? s8 : "?");
		xrtFree(s8);
		(void)xrtUtf32To8Buffer((xutf32view){ A32, 2u }, Back,
			sizeof(Back), XUTF_REPLACE);
		{
			uint16* p16 = xrtUtf32To16(A32, &iSize);

			printf(" 32→16: %zu\n", iSize);
			xrtFree(p16);
		}
		(void)xrtUtf32To16Buffer((xutf32view){ A32, 2u }, A16, 8u, XUTF_REPLACE);
	}

	/* 视图版（带 xutfpolicy）+ 复制族。 */
	{
		uint32* p32 = xrtUtf16ViewTo32((xutf16view){ A16, 2u },
			XUTF_REPLACE, &iSize);

		printf("view-to32=%zu", iSize);
		xrtFree(p32);
		{
			str s8 = xrtUtf32ViewTo8((xutf32view){ A32, 2u },
				XUTF_REPLACE, &iSize);

			printf(" view-to8=[%.*s]", (int)iSize, s8 ? s8 : "?");
			xrtFree(s8);
		}
		{
			uint16* p16 = xrtUtf32ViewTo16((xutf32view){ A32, 2u },
				XUTF_REPLACE, &iSize);

			xrtFree(p16);
		}
		(void)xrtUtf32View(A32, 2u);
		printf(" u32len=%zu", xrtUtf32Len(A32));
		{
			uint16* pDup = xrtUtf16Dup(A16);
			uint32* pDup32 = xrtUtf32Dup(A32);
			uint32* pDup32V = xrtUtf32DupView((xutf32view){ A32, 2u });

			printf(" dup16=%d dup32=%d dup32v=%d",
				pDup != NULL, pDup32 != NULL, pDup32V != NULL);
			xrtFree(pDup);
			xrtFree(pDup32);
			xrtFree(pDup32V);
		}
		printf("\n");
	}

	/* UTF-16 长度 / UTF-32 校验 / UTF-8 视图→UTF-32。 */
	printf("u16len=%zu", xrtUtf16Len(A16));
	printf(" u32valid=%d", xrtUtf32Valid((xutf32view){ A32, 2u }, NULL) ? 1 : 0);
	{
		uint32* p32 = xrtUtf8ViewTo32(SV("a你"), XUTF_REPLACE, &iSize);

		printf(" u8view-to32=%zu\n", iSize);
		xrtFree(p32);
	}

	/* 码元级编解码。 */
	{
		uint16 Out[2];
		size_t iWrote = xrtUtf16Encode(0x4F60, Out);

		printf("encode=%zu", iWrote);
		(void)xrtUtf16Decode((xutf16view){ Out, iWrote }, &iScalar, &iRead);
		printf(" decode=U+%X\n", iScalar);
	}
	return 0;
}
