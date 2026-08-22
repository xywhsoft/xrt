#include <stdio.h>

#include <xrt.h>



/* 演示显式长度二进制数据的 HEX 编解码。 */
int main(void)
{
	static const uint8 arrData[] = { 0, 1, 2, 0xFEu, 0xFFu };
	str sText = xrtHexEncodeNew(arrData, sizeof(arrData), (uint32)XHEX_UPPER);
	size_t iSize;
	bytes pData;

	if ( sText == NULL ) {
		return 1;
	}
	pData = xrtHexDecodeNew((xstrview){ sText, sizeof(arrData) * 2u }, &iSize, 0);
	if ( pData == NULL ) {
		xrtFree(sText);
		return 1;
	}
	printf("%s (%llu bytes)\n", sText, (unsigned long long)iSize);
	xrtFree(pData);
	xrtFree(sText);
	return 0;
}
