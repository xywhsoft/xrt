/*
 * 范例：codec/tour —— HEX/Base64/Percent 补集（缓冲版与流式族）
 * ----------------------------------------------------------------
 * 演示 API：
 *   【HEX】     xrtHexEncode（两段式缓冲版）/ HexDecode（可原地）
 *   【Base64】  xrtBase64Decode（缓冲版）/ EncodeNew（分配版）
 *   【Percent 流式】 PercentMapInit（可复用字符位图）/
 *              Measure + WriteMeasured + EncodeMeasured（三段式）/
 *              DecodeMeasure + DecodeMeasured（两段式）/
 *              PercentNext（游标逐字节）/ PercentWrite（无终止零片段）
 * 模块宏：XRT_MODULE_CODEC
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/codec/tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   codec: hex encode/decode roundtrip ok
 *   codec: base64 decode(3)/encode-new(4) ok
 *   codec: percent map+measure+write "%41%20%2F" ok
 *   codec: percent decode-measured "A /" ok
 *   codec: percent next=0x41 write-frag "%2F" ok
 *
 * HEX 原地解码（输出与输入同址）、Base64 缓冲查询+写入两段式、
 *   Percent 用预编译字符位图做 measure→write 三段式流水线。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

#define SV(x) XRT_STR_LITERAL(x)

int main(void)
{
	xpercentmap Map;
	char Text[32];
	char Raw[16];
	bytes pDecoded = NULL;
	str sEncoded = NULL;
	size_t iSize = 0;
	size_t iOffset = 0;
	uint8 uValue = 0;
	int iResult = 1;

	/* ---- HEX：两段式编码 + 原地解码 ---- */
	if ( !xrtHexEncode("AB", 2u, NULL, 0u, &iSize, 0u) ||
		(iSize != 4u) ||
		!xrtHexEncode("AB", 2u, Text, sizeof(Text), &iSize,
			0u) ||
		(strcmp(Text, "4142") != 0) ) {
		goto Cleanup;
	}
	/* 原地：解码到同一缓冲（输入 4 字节 → 输出 2 字节）。 */
	if ( !xrtHexDecode(SV("4142"), Text, 4u, &iSize, 0u) ||
		(iSize != 2u) ||
		(memcmp(Text, "AB", 2u) != 0) ) {
		goto Cleanup;
	}
	printf("codec: hex encode/decode roundtrip ok\n");

	/* ---- Base64：缓冲查询 + 写入 + 分配版 ---- */
	if ( !xrtBase64Decode("QUJD", 4u, NULL, 0u, &iSize, NULL) ||
		(iSize != 3u) ||
		!xrtBase64Decode("QUJD", 4u, Raw, sizeof(Raw), &iSize,
			NULL) ||
		(iSize != 3u) ||
		(memcmp(Raw, "ABC", 3u) != 0) ) {
		goto Cleanup;
	}
	sEncoded = xrtBase64EncodeNew("ABC", 3u, NULL);
	if ( (sEncoded == NULL) ||
		(strcmp(sEncoded, "QUJD") != 0) ) {
		goto Cleanup;
	}
	xrtFree(sEncoded);
	sEncoded = NULL;
	printf("codec: base64 decode(3)/encode-new(4) ok\n");

	/* ---- Percent：字符位图 + 三段式编码 ---- */
	if ( !xrtPercentMapInit(&Map, SV(""), false) ||
		!xrtPercentMeasure("A /", 3u, &Map, false, &iSize) ||
		(iSize != 9u) ) {  /* 每个非安全字符 3 字节 */
		goto Cleanup;
	}
	{
		size_t iWritten = xrtPercentWriteMeasured("A /", 3u,
			&Map, false, Text);

		if ( (iWritten != 9u) ||
			(memcmp(Text, "%41%20%2F", 9u) != 0) ) {
			goto Cleanup;
		}
		/* EncodeMeasured：可补终止零的同址扩张。 */
		xrtPercentEncodeMeasured("A /", 3u, &Map, false, Text,
			9u, true);
		if ( (memcmp(Text, "%41%20%2F", 9u) != 0) ||
			(Text[9] != '\0') ) {
			goto Cleanup;
		}
	}
	printf("codec: percent map+measure+write \"%s\" ok\n",
		"%41%20%2F");

	/* ---- Percent：两段式解码 ---- */
	if ( !xrtPercentDecodeMeasure(SV("%41%20%2F"), false,
			&iSize) ||
		(iSize != 3u) ) {
		goto Cleanup;
	}
	{
		size_t iDecoded = xrtPercentDecodeMeasured(
			SV("%41%20%2F"), false, Raw);

		if ( (iDecoded != 3u) ||
			(memcmp(Raw, "A /", 3u) != 0) ) {
			goto Cleanup;
		}
	}
	printf("codec: percent decode-measured \"A /\" ok\n");

	/* ---- PercentNext：游标逐字节 + Write 无终止零片段 ---- */
	iOffset = 0;
	if ( (xrtPercentNext(SV("%41"), false, &iOffset, &uValue) !=
			XPERCENT_NEXT_BYTE) ||
		(uValue != 0x41u) ||
		(xrtPercentNext(SV("%41"), false, &iOffset,
			&uValue) != XPERCENT_NEXT_END) ) {
		goto Cleanup;
	}
	/* Write：'/' 不在默认安全集 → %2F（3 字节无终止零）。 */
	if ( !xrtPercentWrite("/", 1u, SV(""), Text, sizeof(Text),
			&iSize) ||
		(iSize != 3u) ||
		(memcmp(Text, "%2F", 3u) != 0) ||
		/* Decode：缓冲版（空输出仅查询）。 */
		!xrtPercentDecode(SV("%2F"), NULL, 0u, &iSize) ) {
		goto Cleanup;
	}
	{
		/* PercentEncodeNew：分配版（含终止零，长度不计零）。 */
		size_t iNew = 0;

		sEncoded = xrtPercentEncodeNew("/", 1u, SV(""),
			&iNew);
		if ( (sEncoded == NULL) || (iNew != 3u) ||
			(strcmp(sEncoded, "%2F") != 0) ) {
			goto Cleanup;
		}
		xrtFree(sEncoded);
		sEncoded = NULL;
	}
	printf("codec: percent next=0x41 write-frag \"%s\" ok\n",
		"%2F");
	(void)pDecoded;
	iResult = 0;

Cleanup:
	xrtFree(sEncoded);
	xrtFree(pDecoded);
	return iResult;
}
