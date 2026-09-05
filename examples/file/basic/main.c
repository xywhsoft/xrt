#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/basic —— 句柄主线：开/写/定位/读/关的完整骨架
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtOpen / xrtClose       打开关闭（标志组合式权限）
 *   xrtWriteFull / ReadFull  读满写满（短读写即失败）
 *   xrtSeek / xrtTell        定位与查询（XSEEK_START 三基准）
 *   xrtReadAtFull            绝对偏移读（不动文件指针）
 *   xrtFileDelete            删除
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/basic/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xrt file
 *
 * Full 后缀族的语义：读到/写到恰好 N 字节才算成功——
 *   提前 EOF 或磁盘满都会失败，省去循环重试样板。
 *   ReadAtFull 读后指针仍在 3（Tell 验证），预读文件头
 *   不扰动流式读取位置。
 */


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
