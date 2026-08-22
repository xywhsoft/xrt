#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



int main(void)
{
	const xrttype* pType = xrtTypeInt64();
	xrttyperegistry* pRegistry = xrtTypeRegistryCreate();
	xrttype ProtocolType = {
		.Kind = XRT_TYPE_PROTOCOL,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("SingleProtocol"),
		.AbiName = XRT_STR_INIT("tests.single.SingleProtocol"),
		.Size = sizeof(ptr),
		.Align = _Alignof(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = _Alignof(ptr)
	};
	xrtprotocol Protocol = { &ProtocolType, 0u, NULL };
	int iResult = 0;

	ProtocolType.Id = xrtTypeId(ProtocolType.AbiName);
	if (
		(pRegistry == NULL) ||
		!xrtTypeValidate(pType) ||
		!xrtTypeValidate(xrtTypeBool32()) ||
		!xrtProtocolValidate(&Protocol) ||
		!xrtTypeRegistryAdd(pRegistry, pType) ||
		(xrtTypeRegistryFindId(pRegistry, pType->Id) != pType)
	) {
		iResult = 1;
	}
	xrtTypeRegistryDestroy(pRegistry);
	return iResult;
}
