#include <stdio.h>

#include <xrt.h>



/* 通过拥有式文件适配器写入并重新读取二进制内容。 */
int main(void)
{
	static const char sPath[] = "xrt-io-example.bin";
	unsigned char arrData[7];
	xwriter* pWriter = xrtWriterOpen(sPath);
	xreader* pReader;
	int iResult = 1;

	if ( (pWriter == NULL) ||
		 !xrtWriterWriteFull(pWriter, "xrt io\n", sizeof(arrData), NULL) ||
		 !xrtWriterDestroy(pWriter) ) {
		return 1;
	}
	pReader = xrtReaderOpen(sPath);
	if ( (pReader != NULL) &&
		 xrtReaderReadFull(pReader, arrData, sizeof(arrData), NULL) ) {
		fwrite(arrData, 1u, sizeof(arrData), stdout);
		iResult = 0;
	}
	xrtReaderDestroy(pReader);
	(void)xrtFileDelete(sPath);
	return iResult;
}
