#include <stdio.h>
#include <xruntime.h>



int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.ValueWeak")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("ValueWeak"),
		.AbiName = XRT_STR_INIT("example.ValueWeak"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtobject* pObject = xrtObjectCreate(&Type);
	xrtweak Weak = { 0 };
	xvalue* pWeakValue;

	if ( (pObject == NULL) || !xrtWeakInit(&Weak, pObject) ) {
		xrtObjectUnref(pObject);
		return 1;
	}
	pWeakValue = xrtValueWeakTake(&Weak);
	if ( pWeakValue == NULL ) {
		xrtWeakUnit(&Weak);
		xrtObjectUnref(pObject);
		return 1;
	}
	xrtObjectUnref(pObject);
	printf("expired=%s\n", xrtValueWeakExpired(pWeakValue)
		? "true" : "false");
	xrtValueRelease(pWeakValue);
	return 0;
}
