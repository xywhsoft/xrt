


// test List struct
typedef struct {
	int Val;
} List_Test_Struct, *List_Test_Object;



// each table
int test_list_eachproc(int64 iKey, List_Test_Object pVal, void* pArg)
{
	printf("\t%d = %d\n", iKey, pVal->Val);
	return 0;
}



// List 库测试
void Test_List(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n List 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("List test subject 1 : create object\n\n");
	xlist objList = xrtListCreate(sizeof(AVLT_Test_Struct), XRT_OBJMODE_LOCAL);
	if ( objList ) {
		printf("List object : %p\t\t\t\t\tpass! √\n", objList);
		printf("\tCount : %d\t\t\t\t=> 0\n", objList->AVLT.Count);
		printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
		printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
		printf("\tMM.arrMMU.Count : %d\t\t\t=> 0\n", objList->AVLT.objMM.arrMMU.Count);
		printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 0\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
		printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 0\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
		printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
		if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
			printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
		} else {
			printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
		}
	} else {
		printf("List object : %p\t\t\t\tfail! ×\n", objList);
	}
	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
	
	
	
	// subject 2 : set key & value
	printf("List test subject 2 : set key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		List_Test_Object pVal = xrtListSet(objList, i, NULL);
		if ( pVal ) {
			pVal->Val = 100 + i;
			printf("List set key = value, %d = %d\t\t\t\tpass! √\n", i, 100 + i);
		} else {
			printf("List set key = value, %d = %d\t\t\t\tfail! ×\n", i, 100 + i);
		}
	}
	
	printf("get key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		List_Test_Object pVal = xrtListGet(objList, i);
		if ( pVal ) {
			printf("List get key = value, %d = %d\t\t\t\tpass! √\n", i, pVal->Val);
		} else {
			printf("List get key = value, %d = %d\t\t\t\tfail! ×\n", i, pVal->Val);
		}
	}
	
	printf("\nList state : \n");
	printf("\tCount : %d\t\t\t\t=> 10\n", objList->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 1\n", objList->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 1\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	printf("\nList List : \n");
	xrtListWalk(objList, (void*)test_list_eachproc, NULL);
	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
	
	
	
	// subject 3 : remove key & value
	printf("List test subject 3 : remove key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		if ( (i & 1) == 0 ) {
			int bRet = xrtListRemove(objList, i);
			if ( bRet ) {
				printf("List remove key (%d)\t\t\t\t\tpass! √\n", i);
			} else {
				printf("List remove key (%d)\t\t\t\t\tfail! ×\n", i);
			}
		}
	}
	
	printf("\nList state : \n");
	printf("\tCount : %d\t\t\t\t=> 5\n", objList->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 1\n", objList->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 1\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	printf("\nList List : \n");
	xrtListWalk(objList, (void*)test_list_eachproc, NULL);
	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
	
	
	
	// subject 4 : 1m k&v stress testing
	printf("List test subject 4 : 1m k&v stress testing\n\n");
	clock_t tStart = clock();
	for ( int i = 0; i < 1000000; i++ ) {
		List_Test_Object pVal = xrtListSet(objList, i, NULL);
		if ( pVal ) {
			pVal->Val = 10000 + i;
		} else {
			printf("List set key = value, %d = %d\t\t\t\tfail! ×\n", i, 10000 + i);
		}
	}
	clock_t tStop = clock();
	printf("List add 1000000 items time interval is: %f seconds\n", (double)(tStop - tStart) / CLOCKS_PER_SEC);
	
	printf("\nList state : \n");
	printf("\tCount : %d\t\t\t\t=> 1000000\n", objList->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 3907\n", objList->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 16\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
	
	
	
	// subject 5 : 10m select stress testing
	printf("List test subject 5 : 10m select stress testing\n\n");
	tStart = clock();
	for ( int i = 0; i < 10000000; i++ ) {
		List_Test_Object pVal = xrtListGet(objList, i / 10);
		if ( pVal == NULL ) {
			printf("List get key (%d) return NULL\t\t\t\t\tfail! ×\n", i);
		}
	}
	tStop = clock();
	printf("List select 10000000 items time interval is: %f seconds\n", (double)(tStop - tStart) / CLOCKS_PER_SEC);
	
	printf("\nList state : \n");
	printf("\tCount : %d\t\t\t\t=> 1000000\n", objList->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 3907\n", objList->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 16\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	printf("\n\n\n");
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	#if defined(_WIN32) || defined(_WIN64)
		system("cls");
	#else
		printf("\033[2J\033[H");
	#endif
	
	
	
	// subject X : struct unit & destroy
	printf("List test subject X : struct unit & destroy\n\n");
	xrtListUnit(objList);
	printf("List object (%p) already unit!\n\n", objList);
	
	printf("\nList state : \n");
	printf("\tCount : %d\t\t\t\t=> 0\n", objList->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objList->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(int64) + sizeof(List_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objList->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 0\n", objList->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 0\n", objList->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 0\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objList->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objList->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objList->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	xrtListDestroy(objList);
	printf("\nList object (%p) already destroyed!\n", objList);
	#if defined(_WIN32) || defined(_WIN64)
		system("pause");
	#else
		printf("Press Enter to continue...");
		getchar();
	#endif
	
	
	
}


