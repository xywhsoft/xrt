#include <stdio.h>
#include <xruntime.h>



int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.Counter")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Counter"),
		.AbiName = XRT_STR_INIT("example.Counter"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtweak Weak = { 0 };
	xrtobject* pCounter = xrtObjectCreate(&Type);

	if ( (pCounter == NULL) || !xrtWeakInit(&Weak, pCounter) ) {
		xrtObjectUnref(pCounter);
		return 1;
	}
	*(int64*)xrtObjectData(pCounter) = 42;
	printf("counter=%lld\n", (long long)*(int64*)xrtObjectData(pCounter));
	xrtObjectUnref(pCounter);
	printf("expired=%s\n", xrtWeakExpired(&Weak) ? "true" : "false");
	xrtWeakUnit(&Weak);
	return 0;
}
