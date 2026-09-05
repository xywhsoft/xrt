#include <stdio.h>
#include <xrt.h>

/*
 * 范例：value/basic —— xvalue 全类型速览：构造、精确读取与判定
 * ----------------------------------------------------------------
 * 演示 API：
 *   构造族   Int / Float / String / Bytes / Time / Pointer / Bool / Null
 *   精确读取 GetInt / GetFloat / GetString / GetBytes / GetTime /
 *            GetPointer / GetBool（类型不符即失败，不做隐式转换）
 *   判定族   ValueType / Is / IsNumber / IsContainer / Truthy
 *   数值相等 ScalarEqual：2 与 2.0 跨类型相等（宽松数值语义）
 *   哈希     ValueHash：与 ScalarEqual 一致——int 2 与 float 2.0 同哈希
 *   ValueTypeName  类型枚举 → 小写类型名（序列化/调试用）
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/basic/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   xrt value API v2
 *   time type: time
 *
 * 精确 Getter 的哲学："读什么类型就给什么类型的出参"——
 *   GetInt 不会悄悄收下 float（要数值宽容用 ScalarEqual）。
 *   这是配置解析场景防类型混乱的第一道闸。
 *   Null 是进程级单例：每次返回同一指针（可安全 == 比较）。
 */





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
