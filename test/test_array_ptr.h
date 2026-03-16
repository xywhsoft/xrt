


int ArraySortProc(void** p1, void** p2)
{
	return (intptr_t)*p1 - (intptr_t)*p2;
}



void Test_Array_Ptr(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Array [Ptr] 库测试 :\n\n");
	
	
	
	// subject 1 : create array (ptr)
	printf("array test subject 1 : create array (ptr)\n\n");
	xparray arr = xrtPtrArrayCreate(XRT_OBJMODE_LOCAL);
	if ( arr ) {
		printf("array object : %p\t\t\t\t\tpass! √\n", arr);
		printf("\tMemory : %p\t\t=> 0\n", arr->Memory);
		printf("\tCount : %d\t\t\t\t=> 0\n", arr->Count);
		printf("\tAllocCount : %d\t\t\t\t=> 0\n", arr->AllocCount);
	} else {
		printf("array object : %p\t\t\t\t\tfail! ×\n", arr);
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
	
	
	
	// subject 2 : append item
	printf("array test subject 2 : append item\n\n");
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = i + 1;
		idx = xrtPtrArrayAppend(arr, (void*)(intptr_t)(idx * 1000 + idx));
		if ( idx ) {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tpass! √\n", idx);
		} else {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 10\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 256\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t1val[10] = { (void*)1001, (void*)2002, (void*)3003, (void*)4004, (void*)5005, (void*)6006, (void*)7007, (void*)8008, (void*)9009, (void*)10010 };
	for ( int i = 1; i <= arr->Count; i++ ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t1val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t1val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t1val[i - 1]);
		}
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
	
	
	
	// subject 3 : insert item
	printf("array test subject 3 : insert item\n\n");
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = i + 1;
		idx = xrtPtrArrayInsert(arr, i * 2, (void*)(intptr_t)(idx * 10000 + idx));
		if ( idx ) {
			printf("xrtPtrArrayInsert idx : %d\t\t\t\t\tpass! √\n", idx);
		} else {
			printf("xrtPtrArrayInsert idx : %d\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 20\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 256\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t2val[20] = { (void*)10001, (void*)1001, (void*)20002, (void*)2002, (void*)30003, (void*)3003, (void*)40004, (void*)4004, (void*)50005, (void*)5005, (void*)60006, (void*)6006, (void*)70007, (void*)7007, (void*)80008, (void*)8008, (void*)90009, (void*)9009, (void*)100010, (void*)10010 };
	for ( int i = 1; i <= arr->Count; i++ ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t2val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t2val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t2val[i - 1]);
		}
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
	
	
	
	// subject 4 : remove item
	printf("array test subject 4 : remove item\n\n");
	if ( xrtPtrArrayRemove(arr, 17, 1) ) {
		printf("xrtPtrArrayRemove idx : 17 (90009)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArrayRemove idx : 17 (90009)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArrayRemove(arr, 14, 1) ) {
		printf("xrtPtrArrayRemove idx : 14 (7007)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArrayRemove idx : 14 (7007)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArrayRemove(arr, 11, 1) ) {
		printf("xrtPtrArrayRemove idx : 11 (60006)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArrayRemove idx : 11 (60006)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArrayRemove(arr, 8, 1) ) {
		printf("xrtPtrArrayRemove idx : 8 (4004)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArrayRemove idx : 8 (4004)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArrayRemove(arr, 5, 1) ) {
		printf("xrtPtrArrayRemove idx : 5 (30003)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArrayRemove idx : 5 (30003)\t\t\t\tfail! ×\n");
	}
	for ( int i = 0; i < 10; i++ ) {
		unsigned int idx = i + 1;
		idx = xrtPtrArrayAppend(arr, (void*)(intptr_t)(idx * 100 + idx));
		if ( idx ) {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tpass! √\n", idx);
		} else {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 25\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 256\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t3val[25] = { (void*)10001, (void*)1001, (void*)20002, (void*)2002, (void*)3003, (void*)40004, (void*)50005, (void*)5005, (void*)6006, (void*)70007, (void*)80008, (void*)8008, (void*)9009, (void*)100010, (void*)10010, (void*)101, (void*)202, (void*)303, (void*)404, (void*)505, (void*)606, (void*)707, (void*)808, (void*)909, (void*)1010 };
	for ( int i = 1; i <= arr->Count; i++ ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t3val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t3val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t3val[i - 1]);
		}
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
	
	
	
	// subject 5 : swap item
	printf("array test subject 5 : swap item\n\n");
	if ( xrtPtrArraySwap(arr, 3, 13) ) {
		printf("xrtPtrArraySwap 3 - 13 (20002 - 9009)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArraySwap 3 - 13 (20002 - 9009)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArraySwap(arr, 5, 15) ) {
		printf("xrtPtrArraySwap 5 - 15 (3003 - 10010)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArraySwap 5 - 15 (3003 - 10010)\t\t\t\tfail! ×\n");
	}
	if ( xrtPtrArraySwap(arr, 7, 17) ) {
		printf("xrtPtrArraySwap 7 - 17 (50005 - 202)\t\t\t\tpass! √\n");
	} else {
		printf("xrtPtrArraySwap 7 - 17 (50005 - 202)\t\t\t\tfail! ×\n");
	}
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 25\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 256\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t4val[25] = { (void*)10001, (void*)1001, (void*)9009, (void*)2002, (void*)10010, (void*)40004, (void*)202, (void*)5005, (void*)6006, (void*)70007, (void*)80008, (void*)8008, (void*)20002, (void*)100010, (void*)3003, (void*)101, (void*)50005, (void*)303, (void*)404, (void*)505, (void*)606, (void*)707, (void*)808, (void*)909, (void*)1010 };
	for ( int i = 1; i <= arr->Count; i++ ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t4val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t4val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t4val[i - 1]);
		}
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
	
	
	
	// subject 6 : malloc
	printf("array test subject 6 : malloc\n\n");
	for ( int i = 0; i < 260; i++ ) {
		unsigned int idx = xrtPtrArrayAppend(arr, (void*)(intptr_t)i);
		if ( idx ) {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tpass! √\n", idx);
		} else {
			printf("xrtPtrArrayAppend idx : %d\t\t\t\t\tfail! ×\n", idx);
		}
	}
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 285\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 512\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t5val[29] = { (void*)10001, (void*)80008, (void*)606, (void*)5, (void*)15, (void*)25, (void*)35, (void*)45, (void*)55, (void*)65, (void*)75, (void*)85, (void*)95, (void*)105, (void*)115, (void*)125, (void*)135, (void*)145, (void*)155, (void*)165, (void*)175, (void*)185, (void*)195, (void*)205, (void*)215, (void*)225, (void*)235, (void*)245, (void*)255 };
	int pos = 0;
	for ( int i = 1; i <= arr->Count; i+=10 ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t5val[pos] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t5val[pos]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t5val[pos]);
		}
		pos++;
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
	
	
	
	// subject 7 : sort
	printf("array test subject 7 : sort\n\n");
	xrtPtrArraySort(arr, ArraySortProc);
	
	printf("\narray state : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 285\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t=> 512\n", arr->AllocCount);
	
	printf("\narray values : \n");
	printf("\tidx\tval\texpected val\n");
	void* t6val[29] = { (void*)0, (void*)10, (void*)20, (void*)30, (void*)40, (void*)50, (void*)60, (void*)70, (void*)80, (void*)90, (void*)100, (void*)109, (void*)119, (void*)129, (void*)139, (void*)149, (void*)159, (void*)169, (void*)179, (void*)189, (void*)199, (void*)208, (void*)218, (void*)228, (void*)238, (void*)248, (void*)258, (void*)1010, (void*)40004 };
	pos = 0;
	for ( int i = 1; i <= arr->Count; i+=10 ) {
		void* val = xrtPtrArrayGet_Unsafe(arr, i);
		if ( val == t6val[pos] ) {
			printf("\t%d\t%d\t%d\t\t\t\t\tsucc! √\n", i, val, t6val[pos]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\t\tfail! ×\n", i, val, t6val[pos]);
		}
		pos++;
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
	printf("array test subject X : struct unit & destroy\n\n");
	xrtPtrArrayUnit(arr);
	printf("array object (%p) already unit!\n\n", arr);
	
	printf("array state (object) : \n");
	if ( arr->Memory ) {
		printf("\tMemory : %p\t\t\t\tfail! ×\n", arr->Memory);
	} else {
		printf("\tMemory : %p\t\t\t\tpass! √\n", arr->Memory);
	}
	printf("\tCount : %d\t\t\t\t=> 0\n", arr->Count);
	printf("\tAllocCount : %d\t\t\t\t=> 0\n", arr->AllocCount);
	
	xrtPtrArrayDestroy(arr);
	printf("\narray object (%p) already destroyed!\n", arr);
	
}


