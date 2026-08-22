#include <stdio.h>

#include <xrt.h>



/* 使用 Buffer Writer 追加和覆盖动态字节内容。 */
int main(void)
{
	xbuffer Buffer;
	xwriter* pWriter;
	int iResult = 1;

	if ( !xrtBufferInit(&Buffer) ) {
		return 1;
	}
	pWriter = xrtWriterFromBuffer(&Buffer);
	if ( (pWriter != NULL) &&
		 xrtWriterWriteFull(pWriter, "hello world", 11u, NULL) &&
		 xrtWriterSeek(pWriter, 6, XSEEK_START, NULL) &&
		 xrtWriterWriteFull(pWriter, "xrt", 3u, NULL) ) {
		printf("%.*s\n", (int)Buffer.Size, (const char*)Buffer.Data);
		iResult = 0;
	}
	xrtWriterDestroy(pWriter);
	xrtBufferUnit(&Buffer);
	return iResult;
}
