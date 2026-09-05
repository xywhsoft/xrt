#include <stdio.h>

#include <xrt.h>



/*
 * 范例：file/walk —— 深度优先目录遍历（事件回调 + 统计）
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtWalkOptionsInit + MaxDepth   深度/跟随符号链接等策略
 *   xrtFileWalk       遍历目录树，ENTER/LEAVE/ITEM 事件回调
 *   xwalkstats        统计：条目/文件/目录/字节数
 * 模块宏：XRT_MODULE_FILE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/file/walk/main.c -lws2_32 -liphlpapi
 * 预期输出（随目录内容变化；MaxDepth=1 只下一层）：
 *   + .
 *     + .git
 *     - .git
 *   items=N files=N directories=N bytes=N
 *
 * ENTER/LEAVE 配对让"目录级收尾"（先删内容再删目录）自然表达；
 *   回调返回控制可跳过子树（SKIP）或中止（ABORT）——
 *   构建系统、备份、清理工具的遍历底座。
 */


/* 按事件打印一个目录树。 */
static xwalkcontrol printEntry(const xwalkentry* pEntry, ptr pUserData)
{
	const char* sKind = pEntry->Event == XWALK_ENTER ? "+" :
		(pEntry->Event == XWALK_LEAVE ? "-" : " ");

	(void)pUserData;
	printf("%*s%s %s\n", (int)(pEntry->Depth * 2u), "",
		sKind, pEntry->Path);
	return XWALK_CONTINUE;
}



/* 展示深度优先目录遍历。 */
int main(void)
{
	xwalkoptions Options;
	xwalkstats Stats;

	xrtWalkOptionsInit(&Options);
	Options.MaxDepth = 1u;
	if ( !xrtFileWalk(".", &Options, printEntry, NULL, &Stats) ) {
		return 1;
	}
	printf("items=%llu files=%llu directories=%llu bytes=%llu\n",
		(unsigned long long)Stats.Items,
		(unsigned long long)Stats.Files,
		(unsigned long long)Stats.Directories,
		(unsigned long long)Stats.Bytes);
	return 0;
}
