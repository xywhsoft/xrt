#include <stdio.h>

#include <xrt.h>



/* 展示二进制文件最常用的一次创建、完整写入和完整读取。 */
int main(void)
{
	static const char sText[] = "xrt file";
	char arrText[sizeof(sText)];
	uint64 iPosition;
	xfile File = xrtOpen("xrt-file-example.tmp",
		XFILE_READ | XFILE_WRITE | XFILE_CREATE | XFILE_TRUNCATE);

	if ( File == NULL ) {
		return 1;
	}
	if ( !xrtWriteFull(File, sText, sizeof(sText), NULL) ||
		 !xrtSeek(File, 3, XSEEK_START, NULL) ||
		 !xrtReadAtFull(File, 0, arrText, sizeof(arrText), NULL) ||
		 !xrtTell(File, &iPosition) || (iPosition != 3u) ||
		 !xrtSeek(File, 0, XSEEK_START, NULL) ||
		 !xrtReadFull(File, arrText, sizeof(arrText), NULL) ) {
		(void)xrtClose(File);
		return 1;
	}
	printf("%s\n", arrText);
	if ( !xrtClose(File) ) {
		return 1;
	}
	return xrtFileDelete("xrt-file-example.tmp") ? 0 : 1;
}
