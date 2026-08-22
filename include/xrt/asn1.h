#ifndef XRT_ASN1_H
#define XRT_ASN1_H

#include <xrt/error.h>



#if defined(XRT_FEATURE_ASN1_DER) && !defined(XRT_FEATURE_BUFFER)
	#error "XRT_FEATURE_ASN1_DER requires XRT_FEATURE_BUFFER"
#endif



#if defined(XRT_FEATURE_ASN1_DER)

/* DER 写入接口只需要缓冲的不透明指针。 */
typedef struct xbuffer xbuffer;

/* ASN.1 标签类别使用 X.690 的两位稳定值。 */
typedef enum xasn1class {
	XASN1_UNIVERSAL = 0,
	XASN1_APPLICATION,
	XASN1_CONTEXT,
	XASN1_PRIVATE
} xasn1class;



/* X.509、PKCS 与 TLS 常用的 ASN.1 Universal 标签号。 */
typedef enum xasn1universal {
	XASN1_BOOLEAN = 1,
	XASN1_INTEGER = 2,
	XASN1_BIT_STRING = 3,
	XASN1_OCTET_STRING = 4,
	XASN1_NULL = 5,
	XASN1_OBJECT_IDENTIFIER = 6,
	XASN1_ENUMERATED = 10,
	XASN1_UTF8_STRING = 12,
	XASN1_RELATIVE_OID = 13,
	XASN1_SEQUENCE = 16,
	XASN1_SET = 17,
	XASN1_NUMERIC_STRING = 18,
	XASN1_PRINTABLE_STRING = 19,
	XASN1_TELETEX_STRING = 20,
	XASN1_IA5_STRING = 22,
	XASN1_UTC_TIME = 23,
	XASN1_GENERALIZED_TIME = 24,
	XASN1_VISIBLE_STRING = 26,
	XASN1_GENERAL_STRING = 27,
	XASN1_UNIVERSAL_STRING = 28,
	XASN1_BMP_STRING = 30
} xasn1universal;



/* DER 标签保留类别、构造位和完整的高标签号。 */
typedef struct xasn1tag {
	xasn1class Class;
	uint32 Number;
	bool Constructed;
} xasn1tag;



/* DER 值中的所有视图都借用原输入，不分配也不复制。 */
typedef struct xdervalue {
	xasn1tag Tag;
	xbytesview Raw;
	xbytesview Value;
	size_t HeaderSize;
} xdervalue;



/* DER 游标保存不可变输入和下一项偏移，可安全复制后独立遍历。 */
typedef struct xdercursor {
	cbytes Data;
	size_t Size;
	size_t Offset;
} xdercursor;



/* Read/Peek 把正常结束与协议错误分开表达。 */
typedef enum xderresult {
	XDER_ERROR = -1,
	XDER_DONE = 0,
	XDER_VALUE = 1
} xderresult;



/* ASN.1/DER 模块稳定错误码。 */
typedef enum xasn1error {
	XASN1_ERROR_TAG = 1,
	XASN1_ERROR_LENGTH,
	XASN1_ERROR_VALUE,
	XASN1_ERROR_TYPE,
	XASN1_ERROR_END,
	XASN1_ERROR_TRAILING,
	XASN1_ERROR_ORDER,
	XASN1_ERROR_DEPTH,
	XASN1_ERROR_RANGE
} xasn1error;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_ASN1_DER)

/* 初始化一个借用输入的 DER 游标。 */
XRT_API bool xrtDerInit(xdercursor* pCursor, const void* pData, size_t iSize);



/* 读取下一项并推进游标；失败时游标和输出保持不变。 */
XRT_API xderresult xrtDerRead(xdercursor* pCursor, xdervalue* pValue);



/* 查看下一项但不推进游标；失败时输出保持不变。 */
XRT_API xderresult xrtDerPeek(const xdercursor* pCursor, xdervalue* pValue);



/* 读取并要求下一项具有指定标签；标签不符时不推进游标。 */
XRT_API bool xrtDerExpect(
	xdercursor* pCursor,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xdervalue* pValue
);



/* 从一个构造值的内容初始化子游标。 */
XRT_API bool xrtDerEnter(const xdervalue* pValue, xdercursor* pCursor);



/* 返回游标是否已经恰好消费完全部内容。 */
XRT_API bool xrtDerDone(const xdercursor* pCursor);



/* 返回游标尚未消费的字节数；非法游标返回零并设置参数错误。 */
XRT_API size_t xrtDerRemaining(const xdercursor* pCursor);



/* 仅判断值的标签，不修改错误状态。 */
XRT_API bool xrtDerIs(
	const xdervalue* pValue,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed
);



/* 验证输入恰好包含一个完整、规范且最大嵌套 64 层的 DER 值。 */
XRT_API bool xrtDerValidate(const void* pData, size_t iSize);



/* 读取规范 DER BOOLEAN。 */
XRT_API bool xrtDerBoolean(const xdervalue* pValue, bool* pResult);



/* 读取非负 DER INTEGER，并返回去除可选符号零后的借用字节。 */
XRT_API bool xrtDerUnsigned(const xdervalue* pValue, xbytesview* pResult);



/* 读取不超过 64 位的非负 DER INTEGER。 */
XRT_API bool xrtDerUInt64(const xdervalue* pValue, uint64* pResult);



/* 读取不超过 64 位的有符号 DER INTEGER。 */
XRT_API bool xrtDerInt64(const xdervalue* pValue, int64* pResult);



/* 读取 DER BIT STRING，返回数据和末字节未使用位数。 */
XRT_API bool xrtDerBitString(
	const xdervalue* pValue,
	xbytesview* pResult,
	uint8* pUnusedBits
);



/* 读取 DER OCTET STRING 的借用内容。 */
XRT_API bool xrtDerOctets(const xdervalue* pValue, xbytesview* pResult);



/* 读取规范 OBJECT IDENTIFIER 的借用内容。 */
XRT_API bool xrtDerOid(const xdervalue* pValue, xbytesview* pResult);



/* 比较 DER OBJECT IDENTIFIER 与调用方提供的内容八位组。 */
XRT_API bool xrtDerOidEqual(
	const xdervalue* pValue,
	const void* pOid,
	size_t iOidSize
);



/*
	向缓冲尾部追加一个 DER TLV。Content 是借用的原始内容；
	对于构造类型，调用方负责提供已经规范编码的子项。
	失败时不会发布半个 TLV。
*/
XRT_API bool xrtDerAppend(
	xbuffer* pOutput,
	xasn1class Class,
	uint32 iNumber,
	bool bConstructed,
	xbytesview Content
);



/* 追加常用的规范 DER primitive 值。 */
XRT_API bool xrtDerAppendBoolean(xbuffer* pOutput, bool bValue);
XRT_API bool xrtDerAppendUInt64(xbuffer* pOutput, uint64 iValue);
XRT_API bool xrtDerAppendInt64(xbuffer* pOutput, int64 iValue);
XRT_API bool xrtDerAppendOctets(xbuffer* pOutput, xbytesview Content);
XRT_API bool xrtDerAppendNull(xbuffer* pOutput);
XRT_API bool xrtDerAppendBitString(
	xbuffer* pOutput,
	xbytesview Content,
	uint8 iUnusedBits
);



/*
	OID 内容与点分文本之间转换。编码/解码都向 Output 尾部追加，
	失败时不会修改 Output。
*/
XRT_API bool xrtDerOidEncode(xstrview Text, xbuffer* pOutput);
XRT_API bool xrtDerOidDecode(xbytesview Oid, xbuffer* pOutput);
XRT_API bool xrtDerAppendOid(xbuffer* pOutput, xstrview Text);

#endif



XRT_EXTERN_C_END

#endif
