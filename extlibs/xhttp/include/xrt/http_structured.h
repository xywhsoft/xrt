#ifndef XRT_HTTP_STRUCTURED_H
#define XRT_HTTP_STRUCTURED_H

#include <xrt/charset.h>
#include <xrt/codec.h>
#include <xrt/http.h>



#if defined(XHTTP_FEATURE_HTTP_STRUCTURED) && \
	(!defined(XRT_FEATURE_HTTP) || \
	 !defined(XRT_FEATURE_CODEC_BASE64) || \
	 !defined(XRT_FEATURE_UNICODE))
	#error "XRT HTTP Structured Fields requires HTTP, Base64 and Unicode support"
#endif

#if defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE) && \
	!defined(XHTTP_FEATURE_HTTP_STRUCTURED)
	#error "XRT HTTP Structured Fields writer requires parser support"
#endif



#if defined(XHTTP_FEATURE_HTTP_STRUCTURED)

/* RFC 9651 裸值类型；Decimal 的 Number 使用千分之一为单位。 */
typedef enum xhttpstructuredtype {
	XHTTP_STRUCTURED_INTEGER = 1,
	XHTTP_STRUCTURED_DECIMAL,
	XHTTP_STRUCTURED_STRING,
	XHTTP_STRUCTURED_TOKEN,
	XHTTP_STRUCTURED_BYTES,
	XHTTP_STRUCTURED_BOOLEAN,
	XHTTP_STRUCTURED_DATE,
	XHTTP_STRUCTURED_DISPLAY
} xhttpstructuredtype;



/*
	解析值借用线路文本。
	String、Byte Sequence 与 Display String 的 Encoded 不含外层定界符，
	Token 的 Encoded 是原 token；数值、布尔与日期不使用 Encoded。
*/
typedef struct xhttpstructuredbare {
	xhttpstructuredtype Type;
	int64 Number;
	xstrview Encoded;
} xhttpstructuredbare;



/* Item 由一个裸值和原始分号参数区组成。 */
typedef struct xhttpstructureditem {
	xhttpstructuredbare Bare;
	xstrview Parameters;
} xhttpstructureditem;



/* List 与 Dictionary 的成员可以是 Item 或 Inner List。 */
typedef enum xhttpstructuredmemberkind {
	XHTTP_STRUCTURED_MEMBER_ITEM = 1,
	XHTTP_STRUCTURED_MEMBER_INNER_LIST
} xhttpstructuredmemberkind;



/* Inner 借用括号内文本；Parameters 对 Item 和 Inner List 都有效。 */
typedef struct xhttpstructuredmember {
	xhttpstructuredmemberkind Kind;
	xhttpstructuredbare Bare;
	xstrview Inner;
	xstrview Parameters;
} xhttpstructuredmember;



/* 参数值省略时按 Boolean true 发布。 */
typedef struct xhttpstructuredparameter {
	xstrview Key;
	xhttpstructuredbare Value;
} xhttpstructuredparameter;



/* Dictionary 成员保留 key 和统一成员表示。 */
typedef struct xhttpstructureddictionarymember {
	xstrview Key;
	xhttpstructuredmember Member;
} xhttpstructureddictionarymember;



/* 重复字段游标绑定首次迭代的字段数组、字段名和顶层类型。 */
typedef struct xhttpstructuredfieldcursor {
	const void* Source;
	const void* Name;
	size_t SourceSize;
	size_t NameSize;
	size_t Field;
	size_t Offset;
	uint8 State;
} xhttpstructuredfieldcursor;



/* 有序 Map 游标绑定首次迭代的单值或重复字段来源。 */
typedef struct xhttpstructuredmapcursor {
	const void* Source;
	const void* Name;
	size_t SourceSize;
	size_t NameSize;
	size_t Field;
	size_t Offset;
	size_t Order;
	uint8 State;
} xhttpstructuredmapcursor;



XRT_EXTERN_C_BEGIN



/* 验证 Structured Fields key 的小写 ASCII 语法。 */
XRT_API bool xrtHttpStructuredKeyValid(xstrview Key);



/* 验证 Structured Fields token 语法。 */
XRT_API bool xrtHttpStructuredTokenValid(xstrview Token);



/* 从当前位置解析一个裸值，不跳过空白且不要求消耗完整输入。 */
XRT_API xhttpnext xrtHttpStructuredBareNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredbare* pBare
);



/* 严格解析完整 Item；顶层两侧只允许 SP。 */
XRT_API bool xrtHttpStructuredItemParse(
	xstrview Value,
	xhttpstructureditem* pItem
);



/* 迭代 Item 或 Inner List 的原始参数区。 */
XRT_API xhttpnext xrtHttpStructuredParameterNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpstructuredparameter* pParameter
);



/* 返回去重后的参数数量；格式错误返回 XRT_NPOS。 */
XRT_API size_t xrtHttpStructuredParameterCount(xstrview Parameters);



/* 按首次出现顺序读取参数，重复 key 的值取最后一次。 */
XRT_API xhttpnext xrtHttpStructuredParameterAt(
	xstrview Parameters,
	size_t iIndex,
	xhttpstructuredparameter* pParameter
);



/* 按 key 读取最后一次参数值。 */
XRT_API xhttpnext xrtHttpStructuredParameterFind(
	xstrview Parameters,
	xstrview Key,
	xhttpstructuredparameter* pParameter
);



/* 迭代完整 Inner List；游标从零开始，首次调用先验证全文，期间输入不可变。 */
XRT_API xhttpnext xrtHttpStructuredInnerNext(
	xstrview Inner,
	size_t* pOffset,
	xhttpstructureditem* pItem
);



/* 严格验证完整 Inner List 内容。 */
XRT_API bool xrtHttpStructuredInnerValid(xstrview Inner);



/* 迭代完整 List；游标从零开始，首次调用先验证全文，期间输入不可变。 */
XRT_API xhttpnext xrtHttpStructuredListNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructuredmember* pMember
);



/* 严格验证完整 Structured List。 */
XRT_API bool xrtHttpStructuredListValid(xstrview Value);



/* 迭代完整 Dictionary；游标从零开始，首次调用先验证全文，期间输入不可变。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryNext(
	xstrview Value,
	size_t* pOffset,
	xhttpstructureddictionarymember* pMember
);



/* 严格验证完整 Structured Dictionary。 */
XRT_API bool xrtHttpStructuredDictionaryValid(xstrview Value);



/* 初始化单值或重复字段 Dictionary 有序 Map 游标。 */
XRT_API void xrtHttpStructuredMapCursorInit(
	xhttpstructuredmapcursor* pCursor
);



/* 迭代有序 Map；key 按首次出现顺序，值取最后一次出现。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryMapNext(
	xstrview Value,
	xhttpstructuredmapcursor* pCursor,
	xhttpstructureddictionarymember* pMember
);



/* 返回去重后的 Dictionary 成员数量；格式错误返回 XRT_NPOS。 */
XRT_API size_t xrtHttpStructuredDictionaryCount(xstrview Value);



/* 按首次出现顺序读取成员，重复 key 的值取最后一次。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryAt(
	xstrview Value,
	size_t iIndex,
	xhttpstructureddictionarymember* pMember
);



/* 按 key 读取 Dictionary 中最后一次成员值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFind(
	xstrview Value,
	xstrview Key,
	xhttpstructureddictionarymember* pMember
);



/* 初始化来源绑定的重复字段行游标。 */
XRT_API void xrtHttpStructuredFieldCursorInit(
	xhttpstructuredfieldcursor* pCursor
);



/* 按线路顺序跨重复同名字段行迭代；期间字段数组、字段名和内容不可变。 */
XRT_API xhttpnext xrtHttpStructuredListFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredfieldcursor* pCursor,
	xhttpstructuredmember* pMember
);



/* 按线路顺序跨重复同名字段行迭代；游标不可切换来源或顶层类型。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredfieldcursor* pCursor,
	xhttpstructureddictionarymember* pMember
);



/* 跨重复字段行迭代有序 Map，重复 key 采用最后一次值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryMapFieldNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructuredmapcursor* pCursor,
	xhttpstructureddictionarymember* pMember
);



/* 返回重复字段行逻辑组合并去重后的 Dictionary 成员数。 */
XRT_API size_t xrtHttpStructuredDictionaryFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);



/* 按首次出现顺序读取跨字段 Dictionary，重复 key 取最后值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldAt(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iIndex,
	xhttpstructureddictionarymember* pMember
);



/* 按 key 读取跨字段 Dictionary 中最后一次成员值。 */
XRT_API xhttpnext xrtHttpStructuredDictionaryFieldFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Key,
	xhttpstructureddictionarymember* pMember
);



/* 解析唯一同名字段行中的 Structured Item；缺失返回 END。 */
XRT_API xhttpnext xrtHttpStructuredItemField(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpstructureditem* pItem
);



/* 解码 String；空输出查询长度，允许与 Encoded 同址，长度输出可未对齐。 */
XRT_API bool xrtHttpStructuredStringDecode(
	const xhttpstructuredbare* pBare,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 解码 Byte Sequence；空输出查询长度，允许与 Encoded 同址，长度可未对齐。 */
XRT_API bool xrtHttpStructuredBytesDecode(
	const xhttpstructuredbare* pBare,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 解码并校验 Display UTF-8；允许与 Encoded 同址，长度输出可未对齐。 */
XRT_API bool xrtHttpStructuredDisplayDecode(
	const xhttpstructuredbare* pBare,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif



#if defined(XHTTP_FEATURE_HTTP_STRUCTURED_WRITE)

/*
	待序列化裸值使用抽象内容。
	String、Token、Display 是未编码文本，Bytes 的 Data 可以包含任意字节。
*/
typedef struct xhttpstructuredvalue {
	xhttpstructuredtype Type;
	int64 Number;
	xstrview Data;
} xhttpstructuredvalue;



/* 待序列化参数；Parameters 数组必须是 key 唯一的有序 map。 */
typedef struct xhttpstructuredparameterentry {
	xstrview Key;
	xhttpstructuredvalue Value;
} xhttpstructuredparameterentry;



/* 待序列化 Item。 */
typedef struct xhttpstructureditemvalue {
	xhttpstructuredvalue Bare;
	const xhttpstructuredparameterentry* Parameters;
	size_t ParameterCount;
} xhttpstructureditemvalue;



/*
	Item 成员使用 Item；Inner List 成员使用 Inner、InnerCount 和外层 Parameters。
	未被 Kind 选中的字段必须为空，避免描述符携带含义不明的数据。
*/
typedef struct xhttpstructuredmembervalue {
	xhttpstructuredmemberkind Kind;
	xhttpstructureditemvalue Item;
	const xhttpstructureditemvalue* Inner;
	size_t InnerCount;
	const xhttpstructuredparameterentry* Parameters;
	size_t ParameterCount;
} xhttpstructuredmembervalue;



/* 待序列化 Dictionary 有序 map 成员。 */
typedef struct xhttpstructureddictionaryentry {
	xstrview Key;
	xhttpstructuredmembervalue Member;
} xhttpstructureddictionaryentry;



XRT_EXTERN_C_BEGIN



/* 以下写出入口支持空输出查询长度，且长度输出允许未对齐存储。 */



/* 规范序列化一个抽象裸值；输出不附加零字节。 */
XRT_API bool xrtHttpStructuredBareWrite(
	const xhttpstructuredvalue* pValue,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范序列化一个 Item 及其参数；输出不附加零字节。 */
XRT_API bool xrtHttpStructuredItemWrite(
	const xhttpstructureditemvalue* pItem,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范序列化完整 List；空数组产生长度为零的省略值。 */
XRT_API bool xrtHttpStructuredListWrite(
	const xhttpstructuredmembervalue* pMembers,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 规范序列化完整 Dictionary；key 必须唯一。 */
XRT_API bool xrtHttpStructuredDictionaryWrite(
	const xhttpstructureddictionaryentry* pEntries,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



XRT_EXTERN_C_END

#endif

#endif
