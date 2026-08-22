/*
 * XRT Example - File Encrypt
 * XRT 范例 - 文件加解密
 *
 * Description / 说明:
 *   EN: Demonstrates password-derived AES-256-GCM file encryption and decryption.
 *   CN: 演示基于密码派生 AES-256-GCM 的文件加解密。
 *
 * Build / 编译:
 *   tcc main.c -o ../../bin/tools_file_encrypt.exe -lWs2_32 -lIPHLPAPI -lShell32
 *   gcc main.c -o ../../bin/tools_file_encrypt -lm -lpthread
 */
#define XRT_IMPLEMENTATION
#include "../../../singlehead/xrt.h"
#include "../../example_common.h"

typedef struct {
	uint8 arrSalt[16];
	uint8 arrNonce[12];
} enc_header;


int main()
{
	str sInputPath = NULL;
	str sEncPath = NULL;
	str sDecPath = NULL;
	const uint8 arrPassword[] = "xrt-password";
	uint8 arrPrk[32];
	uint8 arrKey[32];
	enc_header tHeader;
	size_t iPlainSize = 0;
	size_t iEncSize = 0;
	uint8* pPlain = NULL;
	uint8* pCipher = NULL;
	uint8* pPacked = NULL;
	uint8* pPackedRead = NULL;
	uint8* pDecoded = NULL;

	xrtInit();

	sInputPath = exMakeAppFilePath("file_encrypt_demo.txt");
	sEncPath = exMakeAppFilePath("file_encrypt_demo.bin");
	sDecPath = exMakeAppFilePath("file_encrypt_demo.out.txt");
	xrtFileWriteAll(sInputPath, "encrypt me with aes-gcm", 23, XRT_CP_UTF8);

	pPlain = (uint8*)xrtFileGetAll(sInputPath, &iPlainSize);
	xrtRandomBytes(tHeader.arrSalt, sizeof(tHeader.arrSalt));
	xrtRandomBytes(tHeader.arrNonce, sizeof(tHeader.arrNonce));
	xrtHKDFExtract(arrPrk, tHeader.arrSalt, sizeof(tHeader.arrSalt), arrPassword, strlen((str)arrPassword));
	xrtHKDFExpand(arrKey, sizeof(arrKey), arrPrk, sizeof(arrPrk), (const uint8*)"file-encrypt", 12);

	pCipher = (uint8*)xrtMalloc(iPlainSize + 16u);
	xrtAES256GCMEncrypt(pCipher, arrKey, tHeader.arrNonce, sizeof(tHeader.arrNonce), NULL, 0, pPlain, iPlainSize);

	iEncSize = sizeof(tHeader) + iPlainSize + 16u;
	pPacked = (uint8*)xrtMalloc(iEncSize);
	memcpy(pPacked, &tHeader, sizeof(tHeader));
	memcpy(pPacked + sizeof(tHeader), pCipher, iPlainSize + 16u);
	xrtFilePutAll(sEncPath, pPacked, iEncSize);

	pPackedRead = (uint8*)xrtFileGetAll(sEncPath, &iEncSize);
	memcpy(&tHeader, pPackedRead, sizeof(tHeader));
	xrtHKDFExtract(arrPrk, tHeader.arrSalt, sizeof(tHeader.arrSalt), arrPassword, strlen((str)arrPassword));
	xrtHKDFExpand(arrKey, sizeof(arrKey), arrPrk, sizeof(arrPrk), (const uint8*)"file-encrypt", 12);

	pDecoded = (uint8*)xrtMalloc(iPlainSize + 1u);
	if ( xrtAES256GCMDecrypt(pDecoded, arrKey, tHeader.arrNonce, sizeof(tHeader.arrNonce), NULL, 0, pPackedRead + sizeof(tHeader), iEncSize - sizeof(tHeader)) ) {
		xrtFilePutAll(sDecPath, pDecoded, iPlainSize);
		printf("decrypt_match = %s\n", exBoolText(exBytesEqual(pPlain, pDecoded, iPlainSize)));
	}

	xrtFree(pPlain);
	xrtFree(pCipher);
	xrtFree(pPacked);
	xrtFree(pPackedRead);
	xrtFree(pDecoded);
	xrtFileDelete(sInputPath);
	xrtFileDelete(sEncPath);
	xrtFileDelete(sDecPath);
	xrtFree(sInputPath);
	xrtFree(sEncPath);
	xrtFree(sDecPath);
	xrtUnit();
	return 0;
}
