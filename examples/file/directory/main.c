#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/directory —— 目录枚举（零额外 stat）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtDirOpen / DirClose   打开关闭目录流
 *   xrtDirNext              逐项枚举（三态 ITEM/END/ERROR）
 *   xdirentry               条目：名字视图 + 类型/尺寸元数据
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/directory/main.c -lws2_32 -liphlpapi
 * 预期输出（当前目录条目名列表，随仓库内容变化）：
 *   .git
 *   .gitattributes
 *   ...
 *
 * "零额外 stat"：目录 API 一次系统调用即带出类型与尺寸
 *   （Windows FindFirst / Linux getdents 原生信息）——
 *   传统 readdir + 每项 stat 的 2N 次调用在这里是 N 次，
 *   大目录枚举性能差一个数量级。
 */


/* 展示零额外 stat 的目录枚举。 */
int main(void)
{
	xdir Dir = xrtDirOpen(".", 0u);
	xdirentry Entry;
	xdirnext Next;

	if ( Dir == NULL ) {
		return 1;
	}
	while ( (Next = xrtDirNext(Dir, &Entry)) == XDIR_NEXT_ITEM ) {
		printf("%.*s\n", (int)Entry.Name.Size, Entry.Name.Data);
	}
	if ( Next == XDIR_NEXT_ERROR ) {
		(void)xrtDirClose(Dir);
		return 1;
	}
	return xrtDirClose(Dir) ? 0 : 1;
}
