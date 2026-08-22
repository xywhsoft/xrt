#include <stdio.h>
#include <xruntime.h>



/* 展示类型字典的复制键、默认值和插入顺序迭代。 */
int main(void)
{
	xtypeddict Scores;
	xtypeddictiter Iterator;
	xstrview Key;
	int64 iAlice = 95;
	int64 iBob = 88;
	int64* pScore;

	if ( !xrtTypedDictInit(&Scores, xrtTypeInt64()) ||
		 !xrtTypedDictSet(&Scores, XRT_STR_LITERAL("alice"), &iAlice) ||
		 !xrtTypedDictSet(&Scores, XRT_STR_LITERAL("bob"), &iBob) ) {
		return 1;
	}
	pScore = (int64*)xrtTypedDictGetOrAdd(
		&Scores, XRT_STR_LITERAL("carol"), NULL
	);
	if ( pScore == NULL ) {
		xrtTypedDictUnit(&Scores);
		return 2;
	}
	*pScore = 91;
	if ( !xrtTypedDictIterBegin(&Scores, &Iterator) ) {
		xrtTypedDictUnit(&Scores);
		return 3;
	}
	while ( (pScore = (int64*)xrtTypedDictIterNext(
		&Iterator, &Key
	)) != NULL ) {
		printf("%.*s=%lld\n", (int)Key.Size, Key.Data, (long long)*pScore);
	}
	xrtTypedDictIterEnd(&Iterator);
	xrtTypedDictUnit(&Scores);
	return 0;
}
