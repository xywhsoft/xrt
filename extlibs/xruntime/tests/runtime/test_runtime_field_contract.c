#include "../test.h"



typedef struct testfieldpair {
	int64 First;
	int64 Second;
} testfieldpair;



/* 构造字段契约测试使用的类类型。 */
static xrttype testContractType(
	cstr sName,
	cstr sAbiName,
	const xrttype* pBase,
	size_t iSize,
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
	Type.InstanceSize = iSize;
	Type.InstanceAlign = TEST_ALIGNOF(testfieldpair);
	Type.Base = pBase;
	Type.Fields = pFields;
	return Type;
}



/* 验证最近一次字段验证错误。 */
static void testContractError(xerrkind Kind)
{
	const xerror* pError = xrtGetError();

	testRequire(pError != NULL, "field contract error is missing");
	testRequire(
		strcmp(xrtErrorDomain(pError), "xrt.field") == 0,
		"field contract error domain mismatch"
	);
	testRequire(
		xrtErrorCode(pError) == XFIELD_ERROR_DESCRIPTOR,
		"field contract error code mismatch"
	);
	testRequire(xrtErrorKind(pError) == Kind, "field contract error kind mismatch");
}



/* 验证描述数组、字段类型、标志、边界、对齐和局部冲突。 */
static void testLocalDescriptorRejection(void)
{
	xrtfielddesc arrFields[] = {
		{ XRT_STR_INIT("first"), xrtTypeInt64(), 0u, 0u },
		{ XRT_STR_INIT("second"), xrtTypeInt64(), sizeof(int64), 0u }
	};
	xrtfieldtable Table = { 2u, arrFields };
	xrttype Type = testContractType(
		"Pair", "tests.field.Pair", NULL, sizeof(testfieldpair), &Table
	);
	xrtfieldtable Missing = { 1u, NULL };
	xrttype BadValue = *xrtTypeInt64();
	xrtfielddesc Saved = arrFields[1];

	testRequire(xrtTypeFieldsValidate(&Type), "valid field pair was rejected");

	Type.Fields = &Missing;
	xrtClearError();
	testRequire(!xrtTypeFieldsValidate(&Type), "missing field array was accepted");
	testContractError(XERR_ARGUMENT);
	Type.Fields = &Table;

	arrFields[1].Name = (xstrview){ NULL, 0u };
	testRequire(!xrtTypeFieldsValidate(&Type), "empty field name was accepted");
	arrFields[1] = Saved;

	arrFields[1].Flags = UINT32_C(0x80000000);
	testRequire(!xrtTypeFieldsValidate(&Type), "unknown field flag was accepted");
	arrFields[1] = Saved;

	arrFields[1].Offset = sizeof(testfieldpair);
	testRequire(!xrtTypeFieldsValidate(&Type), "out-of-bounds field was accepted");
	arrFields[1] = Saved;

	arrFields[1].Offset = 1u;
	testRequire(!xrtTypeFieldsValidate(&Type), "unaligned field was accepted");
	arrFields[1] = Saved;

	arrFields[1].Offset = 0u;
	testRequire(!xrtTypeFieldsValidate(&Type), "overlapping fields were accepted");
	arrFields[1] = Saved;

	arrFields[1].Name = arrFields[0].Name;
	xrtClearError();
	testRequire(!xrtTypeFieldsValidate(&Type), "duplicate field name was accepted");
	testContractError(XERR_EXISTS);
	arrFields[1] = Saved;

	BadValue.Id ^= UINT64_C(1);
	arrFields[1].Type = &BadValue;
	xrtClearError();
	testRequire(!xrtTypeFieldsValidate(&Type), "invalid field type was accepted");
	testContractError(XERR_ARGUMENT);
	testRequire(
		(xrtErrorCause(xrtGetError()) != NULL) &&
		(strcmp(xrtErrorDomain(xrtErrorCause(xrtGetError())), "xrt.type") == 0),
		"invalid field type cause was not preserved"
	);
}



/* 验证继承字段不能覆盖基类负载或隐藏已有名称。 */
static void testInheritanceRejection(void)
{
	xrtfielddesc BaseField = {
		XRT_STR_INIT("first"), xrtTypeInt64(), 0u, 0u
	};
	xrtfieldtable BaseTable = { 1u, &BaseField };
	xrttype Base = testContractType(
		"Base", "tests.field.ContractBase", NULL, sizeof(int64), &BaseTable
	);
	xrtfielddesc DerivedField = {
		XRT_STR_INIT("second"), xrtTypeInt64(), sizeof(int64), 0u
	};
	xrtfieldtable DerivedTable = { 1u, &DerivedField };
	xrttype Derived = testContractType(
		"Derived", "tests.field.ContractDerived", &Base,
		sizeof(testfieldpair), &DerivedTable
	);

	testRequire(xrtTypeFieldsValidate(&Derived), "valid derived field was rejected");
	DerivedField.Offset = 0u;
	testRequire(
		!xrtTypeFieldsValidate(&Derived),
		"derived field overlapping base payload was accepted"
	);
	DerivedField.Offset = sizeof(int64);
	DerivedField.Name = BaseField.Name;
	xrtClearError();
	testRequire(
		!xrtTypeFieldsValidate(&Derived),
		"derived field hiding inherited name was accepted"
	);
	testContractError(XERR_EXISTS);
}



/* 验证字段只允许附着到记录和类。 */
static void testOwnerKindRejection(void)
{
	xrtfielddesc Field = {
		XRT_STR_INIT("value"), xrtTypeInt64(), 0u, 0u
	};
	xrtfieldtable Table = { 1u, &Field };
	xrttype Type = testContractType(
		"Handle", "tests.field.Handle", NULL, sizeof(int64), &Table
	);

	Type.Kind = XRT_TYPE_HANDLE;
	xrtClearError();
	testRequire(
		!xrtTypeFieldsValidate(&Type),
		"non-record field owner was accepted"
	);
	testContractError(XERR_ARGUMENT);
}



/* 执行运行时字段拒绝边界测试。 */
int main(void)
{
	testLocalDescriptorRejection();
	testInheritanceRejection();
	testOwnerKindRejection();
	xrtClearError();
	printf("[PASS] runtime field contract\n");
	return 0;
}
