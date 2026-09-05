#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/tree —— 目录树复制与递归清理
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDirCreate / DirExists   建目录 / 存在判断
 *   xrtDirCopy                 递归复制整棵树
 *   xrtDirRemoveAll            递归删除（内容 + 自身）
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/tree/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   copied xrt-tree-example-source to xrt-tree-example-target
 *
 * RemoveAll 是清理类工具的地基（构建产物、缓存目录），
 *   自底向上先内容后目录；Copy 保留层级结构。
 *   起点先清理旧残留是范例工程的幂等写法——
 *   重复运行结果一致。
 */


/* 展示常用目录树复制和递归清理路径。 */
int main(void)
{
	static const char sSource[] = "xrt-tree-example-source";
	static const char sTarget[] = "xrt-tree-example-target";
	static const char sFile[] = "xrt-tree-example-source/item.txt";
	xfile File;
	int iResult = 1;

	if ( xrtDirExists(sSource) ) {
		(void)xrtDirRemoveAll(sSource);
	}
	if ( xrtDirExists(sTarget) ) {
		(void)xrtDirRemoveAll(sTarget);
	}
	xrtClearError();
	if ( !xrtDirCreate(sSource) ) {
		return 1;
	}
	File = xrtOpen(sFile, XFILE_WRITE | XFILE_CREATE | XFILE_EXCLUSIVE);
	if ( (File != NULL) && xrtWriteFull(File, "example", 7u, NULL) &&
		xrtClose(File) && xrtDirCopy(sSource, sTarget, false) ) {
		printf("copied %s to %s\n", sSource, sTarget);
		iResult = 0;
	}
	(void)xrtDirRemoveAll(sTarget);
	(void)xrtDirRemoveAll(sSource);
	return iResult;
}
