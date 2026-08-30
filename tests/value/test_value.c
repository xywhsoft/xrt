#include "../test.h"

#include <math.h>



/* 验证静态值、类型查询和统一引用手感。 */
static void testValueStatic(void)
{
	xvalue* pNull = xrtValueNull();
	xvalue* pTrue = xrtValueBool(true);
	xvalue* pFalse = xrtValueBool(false);
	bool bValue;

	testRequire((pNull == xrtValueNull()) && xrtValueIs(pNull, XVALUE_NULL), "null singleton mismatch");
	testRequire((pTrue == xrtValueBool(true)) && (pFalse == xrtValueBool(false)), "bool singleton mismatch");
	testRequire(xrtValueGetBool(pTrue, &bValue) && bValue, "true getter mismatch");
	testRequire(xrtValueGetBool(pFalse, &bValue) && !bValue, "false getter mismatch");
	testRequire(strcmp(xrtValueTypeName(XVALUE_OBJECT), "object") == 0, "value type name mismatch");
	testRequire(xrtValueType(NULL) == XVALUE_INVALID, "null pointer type mismatch");
	testRequire(xrtValueRetain(pNull) == pNull, "static retain mismatch");
	xrtValueRelease(pNull);
	xrtValueRelease(pTrue);
	xrtValueRelease(pFalse);
}



/* 验证可选语义类型身份只允许一次性、一致绑定。 */
static void testValueTypeId(void)
{
	const uint64 iTypeId = UINT64_C(0x91A2B3C4D5E6F708);
	xvalue* pValue = xrtValueInt(7);
	xvalue* pShared;

	testRequire(pValue != NULL, "type identity fixture failed");
	testRequire(xrtValueTypeId(NULL) == 0, "null type identity mismatch");
	testRequire(xrtValueTypeId(pValue) == 0, "fresh value has a type identity");
	testRequire(
		xrtValueTypeIdBind(pValue, iTypeId) &&
		xrtValueTypeIdBind(pValue, iTypeId) &&
		(xrtValueTypeId(pValue) == iTypeId),
		"type identity bind mismatch"
	);
	xrtClearError();
	testRequire(
		!xrtValueTypeIdBind(pValue, iTypeId + 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtValueTypeId(pValue) == iTypeId),
		"conflicting type identity changed the value"
	);
	xrtClearError();
	pShared = xrtValueRetain(pValue);
	testRequire(pShared == pValue, "type identity shared fixture mismatch");
	testRequire(
		!xrtValueTypeIdRebind(pValue, iTypeId + 1u) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE) &&
		(xrtValueTypeId(pValue) == iTypeId),
		"shared value accepted a type identity rebind"
	);
	xrtValueRelease(pShared);
	xrtClearError();
	testRequire(
		xrtValueTypeIdRebind(pValue, iTypeId + 1u) &&
		xrtValueTypeIdRebind(pValue, iTypeId + 1u) &&
		(xrtValueTypeId(pValue) == iTypeId + 1u),
		"unique value type identity rebind mismatch"
	);
	testRequire(
		!xrtValueTypeIdBind(xrtValueNull(), iTypeId) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"static value accepted a type identity"
	);
	xrtClearError();
	testRequire(
		!xrtValueTypeIdRebind(xrtValueNull(), iTypeId) &&
		(xrtErrorKind(xrtGetError()) == XERR_STATE),
		"static value accepted a type identity rebind"
	);
	xrtValueRelease(pValue);
}



/* 验证标量精确读取、真值和错误类别。 */
static void testValueScalars(void)
{
	xvalue* pInt = xrtValueInt(-42);
	xvalue* pUInt = xrtValueUInt(UINT64_MAX);
	xvalue* pFloat = xrtValueFloat(3.5);
	xvalue* pTime = xrtValueTime((xtime)1234567);
	xvalue* pZeroTime = xrtValueTime(0);
	xvalue* pPointer = xrtValuePointer((ptr)(uintptr_t)0x1234u);
	xvalue* pNullPointer = xrtValuePointer(NULL);
	int64 iValue;
	uint64 iUnsigned;
	double fValue;
	xtime Time;
	ptr pResult;

	testRequire(
		(pInt != NULL) && (pUInt != NULL) && (pFloat != NULL) && (pTime != NULL) &&
		(pZeroTime != NULL) && (pPointer != NULL) && (pNullPointer != NULL),
		"scalar creation failed"
	);
	testRequire(xrtValueGetInt(pInt, &iValue) && (iValue == -42), "integer getter mismatch");
	testRequire(
		xrtValueGetUInt(pUInt, &iUnsigned) && (iUnsigned == UINT64_MAX),
		"unsigned integer getter mismatch"
	);
	testRequire(xrtValueGetFloat(pFloat, &fValue) && (fValue == 3.5), "float getter mismatch");
	testRequire(xrtValueGetTime(pTime, &Time) && (Time == (xtime)1234567), "time getter mismatch");
	testRequire(xrtValueGetPointer(pPointer, &pResult) && (pResult == (ptr)(uintptr_t)0x1234u), "pointer getter mismatch");
	testRequire(
		xrtValueTruthy(pInt) && xrtValueTruthy(pUInt) &&
		xrtValueTruthy(pFloat) && xrtValueTruthy(pTime),
		"scalar truth mismatch"
	);
	testRequire(
		strcmp(xrtValueTypeName(XVALUE_UINT), "uint") == 0 &&
		xrtValueIsNumber(pUInt),
		"unsigned integer type identity mismatch"
	);
	testRequire(
		xrtValueTruthy(pZeroTime) && xrtValueTruthy(pNullPointer),
		"non-null value object truth mismatch"
	);

	xrtClearError();
	fValue = 9.25;
	testRequire(!xrtValueGetFloat(pInt, &fValue), "wrong scalar getter should fail");
	testRequire(xrtErrorKind(xrtGetError()) == XERR_TYPE, "wrong scalar getter error mismatch");
	testRequire(fValue == 9.25, "failed scalar getter changed output");
	xrtValueRelease(pNullPointer);
	xrtValueRelease(pPointer);
	xrtValueRelease(pZeroTime);
	xrtValueRelease(pTime);
	xrtValueRelease(pFloat);
	xrtValueRelease(pUInt);
	xrtValueRelease(pInt);
}



/* 验证字符串与字节的复制、移交、内嵌零和视图生命周期。 */
static void testValueBlobs(void)
{
	char arrText[] = { 'a', '\0', 'b' };
	unsigned char arrBytes[] = { 1, 0, 2, 3 };
	xvalue* pString = xrtValueString((xstrview){ arrText, sizeof(arrText) });
	xvalue* pBytes = xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) });
	str sOwned = (str)xrtMalloc(6);
	bytes pOwned = (bytes)xrtMalloc(3);
	xvalue* pTakenString;
	xvalue* pTakenBytes;
	xvalue* pEmptyString = xrtValueString((xstrview){ NULL, 0 });
	xvalue* pEmptyBytes = xrtValueBytes((xbytesview){ NULL, 0 });
	xstrview Text;
	xbytesview Data;

	testRequire(
		(pString != NULL) && (pBytes != NULL) && (pEmptyString != NULL) &&
		(pEmptyBytes != NULL) && (sOwned != NULL) && (pOwned != NULL),
		"blob fixture failed"
	);
	testRequire(
		!xrtValueTruthy(pEmptyString) && !xrtValueTruthy(pEmptyBytes),
		"empty blob truth mismatch"
	);
	memcpy(sOwned, "hello", 6);
	pOwned[0] = 7;
	pOwned[1] = 8;
	pOwned[2] = 9;
	arrText[0] = 'z';
	arrBytes[0] = 9;
	testRequire(xrtValueGetString(pString, &Text), "string view failed");
	testRequire((Text.Size == 3) && (Text.Data[0] == 'a') && (Text.Data[1] == '\0') && (Text.Data[2] == 'b') && (Text.Data[3] == '\0'), "string copy mismatch");
	testRequire(xrtValueGetBytes(pBytes, &Data) && (Data.Size == 4) && (Data.Data[0] == 1), "bytes copy mismatch");

	pTakenString = xrtValueStringTake(&sOwned, 5);
	pTakenBytes = xrtValueBytesTake(&pOwned, 3);
	testRequire((pTakenString != NULL) && (sOwned == NULL), "string take mismatch");
	testRequire((pTakenBytes != NULL) && (pOwned == NULL), "bytes take mismatch");
	testRequire(xrtValueGetString(pTakenString, &Text) && (memcmp(Text.Data, "hello", 5) == 0), "taken string mismatch");
	testRequire(xrtValueGetBytes(pTakenBytes, &Data) && (Data.Data[2] == 9), "taken bytes mismatch");

	xrtValueRelease(pTakenBytes);
	xrtValueRelease(pTakenString);
	xrtValueRelease(pEmptyBytes);
	xrtValueRelease(pEmptyString);
	xrtValueRelease(pBytes);
	xrtValueRelease(pString);
}



/* 验证 Getter 和 Hash 输出不能覆盖值拥有的字符串或字节。 */
static void testValueOutputAliases(void)
{
	unsigned char arrBytes[32];
	char arrText[32];
	unsigned char arrBytesCopy[32];
	char arrTextCopy[32];
	xvalue* pString;
	xvalue* pBytes;
	xstrview Text;
	xbytesview Data;

	memset(arrText, 'x', sizeof(arrText));
	memset(arrBytes, 0xA5, sizeof(arrBytes));
	memcpy(arrTextCopy, arrText, sizeof(arrText));
	memcpy(arrBytesCopy, arrBytes, sizeof(arrBytes));
	pString = xrtValueString((xstrview){ arrText, sizeof(arrText) });
	pBytes = xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) });
	testRequire(
		(pString != NULL) && (pBytes != NULL),
		"value output alias fixture failed"
	);
	testRequire(xrtValueGetString(pString, &Text), "string alias view failed");
	testRequire(xrtValueGetBytes(pBytes, &Data), "bytes alias view failed");

	xrtClearError();
	testRequire(
		!xrtValueGetString(pString, (xstrview*)(ptr)Text.Data),
		"string getter accepted owned output"
	);
	testRequire(
		xrtErrorKind(xrtGetError()) == XERR_ARGUMENT,
		"string getter alias error mismatch"
	);
	testRequire(
		memcmp(Text.Data, arrTextCopy, sizeof(arrTextCopy)) == 0,
		"failed string getter changed owned text"
	);

	xrtClearError();
	testRequire(
		!xrtValueHash(pString, (uint64*)(ptr)Text.Data),
		"value hash accepted owned output"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Text.Data, arrTextCopy, sizeof(arrTextCopy)) == 0),
		"failed value hash changed owned text"
	);

	xrtClearError();
	testRequire(
		!xrtValueGetBytes(pBytes, (xbytesview*)(ptr)Data.Data),
		"bytes getter accepted owned output"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(memcmp(Data.Data, arrBytesCopy, sizeof(arrBytesCopy)) == 0),
		"failed bytes getter changed owned data"
	);

	xrtValueRelease(pBytes);
	xrtValueRelease(pString);
}



/* 为句柄别名测试提供签名匹配的释放器。 */
static void testValueAliasDrop(ptr pHandle, ptr pUserData)
{
	(void)pUserData;
	xrtFree(pHandle);
}



/* 验证 Take 来源指针槽不能位于准备接管的内存中。 */
static void testValueTakeAliases(void)
{
	static const xvaluehandleops tHandleOps = {
		NULL,
		testValueAliasDrop,
		NULL,
		NULL
	};
	bytes pBytes = (bytes)xrtMalloc(32);
	str sText = (str)xrtMalloc(32);
	ptr pHandle = xrtMalloc(32);

	testRequire(
		(pBytes != NULL) && (sText != NULL) && (pHandle != NULL),
		"value take alias allocation failed"
	);
	*(bytes*)pBytes = pBytes;
	*(str*)sText = sText;
	*(ptr*)pHandle = pHandle;

	xrtClearError();
	testRequire(
		xrtValueBytesTake((bytes*)pBytes, 32) == NULL,
		"bytes take accepted source-slot alias"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(*(bytes*)pBytes == pBytes),
		"bytes take alias changed source"
	);

	xrtClearError();
	testRequire(
		xrtValueStringTake((str*)sText, 16) == NULL,
		"string take accepted source-slot alias"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(*(str*)sText == sText),
		"string take alias changed source"
	);

	xrtClearError();
	testRequire(
		xrtValueHandleTake((ptr*)pHandle, &tHandleOps, NULL) == NULL,
		"handle take accepted source-slot alias"
	);
	testRequire(
		(xrtErrorKind(xrtGetError()) == XERR_ARGUMENT) &&
		(*(ptr*)pHandle == pHandle),
		"handle take alias changed source"
	);

	xrtFree(pHandle);
	xrtFree(sText);
	xrtFree(pBytes);
}



/* 验证跨整数和浮点数的一致哈希边界。 */
static void testValueHashing(void)
{
	xvalue* pInt = xrtValueInt(42);
	xvalue* pFloat = xrtValueFloat(42.0);
	xvalue* pUInt = xrtValueUInt(42u);
	xvalue* pUIntHigh = xrtValueUInt(UINT64_C(9223372036854775808));
	xvalue* pFloatHigh = xrtValueFloat(9223372036854775808.0);
	xvalue* pUIntMax = xrtValueUInt(UINT64_MAX);
	xvalue* pNegative = xrtValueInt(-1);
	xvalue* pZero = xrtValueInt(0);
	xvalue* pUIntZero = xrtValueUInt(0u);
	xvalue* pNegativeZero = xrtValueFloat(-0.0);
	xvalue* pNanA = xrtValueFloat(NAN);
	xvalue* pNanB = xrtValueFloat(NAN);
	uint64 iHashA;
	uint64 iHashB;

	testRequire(xrtValueHash(pInt, &iHashA) && xrtValueHash(pFloat, &iHashB) && (iHashA == iHashB), "numeric hash mismatch");
	testRequire(
		xrtValueHash(pUInt, &iHashB) && (iHashA == iHashB) &&
		xrtValueScalarEqual(pInt, pUInt),
		"signed/unsigned numeric hash mismatch"
	);
	testRequire(
		xrtValueHash(pUIntHigh, &iHashA) && xrtValueHash(pFloatHigh, &iHashB) &&
		(iHashA == iHashB) && xrtValueScalarEqual(pUIntHigh, pFloatHigh),
		"high unsigned/float numeric hash mismatch"
	);
	testRequire(
		!xrtValueScalarEqual(pUIntMax, pNegative),
		"negative signed value aliased unsigned maximum"
	);
	testRequire(xrtValueHash(pZero, &iHashA) && xrtValueHash(pNegativeZero, &iHashB) && (iHashA == iHashB), "negative zero hash mismatch");
	testRequire(xrtValueScalarEqual(pZero, pUIntZero), "unsigned zero equality mismatch");
	testRequire(xrtValueHash(pNanA, &iHashA) && xrtValueHash(pNanB, &iHashB) && (iHashA == iHashB), "NaN hash mismatch");
	testRequire(xrtValueScalarEqual(pInt, pFloat), "numeric scalar equality mismatch");
	testRequire(
		xrtValueScalarEqual(pZero, pNegativeZero),
		"negative zero scalar equality mismatch"
	);
	testRequire(xrtValueScalarEqual(pNanA, pNanB), "NaN scalar equality mismatch");
	xrtValueRelease(pNanB);
	xrtValueRelease(pNanA);
	xrtValueRelease(pNegativeZero);
	xrtValueRelease(pUIntZero);
	xrtValueRelease(pZero);
	xrtValueRelease(pUIntMax);
	xrtValueRelease(pNegative);
	xrtValueRelease(pFloatHigh);
	xrtValueRelease(pUIntHigh);
	xrtValueRelease(pUInt);
	xrtValueRelease(pFloat);
	xrtValueRelease(pInt);
}



/* 运行动态值核心回归。 */
int main(void)
{
	testValueStatic();
	testValueTypeId();
	testValueScalars();
	testValueBlobs();
	testValueOutputAliases();
	testValueTakeAliases();
	testValueHashing();
	printf("[PASS] value\n");
	return 0;
}
