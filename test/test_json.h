


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
	
	
	
	// subject 2 : Parse JSON
	printf("Value test subject 2 : Parse JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "json2.txt");
	xvalue varJSON2 = xrtParseJSON_File(sPath);
	xvoPrintValue(varJSON2, 0, 0, 0, NULL);
	xvoUnref(varJSON2);
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
	
	
	
	// subject 3 : Parse JSON
	printf("Value test subject 3 : Parse JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "json3.txt");
	xvalue varJSON3 = xrtParseJSON_File(sPath);
	xvoPrintValue(varJSON3, 0, 0, 0, NULL);
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
	
	
	
	// subject 4 : Stringify JSON
	printf("Value test subject 4 : Stringify JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "out_json_format.txt");
	printf("[format] json save as : %s\n", sPath);
	xrtStringifyJSON_File(sPath, varJSON3, TRUE);
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
	
	
	
	// subject 5 : Stringify JSON
	printf("Value test subject 5 : Stringify JSON\n\n");
	sPath = xrtPathJoin(3, xCore->AppPath, "test", "out_json.txt");
	printf("[unformat] json save as : %s\n", sPath);
	xrtStringifyJSON_File(sPath, varJSON3, FALSE);
	xvoUnref(varJSON3);
	
	// subject 6 : Parse JSON from string
	printf("\nValue test subject 6 : Parse JSON from string\n\n");
	str sJSON1 = "{\"name\":\"Alice\",\"age\":30,\"active\":true}";
	printf("JSON string: %s\n", sJSON1);
	xvalue varJSON4 = xrtParseJSON(sJSON1, 0);
	xvoPrintValue(varJSON4, 0, 0, 0, NULL);
	xvoUnref(varJSON4);
	
	// subject 7 : Create JSON object
	printf("\nValue test subject 7 : Create JSON object\n\n");
	xvalue obj = xvoCreateTable();
	xvoTableSetValue(obj, "count", 0, xvoCreateInt(42), TRUE);
	xvoTableSetValue(obj, "bigNum", 0, xvoCreateInt(9007199254740991LL), TRUE);
	xvoTableSetValue(obj, "price", 0, xvoCreateFloat(19.99), TRUE);
	xvoTableSetValue(obj, "product", 0, xvoCreateText("XRT Library", 13, TRUE), TRUE);
	xvoTableSetValue(obj, "inStock", 0, xvoCreateBool(TRUE), TRUE);
	xvoTableSetValue(obj, "discount", 0, xvoCreateNull(), TRUE);
	printf("Created object:\n");
	xvoPrintValue(obj, 0, 0, 0, NULL);
	xvoUnref(obj);
	
	// subject 8 : Create JSON array
	printf("\nValue test subject 8 : Create JSON array\n\n");
	xvalue arr = xvoCreateArray();
	xvoArrayAppendValue(arr, xvoCreateInt(10), TRUE);
	xvoArrayAppendValue(arr, xvoCreateInt(20), TRUE);
	xvoArrayAppendValue(arr, xvoCreateInt(30), TRUE);
	xvoArrayAppendValue(arr, xvoCreateText("forty", 5, TRUE), TRUE);
	xvoArrayAppendValue(arr, xvoCreateFloat(50.5), TRUE);
	xvoArrayAppendValue(arr, xvoCreateBool(TRUE), TRUE);
	xvoArrayAppendValue(arr, xvoCreateNull(), TRUE);
	printf("Created array:\n");
	xvoPrintValue(arr, 0, 0, 0, NULL);
	xvoUnref(arr);
	
	// subject 9 : Nested JSON structures
	printf("\nValue test subject 9 : Nested JSON structures\n\n");
	xvalue nested = xvoCreateTable();
	xvoTableSetValue(nested, "name", 0, xvoCreateText("Root", 4, TRUE), TRUE);
	
	xvalue child1 = xvoCreateTable();
	xvoTableSetValue(child1, "type", 0, xvoCreateText("Folder", 6, TRUE), TRUE);
	xvoTableSetValue(child1, "size", 0, xvoCreateInt(1024), TRUE);
	xvoTableSetValue(nested, "folder1", 0, child1, TRUE);
	
	xvalue child2 = xvoCreateArray();
	xvoArrayAppendValue(child2, xvoCreateText("file1.txt", 8, TRUE), TRUE);
	xvoArrayAppendValue(child2, xvoCreateText("file2.txt", 8, TRUE), TRUE);
	xvoTableSetValue(nested, "files", 0, child2, TRUE);
	
	printf("Nested structure:\n");
	xvoPrintValue(nested, 0, 0, 0, NULL);
	xvoUnref(nested);
	
	// subject 10 : JSON stringification
	printf("\nValue test subject 10 : JSON stringification\n\n");
	xvalue testObj = xvoCreateTable();
	xvoTableSetValue(testObj, "string", 0, xvoCreateText("Hello World", 11, TRUE), TRUE);
	xvoTableSetValue(testObj, "number", 0, xvoCreateInt(123), TRUE);
	xvoTableSetValue(testObj, "boolean", 0, xvoCreateBool(FALSE), TRUE);
	
	str sFormatted = xrtStringifyJSON(testObj, TRUE, NULL);
	str sUnformatted = xrtStringifyJSON(testObj, FALSE, NULL);
	printf("Formatted JSON:\n%s\n", sFormatted);
	printf("Unformatted JSON:\n%s\n", sUnformatted);
	
	xrtFree(sFormatted);
	xrtFree(sUnformatted);
	xvoUnref(testObj);
	
	// subject 11 : JSON error handling
	printf("\nValue test subject 11 : JSON error handling\n\n");
	str sInvalidJSON = "{invalid json}";
	printf("Invalid JSON: %s\n", sInvalidJSON);
	xvalue invalid = xrtParseJSON(sInvalidJSON, 0);
	if ( xvoIsNull(invalid) ) {
		printf("✓ Correctly returned null for invalid JSON\n");
	} else {
		printf("✗ Should return null for invalid JSON\n");
	}
	xvoUnref(invalid);
	
	// subject 12 : JSON value type checking
	printf("\nValue test subject 12 : JSON value type checking\n\n");
	xvalue typeTest = xvoCreateTable();
	xvoTableSetValue(typeTest, "strKey", 0, xvoCreateText("text", 4, TRUE), TRUE);
	xvoTableSetValue(typeTest, "intKey", 0, xvoCreateInt(42), TRUE);
	xvoTableSetValue(typeTest, "boolKey", 0, xvoCreateBool(TRUE), TRUE);
	xvoTableSetValue(typeTest, "nullKey", 0, xvoCreateNull(), TRUE);
	
	xvalue strVal = xvoTableGetValue(typeTest, "strKey", 0);
	xvalue intVal = xvoTableGetValue(typeTest, "intKey", 0);
	xvalue boolVal = xvoTableGetValue(typeTest, "boolKey", 0);
	xvalue nullVal = xvoTableGetValue(typeTest, "nullKey", 0);
	
	printf("String value type: %d (isText=%d)\n", xvoType(strVal), xvoType(strVal) == XVO_DT_TEXT);
	printf("Int value type: %d (isInt=%d)\n", xvoType(intVal), xvoType(intVal) == XVO_DT_INT);
	printf("Bool value type: %d (isBool=%d)\n", xvoType(boolVal), xvoType(boolVal) == XVO_DT_BOOL);
	printf("Null value type: %d (isNull=%d)\n", xvoType(nullVal), xvoIsNull(nullVal));
	
	xvoUnref(typeTest);
	
	

}


