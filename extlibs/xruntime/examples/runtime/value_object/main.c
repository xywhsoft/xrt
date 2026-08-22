#include <stdio.h>
#include <xruntime.h>



int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.ValueCounter")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueCounter"),
		.AbiName = XRT_STR_INIT("example.ValueCounter"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtobject* pObject = xrtObjectCreate(&Type);
	xvalue* pValue;

	if ( pObject == NULL ) {
		return 1;
	}
	*(int64*)xrtObjectData(pObject) = 42;
	pValue = xrtValueRuntimeObjectTake(&pObject);
	if ( pValue == NULL ) {
		xrtObjectUnref(pObject);
		return 1;
	}
	printf("counter=%lld\n", (long long)*(int64*)xrtObjectData(
		xrtValueGetRuntimeObject(pValue)));
	xrtValueRelease(pValue);
	return 0;
}
