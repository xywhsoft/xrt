#include <stdio.h>
#include <string.h>
#include <xrt.h>

/*
 * 范例：value/ownership —— Take 系列所有权移交与 Retain/Clone 语义
 * ----------------------------------------------------------------
 * 演示 API：
 *   xrtValueStringTake / BytesTake   接管裸缓冲为值（槽清零防双释放）
 *   xrtValueRetain    增引用返回同一指针（共享外壳）
 *   xrtValueClone     标量克隆也返回同一指针（不可变值免拷贝）
 *   xrtValueGetString / GetBytes     借用读取内容视图
 * 模块宏：XRT_MODULE_VALUE
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single impl.c  *       examples/value/ownership/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   hello: 7 8 9
 *
 * 三种"拿到值"的区别（本例验证 pRetained == pCloned == pText）：
 *   Take    移交缓冲所有权，零拷贝；
 *   Retain  +1 引用，多持有者共享同一份；
 *   Clone   标量（含字符串）克隆 = Retain——不可变内容没必要复制，
 *           容器克隆才是浅拷贝外壳（见 containers 范例）。
 */





/* 演示字符串和字节接管，以及 Retain 与标量 Clone 的引用语义。 */
int main(void)
{
	str sText = (str)xrtMalloc(6);
	bytes pData = (bytes)xrtMalloc(3);
	xvalue* pText = NULL;
	xvalue* pBytes = NULL;
	xvalue* pRetained = NULL;
	xvalue* pCloned = NULL;
	xstrview Text;
	xbytesview Data;
	int iResult = 0;

	if ( (sText == NULL) || (pData == NULL) ) {
		iResult = 1;
		goto cleanup;
	}
	memcpy(sText, "hello", 6);
	pData[0] = 7;
	pData[1] = 8;
	pData[2] = 9;
	pText = xrtValueStringTake(&sText, 5);
	pBytes = xrtValueBytesTake(&pData, 3);
	if ( (pText == NULL) || (pBytes == NULL) ) {
		iResult = 2;
		goto cleanup;
	}

	pRetained = xrtValueRetain(pText);
	pCloned = xrtValueClone(pText);
	if (
		(pRetained == NULL) ||
		(pCloned == NULL) ||
		(pRetained != pText) ||
		(pCloned != pText) ||
		!xrtValueGetString(pText, &Text) ||
		!xrtValueGetBytes(pBytes, &Data)
	) {
		iResult = 3;
		goto cleanup;
	}
	printf(
		"%.*s: %u %u %u\n",
		(int)Text.Size,
		Text.Data,
		(unsigned)Data.Data[0],
		(unsigned)Data.Data[1],
		(unsigned)Data.Data[2]
	);

cleanup:
	xrtValueRelease(pCloned);
	xrtValueRelease(pRetained);
	xrtValueRelease(pBytes);
	xrtValueRelease(pText);
	xrtFree(pData);
	xrtFree(sText);
	return iResult;
}
