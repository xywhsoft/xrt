#include <stdio.h>

#include <xrt/http_content_disposition.h>



/* 解析 Content-Disposition 并优先读取 UTF-8 扩展文件名。 */
int main(void)
{
	xcontentdisposition Disposition;
	char FileName[64];
	size_t iSize;

	if ( !xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"attachment; filename=\"fallback.txt\"; "
			"filename*=UTF-8''%E4%B8%AD%E6%96%87.txt"
		), &Disposition
	) || !xrtHttpContentDispositionFileNameWrite(
		&Disposition, FileName, sizeof(FileName), &iSize
	) ) {
		return 1;
	}
	printf("filename = %.*s\n", (int)iSize, FileName);
	return 0;
}
