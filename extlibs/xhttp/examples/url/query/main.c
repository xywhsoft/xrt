#include <stdio.h>

#include <xhttp.h>



/* 演示无分配遍历重复查询键，并区分缺失值和空值。 */
int main(void)
{
	xstrview Query = XRT_STR_LITERAL("?tag=c&tag=xlang&debug&empty=");
	xquerypair Pair;
	size_t iOffset = 0;

	for ( ;; ) {
		xquerynext Next = xrtQueryNext(Query, &iOffset, &Pair);

		if ( Next == XQUERY_NEXT_END ) {
			break;
		}
		if ( Next == XQUERY_NEXT_ERROR ) {
			return 1;
		}
		printf("%.*s", (int)Pair.Key.Size, Pair.Key.Data);
		if ( (Pair.Flags & XQUERY_HAS_VALUE) != 0 ) {
			printf(" = %.*s", (int)Pair.Value.Size, Pair.Value.Data);
		} else {
			printf(" = <missing>");
		}
		printf("\n");
	}
	return 0;
}
