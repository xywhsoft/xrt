#include <stdio.h>

#include <xrt.h>



/* 使用通用 IO 把固定输入复制到固定输出。 */
int main(void)
{
	unsigned char arrOutput[32] = { 0 };
	xreader* pReader = xrtReaderFromMemory(
		XRT_BYTES_LITERAL("hello xrt io")
	);
	xwriter* pWriter = xrtWriterFromMemory(
		arrOutput,
		sizeof(arrOutput)
	);
	uint64 iCopied = 0;
	int iResult = 1;

	if ( (pReader != NULL) && (pWriter != NULL) &&
		 xrtReaderCopy(pReader, pWriter, &iCopied) ) {
		printf("%.*s (%llu bytes)\n",
			(int)iCopied,
			(const char*)arrOutput,
			(unsigned long long)iCopied);
		iResult = 0;
	}
	xrtReaderDestroy(pReader);
	xrtWriterDestroy(pWriter);
	return iResult;
}
