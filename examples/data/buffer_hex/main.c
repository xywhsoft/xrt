#include <stdio.h>
#include <xrt.h>



/* 把十六进制文本直接解码为拥有型 Buffer。 */
int main(void)
{
	xbuffer* pBuffer = xrtBufferFromHex(
		XRT_STR_LITERAL("48656c6c6f"),
		0
	);

	if ( pBuffer == NULL ) {
		return 1;
	}
	printf(
		"%.*s\n",
		(int)pBuffer->Size,
		(const char*)pBuffer->Data
	);
	xrtBufferDestroy(pBuffer);
	return 0;
}
