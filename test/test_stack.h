


// test SSSTK struct
typedef struct {
	int iVal;
	char sVal[32];
} SSSTK_Test_Struct, *SSSTK_Test_Object;



// Stack 库测试
void Test_Stack(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Stack 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("Stack test subject 1 : create object\n\n");
	xstack objSTK = xrtStackCreate(24, sizeof(SSSTK_Test_Struct));
	if ( objSTK ) {
		printf("Stack object : %p\t\t\t\tpass! √\n", objSTK);
		printf("\tItemLength : %d\t\t\t\t=> %d\n", objSTK->ItemLength, sizeof(SSSTK_Test_Struct));
		printf("\tCount : %d\t\t\t\t=> 0\n", objSTK->Count);
		printf("\tMaxCount : %d\t\t\t\t=> 24\n", objSTK->MaxCount);
		if ( objSTK->Memory ) {
			printf("\tMemory : %p\t\t\tpass! √\n", objSTK->Memory);
		} else {
			printf("\tMemory : %p\t\t\tfail! ×\n", objSTK->Memory);
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
		SSSTK_Test_Object pDat1 = xrtStackPush(objSTK);
		pDat1->iVal = i;
		sprintf(pDat1->sVal, "Push Item idx : %d", i);
		printf("push stack ptr : %p (%s)\n", pDat1, pDat1->sVal);
	}
	
	printf("\nStack state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objSTK->ItemLength, sizeof(SSSTK_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 5\n", objSTK->Count);
	printf("\tMaxCount : %d\t\t\t\t=> 24\n", objSTK->MaxCount);
	if ( objSTK->Memory ) {
		printf("\tMemory : %p\t\t\tpass! √\n", objSTK->Memory);
	} else {
		printf("\tMemory : %p\t\t\tfail! ×\n", objSTK->Memory);
	}
	SSSTK_Test_Object pTop = xrtStackTop(objSTK);
	printf("\tstack top iVal : %d\n", pTop->iVal);
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\t\t\tiVal\tsVal\t\t\texpected val\n");
	int t1val[5] = { 0, 1, 2, 3, 4 };
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		SSSTK_Test_Object pDat1 = xrtStackGetPos_Unsafe(objSTK, i);
		if ( pDat1->iVal == t1val[i - 1] ) {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tsucc! √\n", i, pDat1, pDat1->iVal, pDat1->sVal, t1val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tfail! ×\n", i, pDat1, pDat1->iVal, pDat1->sVal, t1val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : pop
	printf("Stack test subject 3 : pop\n\n");
	SSSTK_Test_Object pPop = xrtStackPop(objSTK);
	if ( pPop->iVal == 4 ) {
		printf("pop value : %d (%s)\t\t\tpass! √\n", pPop->iVal, pPop->sVal);
	} else {
		printf("pop value : %d (%s)\t\t\tfail! ×\n", pPop->iVal, pPop->sVal);
	}
	pPop = xrtStackPop(objSTK);
	if ( pPop->iVal == 3 ) {
		printf("pop value : %d (%s)\t\t\tpass! √\n", pPop->iVal, pPop->sVal);
	} else {
		printf("pop value : %d (%s)\t\t\tfail! ×\n", pPop->iVal, pPop->sVal);
	}
	
	printf("\nStack state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objSTK->ItemLength, sizeof(SSSTK_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 3\n", objSTK->Count);
	printf("\tMaxCount : %d\t\t\t\t=> 24\n", objSTK->MaxCount);
	if ( objSTK->Memory ) {
		printf("\tMemory : %p\t\t\tpass! √\n", objSTK->Memory);
	} else {
		printf("\tMemory : %p\t\t\tfail! ×\n", objSTK->Memory);
	}
	pTop = xrtStackTop(objSTK);
	printf("\tstack top iVal : %d\n", pTop->iVal);
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\t\t\tiVal\tsVal\t\t\texpected val\n");
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		SSSTK_Test_Object pDat1 = xrtStackGetPos_Unsafe(objSTK, i);
		if ( pDat1->iVal == t1val[i - 1] ) {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tsucc! √\n", i, pDat1, pDat1->iVal, pDat1->sVal, t1val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tfail! ×\n", i, pDat1, pDat1->iVal, pDat1->sVal, t1val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : push data
	printf("Stack test subject 4 : push data\n\n");
	for ( int i = 0; i < 5; i++ ) {
		SSSTK_Test_Struct stuDat;
		stuDat.iVal = (i + 1) * 10;
		sprintf(stuDat.sVal, "Push Data idx : %d", stuDat.iVal);
		int idx = xrtStackPushData(objSTK, &stuDat);
		printf("push stack idx : %d (%s)\n", idx, stuDat.sVal);
	}
	
	printf("\nStack state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objSTK->ItemLength, sizeof(SSSTK_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 8\n", objSTK->Count);
	printf("\tMaxCount : %d\t\t\t\t=> 24\n", objSTK->MaxCount);
	if ( objSTK->Memory ) {
		printf("\tMemory : %p\t\t\tpass! √\n", objSTK->Memory);
	} else {
		printf("\tMemory : %p\t\t\tfail! ×\n", objSTK->Memory);
	}
	pTop = xrtStackTop(objSTK);
	printf("\tstack top iVal : %d\n", pTop->iVal);
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\t\t\tiVal\tsVal\t\t\texpected val\n");
	int t2val[8] = { 0, 1, 2, 10, 20, 30, 40, 50 };
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		SSSTK_Test_Object pDat1 = xrtStackGetPos_Unsafe(objSTK, i);
		if ( pDat1->iVal == t2val[i - 1] ) {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tsucc! √\n", i, pDat1, pDat1->iVal, pDat1->sVal, t2val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tfail! ×\n", i, pDat1, pDat1->iVal, pDat1->sVal, t2val[i - 1]);
		}
	}
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : overflow
	printf("Stack test subject 5 : overflow\n\n");
	pPop = xrtStackPop(objSTK);
	if ( pPop->iVal == 50 ) {
		printf("pop value : %d (%s)\t\t\tpass! √\n", pPop->iVal, pPop->sVal);
	} else {
		printf("pop value : %d (%s)\t\t\tfail! ×\n", pPop->iVal, pPop->sVal);
	}
	for ( int i = 0; i < 17; i++ ) {
		SSSTK_Test_Object pDat1 = xrtStackPush(objSTK);
		pDat1->iVal = (i + 1) * 100;
		sprintf(pDat1->sVal, "New Item idx : %d", pDat1->iVal);
		printf("push stack ptr : %p (%s)\n", pDat1, pDat1->sVal);
	}
	// overflow item
	SSSTK_Test_Object pDat = xrtStackPush(objSTK);
	if ( pDat ) {
		printf("xrtStackPush Succ (Should be fail)\t\t\tfail! ×\n");
	} else {
		printf("xrtStackPush Fail (Should be fail)\t\t\tsucc! √\n");
	}
	
	printf("\nStack state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objSTK->ItemLength, sizeof(SSSTK_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 24\n", objSTK->Count);
	printf("\tMaxCount : %d\t\t\t\t=> 24\n", objSTK->MaxCount);
	if ( objSTK->Memory ) {
		printf("\tMemory : %p\t\t\tpass! √\n", objSTK->Memory);
	} else {
		printf("\tMemory : %p\t\t\tfail! ×\n", objSTK->Memory);
	}
	pTop = xrtStackTop(objSTK);
	printf("\tstack top iVal : %d\n", pTop->iVal);
	
	printf("\nStack values : \n");
	printf("\tidx\tptr\t\t\tiVal\tsVal\t\t\texpected val\n");
	int t3val[24] = { 0, 1, 2, 10, 20, 30, 40, 100, 200, 300, 400, 500, 600, 700, 800, 900, 1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700 };
	for ( int i = 1; i <= objSTK->Count; i++ ) {
		SSSTK_Test_Object pDat1 = xrtStackGetPos_Unsafe(objSTK, i);
		if ( pDat1->iVal == t3val[i - 1] ) {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tsucc! √\n", i, pDat1, pDat1->iVal, pDat1->sVal, t3val[i - 1]);
		} else {
			printf("\t%d\t%p\t%d\t%s\t%d\t\tfail! ×\n", i, pDat1, pDat1->iVal, pDat1->sVal, t3val[i - 1]);
		}
	}
	
	
	
	// not require testing
	xrtStackDestroy(objSTK);
	
	
	
}


