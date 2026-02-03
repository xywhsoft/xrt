


// test AVLTree Iterator
typedef struct {
	int Val;
} AVLT_Test_Iter_Struct, *AVLT_Test_Iter_Object;



// AVLTree compear function
int avltree_iter_comp_proc(AVLT_Test_Iter_Object pNode, ptr pKey)
{
	return (intptr_t)pKey - pNode->Val;
}



// print callback function
bool avltree_iter_print_callback(ptr pNode, ptr pArg)
{
	AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)pNode;
	printf("%d ", pData->Val);
	(*(int*)pArg)++;
	return FALSE;
}



// AVLTree Iterator 测试
void Test_AVLTree_Iterator(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n AVLTree Iterator 测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("AVLTree Iterator test subject 1 : create object\n");
	xavltree objTree = xrtAVLTreeCreate(sizeof(AVLT_Test_Iter_Struct), (void*)avltree_iter_comp_proc);
	if (objTree) {
		printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objTree->objMM.ItemLength, sizeof(AVLT_Test_Iter_Struct) + sizeof(xavltnode_struct));
		
		// 插入测试数据
		int testVals[] = {5, 3, 7, 1, 9, 4, 6, 2, 8, 10, 0, 11};
		for (int i = 0; i < 12; i++) {
			bool isNew;
			AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)xrtAVLTreeInsert(objTree, (ptr)(intptr_t)testVals[i], &isNew);
			if (pData && isNew) {
				pData->Val = testVals[i];
				printf("\tInsert: %d  [New]\n", testVals[i]);
			} else if (pData) {
				printf("\tInsert: %d  [Exists]\n", testVals[i]);
			}
		}
		printf("\tTotal nodes: %u\n\n", objTree->Count);
		
		
		
		// subject 2 : test callback walk (中序遍历）
		printf("AVLTree Iterator test subject 2 : callback walk\n");
		printf("\tRoot value: ");
		if (objTree->RootNode) {
			AVLT_Test_Iter_Struct* pRoot = (AVLT_Test_Iter_Struct*)xrtAVLTreeGetNodeData(objTree->RootNode);
			printf("%d\n", pRoot->Val);
		} else {
			printf("NULL\n");
		}
		printf("\tCallback walk: ");
		
		int printCount = 0;
		printCount = 0;
		xrtAVLTreeWalk(objTree, avltree_iter_print_callback, (ptr)&printCount);
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 3 : test iterator with macro
		printf("AVLTree Iterator test subject 3 : iterator with macro\n");
		printf("\tIterator (macro): ");
		
		printCount = 0;
		AVLT_Test_Iter_Struct* pData;
		XRT_FOREACH_AVLTREE_BASE((xavltbase)objTree, AVLT_Test_Iter_Struct, pData) {
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 4 : test iterator with index
		printf("AVLTree Iterator test subject 4 : iterator with index\n");
		printf("\tIterator (with index):\n");
		
		int idx = 0;
		XRT_FOREACH_AVLTREE_BASE_INDEX((xavltbase)objTree, AVLT_Test_Iter_Struct, pData, idx) {
			printf("\t  [%d] = %d\n", idx, pData->Val);
		}
		printf("  [Total: %d]\n\n", idx);
		
		
		
		// subject 5 : test iterator manually
		printf("AVLTree Iterator test subject 5 : iterator manually\n");
		printf("\tIterator (manual): ");
		
		xavliterator_base iter = xrtAVLTB_IteratorCreate((xavltbase)objTree);
		printCount = 0;
		while (!xrtAVLTB_IteratorEnd(iter)) {
			pData = (AVLT_Test_Iter_Struct*)xrtAVLTB_IteratorNext(iter);
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 6 : test empty tree
		printf("AVLTree Iterator test subject 6 : empty tree\n");
		xavltree emptyTree = xrtAVLTreeCreate(sizeof(AVLT_Test_Iter_Struct), (void*)avltree_iter_comp_proc);
		if (emptyTree) {
			printf("\tEmpty tree count: %u\n", emptyTree->Count);
			
			xavliterator_base emptyIter = xrtAVLTB_IteratorCreate((xavltbase)emptyTree);
			if (xrtAVLTB_IteratorEnd(emptyIter)) {
				printf("\tEmpty iterator correctly reports END\n");
			}
			printf("\n");
		}
		
		
		
		// 清理
		xrtAVLTreeDestroy(emptyTree);
	}
	
	// 清理
	xrtAVLTreeDestroy(objTree);
	
	printf("AVLTree Iterator test completed!\n");
}
