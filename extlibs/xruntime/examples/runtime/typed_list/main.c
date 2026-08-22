#include <stdio.h>
#include <xruntime.h>



/* 展示类型列表的稀疏键、自动追加和有序迭代。 */
int main(void)
{
	xtypedlist Values;
	xtypedlistiter Iterator;
	int64 iFirst = 7;
	int64 iSecond = 11;
	int64 iKey;
	int64* pValue;

	if ( !xrtTypedListInit(&Values, xrtTypeInt64()) ) {
		return 1;
	}
	if ( !xrtTypedListSet(&Values, -10, &iFirst) ||
		 !xrtTypedListAppend(&Values, &iSecond, &iKey) ||
		 !xrtTypedListIterBegin(&Values, &Iterator) ) {
		xrtTypedListUnit(&Values);
		return 2;
	}
	while ( (pValue = (int64*)xrtTypedListIterNext(
		&Iterator, &iKey
	)) != NULL ) {
		printf("key=%lld value=%lld\n",
			(long long)iKey, (long long)*pValue);
	}
	xrtTypedListIterEnd(&Iterator);
	xrtTypedListUnit(&Values);
	return 0;
}
