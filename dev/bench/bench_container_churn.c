#include "xnet2/bench_common.h"
#include "../../xrt.h"

typedef struct {
	int iKey;
	int iValue;
} __bench_avl_item;

static int __benchAvlComp(ptr pNode, ptr pKey)
{
	const __bench_avl_item* pItem = (const __bench_avl_item*)pNode;
	int iNodeKey = pItem ? pItem->iKey : 0;
	int iSearchKey = (int)(intptr_t)pKey;
	if ( iNodeKey < iSearchKey ) return -1;
	if ( iNodeKey > iSearchKey ) return 1;
	return 0;
}

static void __benchArrayLane(uint32_t iIterations, uint32_t iItems)
{
	uint32_t i;
	for ( i = 0; i < iIterations; ++i ) {
		xarray pArr = xrtArrayCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		uint32_t j;
		if ( !pArr ) exit(20);
		for ( j = 0; j < iItems; ++j ) {
			uint32_t iIdx = xrtArrayAppend(pArr, 1u);
			int* pVal = (int*)xrtArrayGet_Unsafe(pArr, iIdx);
			if ( !pVal ) exit(21);
			*pVal = (int)j;
		}
		xrtArrayDestroy(pArr);
	}
}

static void __benchDynStackLane(uint32_t iIterations, uint32_t iItems)
{
	uint32_t i;
	for ( i = 0; i < iIterations; ++i ) {
		xdynstack pStack = xrtDynStackCreate(sizeof(int));
		uint32_t j;
		if ( !pStack ) exit(22);
		for ( j = 0; j < iItems; ++j ) {
			int iVal = (int)j;
			if ( !xrtDynStackPushData(pStack, &iVal) ) exit(23);
		}
		xrtDynStackDestroy(pStack);
	}
}

static void __benchListLane(uint32_t iIterations, uint32_t iItems)
{
	uint32_t i;
	for ( i = 0; i < iIterations; ++i ) {
		xlist pList = xrtListCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		uint32_t j;
		if ( !pList ) exit(24);
		for ( j = 0; j < iItems; ++j ) {
			int* pVal = (int*)xrtListSet(pList, (int64)j, NULL);
			if ( !pVal ) exit(25);
			*pVal = (int)j;
		}
		xrtListDestroy(pList);
	}
}

static void __benchDictLane(uint32_t iIterations, uint32_t iItems)
{
	uint32_t i;
	for ( i = 0; i < iIterations; ++i ) {
		xdict pDict = xrtDictCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		uint32_t j;
		if ( !pDict ) exit(26);
		for ( j = 0; j < iItems; ++j ) {
			char sKey[32];
			int* pVal;
			snprintf(sKey, sizeof(sKey), "k%u", (unsigned)j);
			pVal = (int*)xrtDictSet(pDict, sKey, (uint32_t)strlen(sKey), NULL);
			if ( !pVal ) exit(27);
			*pVal = (int)j;
		}
		xrtDictDestroy(pDict);
	}
}

static void __benchAvlLane(uint32_t iIterations, uint32_t iItems)
{
	uint32_t i;
	for ( i = 0; i < iIterations; ++i ) {
		xavltree pTree = xrtAVLTreeCreate(sizeof(__bench_avl_item), __benchAvlComp, XRT_OBJMODE_LOCAL);
		uint32_t j;
		if ( !pTree ) exit(28);
		for ( j = 0; j < iItems; ++j ) {
			__bench_avl_item* pItem = (__bench_avl_item*)xrtAVLTreeInsert(pTree, (ptr)(intptr_t)j, NULL);
			if ( !pItem ) exit(29);
			pItem->iKey = (int)j;
			pItem->iValue = (int)(j * 2u);
		}
		xrtAVLTreeDestroy(pTree);
	}
}

int main(int argc, char** argv)
{
	uint32_t iIterations = xbenchArgU32(argc, argv, 1, 2000u);
	uint32_t iItems = xbenchArgU32(argc, argv, 2, 128u);
	xbenchtimer tTimer;
	uint64_t iElapsedArray;
	uint64_t iElapsedDynStack;
	uint64_t iElapsedList;
	uint64_t iElapsedDict;
	uint64_t iElapsedAvl;

	printf("xrt memory bench_container_churn\n");
	printf("iterations=%" PRIu32 "\n", iIterations);
	printf("items_per_container=%" PRIu32 "\n", iItems);

	if ( !xrtInit() ) return 1;

	xbenchTimerStart(&tTimer);
	__benchArrayLane(iIterations, iItems);
	xbenchTimerStop(&tTimer);
	iElapsedArray = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchDynStackLane(iIterations, iItems);
	xbenchTimerStop(&tTimer);
	iElapsedDynStack = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchListLane(iIterations, iItems);
	xbenchTimerStop(&tTimer);
	iElapsedList = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchDictLane(iIterations, iItems);
	xbenchTimerStop(&tTimer);
	iElapsedDict = xbenchTimerElapsedNs(&tTimer);

	xbenchTimerStart(&tTimer);
	__benchAvlLane(iIterations, iItems);
	xbenchTimerStop(&tTimer);
	iElapsedAvl = xbenchTimerElapsedNs(&tTimer);

	xbenchPrintMetricU64("array_elapsed_ns", iElapsedArray);
	xbenchPrintMetricDouble("array_containers_per_sec", xbenchSafeRate(iIterations, iElapsedArray));
	xbenchPrintMetricDouble("array_items_per_sec", xbenchSafeRate((uint64_t)iIterations * (uint64_t)iItems, iElapsedArray));

	xbenchPrintMetricU64("dynstack_elapsed_ns", iElapsedDynStack);
	xbenchPrintMetricDouble("dynstack_containers_per_sec", xbenchSafeRate(iIterations, iElapsedDynStack));
	xbenchPrintMetricDouble("dynstack_items_per_sec", xbenchSafeRate((uint64_t)iIterations * (uint64_t)iItems, iElapsedDynStack));

	xbenchPrintMetricU64("list_elapsed_ns", iElapsedList);
	xbenchPrintMetricDouble("list_containers_per_sec", xbenchSafeRate(iIterations, iElapsedList));
	xbenchPrintMetricDouble("list_items_per_sec", xbenchSafeRate((uint64_t)iIterations * (uint64_t)iItems, iElapsedList));

	xbenchPrintMetricU64("dict_elapsed_ns", iElapsedDict);
	xbenchPrintMetricDouble("dict_containers_per_sec", xbenchSafeRate(iIterations, iElapsedDict));
	xbenchPrintMetricDouble("dict_items_per_sec", xbenchSafeRate((uint64_t)iIterations * (uint64_t)iItems, iElapsedDict));

	xbenchPrintMetricU64("avltree_elapsed_ns", iElapsedAvl);
	xbenchPrintMetricDouble("avltree_containers_per_sec", xbenchSafeRate(iIterations, iElapsedAvl));
	xbenchPrintMetricDouble("avltree_items_per_sec", xbenchSafeRate((uint64_t)iIterations * (uint64_t)iItems, iElapsedAvl));

	xrtUnit();
	return 0;
}
