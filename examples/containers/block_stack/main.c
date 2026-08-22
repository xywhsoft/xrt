#include <stdio.h>

#include <xrt.h>



/* 演示跨块增长时仍可保留活动工作帧地址。 */
int main(void)
{
	xblockstack tFrames;
	int* pRoot;

	if ( !xrtBlockStackInitLayout(&tFrames, sizeof(int), sizeof(int), 4) ) {
		return 1;
	}
	pRoot = (int*)xrtBlockStackAdd(&tFrames);
	if ( pRoot == NULL ) {
		xrtBlockStackUnit(&tFrames);
		return 2;
	}
	*pRoot = 100;

	for ( int i = 1; i <= 20; i++ ) {
		if ( !xrtBlockStackPush(&tFrames, &i) ) {
			xrtBlockStackUnit(&tFrames);
			return 3;
		}
	}
	printf(
		"count=%zu blocks=%zu root=%d stable=%s\n",
		tFrames.Count,
		tFrames.Blocks.Count,
		*pRoot,
		xrtBlockStackGet(&tFrames, 0) == pRoot ? "yes" : "no"
	);

	xrtBlockStackUnit(&tFrames);
	return 0;
}
