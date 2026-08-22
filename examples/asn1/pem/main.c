#include <stdio.h>
#include <string.h>

#include <xrt.h>



/* 演示生成、查找并解码一个带标签的 PEM 块。 */
int main(void)
{
	static const uint8 Data[] = { 1, 2, 3, 4, 5 };
	xpemblock Block;
	bytes pDecoded;
	size_t iDecodedSize;
	str sText;

	sText = xrtPemEncodeNew("XRT DATA", Data, sizeof(Data));
	if ( (sText == NULL) ||
		!xrtPemFind(sText, strlen(sText), "XRT DATA", &Block) ) {
		xrtFree(sText);
		return 1;
	}
	pDecoded = xrtPemDecodeNew(&Block, &iDecodedSize);
	if ( (pDecoded == NULL) || (iDecodedSize != sizeof(Data)) ||
		(memcmp(pDecoded, Data, sizeof(Data)) != 0) ) {
		xrtFree(pDecoded);
		xrtFree(sText);
		return 1;
	}
	printf("%s", sText);
	xrtFree(pDecoded);
	xrtFree(sText);
	return 0;
}
