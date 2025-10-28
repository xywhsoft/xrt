


// JSON 库测试
void Test_JSON(xrtGlobalData* xCore)
{
	printf("\n\n\n------------------------------------\n\n JSON 库测试 :\n\n");
	
	
	
	// subject 1 : Parse JSON
	printf("Value test subject 1 : Parse JSON\n\n");
	str sPath = xrtPathJoin(3, xCore->AppPath, "test", "json.txt");
	xvalue varJSON = xrtParseJSON_File(sPath);
	xvoPrintValue(varJSON, 0, 0, 0, NULL);
	xvoUnref(varJSON);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 2 : Parse JSON
	printf("Value test subject 2 : Parse JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "json2.txt");
	xvalue varJSON2 = xrtParseJSON_File(sPath);
	xvoPrintValue(varJSON2, 0, 0, 0, NULL);
	xvoUnref(varJSON2);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 3 : Parse JSON
	printf("Value test subject 3 : Parse JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "json3.txt");
	xvalue varJSON3 = xrtParseJSON_File(sPath);
	xvoPrintValue(varJSON3, 0, 0, 0, NULL);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 4 : Stringify JSON
	printf("Value test subject 4 : Stringify JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "out_json_format.txt");
	printf("[format] json save as : %s\n", sPath);
	xrtStringifyJSON_File(sPath, varJSON3, TRUE);
	printf("\n\n\n");
	system("pause");
	system("cls");
	
	
	
	// subject 5 : Stringify JSON
	printf("Value test subject 5 : Stringify JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "out_json.txt");
	printf("[unformat] json save as : %s\n", sPath);
	xrtStringifyJSON_File(sPath, varJSON3, FALSE);
	xvoUnref(varJSON3);
	
	
	
}


