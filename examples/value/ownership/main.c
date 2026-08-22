#include <stdio.h>
#include <string.h>
#include <xrt.h>



/* 演示字符串和字节接管，以及 Retain 与标量 Clone 的引用语义。 */
int main(void)
{
	str sText = (str)xrtMalloc(6);
	bytes pData = (bytes)xrtMalloc(3);
	xvalue* pText = NULL;
	xvalue* pBytes = NULL;
	xvalue* pRetained = NULL;
	xvalue* pCloned = NULL;
	xstrview Text;
	xbytesview Data;
	int iResult = 0;

	if ( (sText == NULL) || (pData == NULL) ) {
		iResult = 1;
		goto cleanup;
	}
	memcpy(sText, "hello", 6);
	pData[0] = 7;
	pData[1] = 8;
	pData[2] = 9;
	pText = xrtValueStringTake(&sText, 5);
	pBytes = xrtValueBytesTake(&pData, 3);
	if ( (pText == NULL) || (pBytes == NULL) ) {
		iResult = 2;
		goto cleanup;
	}

	pRetained = xrtValueRetain(pText);
	pCloned = xrtValueClone(pText);
	if (
		(pRetained == NULL) ||
		(pCloned == NULL) ||
		(pRetained != pText) ||
		(pCloned != pText) ||
		!xrtValueGetString(pText, &Text) ||
		!xrtValueGetBytes(pBytes, &Data)
	) {
		iResult = 3;
		goto cleanup;
	}
	printf(
		"%.*s: %u %u %u\n",
		(int)Text.Size,
		Text.Data,
		(unsigned)Data.Data[0],
		(unsigned)Data.Data[1],
		(unsigned)Data.Data[2]
	);

cleanup:
	xrtValueRelease(pCloned);
	xrtValueRelease(pRetained);
	xrtValueRelease(pBytes);
	xrtValueRelease(pText);
	xrtFree(pData);
	xrtFree(sText);
	return iResult;
}
