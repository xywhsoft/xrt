#include "../internal/xrt_internal.h"
#include <xrt/xid.h>

#include <stdio.h>



#if defined(XRT_FEATURE_XID)

/* 按 ASCII 递增排列的 URL-safe Base64 字母表保持文本与二进制排序一致。 */
static const char __xrtXidAlphabet[] =
	"-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";



/* 返回 XID 固定文本编码配置。 */
static xbase64config __xrtXidCodec(void)
{
	xbase64config Config;

	Config.Alphabet = __xrtXidAlphabet;
	Config.Flags = XBASE64_NO_PADDING;
	return Config;
}



/* 把有符号 Unix 微秒映射为按字节递增的无符号值。 */
static uint64 __xrtXidOrderTime(xtime iTime)
{
	if ( iTime < 0 ) {
		return (uint64)(iTime - INT64_MIN);
	}
	return (UINT64_C(1) << 63) + (uint64)iTime;
}



/* 把有序时间按网络字节序写入 XID 前缀。 */
static void __xrtXidStoreTime(xid* pXid, xtime iTime)
{
	uint64 iOrdered = __xrtXidOrderTime(iTime);

	for ( size_t i = 0; i < 8u; i++ ) {
		pXid->Data[i] = (uint8)(iOrdered >> (56u - (i * 8u)));
	}
}



/* 从 XID 的有序前缀恢复完整有符号 Unix 微秒。 */
static xtime __xrtXidLoadTime(const xid* pXid)
{
	uint64 iOrdered = 0;

	for ( size_t i = 0; i < 8u; i++ ) {
		iOrdered = (iOrdered << 8u) | (uint64)pXid->Data[i];
	}
	if ( iOrdered < (UINT64_C(1) << 63) ) {
		return INT64_MIN + (int64)iOrdered;
	}
	return (int64)(iOrdered - (UINT64_C(1) << 63));
}



/* 判断一个字符是否属于 XID 的规范字母表。 */
static bool __xrtXidTextByte(char iByte)
{
	return (iByte == '-') || (iByte == '_') ||
		((iByte >= '0') && (iByte <= '9')) ||
		((iByte >= 'A') && (iByte <= 'Z')) ||
		((iByte >= 'a') && (iByte <= 'z'));
}



/* 设置带稳定文本位置的 XID 格式错误。 */
static void __xrtXidFormatError(size_t iOffset, cstr sMessage)
{
	char arrData[64];
	xerrordesc Desc;
	xerror* pError;

	memset(&Desc, 0, sizeof(Desc));
	(void)snprintf(
		arrData,
		sizeof(arrData),
		"offset=%llu",
		(unsigned long long)iOffset
	);
	Desc.Kind = XERR_VALUE;
	Desc.Code = XID_ERROR_FORMAT;
	Desc.Domain = "xrt.xid";
	Desc.Operation = "parse";
	Desc.Message = sMessage;
	Desc.Data = arrData;
	pError = xrtErrorBuild(&Desc);
	if ( pError != NULL ) {
		__xrtErrorSetOwned(pError);
	}
}



/* 生成单个 XID。 */
XRT_API bool xrtXidMake(xid* pXid)
{
	return xrtXidMakeMany(pXid, 1u);
}



/* 批量生成 XID，并保证随机源失败时整批保持全零。 */
XRT_API bool xrtXidMakeMany(xid* pXids, size_t iCount)
{
	size_t iSize;

	if ( iCount == 0 ) {
		return true;
	}
	if ( pXids == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCount > (SIZE_MAX / sizeof(xid)) ) {
		__xrtErrorSetSizeOverflow();
		return false;
	}
	iSize = iCount * sizeof(xid);
	if ( !xrtSecureRandom(pXids, iSize) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		__xrtXidStoreTime(&pXids[i], xrtNow());
	}
	return true;
}



/* 生成并格式化常用文本结果。 */
XRT_API str xrtXidMakeString(void)
{
	xid Value;

	if ( !xrtXidMake(&Value) ) {
		return NULL;
	}
	return xrtXidFormat(&Value);
}



/* 写入固定长度文本，容量不足时不留下部分编码。 */
XRT_API bool xrtXidWrite(
	const xid* pXid,
	char* sOutput,
	size_t iCapacity
)
{
	xbase64config Config;
	size_t iOutputSize = 0;

	if ( (pXid == NULL) || (sOutput == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iCapacity < XID_TEXT_CAPACITY ) {
		if ( iCapacity != 0 ) {
			sOutput[0] = 0;
		}
		__xrtErrorSetRange();
		return false;
	}
	Config = __xrtXidCodec();
	return xrtBase64Encode(
		pXid->Data,
		XID_BINARY_SIZE,
		sOutput,
		iCapacity,
		&iOutputSize,
		&Config
	) && (iOutputSize == XID_TEXT_SIZE);
}



/* 分配并写入固定长度文本。 */
XRT_API str xrtXidFormat(const xid* pXid)
{
	xbase64config Config;

	if ( pXid == NULL ) {
		__xrtErrorSetInvalidArgument();
		return NULL;
	}
	Config = __xrtXidCodec();
	return xrtBase64EncodeNew(
		pXid->Data,
		XID_BINARY_SIZE,
		&Config
	);
}



/* 校验规范字母表后复用 Base64 底座解码，成功后一次提交输出。 */
XRT_API bool xrtXidParse(xstrview Text, xid* pXid)
{
	xbase64config Config;
	xid Value;
	size_t iOutputSize = 0;

	if ( (pXid == NULL) ||
		 ((Text.Data == NULL) && (Text.Size != 0)) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( Text.Size != XID_TEXT_SIZE ) {
		__xrtXidFormatError(
			Text.Size < XID_TEXT_SIZE ? Text.Size : XID_TEXT_SIZE,
			"XID text must contain exactly 32 bytes"
		);
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( !__xrtXidTextByte(Text.Data[i]) ) {
			__xrtXidFormatError(i, "XID text contains an invalid byte");
			return false;
		}
	}
	Config = __xrtXidCodec();
	if ( !xrtBase64Decode(
		Text.Data,
		Text.Size,
		Value.Data,
		sizeof(Value.Data),
		&iOutputSize,
		&Config
	) || (iOutputSize != sizeof(Value.Data)) ) {
		return false;
	}
	*pXid = Value;
	return true;
}



/* 读取完整范围内的生成时间。 */
XRT_API bool xrtXidTime(const xid* pXid, xtime* pTime)
{
	if ( (pXid == NULL) || (pTime == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	*pTime = __xrtXidLoadTime(pXid);
	return true;
}



/* 比较规范网络字节序数据。 */
XRT_API int xrtXidCompare(const xid* pLeft, const xid* pRight)
{
	int iCompare;

	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return 0;
	}
	iCompare = memcmp(pLeft->Data, pRight->Data, XID_BINARY_SIZE);
	return iCompare < 0 ? -1 : (iCompare > 0 ? 1 : 0);
}



/* 比较全部 XID 字节。 */
XRT_API bool xrtXidEqual(const xid* pLeft, const xid* pRight)
{
	if ( (pLeft == NULL) || (pRight == NULL) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	return memcmp(pLeft->Data, pRight->Data, XID_BINARY_SIZE) == 0;
}



/* 扫描固定 24 字节，不依赖结构填充。 */
XRT_API bool xrtXidIsZero(const xid* pXid)
{
	uint8 iBits = 0;

	if ( pXid == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < XID_BINARY_SIZE; i++ ) {
		iBits |= pXid->Data[i];
	}
	return iBits == 0;
}



/* 读取 XID 格式错误的稳定字节位置。 */
XRT_API bool xrtXidErrorOffset(
	const xerror* pError,
	size_t* pOffset
)
{
	cstr sData;
	unsigned long long iValue;
	char iTail;

	if ( pOffset == NULL ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( (pError == NULL) || (xrtErrorDomain(pError) == NULL) ||
		 (strcmp(xrtErrorDomain(pError), "xrt.xid") != 0) ) {
		return false;
	}
	sData = xrtErrorData(pError);
	if ( (sData == NULL) ||
		 (sscanf(sData, "offset=%llu%c", &iValue, &iTail) != 1) ||
		 (iValue > (unsigned long long)SIZE_MAX) ) {
		return false;
	}
	*pOffset = (size_t)iValue;
	return true;
}

#endif
