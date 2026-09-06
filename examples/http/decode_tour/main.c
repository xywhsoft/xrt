/*
 * 范例：http/decode_tour —— 解码器配置/模式/计量与复位复用
 * ----------------------------------------------------------------
 * 演示 API：
 *   【配置】      xrtHttpDecodeConfigInit（兼容：四层/64KiB 头/不限长）
 *                 xrtHttpDecodeConfigInitSafe（安全：显式输出上限）
 *   【计量】      xrtHttpDecodeInputSize（线路字节数）
 *                 xrtHttpDecodeOutputSize（已交付字节数）
 *   【模式与复位】xrtHttpDecodeMode（IDENTITY/CONTENT/RAW）
 *                 xrtHttpDecodeReset（复用 Inflate 窗口换下一条消息）
 * 模块宏：XRT_MODULE_HTTP_DECODE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http/decode_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   decode: config compat vs safe + create ok
 *   decode: gzip content mode + input/output sizes ok
 *   decode: reset to identity passthrough ok
 *
 * Reset 复用已分配的解压窗口——高 QPS 网关逐消息复位
 *   而不是销毁重建；IDENTITY 模式不复制输入。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

/* 一段 gzip 压缩的 "identity-passthrough-check"。 */
static const uint8 arrGzip[46] = {
	0x1F, 0x8B, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x0A,
	0xCB, 0x4C, 0x49, 0xCD, 0x2B, 0xC9, 0x2C, 0xA9, 0xD4, 0x2D,
	0x48, 0x2C, 0x2E, 0x2E, 0xC9, 0x28, 0xCA, 0x2F, 0x4D, 0xCF,
	0xD0, 0x4D, 0xCE, 0x48, 0x4D, 0xCE, 0x06, 0x00, 0x9A, 0xFB,
	0x0B, 0xA2, 0x1A, 0x00, 0x00, 0x00
};

/* 输出回调：累计并比对内容。 */
typedef struct exampleout {
	size_t iBytes;
	char arrText[64];
} exampleout;

static bool exampleOutput(xbytesview Data, ptr pUserData)
{
	exampleout* pOut = (exampleout*)pUserData;

	if ( pOut->iBytes + Data.Size > sizeof(pOut->arrText) ) {
		return false;
	}
	memcpy(pOut->arrText + pOut->iBytes, Data.Data, Data.Size);
	pOut->iBytes += Data.Size;
	return true;
}

int main(void)
{
	xhttpdecodeconfig Compat;
	xhttpdecodeconfig Safe;
	xhttpdecode* pDecode = NULL;
	xhttpfield Fields[2];
	exampleout Out;
	int iResult = 1;

	/* ---- 两套配置 + gzip 字段建解码器。 ---- */
	xrtHttpDecodeConfigInit(&Compat);
	xrtHttpDecodeConfigInitSafe(&Safe);
	Fields[0].Name = XRT_STR_LITERAL("Content-Encoding");
	Fields[0].Value = XRT_STR_LITERAL("gzip");
	pDecode = xrtHttpDecodeCreate(Fields, 1u, &Compat);
	if ( (pDecode == NULL) ||
		/* 兼容配置不设输出上限；安全配置有显式上限。 */
		(Compat.OutputLimit != XHTTP_DECODE_OUTPUT_UNLIMITED) ||
		(Safe.OutputLimit == Compat.OutputLimit) ) {
		goto Cleanup;
	}
	printf("decode: config compat vs safe + create ok\n");

	/* ---- gzip 模式 + 输入输出计量。 ---- */
	memset(&Out, 0, sizeof(Out));
	if ( (xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_CONTENT) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, sizeof(arrGzip) }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 26u) ||
		(memcmp(Out.arrText, "identity-passthrough-check",
			26u) != 0) ||
		(xrtHttpDecodeInputSize(pDecode) != 46u) ||
		(xrtHttpDecodeOutputSize(pDecode) != 26u) ) {
		goto Cleanup;
	}
	printf("decode: gzip content mode + input/output sizes ok\n");

	/* ---- Reset 换 identity 字段：窗口复用 + 透传模式。 ---- */
	Fields[0].Value = XRT_STR_LITERAL("identity");
	memset(&Out, 0, sizeof(Out));
	if ( !xrtHttpDecodeReset(pDecode, Fields, 1u, &Compat) ||
		(xrtHttpDecodeMode(pDecode) != XHTTP_DECODE_IDENTITY) ||
		!xrtHttpDecodeWrite(pDecode,
			(xbytesview) { arrGzip, 4u }, true,
			exampleOutput, (ptr)&Out) ||
		!xrtHttpDecodeDone(pDecode) ||
		(Out.iBytes != 4u) ||
		(xrtHttpDecodeInputSize(pDecode) != 4u) ) {
		goto Cleanup;
	}
	printf("decode: reset to identity passthrough ok\n");
	iResult = 0;

Cleanup:
	xrtHttpDecodeDestroy(pDecode);
	return iResult;
}
