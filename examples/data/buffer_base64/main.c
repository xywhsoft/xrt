#include <stdio.h>
#include <xrt.h>



/* 把 Base64 文本直接解码为拥有型 Buffer。 */
int main(void)
{
	xbuffer* pBuffer = xrtBufferFromBase64(
		XRT_STR_LITERAL("SGVsbG8="),
		NULL
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
