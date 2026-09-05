/*
 * 范例：asn1/pem_tour —— PEM 流式族：Init/Read 游标 + Encode/Decode
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPemInit       初始化借用输入的 PEM 游标（严格有界）
 *   xrtPemRead       读取下一个 PEM 块（游标式，不分配）
 *   xrtPemEncode     标签 + 字节 → PEM 文本（缓冲版）
 *   xrtPemDecode     PEM 块 → 字节（缓冲版）
 * 模块宏：XRT_MODULE_PEM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/asn1/pem_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   blocks=1 label=CERTIFICATE data=5
 *   encode=ok roundtrip=ok
 *
 * 与 asn1/pem（Find/DecodeNew 一次性版）互补：
 *   PemInit/Read 是游标——多块文件逐个消费（证书链的标准姿势）；
 *   PemEncode/Decode 是缓冲版——容量自管。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const char sText[] =
		"-----BEGIN CERTIFICATE-----\n"
		"AQIDBAU=\n"
		"-----END CERTIFICATE-----\n";
	xpemcursor Cursor;
	xpemblock Block;
	char Text[256];
	size_t iTextSize = 0;
	unsigned char Data[16];
	size_t iDataSize = 0;
	size_t iBlocks = 0;

	/* 游标式：多块文件逐个消费。 */
	if ( !xrtPemInit(&Cursor, sText, sizeof(sText) - 1u) ) {
		return 1;
	}
	while ( xrtPemRead(&Cursor, &Block) == XPEM_BLOCK ) {
		iBlocks++;
		printf("blocks=%zu label=%.*s", iBlocks,
			(int)Block.Label.Size, Block.Label.Data);
		if ( xrtPemDecode(&Block, Data, sizeof(Data), &iDataSize) ) {
			printf(" data=%zu\n", iDataSize);
		}
	}

	/* 缓冲版：Encode → Decode 往返。 */
	if ( !xrtPemEncode("DATA", Data, iDataSize, Text, sizeof(Text),
		&iTextSize) ) {
		return 2;
	}
	printf("encode=ok(%zu chars)", iTextSize);
	if ( xrtPemFind(Text, iTextSize, "DATA", &Block) ) {
		unsigned char Round[16];
		size_t iRound = 0;

		if ( xrtPemDecode(&Block, Round, sizeof(Round), &iRound) &&
			(iRound == iDataSize) &&
			(memcmp(Round, Data, iRound) == 0) ) {
			printf(" roundtrip=ok\n");
		}
	}
	return 0;
}
