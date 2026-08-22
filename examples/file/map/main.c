#include <stdio.h>

#include <xrt.h>



/* 展示使用只读映射检查文件内容。 */
int main(void)
{
	static const char sPath[] = "xrt-file-map-example.tmp";
	xfile File;
	xfilemap Map;
	int iResult = 1;

	File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);
	if ( (File == NULL) ||
		 !xrtWriteFull(File, "mapped", 6u, NULL) ) {
		(void)xrtClose(File);
		return 1;
	}
	Map = xrtFileMap(File, 0u, 0u, XFILE_MAP_READ);
	if ( Map != NULL ) {
		printf("%.*s\n", (int)xrtFileMapSize(Map),
			(const char*)xrtFileMapData(Map));
		if ( xrtFileUnmap(Map) && xrtClose(File) &&
			 xrtFileDelete(sPath) ) {
			iResult = 0;
		}
	}
	return iResult;
}
