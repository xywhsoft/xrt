


// Value 库测试
void Test_Value(xrtGlobalData* xCore)
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
	xvalue pValText = xvoCreateText("Hello World~!", 0, XVO_SDT_STR_U8, FALSE);
	xvoPrintValue(pValText, 0, 0, 0, NULL);
	xvoUnref(pValText);
	pValText = xvoCreateText("壮志饥餐胡虏肉，笑谈渴饮匈奴血", 0, XVO_SDT_STR_U8, FALSE);
	xvoPrintValue(pValText, 0, 0, 0, NULL);
	xvoUnref(pValText);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
}


