#include <stdio.h>
#include <xruntime.h>



typedef struct userrecord {
	uint64 Id;
	bool Enabled;
} userrecord;



/* 展示字段表验证、名称查询和实例数据访问。 */
int main(void)
{
	xrtfielddesc arrFields[] = {
		{ XRT_STR_INIT("id"), xrtTypeUInt64(), offsetof(userrecord, Id), 0u },
		{
			XRT_STR_INIT("enabled"), xrtTypeBool(),
			offsetof(userrecord, Enabled), 0u
		}
	};
	xrtfieldtable Fields = { 2u, arrFields };
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("example.User")),
		.Kind = XRT_TYPE_RECORD,
		.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
			XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE,
		.Name = XRT_STR_INIT("User"),
		.AbiName = XRT_STR_INIT("example.User"),
		.Size = sizeof(userrecord),
		.Align = _Alignof(userrecord),
		.InstanceSize = sizeof(userrecord),
		.InstanceAlign = _Alignof(userrecord),
		.Fields = &Fields
	};
	userrecord User = { 42u, true };
	const xrtfielddesc* pId;

	if ( !xrtTypeFieldsValidate(&Type) ) {
		return 1;
	}
	pId = xrtTypeFindField(&Type, XRT_STR_LITERAL("id"));
	if ( pId == NULL ) {
		return 2;
	}
	printf("fields=%zu id=%llu\n",
		xrtTypeFieldCount(&Type),
		(unsigned long long)*(const uint64*)xrtFieldConstData(
			&Type, pId, &User));
	return 0;
}
