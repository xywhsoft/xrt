


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
		AVLTREE_FOREACH(objTree, var) {
			AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)var;
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 4 : test iterator with macro [type]
		printf("AVLTree Iterator test subject 4 : iterator with macro [type]\n");
		printf("\tIterator (macro): ");
		
		printCount = 0;
		AVLTREE_FOREACH_TYPE(objTree, pData, AVLT_Test_Iter_Struct*) {
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 5 : test iterator manually
		printf("AVLTree Iterator test subject 5 : iterator manually\n");
		printf("\tIterator (manual): ");
		
		xrtAVLTreeIterBegin(objTree);
		printCount = 0;
		for ( ptr var = xrtAVLTreeIterNext(objTree); var != NULL; var = xrtAVLTreeIterNext(objTree) ) {
			AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)var;
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
		
		
		
		// subject 6 : test empty tree
		printf("AVLTree Iterator test subject 6 : empty tree\n");
		xavltree emptyTree = xrtAVLTreeCreate(sizeof(AVLT_Test_Iter_Struct), (void*)avltree_iter_comp_proc);
		if (emptyTree) {
			printf("\tEmpty tree count: %u\n", emptyTree->Count);
			
			xrtAVLTreeIterBegin(emptyTree);
			ptr result = xrtAVLTreeIterNext(emptyTree);
			if (result == NULL) {
				printf("\tEmpty iterator correctly returns NULL\n");
			}
			printf("\n");
			
			// 清理
			xrtAVLTreeDestroy(emptyTree);
		}
		
		
		
		// subject 7 : test iterator early end
		printf("AVLTree Iterator test subject 7 : iterator early end\n");
		printf("\tIterator (early end): ");
		
		xrtAVLTreeIterBegin(objTree);
		printCount = 0;
		for (ptr var = xrtAVLTreeIterNext(objTree); var != NULL; var = xrtAVLTreeIterNext(objTree)) {
			AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)var;
			printf("%d ", pData->Val);
			printCount++;
			if (printCount >= 5) {
				AVLTREE_BREAK(objTree);
			}
		}
		printf("  [Count: %d, early break]\n\n", printCount);
		
		
		
		// subject 8 : test iterator after operations
		printf("AVLTree Iterator test subject 8 : iterator after operations\n");
		printf("\tIterator (after remove node 5): ");
		
		xrtAVLTreeRemove(objTree, (ptr)(intptr_t)5);
		
		xrtAVLTreeIterBegin(objTree);
		printCount = 0;
		for (ptr var = xrtAVLTreeIterNext(objTree); var != NULL; var = xrtAVLTreeIterNext(objTree)) {
			AVLT_Test_Iter_Struct* pData = (AVLT_Test_Iter_Struct*)var;
			printf("%d ", pData->Val);
			printCount++;
		}
		printf("  [Count: %d]\n\n", printCount);
	}
	
	// 清理
	xrtAVLTreeDestroy(objTree);
	
	printf("AVLTree Iterator test completed!\n");
}
