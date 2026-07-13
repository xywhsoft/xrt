#include "../xrt.h"

#include <stdio.h>

static int check(bool condition, const char* message)
{
	if ( condition ) {
		return 1;
	}
	fprintf(stderr, "test_value_cow: %s\n", message);
	return 0;
}

int main(void)
{
	xvalue array = NULL;
	xvalue arrayCopy = NULL;
	xvalue list = NULL;
	xvalue listCopy = NULL;
	xvalue set = NULL;
	xvalue setCopy = NULL;
	xvalue dict = NULL;
	xvalue dictCopy = NULL;
	xvalue sharedArray = NULL;
	xvalue sharedArrayCopy = NULL;
	xvalue nested = NULL;
	xvalue nestedCopy = NULL;
	xvalue nestedChild = NULL;
	xvalue mutableChild = NULL;
	int ok = 1;
	int i;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	array = xvoCreateArray();
	xvoArrayAppendInt(array, 1);
	arrayCopy = xvoShare(array);
	ok &= check(arrayCopy != NULL && arrayCopy != array, "array share must create another value shell");
	ok &= check(arrayCopy->vArray == array->vArray, "array backing should be shared before write");
	ok &= check(xvoArrayAppendInt(arrayCopy, 2), "array copy append failed");
	ok &= check(arrayCopy->vArray != array->vArray, "array backing was not detached");
	ok &= check(xvoArrayItemCount(array) == 1 && xvoArrayItemCount(arrayCopy) == 2,
		"array copy-on-write changed the source");
	ok &= check(xvoArrayMerge(arrayCopy, arrayCopy), "array self merge failed");
	ok &= check(xvoArrayItemCount(arrayCopy) == 4, "array self merge count mismatch");

	list = xvoCreateList();
	xvoListSetInt(list, 7, 70);
	listCopy = xvoShare(list);
	ok &= check(listCopy != NULL && listCopy->vList == list->vList, "list backing was not shared");
	ok &= check(xvoListSetInt(listCopy, 8, 80), "list copy set failed");
	ok &= check(listCopy->vList != list->vList, "list backing was not detached");
	ok &= check(xvoIsNull(xvoListGetValue(list, 8)) && xvoGetInt(xvoListGetValue(listCopy, 8)) == 80,
		"list copy-on-write changed the source");
	ok &= check(xvoListMerge(listCopy, listCopy, TRUE), "list self merge failed");
	ok &= check(xvoListItemCount(listCopy) == 2, "list self merge count mismatch");

	set = xvoCreateSet();
	xvoCollSetInt(set, 1);
	setCopy = xvoShare(set);
	ok &= check(setCopy != NULL && setCopy->vSet == set->vSet, "set backing was not shared");
	ok &= check(xvoCollSetInt(setCopy, 2), "set copy add failed");
	ok &= check(setCopy->vSet != set->vSet, "set backing was not detached");
	ok &= check(xvoCollItemCount(set) == 1 && xvoCollItemCount(setCopy) == 2,
		"set copy-on-write changed the source");
	ok &= check(xvoCollMerge(setCopy, setCopy), "set self merge failed");
	ok &= check(xvoCollItemCount(setCopy) == 2, "set self merge count mismatch");

	dict = xvoCreateTable();
	xvoTableSetInt(dict, "a", 1, 1);
	dictCopy = xvoShare(dict);
	ok &= check(dictCopy != NULL && dictCopy->vTable == dict->vTable, "dict backing was not shared");
	ok &= check(xvoTableSetInt(dictCopy, "b", 1, 2), "dict copy set failed");
	ok &= check(dictCopy->vTable != dict->vTable, "dict backing was not detached");
	ok &= check(!xvoTableExists(dict, (str)"b", 1) && xvoGetInt(xvoTableGetValue(dictCopy, "b", 1)) == 2,
		"dict copy-on-write changed the source");
	ok &= check(xvoTableMerge(dictCopy, dictCopy, TRUE), "dict self merge failed");
	ok &= check(xvoTableItemCount(dictCopy) == 2, "dict self merge count mismatch");

	ok &= check(xvoEnsureUnique(array), "unique array check failed");
	ok &= check(xvoEnsureUnique(arrayCopy), "detached array uniqueness check failed");

	sharedArray = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	ok &= check(xvoArrayAppendInt(sharedArray, 11), "shared array initial append failed");
	sharedArrayCopy = xvoShare(sharedArray);
	ok &= check(sharedArrayCopy != NULL && sharedArrayCopy->vArray == sharedArray->vArray,
		"shared array backing was not shared");
	if ( !xvoEnsureUnique(sharedArrayCopy) ) {
		fprintf(stderr, "test_value_cow: shared array detach error: %s\n", xrtGetError());
		ok = 0;
	}
	ok &= check(xvoArraySetInt(sharedArrayCopy, 0, 22), "shared array copy set failed");
	ok &= check(sharedArrayCopy->vArray != sharedArray->vArray, "shared array backing was not detached");
	ok &= check(xvoGetInt(xvoArrayGetValue(sharedArray, 0)) == 11 &&
		xvoGetInt(xvoArrayGetValue(sharedArrayCopy, 0)) == 22,
		"shared array copy-on-write changed the source");

	nested = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	nestedChild = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	xvoArrayAppendInt(nestedChild, 31);
	xvoArrayAppendValue(nested, nestedChild, TRUE);
	nestedChild = NULL;
	nestedCopy = xvoShare(nested);
	mutableChild = xvoIndexGetMutableI64(nestedCopy, 0);
	ok &= check(mutableChild != NULL, "nested mutable child lookup failed");
	ok &= check(xvoArraySetInt(mutableChild, 0, 32), "nested mutable child set failed");
	ok &= check(xvoGetInt(xvoIndexGetI64(xvoIndexGetI64(nested, 0), 0)) == 31,
		"nested copy-on-write changed the source");
	ok &= check(xvoGetInt(xvoIndexGetI64(xvoIndexGetI64(nestedCopy, 0), 0)) == 32,
		"nested copy-on-write did not update the copy");

	/* 反复创建短生命周期值，覆盖线程本地 size-class 对象池路径。 */
	for ( i = 0; i < 100000; ++i ) {
		xvalue value = xvoCreateInt(i);
		if ( value == NULL || xvoGetInt(value) != i ) {
			ok = 0;
			break;
		}
		xvoUnref(value);
	}

	xvoUnref(dictCopy);
	xvoUnref(dict);
	xvoUnref(sharedArrayCopy);
	xvoUnref(sharedArray);
	xvoUnref(nestedCopy);
	xvoUnref(nested);
	xvoUnref(setCopy);
	xvoUnref(set);
	xvoUnref(listCopy);
	xvoUnref(list);
	xvoUnref(arrayCopy);
	xvoUnref(array);
	xrtUnit();
	return ok ? 0 : 1;
}
