



// List 迭代器测试
void Test_List_Iterator(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n List Iterator 测试 :\n\n");
	
	
	
	// subject 1 : 创建列表并插入数据
	printf("Subject 1 : 创建列表并插入数据\n");
	
	xlist objList = xrtListCreate(sizeof(Employee), XRT_OBJMODE_LOCAL);
	if ( objList ) {
		printf("\t列表创建成功，Count: %u\n", objList->AVLT.Count);
		
		// 插入测试数据
		const char* names[] = {"Alice", "Bob", "Charlie", "David", "Eve"};
		int ages[] = {25, 30, 35, 40, 45};
		double salaries[] = {50000.0, 60000.0, 70000.0, 80000.0, 90000.0};
		
		for ( int i = 0; i < 5; i++ ) {
			Employee* pEmp = (Employee*)xrtListSet(objList, (i * 10) + i, NULL);
			if ( pEmp ) {
				strncpy(pEmp->Name, names[i], 31);
				pEmp->Name[31] = '\0';
				pEmp->Age = ages[i];
				pEmp->Salary = salaries[i];
				printf("\t插入: [%ld] -> %s (age=%d, salary=%.2f)\n",
					i, pEmp->Name, pEmp->Age, pEmp->Salary);
			}
		}
		printf("\t列表总元素: %u\n\n", objList->AVLT.Count);
		
		
		
		// subject 2 : 使用原始方式迭代（index + val）
		printf("Subject 2 : 原始迭代方式（idx + val）\n");
		
		xrtAVLTB_IterBegin((xavltbase)objList);
		int count = 0;
		int64* pIdx;
		while ( (pIdx = (int64*)xrtAVLTB_IterNext((xavltbase)objList)) != NULL ) {
			Employee* pEmp = (Employee*)(&pIdx[1]);
			printf("\t[%d] Index: %ld\n", ++count, *pIdx);
			printf("\t    Value: %s, %d岁, %.2f\n", pEmp->Name, pEmp->Age, pEmp->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 3 : 使用改进后的宏迭代（带 idx 和 val）
		printf("Subject 3 : 使用宏迭代（idx + val）\n");
		
		count = 0;
		LIST_FOREACH(objList, idx, val) {
			Employee* pEmp = val;
			printf("\t[%d] Index: %ld\n", ++count, idx);
			printf("\t    Value: %s, %d岁, %.2f\n", pEmp->Name, pEmp->Age, pEmp->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 4 : 使用带类型的宏迭代
		printf("Subject 4 : 使用带类型的宏迭代\n");
		
		count = 0;
		LIST_FOREACH_TYPE(objList, idx, val, Employee*) {
			printf("\t[%d] Index: %ld\n", ++count, idx);
			printf("\t    Value: %s, %d岁, %.2f\n", val->Name, val->Age, val->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 5 : 测试空列表
		printf("Subject 5 : 空列表迭代测试\n");
		
		xlist emptyList = xrtListCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		if ( emptyList ) {
			count = 0;
			LIST_FOREACH(emptyList, idx, val) {
				count++;
				printf("\t不应该执行到这里！\n");
			}
			printf("\t空列表迭代次数: %d ✅\n\n", count);
			xrtListDestroy(emptyList);
		}
		
		
		
		// subject 6 : 测试提前退出
		printf("Subject 6 : 提前退出测试\n");
		
		count = 0;
		LIST_FOREACH(objList, idx, val) {
			count++;
			Employee* pEmp = (Employee*)val;
			printf("\t[%d] %ld\n", count, idx);
			if ( count >= 3 ) {
				printf("\t提前退出\n");
				LIST_BREAK(objList);
			}
		}
		printf("\t实际迭代: %d 个元素\n\n", count);
		
		
		
		// subject 7 : 测试大量数据
		printf("Subject 7 : 大量数据迭代测试\n");
		
		xlist largeList = xrtListCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		if ( largeList ) {
			// 插入 100 条数据
			for ( int i = 0; i < 100; i++ ) {
				int* pVal = (int*)xrtListSet(largeList, i, NULL);
				if ( pVal ) {
					*pVal = i * 10;
				}
			}
			
			// 迭代并验证
			count = 0;
			int sum = 0;
			LIST_FOREACH_TYPE(largeList, idx, pVal, int*) {
				count++;
				sum += *pVal;
			}
			printf("\t插入: 100 个元素\n");
			printf("\t迭代: %d 个元素\n", count);
			printf("\t总和: %d (应该等于 49500)\n\n", sum);
			
			xrtListDestroy(largeList);
		}
		
		
		
		// subject 8 : 测试修改值
		printf("Subject 8 : 迭代时修改值\n");
		
		LIST_FOREACH_TYPE(objList, idx, pEmp, Employee*) {
			pEmp->Salary *= 1.1;  // 涨薪 10%
		}
		
		count = 0;
		LIST_FOREACH_TYPE(objList, idx, pEmp, Employee*) {
			printf("\t[%d] %ld: %.2f\n", ++count, idx, pEmp->Salary);
		}
		printf("\t所有员工薪资已上调 10%%\n\n");
		
		
		
		// subject 9 : 测试指针类型值
		printf("Subject 9 : 指针类型值测试\n");
		
		xlist ptrList = xrtListCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
		if ( ptrList ) {
			// 插入一些指针值
			for ( int i = 0; i < 3; i++ ) {
				char* str = xrtMalloc(32);
				sprintf(str, "pointer_value_%d", i);
				
				ptr* pVal = (ptr*)xrtListSet(ptrList, i, NULL);
				if ( pVal ) {
					*pVal = str;
				}
			}
			
			// 迭代并释放
			count = 0;
			LIST_FOREACH_TYPE(ptrList, idx, pVal, ptr*) {
				printf("\t[%d] %ld -> %s\n", ++count, idx, (char*)*pVal);
				xrtFree(*pVal);  // 释放指针指向的内存
			}
			printf("\t共释放 %d 个指针\n\n", count);
			
			xrtListDestroy(ptrList);
		}
		
		
		
		// 清理
		printf("清理列表...\n");
		xrtListDestroy(objList);
	}
	
	
	
	printf("\nList Iterator 测试完成！\n");
}


