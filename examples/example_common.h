#ifndef XRT_EXAMPLE_COMMON_H
#define XRT_EXAMPLE_COMMON_H

#include <stdio.h>
#include <string.h>

static const char* exBoolText(bool bValue)
{
	return bValue ? "TRUE" : "FALSE";
}



static void exPrintHex(const uint8* pData, size_t iLen)
{
	size_t i;

	for ( i = 0; i < iLen; i++ ) {
		printf("%02X", pData[i]);
	}
}



static bool exBytesEqual(const void* pLeft, const void* pRight, size_t iLen)
{
	return memcmp(pLeft, pRight, iLen) == 0;
}



static str exMakeAppFilePath(str sFileName)
{
	return xrtPathJoin(2, xCore.AppPath, sFileName);
}

#endif
