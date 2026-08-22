#include <stdio.h>

#include <xrt.h>



/* 连接对象仅用于演示稳定代际句柄。 */
typedef struct connection {
	int ID;
} connection;



/* 演示槽复用后旧句柄不会误命中新连接。 */
int main(void)
{
	xslotmap tConnections;
	connection tFirst = { 100 };
	connection tSecond = { 200 };
	connection tReplacement = { 300 };
	xslot First;
	xslot Second;
	xslot Replacement;

	if ( !xrtSlotMapInit(&tConnections) ) {
		return 1;
	}
	First = xrtSlotMapInsert(&tConnections, &tFirst);
	Second = xrtSlotMapInsert(&tConnections, &tSecond);
	if ( (First == XRT_SLOT_INVALID) || (Second == XRT_SLOT_INVALID) ) {
		xrtSlotMapUnit(&tConnections);
		return 2;
	}

	if ( !xrtSlotMapRemove(&tConnections, First, NULL) ) {
		xrtSlotMapUnit(&tConnections);
		return 3;
	}
	Replacement = xrtSlotMapInsert(&tConnections, &tReplacement);
	if ( Replacement == XRT_SLOT_INVALID ) {
		xrtSlotMapUnit(&tConnections);
		return 4;
	}

	printf(
		"old=%llu replacement=%llu same-index=%s stale-valid=%s\n",
		(unsigned long long)First,
		(unsigned long long)Replacement,
		xrtSlotIndex(First) == xrtSlotIndex(Replacement) ? "yes" : "no",
		xrtSlotMapContains(&tConnections, First) ? "yes" : "no"
	);
	xrtSlotMapUnit(&tConnections);
	return 0;
}
