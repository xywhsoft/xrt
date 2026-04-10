


// test BSMM struct
typedef struct {
	int iVal;
} BSMM_Test_Struct, *BSMM_Test_Object;



// BSMM 库测试
void Test_BSMM(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n BSMM 库测试 :\n\n");
	
	
	
	// subject 1 : create object
	printf("BSMM test subject 1 : create object\n\n");
	xbsmm objBSMM = xrtBsmmCreate(sizeof(BSMM_Test_Struct), XRT_OBJMODE_LOCAL);
	if ( objBSMM ) {
		printf("BSMM object : %p\t\t\t\t\tpass! √\n", objBSMM);
		printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
		printf("\tCount : %d\t\t\t\t=> 0\n", objBSMM->Count);
		printf("\tPageMMU.Count : %d\t\t\t=> 0\n", objBSMM->PageMMU.Count);
		printf("\tPageMMU.AllocCount : %d\t\t\t=> 0\n", objBSMM->PageMMU.AllocCount);
		printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
		if ( objBSMM->PageMMU.Memory ) {
			printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
		} else {
			printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
		}
		
		printf("\nBSMM LList : \n");
		if ( objBSMM->LL_Free ) {
			printf("\tLL_Free : %p\t\t\t\tfail! ×\n", objBSMM->LL_Free);
		} else {
			printf("\tLL_Free : null\n");
		}
	} else {
		printf("BSMM object : %p\t\t\t\t\tfail! ×\n", objBSMM);
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
	
	
	
	// subject 2 : alloc struct memory
	printf("BSMM test subject 2 : alloc struct memory\n\n");
	BSMM_Test_Object ptr[10];
	for ( int i = 0; i < 10; i++ ) {
		ptr[i] = xrtBsmmAlloc(objBSMM);
		if ( ptr[i] ) {
			ptr[i]->iVal = i * 10;
			printf("xrtBsmmAlloc ptr (%p) = %d\t\t\tpass! √\n", ptr[i], ptr[i]->iVal);
		} else {
			printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", ptr[i], i);
		}
	}
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 10\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 1\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			printf("\t\t%p\t%p\t%p\n", pNode, pNode->Next, pNode->Ptr);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
	}
	
	printf("\nBSMM values : \n");
	printf("\tidx\tptr\t\t\tiVal\texpected val\n");
	int t1val[10] = { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90 };
	for ( int i = 0; i < 10; i++ ) {
		if ( ptr[i]->iVal == t1val[i] ) {
			printf("\t%d\t%p\t%d\t%d\t\tsucc! √\n", i, ptr[i], ptr[i]->iVal, t1val[i]);
		} else {
			printf("\t%d\t%p\t%d\t%d\t\tfail! ×\n", i, ptr[i], ptr[i]->iVal, t1val[i]);
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
	
	
	
	// subject 3 : free struct memory
	printf("BSMM test subject 3 : free struct memory\n\n");
	xrtBsmmFree(objBSMM, ptr[3]);
	xrtBsmmFree(objBSMM, ptr[5]);
	xrtBsmmFree(objBSMM, ptr[7]);
	xrtBsmmFree(objBSMM, ptr[9]);
	printf("free struct memory : %p\n", ptr[3]);
	printf("free struct memory : %p\n", ptr[5]);
	printf("free struct memory : %p\n", ptr[7]);
	printf("free struct memory : %p\n", ptr[9]);
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 10\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 1\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			BSMM_Test_Object pVal = pNode->Ptr;
			printf("\t\t%p\t%p\t%p\t%d\n", pNode, pNode->Next, pNode->Ptr, pVal->iVal);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
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
	
	
	
	// subject 3 : alloc struct memory
	printf("BSMM test subject 3 : alloc struct memory\n\n");
	BSMM_Test_Object pVal = xrtBsmmAlloc(objBSMM);
	if ( pVal ) {
		pVal->iVal = 1000;
		printf("xrtBsmmAlloc ptr (%p) = %d\t\t\tpass! √\n", pVal, pVal->iVal);
	} else {
		printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", pVal, 1);
	}
	pVal = xrtBsmmAlloc(objBSMM);
	if ( pVal ) {
		pVal->iVal = 2000;
		printf("xrtBsmmAlloc ptr (%p) = %d\t\t\tpass! √\n", pVal, pVal->iVal);
	} else {
		printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", pVal, 2);
	}
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 10\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 1\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			BSMM_Test_Object pVal = pNode->Ptr;
			printf("\t\t%p\t%p\t%p\t%d\n", pNode, pNode->Next, pNode->Ptr, pVal->iVal);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
	}
	
	printf("\nBSMM values : \n");
	printf("\tidx\tptr\t\t\tiVal\texpected val\n");
	int t2val[10] = { 0, 10, 20, 30, 40, 50, 60, 2000, 80, 1000 };
	for ( int i = 0; i < 10; i++ ) {
		if ( ptr[i]->iVal == t2val[i] ) {
			printf("\t%d\t%p\t%d\t%d\t\tsucc! √\n", i, ptr[i], ptr[i]->iVal, t2val[i]);
		} else {
			printf("\t%d\t%p\t%d\t%d\t\tfail! ×\n", i, ptr[i], ptr[i]->iVal, t2val[i]);
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
	
	
	
	// subject 4 : alloc struct memory x 512
	printf("BSMM test subject 4 : alloc struct memory x 512\n\n");
	for ( int i = 0; i < 512; i++ ) {
		pVal = xrtBsmmAlloc(objBSMM);
		if ( pVal == NULL ) {
			printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", pVal, i);
		}
	}
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 520\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 3\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			BSMM_Test_Object pVal = pNode->Ptr;
			printf("\t\t%p\t%p\t%p\t%d\n", pNode, pNode->Next, pNode->Ptr, pVal->iVal);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
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
	
	
	
	// subject 5 : free struct memory
	printf("BSMM test subject 5 : free struct memory\n\n");
	xrtBsmmFree(objBSMM, ptr[2]);
	xrtBsmmFree(objBSMM, ptr[4]);
	xrtBsmmFree(objBSMM, ptr[6]);
	xrtBsmmFree(objBSMM, ptr[8]);
	printf("free struct memory : %p\n", ptr[2]);
	printf("free struct memory : %p\n", ptr[4]);
	printf("free struct memory : %p\n", ptr[6]);
	printf("free struct memory : %p\n", ptr[8]);
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 520\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 3\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			BSMM_Test_Object pVal = pNode->Ptr;
			printf("\t\t%p\t%p\t%p\t%d\n", pNode, pNode->Next, pNode->Ptr, pVal->iVal);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
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
	
	
	
	// subject 6 : alloc struct memory
	printf("BSMM test subject 6 : alloc struct memory\n\n");
	pVal = xrtBsmmAlloc(objBSMM);
	if ( pVal ) {
		pVal->iVal = 3000;
		printf("xrtBsmmAlloc ptr (%p) = %d\t\t\tpass! √\n", pVal, pVal->iVal);
	} else {
		printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", pVal, 1);
	}
	pVal = xrtBsmmAlloc(objBSMM);
	if ( pVal ) {
		pVal->iVal = 4000;
		printf("xrtBsmmAlloc ptr (%p) = %d\t\t\tpass! √\n", pVal, pVal->iVal);
	} else {
		printf("xrtBsmmAlloc ptr (%p) pos = %d\t\t\tfail! ×\n", pVal, 2);
	}
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 520\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 3\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t=> 256\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free :\n");
		MemPtr_LLNode* pNode = objBSMM->LL_Free;
		while ( pNode ) {
			BSMM_Test_Object pVal = pNode->Ptr;
			printf("\t\t%p\t%p\t%p\t%d\n", pNode, pNode->Next, pNode->Ptr, pVal->iVal);
			pNode = pNode->Next;
		}
	} else {
		printf("\tLL_Free : null\n");
	}
	
	printf("\nBSMM values : \n");
	printf("\tidx\tptr\t\t\tiVal\texpected val\n");
	int t3val[10] = { 0, 10, 20, 30, 40, 50, 4000, 2000, 3000, 1000 };
	for ( int i = 0; i < 10; i++ ) {
		if ( ptr[i]->iVal == t3val[i] ) {
			printf("\t%d\t%p\t%d\t%d\t\tsucc! √\n", i, ptr[i], ptr[i]->iVal, t3val[i]);
		} else {
			printf("\t%d\t%p\t%d\t%d\t\tfail! ×\n", i, ptr[i], ptr[i]->iVal, t3val[i]);
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
	
	
	
	// subject X : struct unit & destroy
	printf("BSMM test subject X : struct unit & destroy\n\n");
	xrtBsmmUnit(objBSMM);
	printf("BSMM object (%p) already unit!\n\n", objBSMM);
	
	printf("\nBSMM state : \n");
	printf("\tItemLength : %d\t\t\t\t=> %d\n", objBSMM->ItemLength, sizeof(BSMM_Test_Struct));
	printf("\tCount : %d\t\t\t\t=> 0\n", objBSMM->Count);
	printf("\tPageMMU.Count : %d\t\t\t=> 0\n", objBSMM->PageMMU.Count);
	printf("\tPageMMU.AllocCount : %d\t\t\t=> 0\n", objBSMM->PageMMU.AllocCount);
	printf("\tPageMMU.AllocStep : %d\t\t\t=> 256\n", objBSMM->PageMMU.AllocStep);
	if ( objBSMM->PageMMU.Memory ) {
		printf("\tarrMMU.Memory : %p\t\t\tfail! ×\n", objBSMM->PageMMU.Memory);
	} else {
		printf("\tarrMMU.Memory : %p\t\t\tpass! √\n", objBSMM->PageMMU.Memory);
	}
	
	printf("\nBSMM LList : \n");
	if ( objBSMM->LL_Free ) {
		printf("\tLL_Free : %p\t\t\t\tfail! ×\n", objBSMM->LL_Free);
	} else {
		printf("\tLL_Free : null\n");
	}
	
	xrtBsmmDestroy(objBSMM);
	printf("\nBSMM object (%p) already destroyed!\n", objBSMM);
	
	
	
}


