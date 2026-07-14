#define XRT_IMPLEMENTATION
#include "../singlehead/xrt.h"

#include <stdio.h>
#include <string.h>

static int check(int condition, const char* message)
{
	if ( condition ) {
		return 1;
	}
	fprintf(stderr, "test_buffer_insert_contract: %s\n", message);
	return 0;
}

int main(void)
{
	xbuffer pBuffer;
	int ok = 1;
	if ( xrtInit() == NULL ) {
		return 1;
	}
	pBuffer = xrtBufferCreate(4u);
	ok &= check(pBuffer != NULL, "create failed");
	ok &= check(xrtBufferAppend(pBuffer, "ad", 2u, XBUF_ANSI), "append failed");
	ok &= check(xrtBufferInsert(pBuffer, 1u, "bc", 2u, XBUF_ANSI), "middle insert failed");
	ok &= check(pBuffer->Length == 4u && strcmp((const char*)pBuffer->Buffer, "abcd") == 0,
		"middle insert did not preserve suffix");
	ok &= check(xrtBufferInsert(pBuffer, 0u, pBuffer->Buffer + 2u, 2u, XBUF_ANSI),
		"self insert failed");
	ok &= check(pBuffer->Length == 6u && strcmp((const char*)pBuffer->Buffer, "cdabcd") == 0,
		"self insert corrupted aliased input");
	ok &= check(!xrtBufferInsert(pBuffer, pBuffer->Length + 1u, "x", 1u, XBUF_ANSI),
		"out-of-range insert succeeded");
	xrtBufferDestroy(pBuffer);
	{
		char* pOwned = (char*)xrtMalloc(3u);
		xbuffer pAdopted;
		memcpy(pOwned, "xyz", 3u);
		pAdopted = xrtBufferAdopt(pOwned, 3u);
		ok &= check(pAdopted != NULL, "adopt failed");
		ok &= check(pAdopted->Buffer == pOwned, "adopt copied instead of taking ownership");
		ok &= check(pAdopted->Length == 3u && memcmp(pAdopted->Buffer, "xyz", 3u) == 0,
			"adopt payload mismatch");
		xrtBufferDestroy(pAdopted);
	}
	xrtUnit();
	return ok ? 0 : 1;
}
