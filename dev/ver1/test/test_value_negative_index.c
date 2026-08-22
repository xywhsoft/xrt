#include "../xrt.h"

#include <stdio.h>

static int fail(const char* message)
{
	fprintf(stderr, "test_value_negative_index: %s\n", message);
	return 1;
}

int main(void)
{
	xvalue array;
	xvalue list;
	int result = 0;

	if ( xrtInit() == NULL ) {
		return fail("xrtInit failed");
	}

	array = xvoCreateArray();
	list = xvoCreateList();
	if ( array == NULL || list == NULL ||
		 !xvoArrayAppendInt(array, 10) ||
		 !xvoArrayAppendInt(array, 20) ||
		 !xvoArrayAppendInt(array, 30) ) {
		result = fail("fixture creation failed");
		goto cleanup;
	}

	if ( xvoGetInt(xvoIndexGetI64(array, -1)) != 30 ||
		 xvoGetInt(xvoIndexGetI64(array, -3)) != 10 ||
		 !xvoIsNull(xvoIndexGetI64(array, -4)) ) {
		result = fail("array negative read failed");
		goto cleanup;
	}
	if ( !xvoIndexSetI64(array, -2, xvoCreateInt(99), TRUE) ||
		 xvoGetInt(xvoIndexGetI64(array, 1)) != 99 ) {
		result = fail("array negative write failed");
		goto cleanup;
	}

	if ( !xvoIndexSetI64(list, -1, xvoCreateInt(77), TRUE) ||
		 xvoGetInt(xvoIndexGetI64(list, -1)) != 77 ) {
		result = fail("list sparse negative key changed semantics");
		goto cleanup;
	}

cleanup:
	xvoUnref(array);
	xvoUnref(list);
	xrtUnit();
	return result;
}
