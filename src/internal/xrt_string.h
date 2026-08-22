#ifndef XRT_INTERNAL_STRING_H
#define XRT_INTERNAL_STRING_H

#include "xrt_internal.h"



#if defined(XRT_FEATURE_STRING)

/* 判断一个字节是否属于 XRT 统一的 ASCII 空白集合。 */
static inline bool __xrtStrAsciiSpace(unsigned char iByte)
{
	return (iByte == (unsigned char)' ') || (iByte == (unsigned char)'\t') ||
		(iByte == (unsigned char)'\r') || (iByte == (unsigned char)'\n') ||
		(iByte == (unsigned char)'\v') || (iByte == (unsigned char)'\f');
}



/* 检查字符串视图的指针与长度组合是否合法。 */
static inline bool __xrtStrViewValid(xstrview Text)
{
	if ( (Text.Data == NULL) && (Text.Size != 0) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return true;
}



/* 返回 ASCII 小写字节，不受进程区域设置影响。 */
static inline unsigned char __xrtStrAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 设置字符串模块的结构化值错误。 */
static inline void __xrtStrSetValueError(int32 iCode, cstr sOperation, cstr sMessage)
{
	xerrordesc tDesc;
	xerror* pError;

	memset(&tDesc, 0, sizeof(tDesc));
	tDesc.Kind = XERR_VALUE;
	tDesc.Domain = "xrt.string";
	tDesc.Code = iCode;
	tDesc.Operation = sOperation;
	tDesc.Message = sMessage;
	pError = xrtErrorBuild(&tDesc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}

#endif

#endif
