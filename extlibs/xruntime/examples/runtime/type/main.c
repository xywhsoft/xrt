#include <stdio.h>
#include <xruntime.h>



/* 展示内建类型、值操作、注册表和协议描述验证的常用路径。 */
int main(void)
{
	const xrttype* pType = xrtTypeInt64();
	xrttyperegistry* pRegistry = xrtTypeRegistryCreate();
	xrttype ProtocolType = {
		.Kind = XRT_TYPE_PROTOCOL,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Printable"),
		.AbiName = XRT_STR_INIT("example.Printable"),
		.Size = sizeof(ptr),
		.Align = _Alignof(ptr),
		.InstanceSize = sizeof(ptr),
		.InstanceAlign = _Alignof(ptr)
	};
	xrtprotocol Protocol = { &ProtocolType, 0u, NULL };
	int64 iLeft = 7;
	int64 iRight = 11;
	int iCompare;
	uint64 iHash;

	ProtocolType.Id = xrtTypeId(ProtocolType.AbiName);
	if (
		(pRegistry == NULL) ||
		!xrtTypeValidate(pType) ||
		!xrtTypeValidate(xrtTypeBool32()) ||
		!xrtProtocolValidate(&Protocol) ||
		!xrtTypeRegistryAdd(pRegistry, pType) ||
		(xrtTypeRegistryAt(pRegistry, 0u) != pType) ||
		(xrtTypeRegistryFindName(pRegistry, pType->AbiName) != pType) ||
		!xrtTypeCompareValue(pType, &iLeft, &iRight, &iCompare) ||
		!xrtTypeHashValue(pType, &iLeft, &iHash)
	) {
		xrtTypeRegistryDestroy(pRegistry);
		return 1;
	}
	printf(
		"type=%.*s id=%llu size=%zu compare=%d hash=%llu\n",
		(int)pType->Name.Size,
		pType->Name.Data,
		(unsigned long long)pType->Id,
		pType->Size,
		iCompare,
		(unsigned long long)iHash
	);
	if ( !xrtTypeRegistryRemove(pRegistry, pType) ) {
		xrtTypeRegistryDestroy(pRegistry);
		return 2;
	}
	xrtTypeRegistryDestroy(pRegistry);
	return 0;
}
