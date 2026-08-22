#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立接通共享写文件映射。 */
int main(void)
{
	static const char sPath[] = "xrt-single-file-map.tmp";
	xfile File = xrtOpen(sPath, XFILE_READ | XFILE_WRITE |
		XFILE_CREATE | XFILE_TRUNCATE);
	xfilemap Map;
	char arrText[3];
	int iResult = 1;

	if ( (File == NULL) ||
		 !xrtWriteFull(File, "abc", 3u, NULL) ) {
		(void)xrtClose(File);
		return 1;
	}
	Map = xrtFileMap(File, 1u, 2u,
		XFILE_MAP_READ | XFILE_MAP_WRITE);
	if ( Map != NULL ) {
		memcpy(xrtFileMapData(Map), "BC", 2u);
		if ( xrtFileMapFlush(Map, 0u, 0u) ) {
			bool bUnmapped = xrtFileUnmap(Map);

			Map = NULL;
			if ( bUnmapped && xrtReadAtFull(File, 0u,
					arrText, sizeof(arrText), NULL) &&
				 (memcmp(arrText, "aBC", 3u) == 0) ) {
				iResult = 0;
			}
		}
	}
	if ( Map != NULL ) {
		(void)xrtFileUnmap(Map);
		Map = NULL;
	}
	(void)xrtClose(File);
	(void)xrtFileDelete(sPath);
	return iResult;
}
