#ifndef XRT_INTERNAL_HTTP_H
#define XRT_INTERNAL_HTTP_H

#include "xrt_internal.h"

#include <xrt/http1.h>



#if defined(XRT_FEATURE_HTTP)

/* 单行字段解析失败位置用于协议层生成精确的名称或值错误。 */
typedef enum xrt_http_field_error {
	XRT_HTTP_FIELD_VALID = 0,
	XRT_HTTP_FIELD_NAME,
	XRT_HTTP_FIELD_VALUE
} xrt_http_field_error;



/* Content-Length 内部结果额外保留重复值冲突分类。 */
typedef enum xrt_http_content_length {
	XRT_HTTP_CONTENT_LENGTH_INVALID = 0,
	XRT_HTTP_CONTENT_LENGTH_VALID,
	XRT_HTTP_CONTENT_LENGTH_CONFLICT
} xrt_http_content_length;



/* 验证借用字符串视图是一段不会发生地址回绕的连续内存。 */
bool __xrtHttpViewValid(xstrview Text);



/* 验证字段查询使用的非空字段名。 */
bool __xrtHttpLookupNameValid(xstrview Name);



/* 安全累加 HTTP 编解码所需的字节数。 */
bool __xrtHttpSizeAdd(size_t* pSize, size_t iAdd);



/* 验证字段描述符数组及其借用视图的基础内存边界。 */
bool __xrtHttpFieldArrayValid(
	const xhttpfield* pFields,
	size_t iCount
);



/* 从已经验证的字段数组复制一个描述符，兼容未对齐数组存储。 */
void __xrtHttpFieldLoad(
	const xhttpfield* pFields,
	size_t iIndex,
	xhttpfield* pField
);



/* 判断一段内存是否覆盖字段描述符数组或任一借用文本。 */
bool __xrtHttpFieldArrayOverlap(
	const xhttpfield* pFields,
	size_t iCount,
	const void* pMemory,
	size_t iSize
);



/* 完整校验字段数组并测量线路长度。 */
bool __xrtHttpFieldWriteMeasure(
	const xhttpfield* pFields,
	size_t iCount,
	bool bFinalLine,
	size_t* pRequired
);



/* 向已经确认容量的缓冲区写出字段数组。 */
size_t __xrtHttpFieldWriteUnchecked(
	const xhttpfield* pFields,
	size_t iCount,
	bool bFinalLine,
	bytes pOutput
);



/* 按字节精确比较两个借用字符串视图。 */
bool __xrtHttpViewEqual(
	xstrview Left,
	xstrview Right
);



/* 按 ASCII 规则把大写字母转换为小写。 */
unsigned char __xrtHttpAsciiLower(unsigned char iByte);



/* 返回 uint64 十进制线路文本的精确字节数。 */
size_t __xrtHttpUInt64Size(uint64 iValue);



/* 向至少 20 字节的输出区写入 uint64 十进制文本并返回实际长度。 */
size_t __xrtHttpUInt64Write(char* sOutput, uint64 iValue);



/* 判断一个字节是否属于 RFC tchar 集合。 */
bool __xrtHttpTokenByte(unsigned char iByte);



/* 跨重复同名字段读取必须非空的 1#token。 */
xhttpnext __xrtHttpFieldTokenNextRequired(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
);



/* 读取会跳过 quoted-string 内逗号的通用 HTTP 列表成员。 */
xhttpnext __xrtHttpQuotedListNext(
	xstrview Value,
	size_t iOffset,
	size_t* pNext,
	xstrview* pElement
);



/* 判断一个字节是否可直接出现在 quoted-string 正文中。 */
bool __xrtHttpQuotedTextByte(unsigned char iByte);



/* 判断一个字节是否可出现在 quoted-pair 的反斜线之后。 */
bool __xrtHttpQuotedPairByte(unsigned char iByte);



#if defined(XRT_FEATURE_HTTP_HOST)

/* 返回 Host 语法使用的 ASCII 十六进制值。 */
int __xrtHttpAuthorityHex(unsigned char iByte);



/* 判断字节是否可直接出现在 RFC 3986 reg-name 中。 */
bool __xrtHttpAuthorityNameByte(unsigned char iByte);



/* 无错误副作用地验证严格 IPv4 文本。 */
bool __xrtHttpAuthorityIpv4(xstrview Text);



/* 无错误副作用地验证严格 IPv6 文本。 */
bool __xrtHttpAuthorityIpv6(xstrview Text);

#endif



#if defined(XRT_FEATURE_HTTP_PARAM)

/* 读取通用 name/value 列表；调用模块决定分隔符和空成员策略。 */
xhttpnext __xrtHttpNameValueNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam,
	char iSeparator,
	bool bIgnoreEmpty
);



/* 双遍参数 writer 在测量阶段使用空 Output。 */
typedef struct xrt_http_param_writer {
	bytes Output;
	size_t Size;
} xrt_http_param_writer;



/* 把已经验证的值写为 quoted-string，并返回线路字节数。 */
size_t __xrtHttpQuotedWriteUnchecked(
	xstrview Value,
	bytes pOutput
);



/* 解码已经验证的参数值，并返回语义字节数。 */
size_t __xrtHttpParamValueWriteUnchecked(
	const xhttpparam* pParam,
	bytes pOutput
);



/* 读取已经验证的参数值的下一个语义字节。 */
bool __xrtHttpParamSemanticNext(
	const xhttpparam* pParam,
	size_t* pOffset,
	uint8* pByte
);



/* 向双遍参数 writer 追加固定字节。 */
bool __xrtHttpParamWriterBytes(
	xrt_http_param_writer* pWriter,
	const void* pData,
	size_t iSize
);



/* 向双遍参数 writer 追加参数分隔符、名称和等号。 */
bool __xrtHttpParamWriterName(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	bool bFirst
);



/* 向双遍参数 writer 追加 quoted-string 参数。 */
bool __xrtHttpParamWriterQuoted(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
);



/* 向双遍参数 writer 追加已经验证的 token 参数。 */
bool __xrtHttpParamWriterToken(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
);

#endif



/* 解析一行不含 CRLF 的字段，并区分名称和字段值错误。 */
xrt_http_field_error __xrtHttpFieldParse(
	xstrview Line,
	xhttpfield* pField
);



/* 解析单个或逗号分隔且必须完全相同的 Content-Length。 */
xrt_http_content_length __xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
);

#endif



#if defined(XRT_FEATURE_HTTP1_HEAD)

/* 设置 HTTP/1 协议错误，并同步发布精确字节与行位置。 */
xhttp1status __xrtHttp1Fail(
	xhttp1head* pHead,
	xhttp1errorinfo* pInfo,
	xhttp1error Code,
	size_t iOffset,
	size_t iLine,
	xerrkind Kind,
	cstr sOperation,
	cstr sMessage
);



/* 返回指定版本在线路上的固定文本，未知版本返回空指针。 */
cstr __xrtHttp1VersionText(xhttpversion Version);



/* 安全累加 HTTP/1 封包长度。 */
bool __xrtHttp1SizeAdd(size_t* pSize, size_t iAdd);



/* 校验字段数组并累计字段区和最终空行的长度。 */
bool __xrtHttp1WriteMeasure(
	const xhttpfield* pFields,
	size_t iFieldCount,
	size_t iBase,
	size_t* pRequired,
	cstr sOperation
);



/* 在容量已经确认后写入字段区和最终空行。 */
size_t __xrtHttp1FieldsWrite(
	bytes pOutput,
	size_t iPosition,
	const xhttpfield* pFields,
	size_t iFieldCount
);



/* 校验 HTTP/1 封包查询、容量以及输出与输入不重叠的约束。 */
bool __xrtHttp1WriteOutputValid(
	const xstrview* pParts,
	size_t iPartCount,
	const xhttpfield* pFields,
	size_t iFieldCount,
	void* pOutput,
	size_t iCapacity,
	size_t iRequired,
	size_t* pSize,
	cstr sOperation
);

#endif



#if defined(XRT_FEATURE_HTTP1_NET)

typedef xhttp1status (*__xrt_http1_net_parse_proc)(
	xbytesview Input,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError
);



typedef bool (*__xrt_http1_net_pullup_proc)(
	ptr pContext,
	size_t iSize,
	xnetspan* pSpan
);



/* 共享 TCP 与 TLS 的 Header 边界扫描和最小连续化策略。 */
xhttp1status __xrtHttp1NetParse(
	const xnetbuf* pBuffer,
	xhttp1head* pHead,
	const xhttp1limits* pLimits,
	xhttp1errorinfo* pError,
	__xrt_http1_net_parse_proc pParse,
	__xrt_http1_net_pullup_proc pPullup,
	ptr pContext
);

#endif



#if defined(XRT_FEATURE_HTTP1_BODY)

/* 使用 Body Reader 的同一套语法检查原样 chunk 扩展后缀。 */
bool __xrtHttp1ChunkExtensionsValid(xstrview Extensions);

#endif

#endif
