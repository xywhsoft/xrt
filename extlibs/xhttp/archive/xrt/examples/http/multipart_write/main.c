#include <xrt.h>

#include <stdio.h>



/* 使用常用 Helper 构建表单，并保留分段发送大文件正文的低级路径。 */
int main(void)
{
	xmultipartboundary Boundary;
	uint8 Body[512];
	size_t iOffset = 0;
	size_t iSize;

	if ( !xrtMultipartBoundaryParse(
		XRT_STR_LITERAL("xrt-upload"), &Boundary
	) ) {
		return 1;
	}
	if ( !xrtMultipartFieldWrite(
		&Boundary,
		XRT_STR_LITERAL("message"),
		(xbytesview){ (const uint8*)"hello", 5u },
		Body + iOffset,
		sizeof(Body) - iOffset,
		&iSize
	) ) {
		return 1;
	}
	iOffset += iSize;
	if ( !xrtMultipartFileWrite(
		&Boundary,
		XRT_STR_LITERAL("file"),
		XRT_STR_LITERAL("a.txt"),
		XRT_STR_LITERAL("text/plain"),
		(xbytesview){ (const uint8*)"file body", 9u },
		Body + iOffset,
		sizeof(Body) - iOffset,
		&iSize
	) ) {
		return 1;
	}
	iOffset += iSize;
	if ( !xrtMultipartCloseWrite(
		&Boundary,
		Body + iOffset,
		sizeof(Body) - iOffset,
		&iSize
	) ) {
		return 1;
	}
	iOffset += iSize;

	printf("%.*s", (int)iOffset, (cstr)Body);
	return 0;
}
