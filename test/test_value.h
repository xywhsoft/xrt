


typedef struct MyDataStruct {
	int64 iVal;
	double fVal;
	ptr pVal;
	uint8 sVal[32];
} MyDataStruct;



// Value 库测试 - 基础功能测试
void Test_Value_Basic(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Value 库测试 :\n\n");
	
	
	
	// subject 1 : print empty
	printf("Value test subject 1 : print empty\n\n");
	xvoPrintValue(NULL, 0, 0, 0, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : print null
	printf("Value test subject 2 : print null\n\n");
	xvalue pValNull = xvoCreateNull();
	xvoPrintValue(pValNull, 0, 0, 0, NULL);
	xvoUnref(pValNull);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : print bool
	printf("Value test subject 3 : print bool\n\n");
	xvalue pValBool = xvoCreateBool(TRUE);
	xvoPrintValue(pValBool, 0, 0, 0, NULL);
	xvoUnref(pValBool);
	pValBool = xvoCreateBool(FALSE);
	xvoPrintValue(pValBool, 0, 0, 0, NULL);
	xvoUnref(pValBool);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : print int
	printf("Value test subject 4 : print int\n\n");
	xvalue pValInt = xvoCreateInt(12345678);
	xvoPrintValue(pValInt, 0, 0, 0, NULL);
	xvoUnref(pValInt);
	pValInt = xvoCreateInt(87654321);
	xvoPrintValue(pValInt, 0, 0, 0, NULL);
	xvoUnref(pValInt);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : print float
	printf("Value test subject 5 : print float\n\n");
	xvalue pValFloat = xvoCreateFloat(3.1415926);
	xvoPrintValue(pValFloat, 0, 0, 0, NULL);
	xvoUnref(pValFloat);
	pValFloat = xvoCreateFloat(1234.5678);
	xvoPrintValue(pValFloat, 0, 0, 0, NULL);
	xvoUnref(pValFloat);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 6 : print text
	printf("Value test subject 6 : print text\n\n");
	xvalue pValText = xvoCreateText("Hello World~!", 0, FALSE);
	xvoPrintValue(pValText, 0, 0, 0, NULL);
	xvoUnref(pValText);
	pValText = xvoCreateText("壮志饥餐胡虏肉，笑谈渴饮匈奴血", 0, FALSE);
	xvoPrintValue(pValText, 0, 0, 0, NULL);
	xvoUnref(pValText);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 7 : print time
	printf("Value test subject 7 : print time\n\n");
	xvalue pValTime = xvoCreateTime(xrtNow());
	xvoPrintValue(pValTime, 0, 0, 0, NULL);
	xvoUnref(pValTime);
	pValTime = xvoCreateTimeSerial(2000, 10, 1, 12, 30, 30);
	xvoPrintValue(pValTime, 0, 0, 0, NULL);
	xvoUnref(pValTime);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 8 : print point
	printf("Value test subject 8 : print point\n\n");
	xvalue pValPtr = xvoCreatePoint((ptr)0x12345678);
	xvoPrintValue(pValPtr, 0, 0, 0, NULL);
	xvoUnref(pValPtr);
	pValPtr = xvoCreatePoint((ptr)0x87654321);
	xvoPrintValue(pValPtr, 0, 0, 0, NULL);
	xvoUnref(pValPtr);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 9 : print function
	printf("Value test subject 9 : print function\n\n");
	xvalue pValFunc = xvoCreateFunc((ptr)0x12345678);
	xvoPrintValue(pValFunc, 0, 0, 0, NULL);
	xvoUnref(pValFunc);
	pValFunc = xvoCreateFunc((ptr)0x87654321);
	xvoPrintValue(pValFunc, 0, 0, 0, NULL);
	xvoUnref(pValFunc);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 10 : print array
	printf("Value test subject 10 : print array\n\n");
	xvalue pValArray = xvoCreateArray();
	xvoArrayAppendNull(pValArray);
	xvoArrayAppendBool(pValArray, TRUE);
	xvoArrayAppendBool(pValArray, FALSE);
	xvoArrayAppendInt(pValArray, 753951);
	xvoArrayAppendFloat(pValArray, 3.1415926);
	xvoArrayAppendText(pValArray, "莫等闲，白了少年头，空悲切", 0, FALSE);
	xvoArrayAppendTime(pValArray, xrtNow());
	xvoArrayAppendTimeSerial(pValArray, 2000, 10, 1, 12, 30, 30);
	xvoArrayAppendPoint(pValArray, (ptr)0x12345678);
	xvoArrayAppendFunc(pValArray, (ptr)0x87654321);
	xvoPrintValue(pValArray, 0, 0, 0, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 11 : array insert
	printf("Value test subject 11 : array insert\n\n");
	xvoArrayInsertText(pValArray, 6, "插入到 6 位置的值", 0, FALSE);
	xvoArrayInsertText(pValArray, 4, "插入到 4 位置的值", 0, FALSE);
	xvoArrayInsertText(pValArray, 2, "插入到 2 位置的值", 0, FALSE);
	xvoPrintValue(pValArray, 0, 0, 0, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 12 : array set
	printf("Value test subject 12 : array set\n\n");
	xvoArraySetText(pValArray, 8, "修改后 8 位置的值", 0, FALSE);
	xvoArraySetText(pValArray, 5, "修改后 5 位置的值", 0, FALSE);
	xvoArraySetText(pValArray, 2, "修改后 2 位置的值", 0, FALSE);
	xvoPrintValue(pValArray, 0, 0, 0, NULL);
	xvoUnref(pValArray);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 13 : print list
	printf("Value test subject 13 : print list\n\n");
	xvalue pValList = xvoCreateList();
	xvoListSetNull(pValList, 1);
	xvoListSetBool(pValList, 10, TRUE);
	xvoListSetBool(pValList, 100, FALSE);
	xvoListSetInt(pValList, 1000, 753951);
	xvoListSetFloat(pValList, 10000, 3.1415926);
	xvoListSetText(pValList, 100000, "莫等闲，白了少年头，空悲切", 0, FALSE);
	xvoListSetTime(pValList, 1000000, xrtNow());
	xvoListSetTimeSerial(pValList, 10000000, 2000, 10, 1, 12, 30, 30);
	xvoListSetPoint(pValList, 500, (ptr)0x12345678);
	xvoListSetFunc(pValList, 50000, (ptr)0x87654321);
	xvoPrintValue(pValList, 0, 0, 0, NULL);
	xvoUnref(pValList);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 14 : print coll
	printf("Value test subject 14 : print coll\n\n");
	xvalue pValColl = xvoCreateColl();
	xvoCollSetNull(pValColl);
	xvoCollSetBool(pValColl, TRUE);
	xvoCollSetBool(pValColl, FALSE);
	xvoCollSetInt(pValColl, 753951);
	xvoCollSetFloat(pValColl, 3.1415926);
	xvoCollSetText(pValColl, "莫等闲，白了少年头，空悲切", 0, FALSE);
	xvoCollSetTime(pValColl, xrtNow());
	xvoCollSetTimeSerial(pValColl, 2000, 10, 1, 12, 30, 30);
	xvoCollSetPoint(pValColl, (ptr)0x12345678);
	xvoCollSetFunc(pValColl, (ptr)0x87654321);
	xvoPrintValue(pValColl, 0, 0, 0, NULL);
	xvoUnref(pValColl);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 15 : print table
	printf("Value test subject 15 : print table\n\n");
	xvalue pValTable = xvoCreateTable();
	xvoTableSetNull(pValTable, "key-null ", 9);
	xvoTableSetBool(pValTable, "key-true ", 9, TRUE);
	xvoTableSetBool(pValTable, "key-false", 9, FALSE);
	xvoTableSetInt(pValTable, "key-int  ", 9, 753951);
	xvoTableSetFloat(pValTable, "key-float", 9, 3.1415926);
	xvoTableSetText(pValTable, "key-text ", 9, "莫等闲，白了少年头，空悲切", 0, FALSE);
	xvoTableSetTime(pValTable, "key-time1", 9, xrtNow());
	xvoTableSetTimeSerial(pValTable, "key-time2", 9, 2000, 10, 1, 12, 30, 30);
	xvoTableSetPoint(pValTable, "key-point", 9, (ptr)0x12345678);
	xvoTableSetFunc(pValTable, "key-func ", 9, (ptr)0x87654321);
	xvoPrintValue(pValTable, 0, 0, 0, NULL);
	xvoUnref(pValTable);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 16 : print class
	printf("Value test subject 16 : print class\n\n");
	xvalue pValClass = xvoCreateClass(sizeof(MyDataStruct));
	MyDataStruct* pMDS = xvoGetClass(pValClass);
	pMDS->iVal = 12345678;
	pMDS->fVal = 3.1415926;
	pMDS->pVal = (ptr)0x87654321;
	sprintf(pMDS->sVal, "我吹过你吹过的晚风");
	xvoPrintValue(pValClass, 0, 0, 0, NULL);
	pMDS = xvoGetClass(pValClass);
	printf("\t iVal = %lld\n", pMDS->iVal);
	printf("\t fVal = %llf\n", pMDS->fVal);
	printf("\t pVal = %p\n", pMDS->pVal);
	printf("\t sVal = %s\n", pMDS->sVal);
	xvoUnref(pValClass);
	
}



// Value 库测试 - 操作功能测试 (覆盖修复的问题)
void Test_Value_Operations(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Value 操作测试 :\n\n");
	int passed = 0, failed = 0;
	
	// === 1. 测试空值参数检查 ===
	printf("[1. 测试空值参数检查]\n");
	
	// Array 空参数
	if ( !xvoArrayAppendValue(NULL, NULL, FALSE) ) {
		printf("  ✓ xvoArrayAppendValue(NULL, NULL) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoArrayAppendValue(NULL, NULL)\n"); failed++; }
	
	xvalue arr = xvoCreateArray();
	if ( !xvoArrayAppendValue(arr, NULL, FALSE) ) {
		printf("  ✓ xvoArrayAppendValue(arr, NULL) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoArrayAppendValue(arr, NULL)\n"); failed++; }
	
	if ( !xvoArrayAppendValue(NULL, arr, FALSE) ) {
		printf("  ✓ xvoArrayAppendValue(NULL, val) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoArrayAppendValue(NULL, val)\n"); failed++; }
	
	// List 空参数
	if ( !xvoListMerge(NULL, NULL, FALSE) ) {
		printf("  ✓ xvoListMerge(NULL, NULL) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoListMerge(NULL, NULL)\n"); failed++; }
	
	// Coll 空参数
	if ( !xvoCollMerge(NULL, NULL) ) {
		printf("  ✓ xvoCollMerge(NULL, NULL) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoCollMerge(NULL, NULL)\n"); failed++; }
	
	// Table 空参数
	if ( !xvoTableMerge(NULL, NULL, FALSE) ) {
		printf("  ✓ xvoTableMerge(NULL, NULL) 返回 FALSE\n");
		passed++;
	} else { printf("  × xvoTableMerge(NULL, NULL)\n"); failed++; }
	
	xvoUnref(arr);
	printf("\n");
	
	// === 2. 测试 Array Remove 内存释放 ===
	printf("[2. 测试 Array Remove]\n");
	xvalue arrRemove = xvoCreateArray();
	xvoArrayAppendInt(arrRemove, 100);
	xvoArrayAppendInt(arrRemove, 200);
	xvoArrayAppendInt(arrRemove, 300);
	xvoArrayAppendInt(arrRemove, 400);
	xvoArrayAppendInt(arrRemove, 500);
	
	if ( xvoArrayItemCount(arrRemove) == 5 ) {
		printf("  ✓ 初始数组元素数: 5\n");
		passed++;
	} else { printf("  × 初始数组元素数\n"); failed++; }
	
	xvoArrayRemove(arrRemove, 1, 2);  // 删除第2、3个元素 (200, 300)
	
	if ( xvoArrayItemCount(arrRemove) == 3 ) {
		printf("  ✓ 删除2个元素后: 3\n");
		passed++;
	} else { printf("  × 删除后元素数: %d\n", xvoArrayItemCount(arrRemove)); failed++; }
	
	xvalue item1 = xvoArrayGetValue(arrRemove, 0);
	xvalue item2 = xvoArrayGetValue(arrRemove, 1);
	xvalue item3 = xvoArrayGetValue(arrRemove, 2);
	
	if ( xvoGetInt(item1) == 100 && xvoGetInt(item2) == 400 && xvoGetInt(item3) == 500 ) {
		printf("  ✓ 删除后值正确: [100, 400, 500]\n");
		passed++;
	} else { printf("  × 删除后值错误\n"); failed++; }
	
	xvoUnref(arrRemove);
	printf("\n");
	
	// === 3. 测试 Array Copy ===
	printf("[3. 测试 Array Copy]\n");
	xvalue arrOriginal = xvoCreateArray();
	xvoArrayAppendInt(arrOriginal, 10);
	xvoArrayAppendInt(arrOriginal, 20);
	xvoArrayAppendText(arrOriginal, "test", 0, FALSE);
	
	xvalue arrCopy = xvoCopy(arrOriginal);
	
	if ( arrCopy != NULL && arrCopy->Type == XVO_DT_ARRAY ) {
		printf("  ✓ 数组拷贝成功\n");
		passed++;
	} else { printf("  × 数组拷贝失败\n"); failed++; }
	
	if ( xvoArrayItemCount(arrCopy) == 3 ) {
		printf("  ✓ 拷贝数组元素数正确: 3\n");
		passed++;
	} else { printf("  × 拷贝数组元素数\n"); failed++; }
	
	xvalue copyItem = xvoArrayGetValue(arrCopy, 0);
	if ( xvoGetInt(copyItem) == 10 ) {
		printf("  ✓ 拷贝数组值正确\n");
		passed++;
	} else { printf("  × 拷贝数组值错误\n"); failed++; }
	
	xvoUnref(arrOriginal);
	xvoUnref(arrCopy);
	printf("\n");
	
	// === 4. 测试 List Merge 类型检查 ===
	printf("[4. 测试 List Merge]\n");
	xvalue list1 = xvoCreateList();
	xvalue list2 = xvoCreateList();
	xvoListSetInt(list1, 1, 100);
	xvoListSetInt(list1, 2, 200);
	xvoListSetInt(list2, 3, 300);
	xvoListSetInt(list2, 4, 400);
	
	// 错误类型测试
	xvalue wrongType = xvoCreateArray();
	if ( !xvoListMerge(list1, wrongType, FALSE) ) {
		printf("  ✓ List Merge 拒绝错误类型 (Array)\n");
		passed++;
	} else { printf("  × List Merge 应拒绝错误类型\n"); failed++; }
	xvoUnref(wrongType);
	
	// 正确合并
	if ( xvoListMerge(list1, list2, FALSE) ) {
		printf("  ✓ List Merge 成功\n");
		passed++;
	} else { printf("  × List Merge 失败\n"); failed++; }
	
	xvalue merged3 = xvoListGetValue(list1, 3);
	xvalue merged4 = xvoListGetValue(list1, 4);
	if ( merged3 && merged4 && xvoGetInt(merged3) == 300 && xvoGetInt(merged4) == 400 ) {
		printf("  ✓ 合并结果正确\n");
		passed++;
	} else { printf("  × 合并结果错误\n"); failed++; }
	
	xvoUnref(list1);
	xvoUnref(list2);
	printf("\n");
	
	// === 5. 测试 Table Merge 类型检查 ===
	printf("[5. 测试 Table Merge]\n");
	xvalue tbl1 = xvoCreateTable();
	xvalue tbl2 = xvoCreateTable();
	xvoTableSetInt(tbl1, "a", 1, 100);
	xvoTableSetInt(tbl1, "b", 1, 200);
	xvoTableSetInt(tbl2, "c", 1, 300);
	xvoTableSetInt(tbl2, "d", 1, 400);
	
	// 错误类型测试
	xvalue wrongType2 = xvoCreateList();
	if ( !xvoTableMerge(tbl1, wrongType2, FALSE) ) {
		printf("  ✓ Table Merge 拒绝错误类型 (List)\n");
		passed++;
	} else { printf("  × Table Merge 应拒绝错误类型\n"); failed++; }
	xvoUnref(wrongType2);
	
	// 正确合并
	if ( xvoTableMerge(tbl1, tbl2, FALSE) ) {
		printf("  ✓ Table Merge 成功\n");
		passed++;
	} else { printf("  × Table Merge 失败\n"); failed++; }
	
	xvalue mergedC = xvoTableGetValue(tbl1, "c", 1);
	xvalue mergedD = xvoTableGetValue(tbl1, "d", 1);
	if ( mergedC && mergedD && xvoGetInt(mergedC) == 300 && xvoGetInt(mergedD) == 400 ) {
		printf("  ✓ 合并结果正确\n");
		passed++;
	} else { printf("  × 合并结果错误\n"); failed++; }
	
	xvoUnref(tbl1);
	xvoUnref(tbl2);
	printf("\n");
	
	// === 6. 测试 Coll 操作 ===
	printf("[6. 测试 Coll 操作]\n");
	xvalue coll1 = xvoCreateColl();
	xvalue coll2 = xvoCreateColl();
	xvoCollSetInt(coll1, 1);
	xvoCollSetInt(coll1, 2);
	xvoCollSetInt(coll1, 3);
	xvoCollSetInt(coll2, 2);
	xvoCollSetInt(coll2, 3);
	xvoCollSetInt(coll2, 4);
	
	// 交集测试
	xvalue intersection = xvoCollIntersection(coll1, coll2);
	if ( intersection && intersection->Type == XVO_DT_COLL ) {
		printf("  ✓ Coll Intersection 成功\n");
		passed++;
	} else { printf("  × Coll Intersection 失败\n"); failed++; }
	xvoUnref(intersection);
	
	// 并集测试
	xvalue unionSet = xvoCollUnion(coll1, coll2);
	if ( unionSet && unionSet->Type == XVO_DT_COLL ) {
		printf("  ✓ Coll Union 成功\n");
		passed++;
	} else { printf("  × Coll Union 失败\n"); failed++; }
	xvoUnref(unionSet);
	
	// 差集测试
	xvalue diff = xvoCollDifference(coll1, coll2);
	if ( diff && diff->Type == XVO_DT_COLL ) {
		printf("  ✓ Coll Difference 成功\n");
		passed++;
	} else { printf("  × Coll Difference 失败\n"); failed++; }
	xvoUnref(diff);
	
	// 对称差集测试
	xvalue symDiff = xvoCollSymmetricDifference(coll1, coll2);
	if ( symDiff && symDiff->Type == XVO_DT_COLL ) {
		printf("  ✓ Coll SymmetricDifference 成功\n");
		passed++;
	} else { printf("  × Coll SymmetricDifference 失败\n"); failed++; }
	xvoUnref(symDiff);
	
	xvoUnref(coll1);
	xvoUnref(coll2);
	printf("\n");
	
	// === 7. 测试 SetParent 类型检查 ===
	printf("[7. 测试 SetParent 类型检查]\n");
	
	// List SetParent
	xvalue listChild = xvoCreateList();
	xvalue listParent = xvoCreateList();
	xvalue notList = xvoCreateTable();
	
	if ( !xvoListSetParent(listChild, notList) ) {
		printf("  ✓ ListSetParent 拒绝错误类型 (Table)\n");
		passed++;
	} else { printf("  × ListSetParent 应拒绝错误类型\n"); failed++; }
	
	if ( xvoListSetParent(listChild, listParent) ) {
		printf("  ✓ ListSetParent 成功\n");
		passed++;
	} else { printf("  × ListSetParent 失败\n"); failed++; }
	
	xvoUnref(listChild);
	xvoUnref(listParent);
	xvoUnref(notList);
	
	// Coll SetParent
	xvalue collChild = xvoCreateColl();
	xvalue collParent = xvoCreateColl();
	xvalue notColl = xvoCreateArray();
	
	if ( !xvoCollSetParent(collChild, notColl) ) {
		printf("  ✓ CollSetParent 拒绝错误类型 (Array)\n");
		passed++;
	} else { printf("  × CollSetParent 应拒绝错误类型\n"); failed++; }
	
	if ( xvoCollSetParent(collChild, collParent) ) {
		printf("  ✓ CollSetParent 成功\n");
		passed++;
	} else { printf("  × CollSetParent 失败\n"); failed++; }
	
	xvoUnref(collChild);
	xvoUnref(collParent);
	xvoUnref(notColl);
	printf("\n");
	
	// === 8. 测试 Array Merge ===
	printf("[8. 测试 Array Merge]\n");
	xvalue arr1 = xvoCreateArray();
	xvalue arr2 = xvoCreateArray();
	xvoArrayAppendInt(arr1, 1);
	xvoArrayAppendInt(arr1, 2);
	xvoArrayAppendInt(arr2, 3);
	xvoArrayAppendInt(arr2, 4);
	
	if ( xvoArrayMerge(arr1, arr2) ) {
		printf("  ✓ Array Merge 成功\n");
		passed++;
	} else { printf("  × Array Merge 失败\n"); failed++; }
	
	if ( xvoArrayItemCount(arr1) == 4 ) {
		printf("  ✓ 合并后元素数: 4\n");
		passed++;
	} else { printf("  × 合并后元素数错误\n"); failed++; }
	
	xvoUnref(arr1);
	xvoUnref(arr2);
	printf("\n");
	
	// === 测试结果 ===
	printf("======================================\n");
	printf("测试结果: 通过 %d, 失败 %d\n", passed, failed);
	if ( failed == 0 ) {
		printf("✓ Value 操作测试全部通过!\n");
	} else {
		printf("× 有 %d 个测试失败!\n", failed);
	}
	printf("======================================\n");
}



// Value 库测试 - 全面功能测试
void Test_Value_Full(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n Value 全面测试 :\n\n");
	int passed = 0, failed = 0;
	
	// ========== 第一部分: 类型创建与转换 ==========
	printf("=== 第一部分: 类型创建与转换 ===\n\n");
	
	// [1] Null 类型
	printf("[1. Null 类型]\n");
	xvalue vNull = xvoCreateNull();
	if ( xvoType(vNull) == XVO_DT_NULL ) { printf("  ✓ xvoCreateNull\n"); passed++; }
	else { printf("  × xvoCreateNull\n"); failed++; }
	if ( xvoIsNull(vNull) ) { printf("  ✓ xvoIsNull\n"); passed++; }
	else { printf("  × xvoIsNull\n"); failed++; }
	if ( xvoIsNull(NULL) ) { printf("  ✓ xvoIsNull(NULL)\n"); passed++; }
	else { printf("  × xvoIsNull(NULL)\n"); failed++; }
	// Null 是静态值,不需要 Unref
	printf("\n");
	
	// [2] Bool 类型
	printf("[2. Bool 类型]\n");
	xvalue vTrue = xvoCreateBool(TRUE);
	xvalue vFalse = xvoCreateBool(FALSE);
	if ( xvoType(vTrue) == XVO_DT_BOOL && xvoGetBool(vTrue) == TRUE ) { printf("  ✓ xvoCreateBool(TRUE)\n"); passed++; }
	else { printf("  × xvoCreateBool(TRUE)\n"); failed++; }
	if ( xvoType(vFalse) == XVO_DT_BOOL && xvoGetBool(vFalse) == FALSE ) { printf("  ✓ xvoCreateBool(FALSE)\n"); passed++; }
	else { printf("  × xvoCreateBool(FALSE)\n"); failed++; }
	// Bool 也是静态值
	printf("\n");
	
	// [3] Int 类型
	printf("[3. Int 类型]\n");
	xvalue vInt = xvoCreateInt(12345678);
	if ( xvoType(vInt) == XVO_DT_INT ) { printf("  ✓ xvoCreateInt 类型正确\n"); passed++; }
	else { printf("  × xvoCreateInt 类型\n"); failed++; }
	if ( xvoGetInt(vInt) == 12345678 ) { printf("  ✓ xvoGetInt = 12345678\n"); passed++; }
	else { printf("  × xvoGetInt\n"); failed++; }
	if ( xvoGetFloat(vInt) == 12345678.0 ) { printf("  ✓ Int->Float 转换\n"); passed++; }
	else { printf("  × Int->Float 转换\n"); failed++; }
	if ( xvoGetBool(vInt) == TRUE ) { printf("  ✓ Int->Bool 转换\n"); passed++; }
	else { printf("  × Int->Bool 转换\n"); failed++; }
	xvoUnref(vInt);
	printf("\n");
	
	// [4] Float 类型
	printf("[4. Float 类型]\n");
	xvalue vFloat = xvoCreateFloat(3.14159);
	if ( xvoType(vFloat) == XVO_DT_FLOAT ) { printf("  ✓ xvoCreateFloat 类型正确\n"); passed++; }
	else { printf("  × xvoCreateFloat 类型\n"); failed++; }
	double f = xvoGetFloat(vFloat);
	if ( f > 3.14 && f < 3.15 ) { printf("  ✓ xvoGetFloat = 3.14159\n"); passed++; }
	else { printf("  × xvoGetFloat\n"); failed++; }
	if ( xvoGetInt(vFloat) == 3 ) { printf("  ✓ Float->Int 转换\n"); passed++; }
	else { printf("  × Float->Int 转换\n"); failed++; }
	xvoUnref(vFloat);
	printf("\n");
	
	// [5] Text 类型
	printf("[5. Text 类型]\n");
	xvalue vText = xvoCreateText("Hello World", 0, FALSE);
	if ( xvoType(vText) == XVO_DT_TEXT ) { printf("  ✓ xvoCreateText 类型正确\n"); passed++; }
	else { printf("  × xvoCreateText 类型\n"); failed++; }
	if ( strcmp(xvoGetText(vText), "Hello World") == 0 ) { printf("  ✓ xvoGetText 值正确\n"); passed++; }
	else { printf("  × xvoGetText\n"); failed++; }
	if ( xvoGetSize(vText) == 11 ) { printf("  ✓ xvoGetSize = 11\n"); passed++; }
	else { printf("  × xvoGetSize\n"); failed++; }
	// 测试数字字符串转换
	xvalue vTextNum = xvoCreateText("12345", 0, FALSE);
	if ( xvoGetInt(vTextNum) == 12345 ) { printf("  ✓ Text->Int 转换\n"); passed++; }
	else { printf("  × Text->Int 转换\n"); failed++; }
	xvoUnref(vText);
	xvoUnref(vTextNum);
	printf("\n");
	
	// [6] Time 类型
	printf("[6. Time 类型]\n");
	xvalue vTime = xvoCreateTimeSerial(2025, 1, 15, 10, 30, 45);
	if ( xvoType(vTime) == XVO_DT_TIME ) { printf("  ✓ xvoCreateTimeSerial 类型正确\n"); passed++; }
	else { printf("  × xvoCreateTimeSerial 类型\n"); failed++; }
	xtime t = xvoGetTime(vTime);
	if ( t > 0 ) { printf("  ✓ xvoGetTime 返回有效时间\n"); passed++; }
	else { printf("  × xvoGetTime\n"); failed++; }
	str sTime = xvoGetText(vTime);
	if ( strstr(sTime, "2025") != NULL ) { printf("  ✓ Time->Text 转换包含年份\n"); passed++; }
	else { printf("  × Time->Text 转换\n"); failed++; }
	xvoUnref(vTime);
	printf("\n");
	
	// [7] Point/Func 类型
	printf("[7. Point/Func 类型]\n");
	xvalue vPoint = xvoCreatePoint((ptr)0x12345678);
	if ( xvoType(vPoint) == XVO_DT_POINT ) { printf("  ✓ xvoCreatePoint 类型正确\n"); passed++; }
	else { printf("  × xvoCreatePoint 类型\n"); failed++; }
	if ( xvoGetPoint(vPoint) == (ptr)0x12345678 ) { printf("  ✓ xvoGetPoint 值正确\n"); passed++; }
	else { printf("  × xvoGetPoint\n"); failed++; }
	xvalue vFunc = xvoCreateFunc((xfunction)0x87654321);
	if ( xvoType(vFunc) == XVO_DT_FUNC ) { printf("  ✓ xvoCreateFunc 类型正确\n"); passed++; }
	else { printf("  × xvoCreateFunc 类型\n"); failed++; }
	xvoUnref(vPoint);
	xvoUnref(vFunc);
	printf("\n");
	
	// [8] Class/Custom 类型
	printf("[8. Class/Custom 类型]\n");
	xvalue vClass = xvoCreateClass(64);
	if ( xvoType(vClass) == XVO_DT_CLASS ) { printf("  ✓ xvoCreateClass 类型正确\n"); passed++; }
	else { printf("  × xvoCreateClass 类型\n"); failed++; }
	if ( xvoGetSize(vClass) == 64 ) { printf("  ✓ Class size = 64\n"); passed++; }
	else { printf("  × Class size\n"); failed++; }
	ptr pClass = xvoGetClass(vClass);
	if ( pClass != NULL ) { printf("  ✓ xvoGetClass 返回有效指针\n"); passed++; }
	else { printf("  × xvoGetClass\n"); failed++; }
	xvalue vCustom = xvoCreateCustom((ptr)0xABCDEF);
	if ( xvoType(vCustom) == XVO_DT_CUSTOM ) { printf("  ✓ xvoCreateCustom 类型正确\n"); passed++; }
	else { printf("  × xvoCreateCustom 类型\n"); failed++; }
	xvoUnref(vClass);
	xvoUnref(vCustom);
	printf("\n");
	
	// ========== 第二部分: Array 完整操作 ==========
	printf("=== 第二部分: Array 完整操作 ===\n\n");
	
	printf("[9. Array 基础操作]\n");
	xvalue arr = xvoCreateArray();
	if ( xvoType(arr) == XVO_DT_ARRAY ) { printf("  ✓ xvoCreateArray\n"); passed++; }
	else { printf("  × xvoCreateArray\n"); failed++; }
	
	// Append
	xvoArrayAppendInt(arr, 10);
	xvoArrayAppendInt(arr, 20);
	xvoArrayAppendInt(arr, 30);
	if ( xvoArrayItemCount(arr) == 3 ) { printf("  ✓ Append 3个元素\n"); passed++; }
	else { printf("  × Append\n"); failed++; }
	
	// Insert
	xvoArrayInsertInt(arr, 1, 15);  // 在位置1插入 -> [10, 15, 20, 30]
	if ( xvoArrayItemCount(arr) == 4 ) { printf("  ✓ Insert 后元素数=4\n"); passed++; }
	else { printf("  × Insert\n"); failed++; }
	xvalue insVal = xvoArrayGetValue(arr, 1);
	if ( xvoGetInt(insVal) == 15 ) { printf("  ✓ Insert 位置正确\n"); passed++; }
	else { printf("  × Insert 位置\n"); failed++; }
	
	// Set
	xvoArraySetInt(arr, 2, 25);  // [10, 15, 25, 30]
	xvalue setVal = xvoArrayGetValue(arr, 2);
	if ( xvoGetInt(setVal) == 25 ) { printf("  ✓ Set 修改成功\n"); passed++; }
	else { printf("  × Set\n"); failed++; }
	
	// Swap (xvoArraySwap 使用 0-based 索引)
	xvoArraySwap(arr, 0, 3);  // 交换第0和第3个元素: [10, 15, 25, 30] -> [30, 15, 25, 10]
	xvalue sw1 = xvoArrayGetValue(arr, 0);  // 0-based 获取
	xvalue sw4 = xvoArrayGetValue(arr, 3);
	if ( xvoGetInt(sw1) == 30 && xvoGetInt(sw4) == 10 ) { printf("  ✓ Swap 成功\n"); passed++; }
	else { printf("  × Swap: [0]=%lld, [3]=%lld\n", xvoGetInt(sw1), xvoGetInt(sw4)); failed++; }
	
	// Clear
	xvoArrayClear(arr);
	if ( xvoArrayItemCount(arr) == 0 ) { printf("  ✓ Clear 成功\n"); passed++; }
	else { printf("  × Clear\n"); failed++; }
	
	// Alloc
	if ( xvoArrayAlloc(arr, 100) ) { printf("  ✓ Alloc 成功\n"); passed++; }
	else { printf("  × Alloc\n"); failed++; }
	
	xvoUnref(arr);
	printf("\n");
	
	// [10] Array Sort
	printf("[10. Array Sort]\n");
	xvalue arrSort = xvoCreateArray();
	xvoArrayAppendInt(arrSort, 50);
	xvoArrayAppendInt(arrSort, 10);
	xvoArrayAppendInt(arrSort, 40);
	xvoArrayAppendInt(arrSort, 20);
	xvoArrayAppendInt(arrSort, 30);
	// 排序前第一个是50
	xvalue before = xvoArrayGetValue(arrSort, 0);
	if ( xvoGetInt(before) == 50 ) { printf("  ✓ 排序前第一个=50\n"); passed++; }
	else { printf("  × 排序前\n"); failed++; }

	// 执行排序（使用默认比较函数，传入 NULL）
	xvoArraySort(arrSort, NULL);

	// 排序后应该是 [10, 20, 30, 40, 50]
	xvalue after1 = xvoArrayGetValue(arrSort, 0);
	xvalue after2 = xvoArrayGetValue(arrSort, 1);
	xvalue after3 = xvoArrayGetValue(arrSort, 2);
	xvalue after4 = xvoArrayGetValue(arrSort, 3);
	xvalue after5 = xvoArrayGetValue(arrSort, 4);
	if ( xvoGetInt(after1) == 10 && xvoGetInt(after2) == 20 && xvoGetInt(after3) == 30 && xvoGetInt(after4) == 40 && xvoGetInt(after5) == 50 ) {
		printf("  ✓ 排序后数组正确: [10, 20, 30, 40, 50]\n");
		passed++;
	} else {
		printf("  × 排序后数组错误\n");
		failed++;
	}

	xvoUnref(arrSort);
	printf("\n");
	
	// ========== 第三部分: List 完整操作 ==========
	printf("=== 第三部分: List 完整操作 ===\n\n");
	
	printf("[11. List 基础操作]\n");
	xvalue lst = xvoCreateList();
	if ( xvoType(lst) == XVO_DT_LIST ) { printf("  ✓ xvoCreateList\n"); passed++; }
	else { printf("  × xvoCreateList\n"); failed++; }
	
	// Set
	xvoListSetInt(lst, 100, 1000);
	xvoListSetInt(lst, 200, 2000);
	xvoListSetInt(lst, -50, 500);
	if ( xvoListItemCount(lst) == 3 ) { printf("  ✓ Set 3个元素\n"); passed++; }
	else { printf("  × Set 元素数=%d\n", xvoListItemCount(lst)); failed++; }
	
	// Get
	xvalue lstVal = xvoListGetValue(lst, 100);
	if ( xvoGetInt(lstVal) == 1000 ) { printf("  ✓ Get key=100 -> 1000\n"); passed++; }
	else { printf("  × Get\n"); failed++; }
	
	// Exists
	if ( xvoListExists(lst, 200) ) { printf("  ✓ Exists key=200\n"); passed++; }
	else { printf("  × Exists\n"); failed++; }
	if ( !xvoListExists(lst, 999) ) { printf("  ✓ Not Exists key=999\n"); passed++; }
	else { printf("  × Not Exists\n"); failed++; }
	
	// Remove
	if ( xvoListRemove(lst, 200) ) { printf("  ✓ Remove key=200\n"); passed++; }
	else { printf("  × Remove\n"); failed++; }
	if ( xvoListItemCount(lst) == 2 ) { printf("  ✓ Remove 后元素数=2\n"); passed++; }
	else { printf("  × Remove 后元素数\n"); failed++; }
	
	// Clear
	xvoListClear(lst);
	if ( xvoListItemCount(lst) == 0 ) { printf("  ✓ Clear 成功\n"); passed++; }
	else { printf("  × Clear\n"); failed++; }
	
	xvoUnref(lst);
	printf("\n");
	
	// ========== 第四部分: Table 完整操作 ==========
	printf("=== 第四部分: Table 完整操作 ===\n\n");
	
	printf("[12. Table 基础操作]\n");
	xvalue tbl = xvoCreateTable();
	if ( xvoType(tbl) == XVO_DT_TABLE ) { printf("  ✓ xvoCreateTable\n"); passed++; }
	else { printf("  × xvoCreateTable\n"); failed++; }
	
	// Set
	xvoTableSetInt(tbl, "name", 0, 100);
	xvoTableSetText(tbl, "city", 0, "Beijing", 0, FALSE);
	xvoTableSetFloat(tbl, "score", 0, 95.5);
	if ( xvoTableItemCount(tbl) == 3 ) { printf("  ✓ Set 3个元素\n"); passed++; }
	else { printf("  × Set 元素数=%d\n", xvoTableItemCount(tbl)); failed++; }
	
	// Get
	xvalue tblVal = xvoTableGetValue(tbl, "city", 0);
	if ( strcmp(xvoGetText(tblVal), "Beijing") == 0 ) { printf("  ✓ Get key=city -> Beijing\n"); passed++; }
	else { printf("  × Get\n"); failed++; }
	
	// Exists
	if ( xvoTableExists(tbl, "name", 0) ) { printf("  ✓ Exists key=name\n"); passed++; }
	else { printf("  × Exists\n"); failed++; }
	if ( !xvoTableExists(tbl, "notexist", 0) ) { printf("  ✓ Not Exists key=notexist\n"); passed++; }
	else { printf("  × Not Exists\n"); failed++; }
	
	// Remove
	if ( xvoTableRemove(tbl, "name", 0) ) { printf("  ✓ Remove key=name\n"); passed++; }
	else { printf("  × Remove\n"); failed++; }
	if ( xvoTableItemCount(tbl) == 2 ) { printf("  ✓ Remove 后元素数=2\n"); passed++; }
	else { printf("  × Remove 后元素数\n"); failed++; }
	
	// SetParent
	xvalue tblParent = xvoCreateTable();
	if ( xvoTableSetParent(tbl, tblParent) ) { printf("  ✓ TableSetParent 成功\n"); passed++; }
	else { printf("  × TableSetParent\n"); failed++; }
	
	// Clear
	xvoTableClear(tbl);
	if ( xvoTableItemCount(tbl) == 0 ) { printf("  ✓ Clear 成功\n"); passed++; }
	else { printf("  × Clear\n"); failed++; }
	
	xvoUnref(tbl);
	xvoUnref(tblParent);
	printf("\n");
	
	// ========== 第五部分: Coll 完整操作 ==========
	printf("=== 第五部分: Coll 完整操作 ===\n\n");
	
	printf("[13. Coll 基础操作]\n");
	xvalue coll = xvoCreateColl();
	if ( xvoType(coll) == XVO_DT_COLL ) { printf("  ✓ xvoCreateColl\n"); passed++; }
	else { printf("  × xvoCreateColl\n"); failed++; }
	
	// Set
	xvoCollSetInt(coll, 10);
	xvoCollSetInt(coll, 20);
	xvoCollSetInt(coll, 30);
	xvoCollSetInt(coll, 10);  // 重复添加
	if ( xvoCollItemCount(coll) == 3 ) { printf("  ✓ Set 4次，去重后=3\n"); passed++; }
	else { printf("  × Set 元素数=%d\n", xvoCollItemCount(coll)); failed++; }
	
	// Exists
	xvalue valExists = xvoCreateInt(20);
	if ( xvoCollExists(coll, valExists) ) { printf("  ✓ Exists val=20\n"); passed++; }
	else { printf("  × Exists\n"); failed++; }
	xvoUnref(valExists);
	
	xvalue valNotExists = xvoCreateInt(999);
	if ( !xvoCollExists(coll, valNotExists) ) { printf("  ✓ Not Exists val=999\n"); passed++; }
	else { printf("  × Not Exists\n"); failed++; }
	xvoUnref(valNotExists);
	
	// Remove
	xvalue valRemove = xvoCreateInt(20);
	if ( xvoCollRemove(coll, valRemove) ) { printf("  ✓ Remove val=20\n"); passed++; }
	else { printf("  × Remove\n"); failed++; }
	xvoUnref(valRemove);
	if ( xvoCollItemCount(coll) == 2 ) { printf("  ✓ Remove 后元素数=2\n"); passed++; }
	else { printf("  × Remove 后元素数\n"); failed++; }
	
	// Clear
	xvoCollClear(coll);
	if ( xvoCollItemCount(coll) == 0 ) { printf("  ✓ Clear 成功\n"); passed++; }
	else { printf("  × Clear\n"); failed++; }
	
	xvoUnref(coll);
	printf("\n");
	
	// ========== 第六部分: 拷贝操作 ==========
	printf("=== 第六部分: 拷贝操作 ===\n\n");
	
	printf("[14. 浅拷贝测试]\n");
	// 基础类型拷贝
	xvalue intOrig = xvoCreateInt(999);
	xvalue intCopy = xvoCopy(intOrig);
	if ( xvoGetInt(intCopy) == 999 && intCopy != intOrig ) { printf("  ✓ Int 拷贝成功\n"); passed++; }
	else { printf("  × Int 拷贝\n"); failed++; }
	xvoUnref(intOrig);
	xvoUnref(intCopy);
	
	// Text 拷贝
	xvalue textOrig = xvoCreateText("Original Text", 0, FALSE);
	xvalue textCopy = xvoCopy(textOrig);
	if ( strcmp(xvoGetText(textCopy), "Original Text") == 0 && textCopy != textOrig ) { printf("  ✓ Text 拷贝成功\n"); passed++; }
	else { printf("  × Text 拷贝\n"); failed++; }
	xvoUnref(textOrig);
	xvoUnref(textCopy);
	
	// List 拷贝
	xvalue listOrig = xvoCreateList();
	xvoListSetInt(listOrig, 1, 100);
	xvoListSetInt(listOrig, 2, 200);
	xvalue listCopy = xvoCopy(listOrig);
	if ( xvoListItemCount(listCopy) == 2 ) { printf("  ✓ List 拷贝成功\n"); passed++; }
	else { printf("  × List 拷贝\n"); failed++; }
	xvoUnref(listOrig);
	xvoUnref(listCopy);
	
	// Table 拷贝
	xvalue tblOrig = xvoCreateTable();
	xvoTableSetInt(tblOrig, "a", 1, 100);
	xvoTableSetInt(tblOrig, "b", 1, 200);
	xvalue tblCopy = xvoCopy(tblOrig);
	if ( xvoTableItemCount(tblCopy) == 2 ) { printf("  ✓ Table 拷贝成功\n"); passed++; }
	else { printf("  × Table 拷贝\n"); failed++; }
	xvoUnref(tblOrig);
	xvoUnref(tblCopy);
	
	// Coll 拷贝
	xvalue collOrig = xvoCreateColl();
	xvoCollSetInt(collOrig, 1);
	xvoCollSetInt(collOrig, 2);
	xvalue collCopy = xvoCopy(collOrig);
	if ( xvoCollItemCount(collCopy) == 2 ) { printf("  ✓ Coll 拷贝成功\n"); passed++; }
	else { printf("  × Coll 拷贝\n"); failed++; }
	xvoUnref(collOrig);
	xvoUnref(collCopy);
	printf("\n");
	
	printf("[15. 深拷贝测试]\n");
	// 创建嵌套结构
	xvalue deepOrig = xvoCreateTable();
	xvalue nested = xvoCreateArray();
	xvoArrayAppendInt(nested, 111);
	xvoArrayAppendInt(nested, 222);
	xvoTableSetValue(deepOrig, "arr", 0, nested, FALSE);
	xvoTableSetInt(deepOrig, "num", 0, 999);
	
	xvalue deepCopy = xvoDeepCopy(deepOrig);
	if ( xvoTableItemCount(deepCopy) == 2 ) { printf("  ✓ 深拷贝 Table 成功\n"); passed++; }
	else { printf("  × 深拷贝 Table\n"); failed++; }
	
	// 检查嵌套数组是否被独立拷贝
	xvalue nestedCopy = xvoTableGetValue(deepCopy, "arr", 0);
	if ( nestedCopy != nested ) { printf("  ✓ 嵌套数组被独立拷贝\n"); passed++; }
	else { printf("  × 嵌套数组未独立拷贝\n"); failed++; }
	if ( xvoArrayItemCount(nestedCopy) == 2 ) { printf("  ✓ 嵌套数组内容正确\n"); passed++; }
	else { printf("  × 嵌套数组内容\n"); failed++; }
	
	xvoUnref(deepOrig);
	xvoUnref(deepCopy);
	printf("\n");
	
	// ========== 第七部分: Merge 操作(重写模式) ==========
	printf("=== 第七部分: Merge 重写模式 ===\n\n");
	
	printf("[16. List Merge 重写模式]\n");
	xvalue lstMerge1 = xvoCreateList();
	xvalue lstMerge2 = xvoCreateList();
	xvoListSetInt(lstMerge1, 1, 100);
	xvoListSetInt(lstMerge1, 2, 200);
	xvoListSetInt(lstMerge2, 2, 222);  // 重复 key
	xvoListSetInt(lstMerge2, 3, 300);
	
	xvoListMerge(lstMerge1, lstMerge2, TRUE);  // 重写模式
	xvalue lstMVal = xvoListGetValue(lstMerge1, 2);
	if ( xvoGetInt(lstMVal) == 222 ) { printf("  ✓ List Merge 重写 key=2 -> 222\n"); passed++; }
	else { printf("  × List Merge 重写\n"); failed++; }
	xvoUnref(lstMerge1);
	xvoUnref(lstMerge2);
	
	printf("[17. Table Merge 重写模式]\n");
	xvalue tblMerge1 = xvoCreateTable();
	xvalue tblMerge2 = xvoCreateTable();
	xvoTableSetInt(tblMerge1, "a", 0, 100);
	xvoTableSetInt(tblMerge1, "b", 0, 200);
	xvoTableSetInt(tblMerge2, "b", 0, 222);  // 重复 key
	xvoTableSetInt(tblMerge2, "c", 0, 300);
	
	xvoTableMerge(tblMerge1, tblMerge2, TRUE);  // 重写模式
	xvalue tblMVal = xvoTableGetValue(tblMerge1, "b", 0);
	if ( xvoGetInt(tblMVal) == 222 ) { printf("  ✓ Table Merge 重写 key=b -> 222\n"); passed++; }
	else { printf("  × Table Merge 重写\n"); failed++; }
	xvoUnref(tblMerge1);
	xvoUnref(tblMerge2);
	printf("\n");
	
	// ========== 测试结果 ==========
	fflush(stdout);
	printf("\n======================================\n");
	printf("[RESULT] Full Test: passed=%d, failed=%d\n", passed, failed);
	if ( failed == 0 ) {
		printf("[SUCCESS] Value Full Test ALL PASSED!\n");
	} else {
		printf("[FAILED] %d tests failed!\n", failed);
	}
	printf("======================================\n");
	fflush(stdout);
}


