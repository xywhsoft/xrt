#include <stdio.h>

#include <xrt.h>



/* 演示二进制追加、稀疏写入和无复制取走结果。 */
int main(void)
{
	xbuffer tBuffer;
	bytes pResult;
	size_t iSize;

	if ( !xrtBufferInit(&tBuffer) ) {
		return 1;
	}
	if (
		!xrtBufferAppend(&tBuffer, XRT_BYTES_LITERAL("abc")) ||
		!xrtBufferWrite(&tBuffer, 5, XRT_BYTES_LITERAL("z"))
	) {
		xrtBufferUnit(&tBuffer);
		return 2;
	}

	for ( size_t i = 0; i < tBuffer.Size; i++ ) {
		printf("%02x%s", tBuffer.Data[i],
			i + 1u == tBuffer.Size ? "\n" : " ");
	}

	pResult = xrtBufferTake(&tBuffer, &iSize, NULL);
	if ( (pResult == NULL) || (iSize != 6) ) {
		xrtBufferUnit(&tBuffer);
		return 3;
	}
	xrtFree(pResult);
	xrtBufferUnit(&tBuffer);
	return 0;
}
