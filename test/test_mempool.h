


// print Tree
int iHeigthMP = 1;
void mp256_treeprint(FSB_Item* pRoot)
{
	for ( int i = 0; i < iHeigthMP; i++ ) {
		printf("    ");
	}
	if ( pRoot ) {
		printf("Range : %d - %d\n", pRoot->MinLength, pRoot->MaxLength);
		iHeigthMP++;
		mp256_treeprint(pRoot->left);
		mp256_treeprint(pRoot->right);
		iHeigthMP--;
	} else {
		printf("null\n");
	}
}



// MemPool 库测试
void Test_MemPool(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n MemPool 库测试 :\n\n");
	
	
	
	// subject 1 : print memory pool small block plan tree
	printf("MemPool test subject 1 : print memory pool small block plan tree\n\n");
	xmempool objMP = xrtMemPoolCreate(1);
	
	printf("\nMemPool state : \n");
	printf("\tRootNode : %p\t\t=> not 0\n", objMP->FSB_RootNode);
	if ( objMP->FSB_Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objMP->FSB_Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objMP->FSB_Memory);
	}
	
	printf("\nTree print : \n");
	mp256_treeprint(objMP->FSB_RootNode);
	xrtMemPoolDestroy(objMP);
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
	
	
	
	// subject 2 : print memory pool normal block plan tree
	printf("MemPool test subject 2 : print memory pool normal block plan tree\n\n");
	objMP = xrtMemPoolCreate(2);
	
	printf("\nMemPool state : \n");
	printf("\tRootNode : %p\t\t=> not 0\n", objMP->FSB_RootNode);
	if ( objMP->FSB_Memory ) {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tpass! √\n", objMP->FSB_Memory);
	} else {
		printf("\tMM.arrMMU.PageMMU.Memory : %p\t\tfail! ×\n", objMP->FSB_Memory);
	}
	
	printf("\nTree print : \n");
	mp256_treeprint(objMP->FSB_RootNode);
	xrtMemPoolDestroy(objMP);
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
	
	
	
	// subject 3 : create object
	printf("MemPool test subject 3 : create object\n\n");
	objMP = xrtMemPoolCreate(2);
	if ( objMP ) {
		printf("MemPool object : %p\t\t\t\t\tpass! √\n", objMP);
		printf("\tarrMMU.Count : %d\t\t\t=> 0\n", objMP->arrMMU.Count);
		printf("\tBigMM.Count : %d\t\t\t\t=> 0\n", objMP->BigMM.Count);
	} else {
		printf("MemPool object : %p\t\t\t\t\tfail! ×\n", objMP);
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
	
	
	
	// subject 4 : alloc mempry
	printf("MemPool test subject 4 : alloc mempry\n\n");
	void* ptr[64];
	int arrLen[64] = { 1, 16, 17, 32, 33, 48, 49, 64, 65, 80, 81, 96, 97, 128, 129, 160, 161, 192, 193, 224, 225, 256, 257, 320, 321, 384, 385, 448, 449, 512, 513, 640, 641, 768, 769, 896, 897, 1024, 1025, 1280, 1281, 1536, 1537, 1792, 1793, 2048, 2049, 2304, 2305, 2560, 2561, 2816, 2817, 3072, 3073, 3328, 3329, 3584, 3585, 3840, 3841, 4096, 4097, 8192 };
	for ( int i = 0; i < 64; i++ ) {
		ptr[i] = xrtMemPoolAlloc(objMP, arrLen[i]);
		printf("malloc : %p (len : %d, idx : %d)\n", ptr[i], arrLen[i], i);
	}
	
	printf("\nMemPool state : \n");
	printf("\tarrMMU.Count : %d\t\t\t=> 31\n", objMP->arrMMU.Count);
	printf("\tBigMM.Count : %d\t\t\t\t=> 2\n", objMP->BigMM.Count);
	
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
	
	
	
	// subject 5 : free mempry
	printf("MemPool test subject 5 : free mempry\n\n");
	for ( int i = 0; i < 64; i++ ) {
		printf("free : %p (len : %d, idx : %d)\n", ptr[i], arrLen[i], i);
		xrtMemPoolFree(objMP, ptr[i]);
	}
	
	printf("\nMemPool state : \n");
	printf("\tarrMMU.Count : %d\t\t\t=> 31\n", objMP->arrMMU.Count);
	printf("\tBigMM.Count : %d\t\t\t\t=> 2\n", objMP->BigMM.Count);
	
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
	printf("MemPool test subject X : struct unit & destroy\n\n");
	xrtMemPoolUnit(objMP);
	printf("MemPool object (%p) already unit!\n\n", objMP);
	
	printf("\nMemPool state : \n");
	printf("\tarrMMU.Count : %d\t\t\t=> 31\n", objMP->arrMMU.Count);
	printf("\tBigMM.Count : %d\t\t\t\t=> 2\n", objMP->BigMM.Count);
	
	xrtMemPoolDestroy(objMP);
	printf("\nMemPool object (%p) already destroyed!\n", objMP);
	
	
	
}


