#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



int main(void)
{
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("single.Object")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Object"),
		.AbiName = XRT_STR_INIT("single.Object"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(int64),
		.InstanceAlign = sizeof(int64)
	};
	xrtweak Weak = { 0 };
	xrtobject* pObject = xrtObjectCreate(&Type);
	xrtobject* pLocked;

	if (
		(pObject == NULL) ||
		!xrtWeakInit(&Weak, pObject)
	) {
		xrtWeakUnit(&Weak);
		xrtObjectUnref(pObject);
		return 1;
	}
	pLocked = xrtWeakLock(&Weak);
	if ( pLocked == NULL ) {
		xrtWeakUnit(&Weak);
		xrtObjectUnref(pObject);
		return 1;
	}
	xrtObjectUnref(pLocked);
	xrtObjectUnref(pObject);
	if ( !xrtWeakExpired(&Weak) ) {
		xrtWeakUnit(&Weak);
		return 2;
	}
	xrtWeakUnit(&Weak);
	return 0;
}
