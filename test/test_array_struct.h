


// test struct
typedef struct {
	int val;
	double num;
} Test_Struct, *Test_Object;



// array sort proc
int ArraySortProc_Struct(Test_Object v1, Test_Object v2)
{
	return v1->val - v2->val;
}



void Test_Array_Struct(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Array [Struct] 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("array test subject 1 : create array (struct)\n\n");
	xarray arr = xrtArrayCreate(sizeof(Test_Struct));
	if ( arr ) {
		arr->AllocStep = 16;
		printf("array object : %p\t\t\t\t\tpass! √\n", arr);
		printf("\tMemory : %p\t\t=> 0\n", arr->Memory);
		printf("\tItemLength : %d\t\t\t\t=> %d\n", arr->ItemLength, sizeof(Test_Struct));
		printf("\tCount : %d\t\t\t\t=> 0\n", arr->Count);
		printf("\tAllocCount : %d\t\t\t\t=> 0\n", arr->AllocCount);
		printf("\tAllocStep : %d\t\t\t\t=> 16\n", arr->AllocStep);
	} else {
		printf("array object : %p\t\t\t\t\tfail! ×\n", arr);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : struct init
	printf("array test subject 2 : struct init (Please check print)\n\n");
	xarray_struct stuArr;
	xrtArrayInit(&stuArr, sizeof(Test_Struct));
	stuArr.AllocStep = 16;
	printf("\tMemory : %p\t\t=> 0\n", stuArr.Memory);
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 0\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 0\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : malloc
	printf("array test subject 3 : malloc\n\n");
	int bRet = xrtArrayAlloc(&stuArr, 4);
	printf("xrtArrayAlloc return value : %d\t\t\t=> 1\n", bRet);
	if ( stuArr.Memory ) {
		printf("array memory ptr : %p\t\t\t\tpass! √\n", stuArr.Memory);
		printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
		printf("\tCount : %d\t\t\t\t=> 0\n", stuArr.Count);
		printf("\tAllocCount : %d\t\t\t\t=> 4\n", stuArr.AllocCount);
		printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	} else {
		printf("array memory ptr : %p\t\t\t\tfail! ×\n", stuArr.Memory);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : append item
	printf("array test subject 4 : append item\n\n");
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = xrtArrayAppend(&stuArr, 1);
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, idx);
		if ( objStu ) {
			objStu->val = idx * 1000 + idx;
			objStu->num = idx * 1000 + idx / 1000.0;
			printf("xrtArrayAppend idx : %d\t( %d, \t%f\t)\tpass! √\n", idx, objStu->val, objStu->num);
		} else {
			printf("xrtArrayAppend idx : %d\t\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 10\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 21 ( 4 + 1 + 16 )\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
	printf("\narray values : \n");
	printf("\tidx\tptr\t\t\tval\t\tnum\t\texpected val\n");
	int t1val[10] = { 1001, 2002, 3003, 4004, 5005, 6006, 7007, 8008, 9009, 10010 };
	for ( int i = 1; i <= stuArr.Count; i++ ) {
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, i);
		if ( objStu->val == t1val[i - 1] ) {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tsucc! √\n", i, objStu, objStu->val, objStu->num, t1val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tfail! ×\n", i, objStu, objStu->val, objStu->num, t1val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : insert item
	printf("array test subject 5 : insert item\n\n");
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = xrtArrayInsert(&stuArr, i * 2, 1);
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, idx);
		if ( objStu ) {
			idx = i + 1;
			objStu->val = idx * 10000 + idx;
			objStu->num = idx * 10000 + idx / 10000.0;
			printf("xrtArrayInsert idx : %d\t( %d, \t%f\t)\tpass! √\n", idx, objStu->val, objStu->num);
		} else {
			printf("xrtArrayInsert idx : %d\t\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 20\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 21 ( 4 + 1 + 16 )\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
	printf("\narray values : \n");
	printf("\tidx\tptr\t\t\tval\t\tnum\t\texpected val\n");
	int t2val[20] = { 10001, 1001, 20002, 2002, 30003, 3003, 40004, 4004, 50005, 5005, 60006, 6006, 70007, 7007, 80008, 8008, 90009, 9009, 100010, 10010 };
	for ( int i = 1; i <= stuArr.Count; i++ ) {
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, i);
		if ( objStu->val == t2val[i - 1] ) {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tsucc! √\n", i, objStu, objStu->val, objStu->num, t2val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tfail! ×\n", i, objStu, objStu->val, objStu->num, t2val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 6 : remove item
	printf("array test subject 6 : remove item\n\n");
	if ( xrtArrayRemove(&stuArr, 17, 1) ) {
		printf("xrtArrayRemove idx : 17 (90009)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArrayRemove idx : 17 (90009)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArrayRemove(&stuArr, 14, 1) ) {
		printf("xrtArrayRemove idx : 14 (7007)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArrayRemove idx : 14 (7007)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArrayRemove(&stuArr, 11, 1) ) {
		printf("xrtArrayRemove idx : 11 (60006)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArrayRemove idx : 11 (60006)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArrayRemove(&stuArr, 8, 1) ) {
		printf("xrtArrayRemove idx : 8 (4004)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArrayRemove idx : 8 (4004)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArrayRemove(&stuArr, 5, 1) ) {
		printf("xrtArrayRemove idx : 5 (30003)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArrayRemove idx : 5 (30003)\t\t\t\t\tfail! ×\n");
	}
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = xrtArrayAppend(&stuArr, 1);
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, idx);
		if ( objStu ) {
			idx = i + 1;
			objStu->val = idx * 100 + idx;
			objStu->num = idx * 100 + idx / 100.0;
			printf("xrtArrayAppend idx : %d\t(\t%d, \t%f\t)\tpass! √\n", idx, objStu->val, objStu->num);
		} else {
			printf("xrtArrayAppend idx : %d\t\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 25\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 38 ( 4 + 1 + 1 + 16 + 16 )\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
	printf("\narray values : \n");
	printf("\tidx\tptr\t\t\tval\t\tnum\t\texpected val\n");
	int t3val[25] = { 10001, 1001, 20002, 2002, 3003, 40004, 50005, 5005, 6006, 70007, 80008, 8008, 9009, 100010, 10010, 101, 202, 303, 404, 505, 606, 707, 808, 909, 1010 };
	for ( int i = 1; i <= stuArr.Count; i++ ) {
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, i);
		if ( objStu->val == t3val[i - 1] ) {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tsucc! √\n", i, objStu, objStu->val, objStu->num, t3val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tfail! ×\n", i, objStu, objStu->val, objStu->num, t3val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 7 : swap item
	printf("array test subject 7 : swap item\n\n");
	if ( xrtArraySwap(&stuArr, 3, 13) ) {
		printf("xrtArraySwap 3 - 13 (20002 - 9009)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArraySwap 3 - 13 (20002 - 9009)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArraySwap(&stuArr, 5, 15) ) {
		printf("xrtArraySwap 5 - 15 (3003 - 10010)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArraySwap 5 - 15 (3003 - 10010)\t\t\t\t\tfail! ×\n");
	}
	if ( xrtArraySwap(&stuArr, 7, 17) ) {
		printf("xrtArraySwap 7 - 17 (50005 - 202)\t\t\t\t\tpass! √\n");
	} else {
		printf("xrtArraySwap 7 - 17 (50005 - 202)\t\t\t\t\tfail! ×\n");
	}
	
	printf("\narray state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 25\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 38 ( 4 + 1 + 1 + 16 + 16 )\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
	printf("\narray values : \n");
	printf("\tidx\tptr\t\t\tval\t\tnum\t\texpected val\n");
	int t4val[25] = { 10001, 1001, 9009, 2002, 10010, 40004, 202, 5005, 6006, 70007, 80008, 8008, 20002, 100010, 3003, 101, 50005, 303, 404, 505, 606, 707, 808, 909, 1010 };
	for ( int i = 1; i <= stuArr.Count; i++ ) {
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, i);
		if ( objStu->val == t4val[i - 1] ) {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tsucc! √\n", i, objStu, objStu->val, objStu->num, t4val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tfail! ×\n", i, objStu, objStu->val, objStu->num, t4val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 8 : sort
	printf("array test subject 8 : sort\n\n");
	xrtArraySort(&stuArr, ArraySortProc_Struct);
	
	printf("array state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 25\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 38 ( 4 + 1 + 1 + 16 + 16 )\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
	printf("\narray values : \n");
	printf("\tidx\tptr\t\t\tval\t\tnum\t\texpected val\n");
	int t5val[25] = { 101, 202, 303, 404, 505, 606, 707, 808, 909, 1001, 1010, 2002, 3003, 5005, 6006, 8008, 9009, 10001, 10010, 20002, 40004, 50005, 70007, 80008, 100010 };
	for ( int i = 1; i <= stuArr.Count; i++ ) {
		Test_Object objStu = xrtArrayGet_Unsafe(&stuArr, i);
		if ( objStu->val == t5val[i - 1] ) {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tsucc! √\n", i, objStu, objStu->val, objStu->num, t5val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t\t%f\t%d\t\tfail! ×\n", i, objStu, objStu->val, objStu->num, t5val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject X : destroy & struct unit
	printf("array test subject X : destroy & struct unit\n\n");
	xrtArrayDestroy(arr);
	printf("array object (%p) already destroyed!\n\n", arr);
	
	xrtArrayUnit(&stuArr);
	printf("array state (struct) : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", stuArr.ItemLength, sizeof(Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 0\n", stuArr.Count);
	printf("\tAllocCount : %d\t\t\t\t=> 0\n", stuArr.AllocCount);
	printf("\tAllocStep : %d\t\t\t\t=> 16\n", stuArr.AllocStep);
	
}


