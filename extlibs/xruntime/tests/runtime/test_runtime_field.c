#include "../test.h"



typedef struct testfieldbase {
	int64 Id;
} testfieldbase;



typedef struct testfieldderived {
	testfieldbase Base;
	int32 Score;
	uint32 Flags;
} testfieldderived;



typedef struct testenumpayload {
	int64 Value;
	uint32 Code;
} testenumpayload;



/* 构造一个由当前测试栈帧持有的字段类型描述。 */
static xrttype testFieldType(
	cstr sName,
	cstr sAbiName,
	const xrttype* pBase,
	size_t iInstanceSize,
	size_t iInstanceAlign,
	const xrtfieldtable* pFields
)
{
	xrttype Type;

	memset(&Type, 0, sizeof(Type));
	Type.Id = xrtTypeId((xstrview){ sAbiName, strlen(sAbiName) });
	Type.Kind = XRT_TYPE_CLASS;
	Type.Flags = XRT_TYPE_FLAG_REFERENCE | XRT_TYPE_FLAG_NULLABLE;
	Type.Name = (xstrview){ sName, strlen(sName) };
	Type.AbiName = (xstrview){ sAbiName, strlen(sAbiName) };
	Type.Size = sizeof(ptr);
	Type.Align = TEST_ALIGNOF(ptr);
	Type.InstanceSize = iInstanceSize;
	Type.InstanceAlign = iInstanceAlign;
	Type.Base = pBase;
	Type.Fields = pFields;
	return Type;
}



/* 验证最近一次错误属于字段模块的指定代码和操作。 */
static void testFieldError(xfielderror Code, cstr sOperation)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "runtime field error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.field") == 0,
		"runtime field error domain mismatch"
	);
	testRequire(
		xrtErrorCode(pError) == (int32)Code,
		"runtime field error code mismatch"
	);
	testRequire(
		strcmp(xrtErrorOperation(pError), sOperation) == 0,
		"runtime field error operation mismatch"
	);
}



/* 验证字段继承顺序、名称查询、声明者和实例地址。 */
static void testFieldLookup(void)
{
	xrtfielddesc arrBaseFields[] = {
		{ XRT_STR_INIT("id"), xrtTypeInt64(), offsetof(testfieldbase, Id), 0u }
	};
	xrtfieldtable BaseFields = { 1u, arrBaseFields };
	xrttype Base = testFieldType(
		"Base", "tests.field.Base", NULL,
		sizeof(testfieldbase), TEST_ALIGNOF(testfieldbase), &BaseFields
	);
	xrtfielddesc arrDerivedFields[] = {
		{
			XRT_STR_INIT("score"), xrtTypeInt32(),
			offsetof(testfieldderived, Score), 0u
		},
		{
			XRT_STR_INIT("flags"), xrtTypeUInt32(),
			offsetof(testfieldderived, Flags), XRT_FIELD_FLAG_READONLY
		}
	};
	xrtfieldtable DerivedFields = { 2u, arrDerivedFields };
	xrttype Derived = testFieldType(
		"Derived", "tests.field.Derived", &Base,
		sizeof(testfieldderived), TEST_ALIGNOF(testfieldderived), &DerivedFields
	);
	testfieldderived Value = { { 17 }, 29, 3u };
	const testfieldderived ConstValue = { { 31 }, 43, 5u };
	const xrtfielddesc* pField;
	xrtfielddesc Foreign = arrDerivedFields[0];

	testRequire(
		xrtTypeFieldsValidate(&Derived),
		"valid inherited field table was rejected"
	);
	testRequire(
		xrtTypeFieldCount(&Derived) == 3u,
		"inherited field count mismatch"
	);
	testRequire(
		xrtTypeField(&Derived, 0u) == &arrBaseFields[0] &&
		xrtTypeField(&Derived, 1u) == &arrDerivedFields[0] &&
		xrtTypeField(&Derived, 2u) == &arrDerivedFields[1],
		"base-first field order mismatch"
	);
	pField = xrtTypeFindField(&Derived, XRT_STR_LITERAL("id"));
	testRequire(
		(pField == &arrBaseFields[0]) &&
		(xrtTypeFieldOwner(&Derived, pField) == &Base),
		"inherited field lookup or owner mismatch"
	);
	testRequire(
		*(const int64*)xrtFieldConstData(&Derived, pField, &ConstValue) == 31,
		"const inherited field address mismatch"
	);
	pField = xrtTypeFindField(&Derived, XRT_STR_LITERAL("score"));
	testRequire(
		(pField == &arrDerivedFields[0]) &&
		(*(int32*)xrtFieldData(&Derived, pField, &Value) == 29),
		"mutable field lookup or address mismatch"
	);
	*(int32*)xrtFieldData(&Derived, pField, &Value) = 47;
	testRequire(Value.Score == 47, "mutable field address did not reference the payload");
	testRequire(
		(arrDerivedFields[1].Flags & XRT_FIELD_FLAG_READONLY) != 0u,
		"read-only field fact was lost"
	);

	xrtClearError();
	testRequire(
		xrtTypeFindField(&Derived, XRT_STR_LITERAL("missing")) == NULL &&
		(xrtGetError() == NULL),
		"missing field lookup reported an error"
	);
	xrtClearError();
	testRequire(
		xrtTypeField(&Derived, 3u) == NULL,
		"out-of-range field lookup succeeded"
	);
	testFieldError(XFIELD_ERROR_LOOKUP, "field");
	xrtClearError();
	testRequire(
		xrtTypeFieldOwner(&Derived, &Foreign) == NULL,
		"foreign field descriptor was accepted"
	);
	testFieldError(XFIELD_ERROR_ACCESS, "owner");
	xrtClearError();
	testRequire(
		xrtFieldData(&Derived, &arrDerivedFields[0], NULL) == NULL,
		"null instance payload was accepted"
	);
	testFieldError(XFIELD_ERROR_ACCESS, "data");
}



/* 验证空字段表和记录类型的零字段路径。 */
static void testEmptyFields(void)
{
	xrtfieldtable Empty = { 0u, NULL };
	xrttype Record;

	memset(&Record, 0, sizeof(Record));
	Record.Id = xrtTypeId(XRT_STR_LITERAL("tests.field.Empty"));
	Record.Kind = XRT_TYPE_RECORD;
	Record.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
		XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE;
	Record.Name = XRT_STR_LITERAL("Empty");
	Record.AbiName = XRT_STR_LITERAL("tests.field.Empty");
	Record.Size = 0u;
	Record.Align = 1u;
	Record.InstanceSize = 0u;
	Record.InstanceAlign = 1u;
	Record.Fields = &Empty;

	testRequire(
		xrtTypeFieldsValidate(&Record) &&
		(xrtTypeFieldCount(&Record) == 0u),
		"empty record field table failed"
	);
}



/* 验证多字段枚举载荷复用记录字段元数据，而不建立第二套字段模型。 */
static void testEnumRecordPayload(void)
{
	xrtfielddesc arrFields[] = {
		{ XRT_STR_INIT("value"), xrtTypeInt64(), offsetof(testenumpayload, Value), 0u },
		{ XRT_STR_INIT("code"), xrtTypeUInt32(), offsetof(testenumpayload, Code), 0u }
	};
	xrtfieldtable Fields = { 2u, arrFields };
	xrttype PayloadType;
	xrttype EnumType;
	xrtenumvariant arrVariants[2];
	xrtenum Enum;
	const xrtenumvariant* pVariant;

	memset(&PayloadType, 0, sizeof(PayloadType));
	PayloadType.Id = xrtTypeId(XRT_STR_LITERAL("tests.field.ResultPayload"));
	PayloadType.Kind = XRT_TYPE_RECORD;
	PayloadType.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
		XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE;
	PayloadType.Name = XRT_STR_LITERAL("ResultPayload");
	PayloadType.AbiName = XRT_STR_LITERAL("tests.field.ResultPayload");
	PayloadType.Size = sizeof(testenumpayload);
	PayloadType.Align = TEST_ALIGNOF(testenumpayload);
	PayloadType.InstanceSize = sizeof(testenumpayload);
	PayloadType.InstanceAlign = TEST_ALIGNOF(testenumpayload);
	PayloadType.Fields = &Fields;

	memset(&EnumType, 0, sizeof(EnumType));
	EnumType.Id = xrtTypeId(XRT_STR_LITERAL("tests.field.Result"));
	EnumType.Kind = XRT_TYPE_ENUM;
	EnumType.Flags = XRT_TYPE_FLAG_TRIVIAL_COPY |
		XRT_TYPE_FLAG_TRIVIAL_DROP | XRT_TYPE_FLAG_COPYABLE;
	EnumType.Name = XRT_STR_LITERAL("Result");
	EnumType.AbiName = XRT_STR_LITERAL("tests.field.Result");
	EnumType.Size = sizeof(int64) + sizeof(testenumpayload);
	EnumType.Align = TEST_ALIGNOF(testenumpayload);
	EnumType.InstanceSize = EnumType.Size;
	EnumType.InstanceAlign = EnumType.Align;

	arrVariants[0] = (xrtenumvariant){
		XRT_STR_LITERAL("ok"), 0, &PayloadType
	};
	arrVariants[1] = (xrtenumvariant){
		XRT_STR_LITERAL("error"), 1, NULL
	};
	Enum = (xrtenum){ &EnumType, 2u, arrVariants };
	EnumType.Metadata = &Enum;

	testRequire(xrtEnumValidate(&Enum), "record-payload enum validation failed");
	pVariant = xrtEnumFindName(&Enum, XRT_STR_LITERAL("ok"));
	testRequire(
		(pVariant != NULL) && (pVariant->PayloadType == &PayloadType),
		"enum payload record type was lost"
	);
	testRequire(
		xrtTypeFieldsValidate(pVariant->PayloadType),
		"enum payload record fields were invalid"
	);
	testRequire(
		xrtTypeFindField(
			pVariant->PayloadType, XRT_STR_LITERAL("value")
		) == &arrFields[0],
		"enum payload record field lookup failed"
	);
}



/* 执行运行时字段常规测试。 */
int main(void)
{
	testFieldLookup();
	testEmptyFields();
	testEnumRecordPayload();
	xrtClearError();
	printf("[PASS] runtime field\n");
	return 0;
}
