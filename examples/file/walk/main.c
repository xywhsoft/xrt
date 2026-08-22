#include <stdio.h>

#include <xrt.h>



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
