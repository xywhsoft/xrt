#include <stdio.h>
#include <xrt.h>



/* 演示全部基础标量、精确 Getter、类型查询、哈希和数值相等。 */
int main(void)
{
	unsigned char arrBytes[] = { 1, 2, 3 };
	int iMarker = 0;
	xvalue* arrValues[] = {
		xrtValueInt(2),
		xrtValueFloat(2.0),
		xrtValueString(XRT_STR_LITERAL("xrt")),
		xrtValueBytes((xbytesview){ arrBytes, sizeof(arrBytes) }),
		xrtValueTime((xtime)1234567),
		xrtValuePointer(&iMarker)
	};
	xvalue* pTrue = xrtValueBool(true);
	xstrview Name;
	xbytesview Data;
	int64 iVersion;
	double fVersion;
	xtime Time;
	ptr pPointer;
	bool bTrue;
	uint64 iIntHash;
	uint64 iFloatHash;
	int iResult = 0;

	for ( size_t i = 0; i < (sizeof(arrValues) / sizeof(arrValues[0])); i++ ) {
		if ( arrValues[i] == NULL ) {
			iResult = 1;
			goto cleanup;
		}
	}
	if (
		!xrtValueGetBool(pTrue, &bTrue) ||
		!xrtValueGetInt(arrValues[0], &iVersion) ||
		!xrtValueGetFloat(arrValues[1], &fVersion) ||
		!xrtValueGetString(arrValues[2], &Name) ||
		!xrtValueGetBytes(arrValues[3], &Data) ||
		!xrtValueGetTime(arrValues[4], &Time) ||
		!xrtValueGetPointer(arrValues[5], &pPointer) ||
		!xrtValueHash(arrValues[0], &iIntHash) ||
		!xrtValueHash(arrValues[1], &iFloatHash) ||
		!xrtValueScalarEqual(arrValues[0], arrValues[1]) ||
		!bTrue ||
		(iVersion != 2) ||
		(fVersion != 2.0) ||
		(Data.Size != 3) ||
		(Time != (xtime)1234567) ||
		(pPointer != &iMarker) ||
		(iIntHash != iFloatHash) ||
		(xrtValueType(arrValues[2]) != XVALUE_STRING) ||
		!xrtValueIs(arrValues[2], XVALUE_STRING) ||
		!xrtValueIsNumber(arrValues[0]) ||
		xrtValueIsContainer(arrValues[0]) ||
		!xrtValueTruthy(arrValues[2]) ||
		(xrtValueType(xrtValueNull()) != XVALUE_NULL)
	) {
		iResult = 2;
		goto cleanup;
	}

	printf("%.*s value API v%lld\n", (int)Name.Size, Name.Data, (long long)iVersion);
	printf("time type: %s\n", xrtValueTypeName(xrtValueType(arrValues[4])));

cleanup:
	for ( size_t i = 0; i < (sizeof(arrValues) / sizeof(arrValues[0])); i++ ) {
		xrtValueRelease(arrValues[i]);
	}
	xrtValueRelease(pTrue);
	xrtValueRelease(xrtValueNull());
	return iResult;
}
