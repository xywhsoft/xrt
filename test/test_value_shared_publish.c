#include "../xrt.h"

#include <stdio.h>

static const xrt_type_desc typed_dict_desc = {
	.TypeId = 0xD1C70001u,
	.Kind = XRT_TYPE_KIND_DICT,
	.Name = "dict<int>",
	.NameSize = 9u,
	.AbiName = "xvalue",
	.AbiNameSize = 6u,
	.Size = sizeof(xvalue),
	.Align = sizeof(ptr)
};

static int handle_drop_count = 0;

static void drop_owned_handle(ptr object)
{
	ptr* handle = (ptr*)object;
	if ( handle != NULL && *handle != NULL ) {
		handle_drop_count += 1;
		*handle = NULL;
	}
}

static const xrt_type_ops owned_handle_ops = {
	.drop = drop_owned_handle
};

static const xrt_type_desc owned_handle_desc = {
	.TypeId = 0xD1C70002u,
	.Kind = XRT_TYPE_KIND_HANDLE,
	.Name = "owned-handle",
	.NameSize = 12u,
	.AbiName = "ptr",
	.AbiNameSize = 3u,
	.Size = sizeof(ptr),
	.Align = sizeof(ptr),
	.Ops = &owned_handle_ops
};

static int fail(const char* message)
{
	fprintf(stderr, "test_value_shared_publish: %s\n", message);
	return 1;
}

static xvalue make_local_dict(const char* text)
{
	xvalue table = xvoCreateTable();
	xvalue value = xvoCreateText((ptr)text, 0, FALSE);
	if ( table == NULL || value == NULL || !xvoTableSetValue(table, "name", 4, value, TRUE) ) {
		if ( value != NULL ) {
			xvoUnref(value);
		}
		if ( table != NULL ) {
			xvoUnref(table);
		}
		return NULL;
	}
	return table;
}

int main(void)
{
	xvalue array = NULL;
	xvalue list = NULL;
	xvalue table = NULL;
	xvalue source = NULL;
	xvalue copied = NULL;
	xvalue cycle = NULL;
	xvalue cycleTarget = NULL;
	xvalue item;
	int handle_object = 7;
	int result = 0;

	if ( xrtInit() == NULL ) {
		return fail("xrtInit failed");
	}
	if ( xvoIsShared(xvoCreateNull()) ) {
		result = fail("xvoIsShared ownership query failed");
		goto cleanup;
	}

	handle_drop_count = 0;
	source = xvoCreateHandle(&owned_handle_desc, &handle_object, XRT_HANDLE_FLAG_OWNED);
	item = xvoMoveToShared(source);
	if ( item == NULL || item != source || !xvoIsShared(item) ||
		 xvoGetHandleData(item) != &handle_object || handle_drop_count != 0 ) {
		result = fail("xvoMoveToShared did not move an owned typed handle");
		goto cleanup;
	}
	source = NULL;
	xvoUnref(item);
	item = NULL;
	if ( handle_drop_count != 1 ) {
		result = fail("xvoMoveToShared owned typed handle drop count mismatch");
		goto cleanup;
	}

	array = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	source = make_local_dict("array");
	if ( array == NULL || !xvoIsShared(array) || source == NULL || !xvoArrayAppendValue(array, source, FALSE) ) {
		result = fail("shared array append failed");
		goto cleanup;
	}
	item = xvoArrayGetValue(array, 0);
	if ( item == source || !xvoIsShared(item) ||
		 strcmp((const char*)xvoGetText(xvoTableGetValue(item, "name", 4)), "array") != 0 ) {
		result = fail("shared array did not publish an independent nested graph");
		goto cleanup;
	}
	xvoUnref(source);
	source = NULL;

	list = xvoCreateListEx(XRT_OBJMODE_SHARED);
	source = make_local_dict("list");
	if ( list == NULL || source == NULL || !xvoListSetValue(list, -7, source, FALSE) ) {
		result = fail("shared list set failed");
		goto cleanup;
	}
	item = xvoListGetValue(list, -7);
	if ( item == source || !xvoIsShared(item) ||
		 strcmp((const char*)xvoGetText(xvoTableGetValue(item, "name", 4)), "list") != 0 ) {
		result = fail("shared list did not publish an independent nested graph");
		goto cleanup;
	}
	xvoUnref(source);
	source = NULL;

	table = xvoCreateTableEx(XRT_OBJMODE_SHARED);
	source = make_local_dict("table");
	if ( table == NULL || source == NULL || !xvoTableSetValue(table, "child", 5, source, FALSE) ) {
		result = fail("shared table set failed");
		goto cleanup;
	}
	item = xvoTableGetValue(table, "child", 5);
	if ( item == source || !xvoIsShared(item) ||
		 strcmp((const char*)xvoGetText(xvoTableGetValue(item, "name", 4)), "table") != 0 ) {
		result = fail("shared table did not publish an independent nested graph");
		goto cleanup;
	}
	xvoUnref(source);
	source = NULL;

	source = xvoCreateArray();
	item = make_local_dict("copy");
	if ( source == NULL || item == NULL || !xvoArrayAppendValue(source, item, TRUE) ) {
		result = fail("deep-copy fixture creation failed");
		goto cleanup;
	}
	copied = xvoDeepCopyEx(source, XRT_OBJMODE_SHARED);
	if ( copied == NULL || !xvoIsShared(copied) ||
		 !xvoIsShared(xvoArrayGetValue(copied, 0)) ||
		 strcmp((const char*)xvoGetText(xvoTableGetValue(xvoArrayGetValue(copied, 0), "name", 4)), "copy") != 0 ) {
		result = fail("xvoDeepCopyEx shared graph failed");
		goto cleanup;
	}
	xvoUnref(copied);
	copied = NULL;
	xvoUnref(source);
	source = make_local_dict("typed-dict");
	if ( source == NULL || !xvoSetTypeDesc(source, &typed_dict_desc) ) {
		result = fail("typed table fixture creation failed");
		goto cleanup;
	}
	copied = xvoDeepCopyEx(source, XRT_OBJMODE_SHARED);
	if ( copied == NULL || !xvoIsShared(copied) ||
		 !xrtTypeSame(xvoTypeDesc(copied), &typed_dict_desc) ) {
		result = fail("shared table deep copy lost its exact type descriptor");
		goto cleanup;
	}

	cycle = xvoCreateArray();
	cycleTarget = xvoCreateArrayEx(XRT_OBJMODE_SHARED);
	if ( cycle == NULL || cycleTarget == NULL || !xvoArrayAppendValue(cycle, cycle, FALSE) ) {
		result = fail("cycle fixture creation failed");
		goto cleanup;
	}
	if ( xvoArrayAppendValue(cycleTarget, cycle, FALSE) || xvoArrayItemCount(cycleTarget) != 0 ) {
		result = fail("cyclic graph publication should fail without partial insertion");
	}

cleanup:
	if ( cycle != NULL ) {
		xvoArrayRemove(cycle, 0, 1);
		xvoUnref(cycle);
	}
	if ( cycleTarget != NULL ) xvoUnref(cycleTarget);
	if ( copied != NULL ) xvoUnref(copied);
	if ( source != NULL ) xvoUnref(source);
	if ( table != NULL ) xvoUnref(table);
	if ( list != NULL ) xvoUnref(list);
	if ( array != NULL ) xvoUnref(array);
	xrtUnit();
	return result;
}
