/*
 * XRT Example - Binary IO
 * XRT 范例 - 二进制读写
 *
 * Description / 说明:
 *   EN: Demonstrates xrtPut, xrtGet, xrtFilePutAll and xrtFileGetAll.
 *   CN: 演示 xrtPut、xrtGet、xrtFilePutAll 与 xrtFileGetAll。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/file_binary_io.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/file_binary_io -lm -lpthread
 *
 * Note / 注意:
 *   - Temporary files are created under the application path and cleaned afterwards.
 *   - Binary buffers returned by XRT must be freed with xrtFree.
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main()
{
	const uint8 arrBinary[] = { 0x10, 0x20, 0x30, 0x40, 0xAA, 0xBB, 0xCC, 0xDD };
	const uint8 arrPatch[] = { 0x99, 0x88 };
	size_t iReadSize = 0;
	ptr pReadAll = NULL;
	ptr pChunk = NULL;
	xfile hFile = NULL;
	str sPath = NULL;

	xrtInit();

	printf("=== Binary IO ===\n");
	printf("=== 二进制读写 ===\n");

	sPath = exMakeAppFilePath("binary_io_demo.bin");
	if ( xrtPathExists(sPath) ) {
		xrtFileDelete(sPath);
	}

	xrtFilePutAll(sPath, (ptr)arrBinary, sizeof(arrBinary));
	pReadAll = xrtFileGetAll(sPath, &iReadSize);

	printf("xrtFilePutAll/xrtFileGetAll bytes: %zu -> ", iReadSize);
	exPrintHex((const uint8*)pReadAll, iReadSize);
	printf("\n");

	hFile = xrtOpen(sPath, FALSE, XRT_CP_BINARY);
	xrtSeek(hFile, 2, XRT_SEEK_SET);
	xrtPut(hFile, (ptr)arrPatch, sizeof(arrPatch));
	xrtClose(hFile);

	hFile = xrtOpen(sPath, TRUE, XRT_CP_BINARY);
	pChunk = xrtGet(hFile, 6, &iReadSize);
	printf("xrtGet first 6 bytes: ");
	exPrintHex((const uint8*)pChunk, iReadSize);
	printf("\n");
	printf("EOF reached: %s\n", exBoolText(xrtEOF(hFile)));
	xrtClose(hFile);

	xrtFree(pReadAll);
	xrtFree(pChunk);
	xrtFileDelete(sPath);
	xrtFree(sPath);

	xrtUnit();
	return 0;
}
