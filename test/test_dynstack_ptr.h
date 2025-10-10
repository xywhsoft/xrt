


// Stack 库测试
void Test_DynStack_Ptr(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Point Dynamic Stack 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("Stack test subject 1 : create object\n\n");
	xdynstack objSTK = xrtDynStackCreate(sizeof(ptr));
	if ( objSTK ) {
		printf("Stack object : %p\t\t\t\tpass! √\n", objSTK);
		printf("\tCount : %d\t\t\t\t=> 0\n", objSTK->Count);
		printf("\tMMU.Count : %d\t\t\t\t=> 0\n", objSTK->MMU.Count);
		printf("\tMMU.AllocCount : %d\t\t\t=> 0\n", objSTK->MMU.AllocCount);
		if ( objSTK->MMU.Memory ) {
			printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
		} else {
			printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
		}
	} else {
		printf("Stack object : %p\t\t\t\t\tfail! ×\n", objSTK);
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : push
	printf("Stack test subject 2 : push\n\n");
	for ( int i = 0; i < 5; i++ ) {
		int idx = xrtDynStackPushPtr(objSTK, (void*)(intptr_t)i);
		printf("push stack idx : %d (%d)\n", idx, i);
	}
	
	printf("\nStack state : \n");
	printf("\tCount : %d\t\t\t\t=> 5\n", objSTK->Count);
	printf("\tMMU.Count : %d\t\t\t\t=> 1\n", objSTK->MMU.Count);
	printf("\tMMU.AllocCount : %d\t\t\t=> 64\n", objSTK->MMU.AllocCount);
	if ( objSTK->MMU.Memory ) {
		printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
	} else {
		printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
	}
	printf("\tstack top val : %d\n", xrtDynStackTopPtr(objSTK));
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\texpected val\n");
	intptr_t t1val[5] = { 0, 1, 2, 3, 4 };
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		intptr_t iPtr = (intptr_t)xrtDynStackGetPosPtr_Unsafe(objSTK, i);
		if ( iPtr == t1val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\tsucc! √\n", i, iPtr, t1val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\tfail! ×\n", i, iPtr, t1val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : pop
	printf("Stack test subject 3 : pop\n\n");
	intptr_t iPop = (intptr_t)xrtDynStackPopPtr(objSTK);
	if ( iPop == 4 ) {
		printf("pop value : %d\t\t\t\t\t\tpass! √\n", iPop);
	} else {
		printf("pop value : %d\t\t\t\t\t\tfail! ×\n", iPop);
	}
	iPop = (intptr_t)xrtDynStackPopPtr(objSTK);
	if ( iPop == 3 ) {
		printf("pop value : %d\t\t\t\t\t\tpass! √\n", iPop);
	} else {
		printf("pop value : %d\t\t\t\t\t\tfail! ×\n", iPop);
	}
	
	printf("\nStack state : \n");
	printf("\tCount : %d\t\t\t\t=> 3\n", objSTK->Count);
	printf("\tMMU.Count : %d\t\t\t\t=> 1\n", objSTK->MMU.Count);
	printf("\tMMU.AllocCount : %d\t\t\t=> 64\n", objSTK->MMU.AllocCount);
	if ( objSTK->MMU.Memory ) {
		printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
	} else {
		printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
	}
	printf("\tstack top val : %d\n", xrtDynStackTopPtr(objSTK));
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\texpected val\n");
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		intptr_t iPtr = (intptr_t)xrtDynStackGetPosPtr_Unsafe(objSTK, i);
		if ( iPtr == t1val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\tsucc! √\n", i, iPtr, t1val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\tfail! ×\n", i, iPtr, t1val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : push
	printf("Stack test subject 4 : push\n\n");
	for ( int i = 0; i < 5; i++ ) {
		int idx = xrtDynStackPushPtr(objSTK, (void*)(intptr_t)((i + 1) * 10));
		printf("push stack idx : %d (%d)\n", idx, (i + 1) * 10);
	}
	
	printf("\nStack state : \n");
	printf("\tCount : %d\t\t\t\t=> 8\n", objSTK->Count);
	printf("\tMMU.Count : %d\t\t\t\t=> 1\n", objSTK->MMU.Count);
	printf("\tMMU.AllocCount : %d\t\t\t=> 64\n", objSTK->MMU.AllocCount);
	if ( objSTK->MMU.Memory ) {
		printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
	} else {
		printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
	}
	printf("\tstack top val : %d\n", xrtDynStackTopPtr(objSTK));
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\texpected val\n");
	int t2val[8] = { 0, 1, 2, 10, 20, 30, 40, 50 };
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		intptr_t iPtr = (intptr_t)xrtDynStackGetPosPtr_Unsafe(objSTK, i);
		if ( iPtr == t2val[i - 1] ) {
			printf("\t%d\t%d\t%d\t\t\t\tsucc! √\n", i, iPtr, t2val[i - 1]);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\tfail! ×\n", i, iPtr, t2val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : overflow
	printf("Stack test subject 5 : overflow\n\n");
	iPop = (intptr_t)xrtDynStackPopPtr(objSTK);
	if ( iPop == 50 ) {
		printf("pop value : %d\t\t\t\t\t\tpass! √\n", iPop);
	} else {
		printf("pop value : %d\t\t\t\t\t\tfail! ×\n", iPop);
	}
	for ( int i = 0; i < 249; i++ ) {
		int idx = xrtDynStackPushPtr(objSTK, (void*)(intptr_t)((i + 1) * 100));
		printf("push stack idx : %d (%d)\n", idx, (i + 1) * 100);
	}
	// overflow item
	int idx = xrtDynStackPushPtr(objSTK, (char*)123456);
	if ( idx ) {
		printf("xrtDynStackPushPtr Succ (Should be succ)\t\t\tsucc! √\n");
	} else {
		printf("xrtDynStackPushPtr Fail (Should be succ)\t\t\tfail! ×\n");
	}
	
	printf("\nStack state : \n");
	printf("\tCount : %d\t\t\t\t=> 257\n", objSTK->Count);
	printf("\tMMU.Count : %d\t\t\t\t=> 2\n", objSTK->MMU.Count);
	printf("\tMMU.AllocCount : %d\t\t\t=> 64\n", objSTK->MMU.AllocCount);
	if ( objSTK->MMU.Memory ) {
		printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
	} else {
		printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
	}
	printf("\tstack top val : %d\n", xrtDynStackTopPtr(objSTK));
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\texpected val\n");
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		intptr_t iPtr = (intptr_t)xrtDynStackGetPosPtr_Unsafe(objSTK, i);
		int iExpected = i > 7 ? ((i - 7) * 100) : t2val[i - 1];
		if ( i > 256 ) { iExpected = 123456; }
		if ( iPtr == iExpected ) {
			printf("\t%d\t%d\t%d\t\t\t\tsucc! √\n", i, iPtr, iExpected);
		} else {
			printf("\t%d\t%d\t%d\t\t\t\tfail! ×\n", i, iPtr, iExpected);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject X : struct unit & destroy
	printf("Stack test subject X : struct unit & destroy\n\n");
	xrtDynStackUnit(objSTK);
	printf("Stack object (%p) already unit!\n\n", objSTK);
	
	printf("\nStack state : \n");
	printf("\tCount : %d\t\t\t\t=> 0\n", objSTK->Count);
	printf("\tMMU.Count : %d\t\t\t\t=> 0\n", objSTK->MMU.Count);
	printf("\tMMU.AllocCount : %d\t\t\t=> 0\n", objSTK->MMU.AllocCount);
	if ( objSTK->MMU.Memory ) {
		printf("\tMMU.Memory : %p\t\t\tfail! ×\n", objSTK->MMU.Memory);
	} else {
		printf("\tMMU.Memory : %p\t\t\tpass! √\n", objSTK->MMU.Memory);
	}
	
	xrtDynStackDestroy(objSTK);
	printf("\nStack object (%p) already destroyed!\n", objSTK);
	
	
	
}


