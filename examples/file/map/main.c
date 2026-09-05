#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/map —— 内存映射：只读视图访问文件内容
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtFileMap          把文件（或区间）映射进地址空间
 *   xrtFileMapData/Size 映射视图的指针与长度
 *   xrtFileUnmap        解除映射
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/map/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   mapped
 *
 * 映射 vs ReadAll：大文件随机访问不整读——按页按需加载；
 *   只读映射多进程共享同一物理页。参数 (file, offset=0,
 *   size=0, READ)：size 0 = 映射到文件尾。
 *   词典/索引/只读资源文件的加载方式。
 */


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
