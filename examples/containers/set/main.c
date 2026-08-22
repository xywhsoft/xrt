#include <stdio.h>
#include <xrt.h>



/* 演示去重、确定插入顺序和常用集合运算。 */
int main(void)
{
	xset tEnabled;
	xset tRequested;
	xset* pAllowed = NULL;
	xsetiter tIterator;
	const int* pPort;
	int arrEnabled[] = { 80, 443, 8080 };
	int arrRequested[] = { 443, 3000, 8080 };
	int iResult = 0;

	if ( !xrtSetInit(&tEnabled, sizeof(int)) ) {
		return 1;
	}
	if ( !xrtSetInit(&tRequested, sizeof(int)) ) {
		xrtSetUnit(&tEnabled);
		return 1;
	}
	for ( size_t i = 0; i < 3; i++ ) {
		if ( !xrtSetAdd(&tEnabled, &arrEnabled[i]) ||
			!xrtSetAdd(&tRequested, &arrRequested[i]) ) {
			iResult = 2;
			goto cleanup;
		}
	}
	pAllowed = xrtSetIntersection(&tRequested, &tEnabled);
	if ( pAllowed == NULL ) {
		iResult = 3;
		goto cleanup;
	}

	if ( !xrtSetIterBegin(pAllowed, &tIterator) ) {
		iResult = 4;
		goto cleanup;
	}
	while ( (pPort = (const int*)xrtSetIterNext(&tIterator)) != NULL ) {
		printf("allowed port: %d\n", *pPort);
	}
	xrtSetIterEnd(&tIterator);

cleanup:
	if ( pAllowed != NULL ) {
		xrtSetDestroy(pAllowed);
	}
	xrtSetUnit(&tRequested);
	xrtSetUnit(&tEnabled);
	return iResult;
}
