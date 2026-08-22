#define XRUNTIME_IMPLEMENTATION
#include "../../single/xruntime.h"



typedef struct singlefieldvalue {
	int64 Id;
} singlefieldvalue;



/* 验证单头文件中的字段验证与数据访问路径。 */
int main(void)
{
	xrtfielddesc Field = {
		XRT_STR_INIT("id"), xrtTypeInt64(), offsetof(singlefieldvalue, Id), 0u
	};
	xrtfieldtable Fields = { 1u, &Field };
	xrttype Type = {
		.Id = xrtTypeId(XRT_STR_LITERAL("single.Field")),
		.Kind = XRT_TYPE_CLASS,
		.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE,
		.Name = XRT_STR_INIT("Field"),
		.AbiName = XRT_STR_INIT("single.Field"),
		.Size = sizeof(ptr),
		.Align = sizeof(ptr),
		.InstanceSize = sizeof(singlefieldvalue),
		.InstanceAlign = sizeof(int64),
		.Fields = &Fields
	};
	singlefieldvalue Value = { 23 };

	return (!xrtTypeFieldsValidate(&Type) ||
		(xrtTypeFieldCount(&Type) != 1u) ||
		(*(int64*)xrtFieldData(&Type, &Field, &Value) != 23)) ? 1 : 0;
}
