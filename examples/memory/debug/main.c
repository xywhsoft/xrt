#include <stdio.h>

#include <xrt.h>



/* 输出一条活动分配，真实项目可在这里接入日志或报告构建器。 */
static bool printAllocation(const xmemdebugallocation* pAllocation, ptr pUserData)
{
	(void)pUserData;
	printf("live address=%p size=%zu site=%s:%u\n",
		pAllocation->Address,
		pAllocation->Size,
		pAllocation->File != NULL ? pAllocation->File : "unknown",
		(unsigned int)pAllocation->Line);
	return true;
}



/* 演示快照和活动分配访问器的常见组合路径。 */
int main(void)
{
	xmemdebugsnapshot tSnapshot;
	unsigned char* pMemory;

	if ( !xrtMemDebugReset() ) {
		return 1;
	}
	pMemory = (unsigned char*)xrtMalloc(64);
	if ( pMemory == NULL ) {
		return 2;
	}
	pMemory[0] = 0x5A;

	xrtMemDebugSnapshot(&tSnapshot);
	printf("live_count=%zu live_bytes=%zu peak_bytes=%zu\n",
		tSnapshot.LiveCount,
		tSnapshot.LiveBytes,
		tSnapshot.PeakBytes);
	(void)xrtMemDebugVisitLive(printAllocation, NULL);

	xrtFree(pMemory);
	xrtMemDebugSnapshot(&tSnapshot);
	printf("alloc_count=%llu free_count=%llu events=%zu\n",
		(unsigned long long)tSnapshot.AllocCount,
		(unsigned long long)tSnapshot.FreeCount,
		tSnapshot.EventCount);
	return xrtMemDebugReset() ? 0 : 3;
}
