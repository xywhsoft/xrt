


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
	
	
	
	// subject 16 : print stauct
	printf("Value test subject 16 : print stauct\n\n");
	xvalue pValStruct = xvoCreateStruct(sizeof(MyDataStruct));
	MyDataStruct* pMDS = xvoGetStruct(pValStruct);
	pMDS->iVal = 12345678;
	pMDS->fVal = 3.1415926;
	pMDS->pVal = (ptr)0x87654321;
	sprintf(pMDS->sVal, "我吹过你吹过的晚风");
	xvoPrintValue(pValStruct, 0, 0, 0, NULL);
	pMDS = xvoGetStruct(pValStruct);
	printf("\t iVal = %lld\n", pMDS->iVal);
	printf("\t fVal = %llf\n", pMDS->fVal);
	printf("\t pVal = %x\n", pMDS->pVal);
	printf("\t sVal = %s\n", pMDS->sVal);
	xvoUnref(pValStruct);
	
}


