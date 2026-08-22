#include <stdio.h>
#include <xrt.h>



/* 稀疏会话状态直接保存在整数键对应的内联值槽中。 */
typedef struct sessionstate {
	unsigned Requests;
	bool Authenticated;
} sessionstate;



/* 演示零初始化值槽、稀疏键查找和有序迭代。 */
int main(void)
{
	xintmap tSessions;
	xintmapiter tIterator;
	sessionstate* pState;
	int64 iSessionId;
	bool bNew;

	if ( !xrtIntMapInit(&tSessions, sizeof(sessionstate)) ) {
		return 1;
	}

	pState = (sessionstate*)xrtIntMapGetOrAdd(&tSessions, 1000001, &bNew);
	if ( pState == NULL ) {
		xrtIntMapUnit(&tSessions);
		return 2;
	}
	pState->Requests++;
	pState->Authenticated = true;

	pState = (sessionstate*)xrtIntMapGetOrAdd(&tSessions, -9, &bNew);
	if ( pState == NULL ) {
		xrtIntMapUnit(&tSessions);
		return 3;
	}
	pState->Requests = 3;

	if ( !xrtIntMapIterBegin(&tSessions, &tIterator) ) {
		xrtIntMapUnit(&tSessions);
		return 4;
	}
	while ( (pState = (sessionstate*)xrtIntMapIterNext(&tIterator, &iSessionId)) != NULL ) {
		printf(
			"session=%lld requests=%u authenticated=%s\n",
			(long long)iSessionId,
			pState->Requests,
			pState->Authenticated ? "yes" : "no"
		);
	}
	xrtIntMapIterEnd(&tIterator);
	xrtIntMapUnit(&tSessions);
	return 0;
}
