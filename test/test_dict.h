


// test AVLHT32 struct
typedef struct {
	int Val;
} AVLHT32_Test_Struct, *AVLHT32_Test_Object;



// each table
int test_avlht32_eachproc(Dict_Key* pKey, AVLHT32_Test_Object pVal, void* pArg)
{
	printf("\t%s = %d (keylen:%d, Hash:%d)\n", pKey->Key, pVal->Val, pKey->KeyLen, pKey->Hash);
	return 0;
}



// Dict 库测试
void Test_Dict(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Dict 库测试 :\n\n");
	
	
	
	xmempool objMP = xrtMemPoolCreate(1);
	
	
	
	// subject 1 : create object
	printf("Dict test subject 1 : create object\n\n");
	xdict objHT = xrtDictCreate(sizeof(AVLT_Test_Struct));
	if ( objHT ) {
		objHT->MP = objMP;
		printf("Dict object : %p\t\t\t\tpass! √\n", objHT);
		printf("\tCount : %d\t\t\t\t=> 0\n", objHT->AVLT.Count);
		printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
		printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
		printf("\tMM.arrMMU.Count : %d\t\t\t=> 0\n", objHT->AVLT.objMM.arrMMU.Count);
		printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 0\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
		printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 0\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
		printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
		if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
			printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
		} else {
			printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
		}
	} else {
		printf("Dict object : %p\t\t\t\tfail! ×\n", objHT);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : set key & value
	printf("Dict test subject 2 : set key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		char sKey[32];
		int iLen = sprintf(sKey, "key-idx-%d", i);
		AVLHT32_Test_Object pVal = xrtDictSet(objHT, sKey, iLen, NULL);
		if ( pVal ) {
			pVal->Val = i;
			printf("Dict set key = value, %s = %d\t\t\t\tpass! √\n", sKey, i);
		} else {
			printf("Dict set key = value, %s = %d\t\t\t\tfail! ×\n", sKey, i);
		}
	}
	
	printf("get key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		char sKey[32];
		int iLen = sprintf(sKey, "key-idx-%d", i);
		AVLHT32_Test_Object pVal = xrtDictGet(objHT, sKey, iLen);
		if ( pVal ) {
			printf("Dict get key = value, %s = %d\t\t\t\tpass! √\n", sKey, pVal->Val);
		} else {
			printf("Dict get key = value, %s = %d\t\t\t\tfail! ×\n", sKey, pVal->Val);
		}
	}
	
	printf("\nDict state : \n");
	printf("\tCount : %d\t\t\t\t=> 10\n", objHT->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 1\n", objHT->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 1\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	printf("\nDict List : \n");
	xrtDictWalk(objHT, (void*)test_avlht32_eachproc, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : remove key & value
	printf("Dict test subject 3 : remove key & value\n\n");
	for ( int i = 0; i < 10; i++ ) {
		if ( (i & 1) == 0 ) {
			char sKey[32];
			int iLen = sprintf(sKey, "key-idx-%d", i);
			int bRet = xrtDictRemove(objHT, sKey, iLen);
			if ( bRet ) {
				printf("Dict remove key (%s)\t\t\t\t\tpass! √\n", sKey);
			} else {
				printf("Dict remove key (%s)\t\t\t\t\tfail! ×\n", sKey);
			}
		}
	}
	
	printf("\nDict state : \n");
	printf("\tCount : %d\t\t\t\t=> 5\n", objHT->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 1\n", objHT->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 1\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	printf("\nDict List : \n");
	xrtDictWalk(objHT, (void*)test_avlht32_eachproc, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : 1m k&v stress testing
	printf("Dict test subject 4 : 1m k&v stress testing\n\n");
	clock_t tStart = clock();
	for ( int i = 0; i < 1000000; i++ ) {
		char sKey[32];
		int iLen = sprintf(sKey, "key-idx-%d", i);
		AVLHT32_Test_Object pVal = xrtDictSet(objHT, sKey, iLen, NULL);
		if ( pVal ) {
			pVal->Val = i;
		} else {
			printf("Dict set key = value, %s = %d\t\t\t\tfail! ×\n", sKey, i);
		}
	}
	clock_t tStop = clock();
	printf("Dict add 1000000 items time interval is: %f seconds\n", (double)(tStop - tStart) / CLOCKS_PER_SEC);
	
	printf("\nDict state : \n");
	printf("\tCount : %d\t\t\t\t=> 1000000\n", objHT->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 3907\n", objHT->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 16\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : 10m select stress testing
	printf("Dict test subject 5 : 10m select stress testing\n\n");
	tStart = clock();
	for ( int i = 0; i < 10000000; i++ ) {
		char sKey[32];
		int iLen = sprintf(sKey, "key-idx-%d", i / 10);
		AVLHT32_Test_Object pVal = xrtDictGet(objHT, sKey, iLen);
		if ( pVal == NULL ) {
			printf("Dict get key (%s) return NULL\t\t\t\t\tfail! ×\n", sKey);
		}
	}
	tStop = clock();
	printf("Dict select 10000000 items time interval is: %f seconds\n", (double)(tStop - tStart) / CLOCKS_PER_SEC);
	
	printf("\nDict state : \n");
	printf("\tCount : %d\t\t\t\t=> 1000000\n", objHT->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 3907\n", objHT->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 16\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject X : struct unit & destroy
	printf("Dict test subject X : struct unit & destroy\n\n");
	xrtDictUnit(objHT);
	printf("Dict object (%p) already unit!\n\n", objHT);
	
	printf("\nDict state : \n");
	printf("\tCount : %d\t\t\t\t=> 0\n", objHT->AVLT.Count);
	printf("\tMM.ItemLength : %d\t\t\t=> %d\n", objHT->AVLT.objMM.ItemLength, sizeof(xavltnode_struct) + sizeof(Dict_Key) + sizeof(AVLHT32_Test_Struct));
	printf("\tMM.arrMMU.ItemLength : %d\t\t=> %d\n", objHT->AVLT.objMM.arrMMU.ItemLength, sizeof(MMU_LLNode));
	printf("\tMM.arrMMU.Count : %d\t\t\t=> 0\n", objHT->AVLT.objMM.arrMMU.Count);
	printf("\tMM.arrMMU.PageMMU.Count : %d\t\t=> 0\n", objHT->AVLT.objMM.arrMMU.PageMMU.Count);
	printf("\tMM.arrMMU.PageMMU.AllocCount : %d\t=> 0\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocCount);
	printf("\tMM.arrMMU.PageMMU.AllocStep : %d\t=> 64\n", objHT->AVLT.objMM.arrMMU.PageMMU.AllocStep);
	if ( objHT->AVLT.objMM.arrMMU.PageMMU.Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objHT->AVLT.objMM.arrMMU.PageMMU.Memory);
	}
	
	xrtDictDestroy(objHT);
	printf("\nDict object (%p) already destroyed!\n", objHT);
	
	xrtMemPoolDestroy(objMP);
	
	
	
}


