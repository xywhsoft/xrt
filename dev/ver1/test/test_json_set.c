#include "../xrt.h"

#include <stdio.h>


static int Require(int condition, const char* message)
{
	if ( condition ) {
		return 1;
	}
	fprintf(stderr, "[json-set] %s\n", message);
	return 0;
}


static xvalue CreateIntSet(void)
{
	xvalue setValue = xvoCreateSet();
	if ( setValue == NULL ||
		!xvoSetAddValue(setValue, xvoCreateInt(7), TRUE) ||
		!xvoSetAddValue(setValue, xvoCreateInt(11), TRUE) ) {
		xvoUnref(setValue);
		return NULL;
	}
	return setValue;
}


static xvalue CreateLegacyIntSet(void)
{
	xvalue setValue = xvoCreateColl();
	if ( setValue == NULL ||
		!xvoCollSetValue(setValue, xvoCreateInt(7), TRUE) ||
		!xvoCollSetValue(setValue, xvoCreateInt(11), TRUE) ) {
		xvoUnref(setValue);
		return NULL;
	}
	return setValue;
}


static int ArrayHasInt(xvalue arrayValue, int64 expected)
{
	uint32 index;

	if ( arrayValue == NULL || xvoType(arrayValue) != XVO_DT_ARRAY ) {
		return 0;
	}
	for ( index = 0; index < xvoArrayItemCount(arrayValue); ++index ) {
		xvalue item = xvoArrayGetValue(arrayValue, index);
		if ( item != NULL && xvoType(item) == XVO_DT_INT && xvoGetInt(item) == expected ) {
			return 1;
		}
	}
	return 0;
}


static int IsExpectedSetArray(xvalue value)
{
	return value != NULL &&
		xvoType(value) == XVO_DT_ARRAY &&
		xvoArrayItemCount(value) == 2 &&
		ArrayHasInt(value, 7) &&
		ArrayHasInt(value, 11);
}


int main(void)
{
	xvalue setValue = NULL;
	xvalue root = NULL;
	xvalue arrayValue = NULL;
	xvalue listValue = NULL;
	xvalue parsed = NULL;
	xvalue nested;
	str text = NULL;
	int ok = 1;

	if ( xrtInit() == NULL ) {
		return 1;
	}

	// 根集合稳定映射为 JSON array。
	setValue = CreateIntSet();
	text = xrtStringifyJSON(setValue, FALSE, NULL);
	parsed = text != NULL ? xrtParseJSON(text, 0) : NULL;
	ok &= Require(IsExpectedSetArray(parsed), "root set did not round-trip as an array");
	xvoUnref(parsed);
	xrtFree(text);
	xvoUnref(setValue);
	parsed = NULL;
	text = NULL;
	setValue = NULL;

	// 旧 Coll 集合在迁移期也遵守同一 JSON 映射。
	setValue = CreateLegacyIntSet();
	text = xrtStringifyJSON(setValue, FALSE, NULL);
	parsed = text != NULL ? xrtParseJSON(text, 0) : NULL;
	ok &= Require(IsExpectedSetArray(parsed), "legacy root set did not round-trip as an array");
	xvoUnref(parsed);
	xrtFree(text);
	xvoUnref(setValue);
	parsed = NULL;
	text = NULL;
	setValue = NULL;

	// 所有递归容器路径都必须使用同一套集合分派规则。
	root = xvoCreateTable();
	arrayValue = xvoCreateArray();
	listValue = xvoCreateList();
	ok &= Require(root != NULL && arrayValue != NULL && listValue != NULL,
		"container allocation failed");
	ok &= Require(xvoTableSetValue(root, "tableSet", 0, CreateIntSet(), TRUE),
		"table set insertion failed");
	ok &= Require(xvoArrayAppendValue(arrayValue, CreateIntSet(), TRUE),
		"array set insertion failed");
	ok &= Require(xvoListSetValue(listValue, 0, CreateIntSet(), TRUE),
		"list set insertion failed");
	ok &= Require(xvoTableSetValue(root, "array", 0, arrayValue, TRUE),
		"array insertion failed");
	ok &= Require(xvoTableSetValue(root, "list", 0, listValue, TRUE),
		"list insertion failed");
	arrayValue = NULL;
	listValue = NULL;

	text = xrtStringifyJSON(root, FALSE, NULL);
	parsed = text != NULL ? xrtParseJSON(text, 0) : NULL;
	ok &= Require(parsed != NULL && xvoType(parsed) == XVO_DT_TABLE,
		"nested root did not round-trip as an object");
	nested = parsed != NULL ? xvoTableGetValue(parsed, "tableSet", 0) : NULL;
	ok &= Require(IsExpectedSetArray(nested), "table nested set mismatch");
	nested = parsed != NULL ? xvoTableGetValue(parsed, "array", 0) : NULL;
	ok &= Require(nested != NULL && xvoType(nested) == XVO_DT_ARRAY &&
		IsExpectedSetArray(xvoArrayGetValue(nested, 0)), "array nested set mismatch");
	nested = parsed != NULL ? xvoTableGetValue(parsed, "list", 0) : NULL;
	ok &= Require(nested != NULL && xvoType(nested) == XVO_DT_ARRAY &&
		IsExpectedSetArray(xvoArrayGetValue(nested, 0)), "list nested set mismatch");

	xvoUnref(parsed);
	xrtFree(text);
	xvoUnref(root);
	xvoUnref(arrayValue);
	xvoUnref(listValue);
	xrtUnit();
	return ok ? 0 : 1;
}
