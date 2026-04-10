#ifndef XRT_TEST_XSON_H
#define XRT_TEST_XSON_H

// 内部函数：__Test_XSONFail
static int __Test_XSONFail(const char* sMsg)
{
	fprintf(stderr, "[xson] FAIL: %s\n", sMsg ? sMsg : "(no message)");
	return 1;
}

#define __TEST_XSON_REQUIRE(cond, msg)	do { if ( !(cond) ) { return __Test_XSONFail(msg); } } while (0)

// XSON测试
static int Test_XSON(void)
{
	xvalue pVal;
	xvalue pList;
	xvalue pSet;
	xvalue pTime;
	xvalue pClass;
	xvalue pObj;
	str sText;
	size_t iSize;
	uint8 arrBytes[4] = { 0x01, 0x02, 0x03, 0x04 };

	printf("[xson] start\n");

	pVal = xrtParseXSON("{\"name\":\"xrt\",\"nums\":[1,2,3]}", 0);
	__TEST_XSON_REQUIRE(pVal != NULL, "json compatibility parse returned null");
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_TABLE, "json compatibility root type mismatch");
	__TEST_XSON_REQUIRE(strcmp(xvoTableGetText(pVal, "name", 0), "xrt") == 0, "json compatibility text mismatch");
	__TEST_XSON_REQUIRE(xvoType(xvoTableGetValue(pVal, "nums", 0)) == XVO_DT_ARRAY, "json compatibility array type mismatch");
	__TEST_XSON_REQUIRE(xvoArrayItemCount(xvoTableGetValue(pVal, "nums", 0)) == 3, "json compatibility array count mismatch");
	xvoUnref(pVal);

	pList = xrtParseXSON("[1:\"a\",-5:time(2000-01-02 03:04:05)]", 0);
	__TEST_XSON_REQUIRE(xvoType(pList) == XVO_DT_LIST, "implicit list parse type mismatch");
	__TEST_XSON_REQUIRE(strcmp(xvoGetText(xvoListGetValue(pList, 1)), "a") == 0, "implicit list text mismatch");
	__TEST_XSON_REQUIRE(xvoType(xvoListGetValue(pList, -5)) == XVO_DT_TIME, "implicit list time type mismatch");
	__TEST_XSON_REQUIRE(strcmp(xvoGetText(xvoListGetValue(pList, -5)), "2000-01-02 03:04:05") == 0, "implicit list time value mismatch");
	xvoUnref(pList);

	pSet = xrtParseXSON("{1,2,3}", 0);
	__TEST_XSON_REQUIRE(xvoType(pSet) == XVO_DT_COLL, "implicit set parse type mismatch");
	__TEST_XSON_REQUIRE(xvoCollItemCount(pSet) == 3, "implicit set count mismatch");
	pVal = xvoCreateInt(2);
	__TEST_XSON_REQUIRE(xvoCollExists(pSet, pVal) == TRUE, "implicit set value mismatch");
	xvoUnref(pVal);
	xvoUnref(pSet);

	pTime = xrtParseXSON("time(2000-01-02 03:04:05)", 0);
	__TEST_XSON_REQUIRE(xvoType(pTime) == XVO_DT_TIME, "time parse type mismatch");
	__TEST_XSON_REQUIRE(strcmp(xvoGetText(pTime), "2000-01-02 03:04:05") == 0, "time parse value mismatch");
	xvoUnref(pTime);

	pClass = xrtParseXSON("class(AQIDBA==)", 0);
	__TEST_XSON_REQUIRE(xvoType(pClass) == XVO_DT_CLASS, "class parse type mismatch");
	__TEST_XSON_REQUIRE(xvoGetSize(pClass) == 4, "class parse size mismatch");
	__TEST_XSON_REQUIRE(memcmp(xvoGetClass(pClass), arrBytes, 4) == 0, "class parse bytes mismatch");
	xvoUnref(pClass);

	pVal = xrtParseXSON("dict{\"a\":1}", 0);
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_TABLE, "explicit dict parse type mismatch");
	__TEST_XSON_REQUIRE(xvoTableGetInt(pVal, "a", 0) == 1, "explicit dict parse value mismatch");
	xvoUnref(pVal);

	pVal = xrtParseXSON("array[1,2]", 0);
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_ARRAY, "explicit array parse type mismatch");
	__TEST_XSON_REQUIRE(xvoArrayItemCount(pVal) == 2, "explicit array parse count mismatch");
	xvoUnref(pVal);

	pVal = xvoCreateList();
	__TEST_XSON_REQUIRE(xvoListSetInt(pVal, -5, 10), "list build value -5 failed");
	__TEST_XSON_REQUIRE(xvoListSetText(pVal, 1, "a", 0, FALSE), "list build value 1 failed");
	sText = xrtStringifyXSON(pVal, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "list stringify returned null");
	__TEST_XSON_REQUIRE(strcmp(sText, "list[-5:10,1:\"a\"]") == 0, "list stringify mismatch");
	xrtFree(sText);
	xvoUnref(pVal);

	pVal = xvoCreateColl();
	__TEST_XSON_REQUIRE(xvoCollSetInt(pVal, 1), "set build value 1 failed");
	__TEST_XSON_REQUIRE(xvoCollSetInt(pVal, 2), "set build value 2 failed");
	__TEST_XSON_REQUIRE(xvoCollSetInt(pVal, 3), "set build value 3 failed");
	sText = xrtStringifyXSON(pVal, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "set stringify returned null");
	__TEST_XSON_REQUIRE(strcmp(sText, "set{1,2,3}") == 0, "set stringify mismatch");
	xrtFree(sText);
	xvoUnref(pVal);

	pVal = xvoCreateTimeSerial(2000, 1, 2, 3, 4, 5);
	sText = xrtStringifyXSON(pVal, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "time stringify returned null");
	__TEST_XSON_REQUIRE(strcmp(sText, "time(2000-01-02 03:04:05)") == 0, "time stringify mismatch");
	xrtFree(sText);
	xvoUnref(pVal);

	pVal = xvoCreateClass(4);
	__TEST_XSON_REQUIRE(pVal != NULL, "class build failed");
	memcpy(xvoGetClass(pVal), arrBytes, 4);
	sText = xrtStringifyXSON(pVal, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "class stringify returned null");
	__TEST_XSON_REQUIRE(strcmp(sText, "class(AQIDBA==)") == 0, "class stringify mismatch");
	xrtFree(sText);
	xvoUnref(pVal);

	pObj = xvoCreateTable();
	__TEST_XSON_REQUIRE(pObj != NULL, "nested object build failed");
	__TEST_XSON_REQUIRE(xvoTableSetText(pObj, "name", 0, "demo", 0, FALSE), "nested object text build failed");
	__TEST_XSON_REQUIRE(xvoTableSetTimeSerial(pObj, "created", 0, 2000, 1, 2, 3, 4, 5), "nested object time build failed");
	pList = xvoCreateList();
	__TEST_XSON_REQUIRE(pList != NULL, "nested object list build failed");
	__TEST_XSON_REQUIRE(xvoListSetInt(pList, 1, 95), "nested object list item build failed");
	__TEST_XSON_REQUIRE(xvoTableSetValue(pObj, "scores", 0, pList, FALSE), "nested object list attach failed");
	xvoUnref(pList);
	pSet = xvoCreateColl();
	__TEST_XSON_REQUIRE(pSet != NULL, "nested object set build failed");
	__TEST_XSON_REQUIRE(xvoCollSetText(pSet, "dev", 0, FALSE), "nested object set item build failed");
	__TEST_XSON_REQUIRE(xvoTableSetValue(pObj, "tags", 0, pSet, FALSE), "nested object set attach failed");
	xvoUnref(pSet);
	pClass = xvoCreateClass(4);
	__TEST_XSON_REQUIRE(pClass != NULL, "nested object class build failed");
	memcpy(xvoGetClass(pClass), arrBytes, 4);
	__TEST_XSON_REQUIRE(xvoTableSetValue(pObj, "blob", 0, pClass, FALSE), "nested object class attach failed");
	xvoUnref(pClass);
	sText = xrtStringifyXSON(pObj, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "nested object stringify returned null");
	pVal = xrtParseXSON(sText, 0);
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_TABLE, "nested object parse type mismatch");
	pList = xvoTableGetValue(pVal, "scores", 0);
	__TEST_XSON_REQUIRE(xvoType(pList) == XVO_DT_LIST, "nested object list type mismatch");
	__TEST_XSON_REQUIRE(xvoListGetInt(pList, 1) == 95, "nested object list value mismatch");
	pSet = xvoTableGetValue(pVal, "tags", 0);
	__TEST_XSON_REQUIRE(xvoType(pSet) == XVO_DT_COLL, "nested object set type mismatch");
	pClass = xvoTableGetValue(pVal, "blob", 0);
	__TEST_XSON_REQUIRE(xvoType(pClass) == XVO_DT_CLASS, "nested object class type mismatch");
	xvoUnref(pVal);
	xrtFree(sText);
	xvoUnref(pObj);

	pObj = xvoCreateTable();
	__TEST_XSON_REQUIRE(xvoTableSetInt(pObj, "keep", 0, 1), "ignore encode keep build failed");
	__TEST_XSON_REQUIRE(xvoTableSetPoint(pObj, "skip", 0, (ptr)0x1234), "ignore encode skip build failed");
	sText = xrtStringifyXSON(pObj, FALSE, 0, &iSize);
	__TEST_XSON_REQUIRE(sText == NULL, "unsupported encode should fail by default");
	sText = xrtStringifyXSON(pObj, FALSE, XSON_F_IGNORE_UNSUPPORTED_ENCODE, &iSize);
	__TEST_XSON_REQUIRE(sText != NULL, "ignore unsupported encode returned null");
	pVal = xrtParseXSON(sText, 0);
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_TABLE, "ignore unsupported encode parse type mismatch");
	__TEST_XSON_REQUIRE(xvoTableExists(pVal, "keep", 0) == TRUE, "ignore unsupported encode keep missing");
	__TEST_XSON_REQUIRE(xvoTableExists(pVal, "skip", 0) == FALSE, "ignore unsupported encode skip still exists");
	xvoUnref(pVal);
	xrtFree(sText);
	xvoUnref(pObj);

	pVal = xrtParseXSON("{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0);
	__TEST_XSON_REQUIRE(xvoIsNull(pVal) == TRUE, "unsupported decode should fail by default");
	xvoUnref(pVal);

	pVal = xrtParseXSONEx("{\"keep\":1,\"skip\":ptr(123),\"after\":2}", 0, XSON_F_IGNORE_UNSUPPORTED_DECODE);
	__TEST_XSON_REQUIRE(xvoType(pVal) == XVO_DT_TABLE, "ignore unsupported decode type mismatch");
	__TEST_XSON_REQUIRE(xvoTableGetInt(pVal, "keep", 0) == 1, "ignore unsupported decode keep mismatch");
	__TEST_XSON_REQUIRE(xvoTableGetInt(pVal, "after", 0) == 2, "ignore unsupported decode after mismatch");
	__TEST_XSON_REQUIRE(xvoTableExists(pVal, "skip", 0) == FALSE, "ignore unsupported decode skip still exists");
	xvoUnref(pVal);

	printf("[xson] pass\n");
	return 0;
}

#endif
