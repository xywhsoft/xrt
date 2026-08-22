#include <stdio.h>
#include <xruntime.h>



/* 展示类型集合的唯一值、规范查询和稳定插入顺序。 */
int main(void)
{
	xtypedset Values;
	xtypedsetiter Iterator;
	int64 arrValues[] = { 7, 11, 7, 13 };
	const int64* pValue;

	if ( !xrtTypedSetInit(&Values, xrtTypeInt64()) ) {
		return 1;
	}
	for ( size_t i = 0u; i < 4u; i++ ) {
		if ( !xrtTypedSetAdd(&Values, &arrValues[i]) ) {
			xrtTypedSetUnit(&Values);
			return 2;
		}
	}
	if ( !xrtTypedSetIterBegin(&Values, &Iterator) ) {
		xrtTypedSetUnit(&Values);
		return 3;
	}
	while ( (pValue = (const int64*)xrtTypedSetIterNext(
		&Iterator
	)) != NULL ) {
		printf("value=%lld\n", (long long)*pValue);
	}
	xrtTypedSetIterEnd(&Iterator);
	printf("count=%llu\n", (unsigned long long)xrtTypedSetCount(&Values));
	xrtTypedSetUnit(&Values);
	return 0;
}
