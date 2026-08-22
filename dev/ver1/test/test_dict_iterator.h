


// 测试数据结构
typedef struct {
	char Name[32];
	int Age;
	double Salary;
} Employee;



// Dict 迭代器测试
void Test_Dict_Iterator(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Dict Iterator 测试 :\n\n");
	
	
	
	// subject 1 : 创建字典并插入数据
	printf("Subject 1 : 创建字典并插入数据\n");
	
	xdict objDict = xrtDictCreate(sizeof(Employee), XRT_OBJMODE_LOCAL);
	if ( objDict ) {
		printf("\t字典创建成功，Count: %u\n", objDict->AVLT.Count);
		
		// 插入测试数据
		const char* names[] = {"Alice", "Bob", "Charlie", "David", "Eve"};
		int ages[] = {25, 30, 35, 40, 45};
		double salaries[] = {50000.0, 60000.0, 70000.0, 80000.0, 90000.0};
		
		for ( int i = 0; i < 5; i++ ) {
			Dict_Key key;
			key.Key = (ptr)names[i];
			key.KeyLen = strlen(names[i]);
			key.Hash = 0;  // 自动计算
			
			bool bNew;
			Employee* pEmp = (Employee*)xrtDictSetWithKey(objDict, &key, &bNew);
			if ( pEmp ) {
				strncpy(pEmp->Name, names[i], 31);
				pEmp->Name[31] = '\0';
				pEmp->Age = ages[i];
				pEmp->Salary = salaries[i];
				printf("\t插入: %s -> %s (age=%d, salary=%.2f) %s\n",
					names[i], pEmp->Name, pEmp->Age, pEmp->Salary, bNew ? "[NEW]" : "[EXIST]");
			}
		}
		printf("\t字典总元素: %u\n\n", objDict->AVLT.Count);
		
		
		
		// subject 2 : 使用原始方式迭代（只有 key）
		printf("Subject 2 : 原始迭代方式（仅 key）\n");
		
		xrtAVLTB_IterBegin((xavltbase)objDict);
		int count = 0;
		Dict_Key* pKey;
		while ( (pKey = (Dict_Key*)xrtAVLTB_IterNext((xavltbase)objDict)) != NULL ) {
			Employee* pEmp = (Employee*)(&pKey[1]);
			printf("\t[%d] Key: %s (len=%u, hash=%u)\n",
				++count, (char*)pKey->Key, pKey->KeyLen, pKey->Hash);
			printf("\t    Value: %s, %d岁, %.2f\n", pEmp->Name, pEmp->Age, pEmp->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 3 : 使用改进后的宏迭代（带 key 和 val）
		printf("Subject 3 : 使用宏迭代（key + val）\n");
		
		count = 0;
		DICT_FOREACH(objDict, key, val) {
			Employee* pEmp = val;
			printf("\t[%d] Key: %s\n", ++count, (char*)key->Key);
			printf("\t    Value: %s, %d岁, %.2f\n", pEmp->Name, pEmp->Age, pEmp->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 4 : 使用带类型的宏迭代
		printf("Subject 4 : 使用带类型的宏迭代\n");
		
		count = 0;
		DICT_FOREACH_TYPE(objDict, key, val, Employee*) {
			printf("\t[%d] Key: %s\n", ++count, (char*)key->Key);
			printf("\t    Value: %s, %d岁, %.2f\n", val->Name, val->Age, val->Salary);
		}
		printf("\t共迭代: %d 个元素\n\n", count);
		
		
		
		// subject 5 : 测试空字典
		printf("Subject 5 : 空字典迭代测试\n");
		
		xdict emptyDict = xrtDictCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		if ( emptyDict ) {
			count = 0;
			DICT_FOREACH(emptyDict, key, val) {
				count++;
				printf("\t不应该执行到这里！\n");
			}
			printf("\t空字典迭代次数: %d ✅\n\n", count);
			xrtDictDestroy(emptyDict);
		}
		
		
		
		// subject 6 : 测试提前退出
		printf("Subject 6 : 提前退出测试\n");
		
		count = 0;
		DICT_FOREACH(objDict, key, val) {
			count++;
			Employee* pEmp = (Employee*)val;
			printf("\t[%d] %s\n", count, (char*)key->Key);
			if ( count >= 3 ) {
				printf("\t提前退出\n");
				DICT_BREAK(objDict);
			}
		}
		printf("\t实际迭代: %d 个元素\n\n", count);
		
		
		
		// subject 7 : 测试大量数据
		printf("Subject 7 : 大量数据迭代测试\n");
		
		xdict largeDict = xrtDictCreate(sizeof(int), XRT_OBJMODE_LOCAL);
		if ( largeDict ) {
			// 插入 100 条数据
			for ( int i = 0; i < 100; i++ ) {
				char keyStr[32];
				sprintf(keyStr, "key_%d", i);
				
				Dict_Key key;
				key.Key = (ptr)keyStr;
				key.KeyLen = strlen(keyStr);
				key.Hash = 0;
				
				bool bNew;
				int* pVal = (int*)xrtDictSetWithKey(largeDict, &key, &bNew);
				if ( pVal ) {
					*pVal = i * 10;
				}
			}
			
			// 迭代并验证
			count = 0;
			int sum = 0;
			DICT_FOREACH_TYPE(largeDict, key, pVal, int*) {
				count++;
				sum += *pVal;
			}
			printf("\t插入: 100 个元素\n");
			printf("\t迭代: %d 个元素\n", count);
			printf("\t总和: %d (应该等于 49500)\n\n", sum);
			
			xrtDictDestroy(largeDict);
		}
		
		
		
		// subject 8 : 测试修改值
		printf("Subject 8 : 迭代时修改值\n");
		
		DICT_FOREACH_TYPE(objDict, key, pEmp, Employee*) {
			pEmp->Salary *= 1.1;  // 涨薪 10%
		}
		
		count = 0;
		DICT_FOREACH_TYPE(objDict, key, pEmp, Employee*) {
			printf("\t[%d] %s: %.2f\n", ++count, (char*)key->Key, pEmp->Salary);
		}
		printf("\t所有员工薪资已上调 10%%\n\n");
		
		
		
		// subject 9 : 测试指针类型值
		printf("Subject 9 : 指针类型值测试\n");
		
		xdict ptrDict = xrtDictCreate(sizeof(ptr), XRT_OBJMODE_LOCAL);
		if ( ptrDict ) {
			// 插入一些指针值
			for ( int i = 0; i < 3; i++ ) {
				char* str = xrtMalloc(32);
				sprintf(str, "pointer_value_%d", i);
				
				Dict_Key key;
				key.Key = (ptr)str;
				key.KeyLen = strlen(str);
				key.Hash = 0;
				
				bool bNew;
				ptr* pVal = (ptr*)xrtDictSetWithKey(ptrDict, &key, &bNew);
				if ( pVal ) {
					*pVal = str;
				}
			}
			
			// 迭代并释放
			count = 0;
			DICT_FOREACH_TYPE(ptrDict, key, pVal, ptr*) {
				printf("\t[%d] %s -> %s\n", ++count, (char*)key->Key, (char*)*pVal);
				xrtFree(*pVal);  // 释放指针指向的内存
			}
			printf("\t共释放 %d 个指针\n\n", count);
			
			xrtDictDestroy(ptrDict);
		}
		
		
		
		// 清理
		printf("清理字典...\n");
		xrtDictDestroy(objDict);
	}
	
	
	
	printf("\nDict Iterator 测试完成！\n");
}
