#ifndef XRT_INTERNAL_HTTP_H
#define XRT_INTERNAL_HTTP_H

#include "xrt_internal.h"

#include <xrt/http1.h>
#include <xrt/multipart.h>

#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)
	#include <xrt/http_auth.h>
#endif



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



/* 验证 Header 查询、删除和空 trailer 查询使用的非空字段名。 */
bool __xrtHttpLookupNameValid(xstrview Name);



#if defined(XRT_FEATURE_HTTP_EXT_VALUE) || \
	defined(XRT_FEATURE_HTTP_LANGUAGE)

/* 验证 RFC 4647 basic range 使用的语言标签分段边界。 */
bool __xrtHttpLanguageTextValid(
	xstrview Text,
	bool bRange,
	bool bAllowEmpty,
	size_t* pSubtags
);

#endif



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



#if defined(XRT_FEATURE_HTTP_HEADERS)

/* 判断一段内存是否覆盖动态 Header 容器拥有的任一分配区。 */
bool __xrtHttpHeadersOwnedOverlap(
	const xhttpheaders* pHeaders,
	const void* pMemory,
	size_t iSize
);



/* 验证已经复制到自然对齐存储的 Header 容器配置。 */
bool __xrtHttpHeadersConfigValid(
	const xhttpheadersconfig* pConfig
);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST_SESSION)

/* 校验写出区不会覆盖 Digest 会话及可选 Exchange 的保留状态。 */
bool __xrtHttpDigestSessionOutputValid(
	const xhttpdigestsession* pSession,
	const xhttpdigestexchange* pExchange,
	const void* pOutput,
	size_t iSize
);

#endif



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



/*
	跨重复同名字段读取 1#token；字段缺失返回 END，字段存在但无条目返回 ERROR。
	游标模式在第一次调用时绑定，不能与允许空列表的公开迭代器混用。
*/
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



#if defined(XRT_FEATURE_HTTP_AUTH)

/* 认证值和敏感凭据的常见路径使用固定栈缓冲，超出后才回退到堆。 */
#define XRT_HTTP_AUTH_LOCAL_BYTES 256u



/* 内部认证写出回调支持长度查询和精确缓冲写入。 */
typedef bool (*__xrtHttpAuthWriteFunction)(
	const void* pContext,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 内部认证消费回调必须在返回前复制需要保留的值。 */
typedef bool (*__xrtHttpAuthConsumeFunction)(
	void* pContext,
	xstrview Value
);



/* 用临时缓冲写出认证值、同步消费，并在所有路径清零。 */
bool __xrtHttpAuthWriteTemporary(
	__xrtHttpAuthWriteFunction pWrite,
	const void* pWriteContext,
	__xrtHttpAuthConsumeFunction pConsume,
	void* pConsumeContext
);

#endif



/* 判断一个字节是否可直接出现在 quoted-string 正文中。 */
bool __xrtHttpQuotedTextByte(unsigned char iByte);



/* 判断一个字节是否可出现在 quoted-pair 的反斜线之后。 */
bool __xrtHttpQuotedPairByte(unsigned char iByte);



#if defined(XRT_FEATURE_HTTP_PARAM)

/* 读取通用 name/value 列表；调用模块决定分隔符和是否忽略空成员。 */
xhttpnext __xrtHttpNameValueNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam,
	char iSeparator,
	bool bIgnoreEmpty
);



/* 双遍参数 writer 在测量阶段使用空 Output，在写入阶段使用已确认容量的缓冲。 */
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



/* 读取已经验证的参数值的下一个语义字节，quoted-pair 只返回转义后的字节。 */
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



#if defined(XRT_FEATURE_MIME)

/* MIME 参数扫描回调；参数视图只在源文本保持不变期间有效。 */
typedef bool (*xrt_mime_param_visitor)(
	const xhttpparam* pParam,
	void* pContext
);



/* 安全累加 MIME 线路长度。 */
bool __xrtMimeSizeAdd(size_t* pSize, size_t iAdd);



/* 严格验证非空 MIME 参数列表，并拒绝重复参数名称。 */
bool __xrtMimeParametersValid(xstrview Parameters);



/* 严格扫描非空 MIME 参数列表，并在验证每个唯一参数后调用 visitor。 */
bool __xrtMimeParametersInspect(
	xstrview Parameters,
	xrt_mime_param_visitor Visitor,
	void* pContext
);



/* 按 MIME Sniff 规则静默解析媒体类型 essence，并忽略参数尾。 */
bool __xrtMimeSniffTypeParse(
	xstrview Text,
	xmediatype* pType
);



/* 判断输出范围是否覆盖任意借用视图。 */
bool __xrtMimeOutputOverlap(
	const xstrview* pViews,
	size_t iCount,
	const void* pOutput,
	size_t iSize
);



/* 为同构 Writer 分配零结尾结果。 */
str __xrtMimeBuild(
	const void* pValue,
	bool (*Write)(const void*, void*, size_t, size_t*),
	size_t* pSize
);



/* 验证已解析媒体类型结构与严格参数列表。 */
bool __xrtHttpMediaTypeValid(const xmediatype* pType);

#endif



#if defined(XRT_FEATURE_HTTP_AUTH_DIGEST)

/* 把单个十六进制字节转换为数值，无效字节返回负数。 */
int __xrtHttpDigestHexValue(uint8 iByte);



/* 验证线路参数中未转义的非空十六进制值，并返回字节数。 */
bool __xrtHttpDigestHexParamValid(
	const xhttpparam* pParam,
	size_t* pSize
);



/* 验证借用视图是非空十六进制文本。 */
bool __xrtHttpDigestHexViewValid(xstrview Text);



/* 严格读取恰好八位且非 quoted 的 Digest nonce count。 */
bool __xrtHttpDigestNonceCountRead(
	const xhttpparam* pParam,
	uint32* pCount
);



/* 把 nonce count 规范写为八位小写十六进制。 */
void __xrtHttpDigestNonceCountWrite(
	uint32 iCount,
	char sOutput[8]
);



/* 以 quoted-string 形式写出规范小写十六进制参数。 */
bool __xrtHttpDigestWriterHex(
	xrt_http_param_writer* pWriter,
	xstrview Name,
	xstrview Value,
	bool bFirst
);

#endif



#if defined(XRT_FEATURE_HTTP_EXT_VALUE)

/* 不设置错误地拆分并验证 RFC 8187 扩展值，供有语义回退的上层使用。 */
bool __xrtHttpExtValueSplit(
	xstrview Text,
	xhttpextvalue* pValue
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



#if defined(XRT_FEATURE_HTTP_HOST)

/* 以 ASCII 不区分大小写规则比较两个 Host。 */
bool __xrtHttpHostEqual(xstrview Left, xstrview Right);

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



/* 验证 request-target 的 HTTP/1 线路字符边界。 */
bool __xrtHttp1TargetValid(xstrview Target);



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



/* 校验 HTTP/1 封包查询、容量以及输出与借用输入不重叠的约束。 */
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



#if defined(XRT_FEATURE_HTTP1_BODY)

/* 使用 Body Reader 的同一套语法检查原样 chunk 扩展后缀。 */
bool __xrtHttp1ChunkExtensionsValid(xstrview Extensions);

#endif



#if defined(XRT_FEATURE_MULTIPART)

/* 内部分隔行结果额外区分流输入尚未完整。 */
typedef enum xrt_multipart_delimiter {
	XRT_MULTIPART_DELIMITER_INVALID = 0,
	XRT_MULTIPART_DELIMITER_PART,
	XRT_MULTIPART_DELIMITER_CLOSE,
	XRT_MULTIPART_DELIMITER_MORE
} xrt_multipart_delimiter;



/* 验证拥有型 boundary 结构没有被调用方破坏。 */
bool __xrtMultipartBoundaryValid(
	const xmultipartboundary* pBoundary
);



/* 验证借用字节视图的空值一致性。 */
bool __xrtMultipartBytesValid(xbytesview Bytes);



/* 按统一 multipart 限额验证完整正文，并可为 FormData 接受零 Part。 */
bool __xrtMultipartValidate(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	const xmultipartlimits* pLimits,
	bool bAllowEmpty,
	size_t* pPartCount,
	xmultiparterrorinfo* pError
);



/* 解析一条 boundary delimiter；bEnd 表示输入在该位置后可靠结束。 */
xrt_multipart_delimiter __xrtMultipartDelimiterAt(
	xbytesview Body,
	const xmultipartboundary* pBoundary,
	size_t iDash,
	bool bEnd,
	size_t* pNext
);



/* 定位 Part Header 与正文之间的空行。 */
bool __xrtMultipartHeaderBlock(
	xbytesview Body,
	size_t iStart,
	xstrview* pHeaders,
	size_t* pBodyStart
);



/* 解析 Part Header，并拒绝语义歧义的重复专用字段。 */
bool __xrtMultipartPartHeaders(
	xmultipartpart* pPart,
	xmultiparterrorinfo* pError,
	size_t iBase
);

#endif

#endif
