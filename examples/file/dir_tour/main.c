/*
 * 范例：file/dir_tour —— 目录全接口巡礼：创建/清空/移动/统计/根列表
 * ----------------------------------------------------------------
 * 演示 API：
 *   【创建】   xrtDirCreateMode / CreateAll / CreateAllMode
 *   【状态】   xrtDirEmpty / xrtDirSize / xrtDirStats / xrtDirEnsureEmpty
 *              xrtDirClean（清内容留目录）
 *   【移动】   xrtDirMove（bReplace 语义）
 *   【枚举】   xrtDirPath / xrtDirEntryPath / xrtDirRoots / RootsFree
 *   【树操作】 xrtFileTreeCopy / xrtFileTreeRemove / xrtTreeCopyOptionsInit
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c \
 *       examples/file/dir_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   empty=1 size=7 stats(items=1 files=1 bytes=7)
 *   entry=data.txt
 *   moved=1 clean-empty=1 ensure-empty=1
 *   treecopy(items=1) treeremove-keep=1 roots>=1
 *
 * Clean/EnsureEmpty 的区别：Clean 删除全部内容（目录必须存在）；
 *   EnsureEmpty = 存在则清空、不存在则创建（幂等准备目录）。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const char sRoot[] = "xrt-dir-tour";
	static const char sDeep[] = "xrt-dir-tour/a/b/c";
	static const char sFile[] = "xrt-dir-tour/a/b/c/data.txt";
	xfileinfo Info;
	xwalkstats Stats;
	xdirroots Roots;
	xdir Dir;
	bool bEmpty = false;
	uint64 iSize = 0;
	int iResult = 1;

	(void)xrtFileTreeRemove(sRoot, false, NULL);
	xrtClearError();

	/* CreateAll 递归建链（默认模式）+ CreateAllMode（显式模式）。 */
	if ( !xrtDirCreateAll(sDeep) ) {
		goto cleanup;
	}
	if ( !xrtDirCreateAllMode("xrt-dir-tour/mode/a/b", 0700u) ) {
		goto cleanup;
	}
	if ( !xrtDirCreateMode("xrt-dir-tour/single", 0700u) ) {
		goto cleanup;
	}
	if ( !xrtFileWriteAll(sFile, XRT_BYTES_LITERAL("1234567")) ) {
		goto cleanup;
	}

	/* Empty / Size / Stats 三种观测。 */
	if ( !xrtDirEmpty("xrt-dir-tour/single", &bEmpty) || !bEmpty ) {
		goto cleanup;
	}
	printf("empty=%d", bEmpty ? 1 : 0);
	if ( !xrtDirSize(sDeep, true, &iSize) || (iSize != 7u) ) {
		goto cleanup;
	}
	printf(" size=%llu", (unsigned long long)iSize);
	memset(&Stats, 0, sizeof(Stats));
	if ( !xrtDirStats(sDeep, false, &Stats) || (Stats.Files != 1u) ) {
		goto cleanup;
	}
	printf(" stats(items=%llu files=%llu bytes=%llu)\n",
		(unsigned long long)Stats.Items,
		(unsigned long long)Stats.Files,
		(unsigned long long)Stats.Bytes);

	/* 枚举辅助：DirPath 借用 + DirEntryPath 拼拥有路径。 */
	Dir = xrtDirOpen(sDeep, 0u);
	if ( Dir == NULL ) {
		goto cleanup;
	}
	{
		xdirentry Entry;

		printf("dir=%s", xrtDirPath(Dir));
		while ( xrtDirNext(Dir, &Entry) == XDIR_NEXT_ITEM ) {
			str sFull = xrtDirEntryPath(Dir, &Entry);

			printf(" entry=%.*s(full=%s)", (int)Entry.Name.Size,
				Entry.Name.Data, sFull ? sFull : "?");
			xrtFree(sFull);
		}
		printf("\n");
		(void)xrtDirClose(Dir);
	}

	/* Move（目录版）+ Clean + EnsureEmpty。 */
	if ( !xrtDirMove("xrt-dir-tour/single", "xrt-dir-tour/renamed", false) ) {
		goto cleanup;
	}
	printf("moved=1");
	if ( !xrtDirClean(sDeep) ) {
		goto cleanup;
	}
	if ( !xrtDirEmpty(sDeep, &bEmpty) || !bEmpty ) {
		goto cleanup;
	}
	printf(" clean-empty=%d", bEmpty ? 1 : 0);
	if ( !xrtDirEnsureEmpty("xrt-dir-tour/fresh") ) {
		goto cleanup;
	}
	if ( !xrtDirEmpty("xrt-dir-tour/fresh", &bEmpty) || !bEmpty ) {
		goto cleanup;
	}
	printf(" ensure-empty=%d\n", bEmpty ? 1 : 0);

	/* 高级树复制（选项结构）+ 树删除（bKeepRoot）。 */
	(void)xrtFileWriteAll(sFile, XRT_BYTES_LITERAL("1234567"));
	{
		xtreecopyoptions Options;

		xrtTreeCopyOptionsInit(&Options);
		memset(&Stats, 0, sizeof(Stats));
		if ( !xrtFileTreeCopy(sRoot, "xrt-dir-tour-copy", &Options, &Stats) ||
			(Stats.Files < 1u) ) {
			goto cleanup;
		}
		printf("treecopy(items=%llu)", (unsigned long long)Stats.Items);
	}
	memset(&Stats, 0, sizeof(Stats));
	if ( !xrtFileTreeRemove(sRoot, true, &Stats) ) {
		goto cleanup;
	}
	printf(" treeremove-keep=%d", xrtDirEmpty(sRoot, &bEmpty) && bEmpty ? 1 : 0);

	/* 系统根列表（Windows 盘符 / POSIX "/"）。 */
	memset(&Roots, 0, sizeof(Roots));
	if ( !xrtDirRoots(&Roots) || (Roots.Count < 1u) ) {
		goto cleanup;
	}
	printf(" roots>=%zu\n", Roots.Count);
	xrtDirRootsFree(&Roots);

	iResult = 0;

cleanup:
	(void)xrtFileTreeRemove(sRoot, false, NULL);
	(void)xrtFileTreeRemove("xrt-dir-tour-copy", false, NULL);
	return iResult;
}
