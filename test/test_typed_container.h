
static int Test_TypedContainer(xrtGlobalData* xCore)
{
	int iPassed = 0;
	int iFailed = 0;

	(void)xCore;
	printf("[typed_container] begin\n");

	#define XRT_TC_CHECK(expr, name) \
		do { \
			if ( expr ) { \
				iPassed++; \
			} else { \
				iFailed++; \
				printf("[typed_container] FAIL: %s\n", name); \
			} \
		} while ( 0 )

	// ------------------------------------ typed array ------------------------------------
	{
		int64 v1 = 1;
		int64 v2 = 2;
		int64 v3 = 3;
		int64 v9 = 9;
		int64 out = 0;
		xvalue pVal;
		xtarray a = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtarray b;
		xtarray c;

		XRT_TC_CHECK(a != NULL, "array create");
		XRT_TC_CHECK(xrtTypedArrayAppend(a, &v1) != NULL, "array append 1");
		XRT_TC_CHECK(xrtTypedArrayAppend(a, &v3) != NULL, "array append 3");
		XRT_TC_CHECK(xrtTypedArrayInsert(a, 2, &v2) != NULL, "array insert 2");
		XRT_TC_CHECK(xrtTypedArrayCount(a) == 3, "array count");
		XRT_TC_CHECK(*(int64*)xrtTypedArrayGet(a, 2) == 2, "array get inserted");
		XRT_TC_CHECK(xrtTypedArrayContains(a, &v3), "array contains");
		XRT_TC_CHECK(xrtTypedArrayIndexOf(a, &v2) == 1, "array indexOf");
		XRT_TC_CHECK(xrtTypedArrayTake(a, 2, &out) && out == 2, "array take");
		XRT_TC_CHECK(xrtTypedArrayCount(a) == 2, "array count after take");
		XRT_TC_CHECK(xrtTypedArrayAppend(a, &v9) != NULL, "array append 9");
		pVal = xrtTypedArrayPopValue(a);
		XRT_TC_CHECK(xvoGetInt(pVal) == 9, "array pop value");
		xvoUnref(pVal);
		b = xrtTypedArrayClone(a);
		XRT_TC_CHECK(b != NULL && xrtTypedArrayEquals(a, b), "array clone equals");
		XRT_TC_CHECK(xrtTypedArrayReverse(b), "array reverse");
		XRT_TC_CHECK(*(int64*)xrtTypedArrayGet(b, 1) == 3, "array reverse result");
		c = xrtTypedArrayConcat(a, b);
		XRT_TC_CHECK(c != NULL && xrtTypedArrayCount(c) == 4, "array concat");
		XRT_TC_CHECK(xrtTypedArrayClear(c) && xrtTypedArrayCount(c) == 0, "array clear");
		xrtTypedArrayDestroy(a);
		xrtTypedArrayDestroy(b);
		xrtTypedArrayDestroy(c);
	}

	// ------------------------------------ typed list ------------------------------------
	{
		int64 v10 = 10;
		int64 v20 = 20;
		int64 v99 = 99;
		xvalue pVal;
		xtlist a = xrtTypedListCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtlist b = xrtTypedListCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtlist c;

		XRT_TC_CHECK(xrtTypedListSet(a, 1, &v10) != NULL, "list set 1");
		XRT_TC_CHECK(xrtTypedListSet(a, 5, &v20) != NULL, "list set 5");
		XRT_TC_CHECK(xrtTypedListContains(a, &v10), "list contains value");
		c = xrtTypedListClone(a);
		XRT_TC_CHECK(c != NULL && xrtTypedListEquals(a, c), "list clone equals");
		XRT_TC_CHECK(xrtTypedListSet(b, 5, &v99) != NULL, "list set overwrite source");
		XRT_TC_CHECK(xrtTypedListMerge(a, b), "list merge");
		XRT_TC_CHECK(*(int64*)xrtTypedListGet(a, 5) == 99, "list merge right wins");
		pVal = xrtTypedListTakeValue(a, 1);
		XRT_TC_CHECK(xvoGetInt(pVal) == 10 && !xrtTypedListContains(a, &v10), "list take value");
		xvoUnref(pVal);
		XRT_TC_CHECK(xrtTypedListClear(a) && xrtTypedListCount(a) == 0, "list clear");
		xrtTypedListDestroy(a);
		xrtTypedListDestroy(b);
		xrtTypedListDestroy(c);
	}

	// ------------------------------------ typed dict ------------------------------------
	{
		int64 v1 = 1;
		int64 v2 = 2;
		int64 v3 = 3;
		int64 v4 = 4;
		xvalue pVal;
		xtdict a = xrtTypedDictCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtdict b = xrtTypedDictCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtdict c;

		XRT_TC_CHECK(xrtTypedDictSet(a, "a", 1, &v1) != NULL, "dict set a");
		XRT_TC_CHECK(xrtTypedDictSet(a, "b", 1, &v2) != NULL, "dict set b");
		c = xrtTypedDictClone(a);
		XRT_TC_CHECK(c != NULL && xrtTypedDictEquals(a, c), "dict clone equals");
		XRT_TC_CHECK(xrtTypedDictSet(b, "b", 1, &v3) != NULL, "dict set overwrite source");
		XRT_TC_CHECK(xrtTypedDictSet(b, "c", 1, &v4) != NULL, "dict set c");
		XRT_TC_CHECK(xrtTypedDictMerge(a, b), "dict merge");
		XRT_TC_CHECK(*(int64*)xrtTypedDictGet(a, "b", 1) == 3, "dict merge right wins");
		pVal = xrtTypedDictTakeValue(a, "a", 1);
		XRT_TC_CHECK(xvoGetInt(pVal) == 1 && !xrtTypedDictExists(a, "a", 1), "dict take value");
		xvoUnref(pVal);
		XRT_TC_CHECK(xrtTypedDictClear(a) && xrtTypedDictCount(a) == 0, "dict clear");
		xrtTypedDictDestroy(a);
		xrtTypedDictDestroy(b);
		xrtTypedDictDestroy(c);
	}

	// ------------------------------------ typed set ------------------------------------
	{
		int64 v1 = 1;
		int64 v2 = 2;
		int64 v3 = 3;
		int64 v4 = 4;
		xtset a = xrtTypedSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtset b = xrtTypedSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtset u;
		xtset i;
		xtset d;
		xtset s;

		XRT_TC_CHECK(xrtTypedSetAdd(a, &v1), "set add 1");
		XRT_TC_CHECK(xrtTypedSetAdd(a, &v2), "set add 2");
		XRT_TC_CHECK(xrtTypedSetAdd(a, &v3), "set add 3");
		XRT_TC_CHECK(xrtTypedSetAdd(b, &v3), "set b add 3");
		XRT_TC_CHECK(xrtTypedSetAdd(b, &v4), "set b add 4");
		XRT_TC_CHECK(xrtTypedSetExists(a, &v2), "set exists");
		u = xrtTypedSetUnion(a, b);
		i = xrtTypedSetIntersection(a, b);
		d = xrtTypedSetDifference(a, b);
		s = xrtTypedSetSymmetricDifference(a, b);
		XRT_TC_CHECK(u != NULL && xrtTypedSetCount(u) == 4, "set union");
		XRT_TC_CHECK(i != NULL && xrtTypedSetCount(i) == 1 && xrtTypedSetExists(i, &v3), "set intersection");
		XRT_TC_CHECK(d != NULL && xrtTypedSetCount(d) == 2 && !xrtTypedSetExists(d, &v3), "set difference");
		XRT_TC_CHECK(s != NULL && xrtTypedSetCount(s) == 3 && !xrtTypedSetExists(s, &v3), "set symmetric difference");
		XRT_TC_CHECK(xrtTypedSetIsSubset(i, a, FALSE), "set subset");
		XRT_TC_CHECK(xrtTypedSetIsSuperset(a, i, TRUE), "set proper superset");
		XRT_TC_CHECK(xrtTypedSetRemove(a, &v2) && !xrtTypedSetExists(a, &v2), "set remove");
		XRT_TC_CHECK(xrtTypedSetClear(a) && xrtTypedSetCount(a) == 0, "set clear");
		xrtTypedSetDestroy(a);
		xrtTypedSetDestroy(b);
		xrtTypedSetDestroy(u);
		xrtTypedSetDestroy(i);
		xrtTypedSetDestroy(d);
		xrtTypedSetDestroy(s);
	}

	// ------------------------------------ dynamic xvalue set ------------------------------------
	{
		xvalue a = xvoCreateSet();
		xvalue b = xvoCreateSet();
		xvalue u;
		xvalue i;
		xvalue d;
		xvalue s;
		xvoCollSetInt(a, 1);
		xvoCollSetInt(a, 2);
		xvoCollSetInt(a, 3);
		xvoCollSetInt(b, 3);
		xvoCollSetInt(b, 4);
		u = xvoCollUnion(a, b);
		i = xvoCollIntersection(a, b);
		d = xvoCollDifference(a, b);
		s = xvoCollSymmetricDifference(a, b);
		XRT_TC_CHECK(xvoIsColl(u) && xvoCollItemCount(u) == 4, "xvalue set union");
		XRT_TC_CHECK(xvoIsColl(i) && xvoCollItemCount(i) == 1, "xvalue set intersection");
		XRT_TC_CHECK(xvoIsColl(d) && xvoCollItemCount(d) == 2, "xvalue set difference");
		XRT_TC_CHECK(xvoIsColl(s) && xvoCollItemCount(s) == 3, "xvalue set symmetric difference");
		xvoUnref(a);
		xvoUnref(b);
		xvoUnref(u);
		xvoUnref(i);
		xvoUnref(d);
		xvoUnref(s);
	}

	// ------------------------------------ clone mode ------------------------------------
	{
		int64 iValue = 42;
		xtarray pShared = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_SHARED);
		xtarray pPreserved;
		xtarray pLocal;

		XRT_TC_CHECK(pShared != NULL && xrtTypedArrayAppend(pShared, &iValue) != NULL,
			"shared typed array create");
		pPreserved = xrtTypedArrayClone(pShared);
		pLocal = xrtTypedArrayCloneEx(pShared, XRT_OBJMODE_LOCAL);
		XRT_TC_CHECK(pPreserved != NULL &&
			xrtOwnerGetMode(&pPreserved->Array.Owner) == XRT_OBJMODE_SHARED &&
			*(int64*)xrtTypedArrayGet(pPreserved, 1) == 42,
			"typed array clone preserves shared mode");
		XRT_TC_CHECK(pLocal != NULL &&
			xrtOwnerGetMode(&pLocal->Array.Owner) == XRT_OBJMODE_LOCAL &&
			*(int64*)xrtTypedArrayGet(pLocal, 1) == 42,
			"typed array clone ex selects local mode");
		xrtTypedArrayDestroy(pShared);
		xrtTypedArrayDestroy(pPreserved);
		xrtTypedArrayDestroy(pLocal);
	}

	// ------------------------------------ move to shared ------------------------------------
	{
		int64 iValue = 23;
		xtarray pArray = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtlist pList = xrtTypedListCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtset pSet = xrtTypedSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtdict pDict = xrtTypedDictCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);

		XRT_TC_CHECK(xrtTypedArrayAppend(pArray, &iValue) != NULL, "array move shared source");
		XRT_TC_CHECK(xrtTypedListSet(pList, 1, &iValue) != NULL, "list move shared source");
		XRT_TC_CHECK(xrtTypedSetAdd(pSet, &iValue), "set move shared source");
		XRT_TC_CHECK(xrtTypedDictSet(pDict, "v", 1, &iValue) != NULL, "dict move shared source");

		pArray = xrtTypedArrayMoveToShared(pArray);
		pList = xrtTypedListMoveToShared(pList);
		pSet = xrtTypedSetMoveToShared(pSet);
		pDict = xrtTypedDictMoveToShared(pDict);
		XRT_TC_CHECK(pArray != NULL && xrtOwnerGetMode(&pArray->Array.Owner) == XRT_OBJMODE_SHARED &&
			*(int64*)xrtTypedArrayGet(pArray, 1) == 23, "array move to shared");
		XRT_TC_CHECK(pList != NULL && xrtOwnerGetMode(&pList->List.Owner) == XRT_OBJMODE_SHARED &&
			*(int64*)xrtTypedListGet(pList, 1) == 23, "list move to shared");
		XRT_TC_CHECK(pSet != NULL && xrtOwnerGetMode(&pSet->Set.Owner) == XRT_OBJMODE_SHARED &&
			xrtTypedSetExists(pSet, &iValue), "set move to shared");
		XRT_TC_CHECK(pDict != NULL && xrtOwnerGetMode(&pDict->Dict.Owner) == XRT_OBJMODE_SHARED &&
			*(int64*)xrtTypedDictGet(pDict, "v", 1) == 23, "dict move to shared");
		XRT_TC_CHECK(xrtTypedArrayMoveToShared(pArray) == pArray, "array shared move is identity");

		xrtTypedArrayDestroy(pArray);
		xrtTypedListDestroy(pList);
		xrtTypedSetDestroy(pSet);
		xrtTypedDictDestroy(pDict);
	}

	// ------------------------------------ retain/release shared handle ------------------------------------
	{
		int64 iValue = 7;
		xtarray pArray = xrtTypedArrayCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtlist pList = xrtTypedListCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtset pSet = xrtTypedSetCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtdict pDict = xrtTypedDictCreate(xrtTypeInt(), XRT_OBJMODE_LOCAL);
		xtarray pArrayAlias;
		xtlist pListAlias;
		xtset pSetAlias;
		xtdict pDictAlias;

		XRT_TC_CHECK(xrtTypedArrayAppend(pArray, &iValue) != NULL, "array retain source");
		XRT_TC_CHECK(xrtTypedListSet(pList, 1, &iValue) != NULL, "list retain source");
		XRT_TC_CHECK(xrtTypedSetAdd(pSet, &iValue), "set retain source");
		XRT_TC_CHECK(xrtTypedDictSet(pDict, "v", 1, &iValue) != NULL, "dict retain source");

		pArrayAlias = xrtTypedArrayRetain(pArray);
		pListAlias = xrtTypedListRetain(pList);
		pSetAlias = xrtTypedSetRetain(pSet);
		pDictAlias = xrtTypedDictRetain(pDict);
		xrtTypedArrayDestroy(pArray);
		xrtTypedListDestroy(pList);
		xrtTypedSetDestroy(pSet);
		xrtTypedDictDestroy(pDict);

		XRT_TC_CHECK(pArrayAlias != NULL && *(int64*)xrtTypedArrayGet(pArrayAlias, 1) == 7,
			"array retained handle survives owner release");
		XRT_TC_CHECK(pListAlias != NULL && *(int64*)xrtTypedListGet(pListAlias, 1) == 7,
			"list retained handle survives owner release");
		XRT_TC_CHECK(pSetAlias != NULL && xrtTypedSetExists(pSetAlias, &iValue),
			"set retained handle survives owner release");
		XRT_TC_CHECK(pDictAlias != NULL && *(int64*)xrtTypedDictGet(pDictAlias, "v", 1) == 7,
			"dict retained handle survives owner release");

		xrtTypedArrayRelease(pArrayAlias);
		xrtTypedListRelease(pListAlias);
		xrtTypedSetRelease(pSetAlias);
		xrtTypedDictRelease(pDictAlias);
	}

	// ------------------------------------ dynamic to typed bridge ------------------------------------
	{
		xvalue arrayValue = xvoCreateArray();
		xvalue listValue = xvoCreateList();
		xvalue setValue = xvoCreateSet();
		xvalue dictValue = xvoCreateTable();
		xvalue invalidValue = xvoCreateArray();
		xtarray typedArray;
		xtlist typedList;
		xtset typedSet;
		xtdict typedDict;
		xvalue arrayRoundTrip;
		xvalue listRoundTrip;
		xvalue setRoundTrip;
		xvalue dictRoundTrip;
		int64 expected = 20;

		xvoArrayAppendInt(arrayValue, 10);
		xvoArrayAppendInt(arrayValue, 20);
		xvoListSetInt(listValue, 3, 30);
		xvoListSetInt(listValue, 9, 90);
		xvoCollSetInt(setValue, 7);
		xvoCollSetInt(setValue, 8);
		xvoTableSetInt(dictValue, "a", 1, 11);
		xvoTableSetInt(dictValue, "b", 1, 22);
		xvoArrayAppendText(invalidValue, "not-an-int", 0, FALSE);

		typedArray = xrtTypedArrayFromValue(arrayValue, xrtTypeInt(), XRT_OBJMODE_LOCAL);
		typedList = xrtTypedListFromValue(listValue, xrtTypeInt(), XRT_OBJMODE_LOCAL);
		typedSet = xrtTypedSetFromValue(setValue, xrtTypeInt(), XRT_OBJMODE_LOCAL);
		typedDict = xrtTypedDictFromValue(dictValue, xrtTypeInt(), XRT_OBJMODE_LOCAL);

		XRT_TC_CHECK(typedArray != NULL && xrtTypedArrayCount(typedArray) == 2,
			"dynamic array to typed array");
		XRT_TC_CHECK(typedArray != NULL && xrtTypedArrayContains(typedArray, &expected),
			"dynamic array conversion values");
		XRT_TC_CHECK(typedList != NULL && *(int64*)xrtTypedListGet(typedList, 9) == 90,
			"dynamic list to typed list");
		XRT_TC_CHECK(typedSet != NULL && xrtTypedSetCount(typedSet) == 2,
			"dynamic set to typed set");
		XRT_TC_CHECK(typedDict != NULL && *(int64*)xrtTypedDictGet(typedDict, "b", 1) == 22,
			"dynamic dict to typed dict");
		XRT_TC_CHECK(xrtTypedArrayFromValue(invalidValue, xrtTypeInt(), XRT_OBJMODE_LOCAL) == NULL,
			"dynamic conversion rejects incompatible elements");
		XRT_TC_CHECK(xrtTypedArrayFromValue(dictValue, xrtTypeInt(), XRT_OBJMODE_LOCAL) == NULL,
			"dynamic conversion rejects wrong container kind");

		arrayRoundTrip = xrtTypedArrayToValue(typedArray);
		listRoundTrip = xrtTypedListToValue(typedList);
		setRoundTrip = xrtTypedSetToValue(typedSet);
		dictRoundTrip = xrtTypedDictToValue(typedDict);
		XRT_TC_CHECK(arrayRoundTrip != NULL && xvoArrayItemCount(arrayRoundTrip) == 2 &&
			xvoGetInt(xvoArrayGetValue(arrayRoundTrip, 1)) == 20,
			"typed array to dynamic array");
		XRT_TC_CHECK(listRoundTrip != NULL && xvoGetInt(xvoListGetValue(listRoundTrip, 9)) == 90,
			"typed list to dynamic list");
		XRT_TC_CHECK(setRoundTrip != NULL && xvoCollItemCount(setRoundTrip) == 2,
			"typed set to dynamic set");
		XRT_TC_CHECK(dictRoundTrip != NULL &&
			xvoGetInt(xvoTableGetValue(dictRoundTrip, "b", 1)) == 22,
			"typed dict to dynamic dict");

		xrtTypedArrayDestroy(typedArray);
		xrtTypedListDestroy(typedList);
		xrtTypedSetDestroy(typedSet);
		xrtTypedDictDestroy(typedDict);
		xvoUnref(arrayValue);
		xvoUnref(listValue);
		xvoUnref(setValue);
		xvoUnref(dictValue);
		xvoUnref(invalidValue);
		xvoUnref(arrayRoundTrip);
		xvoUnref(listRoundTrip);
		xvoUnref(setRoundTrip);
		xvoUnref(dictRoundTrip);
	}

	#undef XRT_TC_CHECK

	printf("[typed_container] passed=%d failed=%d\n", iPassed, iFailed);
	return iFailed == 0 ? 0 : 1;
}
