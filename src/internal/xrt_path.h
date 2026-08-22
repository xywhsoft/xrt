#ifndef XRT_INTERNAL_PATH_H
#define XRT_INTERNAL_PATH_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_PATH)

/* 设置路径模块结构化错误。 */
void __xrtPathSetError(xerrkind Kind, xpatherror Code,
	cstr sOperation, cstr sMessage, int iSystemCode);

#endif



#if defined(XRT_FEATURE_PATH_SAFE)

/* 可移植路径段状态只保留设备名判断所需的有限前缀。 */
typedef struct xrt_path_safe_segment {
	uint8 Base[7];
	size_t Size;
	size_t BaseSize;
	size_t BaseTrimmedSize;
	uint8 First;
	uint8 Last;
	bool BaseDone;
	bool Invalid;
} xrt_path_safe_segment;



/* 初始化一个可跨任意输入分块工作的路径段检查器。 */
void __xrtPathSafeSegmentInit(
	xrt_path_safe_segment* pState
);



/* 向路径段检查器加入一个字节；false 表示已经确定非法。 */
bool __xrtPathSafeSegmentFeed(
	xrt_path_safe_segment* pState,
	uint8 iValue
);



/* 完成空段、点段、尾部规则和 Windows 设备名检查。 */
bool __xrtPathSafeSegmentFinish(
	const xrt_path_safe_segment* pState
);

#endif



#if defined(XRT_FEATURE_PATH_SYSTEM) && (defined(_WIN32) || defined(_WIN64))

/* 严格把零结尾 UTF-8 路径转换为 Windows UTF-16。 */
uint16* __xrtPathToWide(cstr sPath, size_t* pSize);



/* 严格把 Windows UTF-16 路径转换为 UTF-8。 */
str __xrtPathFromWide(const wchar_t* sPath, size_t iSize);

#endif

#endif
