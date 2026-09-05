/*
 * 范例：asn1/pem —— PEM 块的生成、查找与解码往返
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtPemEncodeNew   把字节编码为 "-----BEGIN <标签>-----" 文本块
 *   xrtPemFind        在文本中定位指定标签的下一个 PEM 块（借用视图）
 *   xrtPemDecodeNew   解码块内 Base64 载荷为新的拥有式字节缓冲
 * 模块宏：XRT_MODULE_PEM
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c \
 *       examples/asn1/pem/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   -----BEGIN XRT DATA-----
 *   AQIDBAU=
 *   -----END XRT DATA-----
 *
 * 所有权：Encode/Decode 返回的文本与字节均由 xrtFree 释放；
 *         Find 只输出借用视图（块在原文中的位置），不分配。
 */

#include <stdio.h>
#include <string.h>

#include <xrt.h>



int main(void)
{
	/* 待封装的 5 字节载荷：01 02 03 04 05（Base64 后为 "AQIDBAU="）。 */
	static const uint8 Data[] = { 1, 2, 3, 4, 5 };
	xpemblock Block;          /* 借用视图：命中的 PEM 块范围 */
	bytes pDecoded;           /* 拥有式：解码后的字节 */
	size_t iDecodedSize;
	str sText;                /* 拥有式：编码后的 PEM 文本 */

	/* 编码：标签自定义（证书场景常见 "CERTIFICATE"/"PRIVATE KEY"）。 */
	sText = xrtPemEncodeNew("XRT DATA", Data, sizeof(Data));
	if ( (sText == NULL) ||
		!xrtPemFind(sText, strlen(sText), "XRT DATA", &Block) ) {
		xrtFree(sText);
		return 1;
	}

	/*
	 * 解码往返：Decode 从借用块中还原字节。
	 * 校验三点——非空、长度一致、内容逐字节一致，
	 * 证明编码 -> 查找 -> 解码链路无损。
	 */
	pDecoded = xrtPemDecodeNew(&Block, &iDecodedSize);
	if ( (pDecoded == NULL) || (iDecodedSize != sizeof(Data)) ||
		(memcmp(pDecoded, Data, sizeof(Data)) != 0) ) {
		xrtFree(pDecoded);
		xrtFree(sText);
		return 1;
	}

	/* 打印 PEM 文本本身（含结尾换行，故不补 \n）。 */
	printf("%s", sText);

	/* 两个拥有式资源逐一释放。 */
	xrtFree(pDecoded);
	xrtFree(sText);
	return 0;
}
