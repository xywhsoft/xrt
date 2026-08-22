/*
 * XRT Example - File Hasher
 * XRT 范例 - 文件哈希工具
 *
 * Description / 说明:
 *   EN: Demonstrates streaming SHA-256 hashing for a file.
 *   CN: 演示文件的流式 SHA-256 计算。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_file_hasher.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_file_hasher -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"


int main(int argc, char** argv)
{
	str sPath = NULL;
	xfile hFile = NULL;
	xsha256_ctx tCtx;
	uint8 arrHash[32];
	size_t iReadSize = 0;
	ptr pChunk = NULL;

	xrtInit();

	if ( argc > 1 ) {
		sPath = argv[1];
	} else {
		sPath = exMakeAppFilePath("file_hasher_demo.txt");
		xrtFileWriteAll(sPath, "hash me with streaming sha256\n", 30, XRT_CP_UTF8);
	}

	hFile = xrtOpen(sPath, TRUE, XRT_CP_BINARY);
	xrtSHA256Init(&tCtx);

	while ( !xrtEOF(hFile) ) {
		pChunk = xrtGet(hFile, 16, &iReadSize);
		if ( pChunk && iReadSize > 0 ) {
			xrtSHA256Update(&tCtx, pChunk, iReadSize);
		}
		xrtFree(pChunk);
		pChunk = NULL;
		if ( iReadSize == 0 ) {
			break;
		}
	}

	xrtSHA256Final(&tCtx, arrHash);
	printf("file = %s\n", sPath);
	printf("sha256 = ");
	exPrintHex(arrHash, sizeof(arrHash));
	printf("\n");

	xrtClose(hFile);
	if ( argc == 1 ) {
		xrtFileDelete(sPath);
		xrtFree(sPath);
	}

	xrtUnit();
	return 0;
}
