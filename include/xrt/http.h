#ifndef XRT_HTTP_H
#define XRT_HTTP_H

#include <xrt/core.h>



#if defined(XRT_FEATURE_HTTP_PARAM) && !defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP parameter support requires XRT_FEATURE_HTTP"
#endif

#if defined(XRT_FEATURE_HTTP_HOST) && !defined(XRT_FEATURE_HTTP)
	#error "XRT HTTP Host support requires HTTP support"
#endif

#if defined(XRT_FEATURE_HTTP_PARAM_HOST) && \
	(!defined(XRT_FEATURE_HTTP_PARAM) || \
	 !defined(XRT_FEATURE_HTTP_HOST))
	#error "XRT HTTP parameter Host support requires parameter and Host support"
#endif

#if defined(XRT_FEATURE_HTTP_TARGET) && \
	(!defined(XRT_FEATURE_HTTP) || !defined(XRT_FEATURE_HTTP_HOST))
	#error "XRT HTTP target support requires HTTP and Host support"
#endif



#if defined(XRT_FEATURE_HTTP)

#define XHTTP_QUALITY_MAX 1000u



/*
	常用 HTTP 方法使用稳定枚举供解析热路径和上层分派直接比较。
	OTHER 表示语法合法但未内置分类的方法；INVALID 表示空值或非法 token。
*/
typedef enum xhttpmethod {
	XHTTP_METHOD_INVALID = 0,
	XHTTP_METHOD_OTHER = 1,
	XHTTP_METHOD_GET = 2,
	XHTTP_METHOD_HEAD = 3,
	XHTTP_METHOD_POST = 4,
	XHTTP_METHOD_PUT = 5,
	XHTTP_METHOD_DELETE = 6,
	XHTTP_METHOD_CONNECT = 7,
	XHTTP_METHOD_OPTIONS = 8,
	XHTTP_METHOD_TRACE = 9,
	XHTTP_METHOD_PATCH = 10
} xhttpmethod;



/* HTTP 版本使用可直接比较的主次版本编码。 */
typedef enum xhttpversion {
	XHTTP_VERSION_1_0 = 10,
	XHTTP_VERSION_1_1 = 11
} xhttpversion;



/*
	HTTP 状态常量只收录 IANA 已正式分配的通用状态。
	未分配、临时分配和明确标记为 Unused 的数值仍可直接使用 uint16 表达。
*/
typedef enum xhttpstatus {
	/* 1xx：信息响应。 */
	XHTTP_STATUS_CONTINUE = 100,
	XHTTP_STATUS_SWITCHING_PROTOCOLS = 101,
	XHTTP_STATUS_PROCESSING = 102,
	XHTTP_STATUS_EARLY_HINTS = 103,

	/* 2xx：成功响应。 */
	XHTTP_STATUS_OK = 200,
	XHTTP_STATUS_CREATED = 201,
	XHTTP_STATUS_ACCEPTED = 202,
	XHTTP_STATUS_NON_AUTHORITATIVE_INFORMATION = 203,
	XHTTP_STATUS_NO_CONTENT = 204,
	XHTTP_STATUS_RESET_CONTENT = 205,
	XHTTP_STATUS_PARTIAL_CONTENT = 206,
	XHTTP_STATUS_MULTI_STATUS = 207,
	XHTTP_STATUS_ALREADY_REPORTED = 208,
	XHTTP_STATUS_IM_USED = 226,

	/* 3xx：重定向响应。 */
	XHTTP_STATUS_MULTIPLE_CHOICES = 300,
	XHTTP_STATUS_MOVED_PERMANENTLY = 301,
	XHTTP_STATUS_FOUND = 302,
	XHTTP_STATUS_SEE_OTHER = 303,
	XHTTP_STATUS_NOT_MODIFIED = 304,
	XHTTP_STATUS_USE_PROXY = 305,
	XHTTP_STATUS_TEMPORARY_REDIRECT = 307,
	XHTTP_STATUS_PERMANENT_REDIRECT = 308,

	/* 4xx：客户端错误响应。 */
	XHTTP_STATUS_BAD_REQUEST = 400,
	XHTTP_STATUS_UNAUTHORIZED = 401,
	XHTTP_STATUS_PAYMENT_REQUIRED = 402,
	XHTTP_STATUS_FORBIDDEN = 403,
	XHTTP_STATUS_NOT_FOUND = 404,
	XHTTP_STATUS_METHOD_NOT_ALLOWED = 405,
	XHTTP_STATUS_NOT_ACCEPTABLE = 406,
	XHTTP_STATUS_PROXY_AUTHENTICATION_REQUIRED = 407,
	XHTTP_STATUS_REQUEST_TIMEOUT = 408,
	XHTTP_STATUS_CONFLICT = 409,
	XHTTP_STATUS_GONE = 410,
	XHTTP_STATUS_LENGTH_REQUIRED = 411,
	XHTTP_STATUS_PRECONDITION_FAILED = 412,
	XHTTP_STATUS_CONTENT_TOO_LARGE = 413,
	XHTTP_STATUS_URI_TOO_LONG = 414,
	XHTTP_STATUS_UNSUPPORTED_MEDIA_TYPE = 415,
	XHTTP_STATUS_RANGE_NOT_SATISFIABLE = 416,
	XHTTP_STATUS_EXPECTATION_FAILED = 417,
	XHTTP_STATUS_MISDIRECTED_REQUEST = 421,
	XHTTP_STATUS_UNPROCESSABLE_CONTENT = 422,
	XHTTP_STATUS_LOCKED = 423,
	XHTTP_STATUS_FAILED_DEPENDENCY = 424,
	XHTTP_STATUS_TOO_EARLY = 425,
	XHTTP_STATUS_UPGRADE_REQUIRED = 426,
	XHTTP_STATUS_PRECONDITION_REQUIRED = 428,
	XHTTP_STATUS_TOO_MANY_REQUESTS = 429,
	XHTTP_STATUS_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
	XHTTP_STATUS_UNAVAILABLE_FOR_LEGAL_REASONS = 451,

	/* 5xx：服务器错误响应。 */
	XHTTP_STATUS_INTERNAL_SERVER_ERROR = 500,
	XHTTP_STATUS_NOT_IMPLEMENTED = 501,
	XHTTP_STATUS_BAD_GATEWAY = 502,
	XHTTP_STATUS_SERVICE_UNAVAILABLE = 503,
	XHTTP_STATUS_GATEWAY_TIMEOUT = 504,
	XHTTP_STATUS_HTTP_VERSION_NOT_SUPPORTED = 505,
	XHTTP_STATUS_VARIANT_ALSO_NEGOTIATES = 506,
	XHTTP_STATUS_INSUFFICIENT_STORAGE = 507,
	XHTTP_STATUS_LOOP_DETECTED = 508,
	XHTTP_STATUS_NOT_EXTENDED = 510,
	XHTTP_STATUS_NETWORK_AUTHENTICATION_REQUIRED = 511
} xhttpstatus;



/* 字段名称和值都是借用视图，不要求零结尾。 */
typedef struct xhttpfield {
	xstrview Name;
	xstrview Value;
} xhttpfield;



/* HTTP 值迭代结果明确区分条目、正常结束和语法错误。 */
typedef enum xhttpnext {
	XHTTP_NEXT_ERROR = -1,
	XHTTP_NEXT_END = 0,
	XHTTP_NEXT_ITEM = 1
} xhttpnext;



/* 重复同名 token-list 字段游标由初始化函数建立，调用方不得直接修改。 */
typedef struct xhttpfieldtokencursor {
	const void* Source;
	xstrview Name;
	size_t Count;
	size_t Field;
	size_t Offset;
	uint8 Validated;
	uint8 Required;
} xhttpfieldtokencursor;



/* 加权 token 借用原字段值，Quality 使用 0 到 1000 的无浮点定点值。 */
typedef struct xhttpweightedtoken {
	xstrview Token;
	uint16 Quality;
} xhttpweightedtoken;

#endif



#if defined(XRT_FEATURE_HTTP_HOST)

/* Authority 包含显式端口分隔符。 */
#define XHTTP_AUTHORITY_HAS_PORT UINT32_C(0x00000001)

/* Host 是 IPv6 或 IPvFuture 字面地址，Host 视图不包含方括号。 */
#define XHTTP_AUTHORITY_IP_LITERAL UINT32_C(0x00000002)

/* 显式端口只有冒号而没有数字。 */
#define XHTTP_AUTHORITY_PORT_EMPTY UINT32_C(0x00000004)

/* Port 保存可由 uint16 无损表达的显式端口。 */
#define XHTTP_AUTHORITY_PORT_VALUE UINT32_C(0x00000008)



/* HTTP authority 借用原始文本，不接受 userinfo。 */
typedef struct xhttpauthority {
	uint32 Flags;
	uint16 Port;
	xstrview Text;
	xstrview Host;
	xstrview PortText;
} xhttpauthority;

#endif



#if defined(XRT_FEATURE_HTTP_TARGET)

/* Request-target 形式由方法与线路文本共同决定。 */
typedef enum xhttptargetform {
	XHTTP_TARGET_ORIGIN = 1,
	XHTTP_TARGET_ABSOLUTE,
	XHTTP_TARGET_AUTHORITY,
	XHTTP_TARGET_ASTERISK
} xhttptargetform;



/* Target 包含 scheme。 */
#define XHTTP_TARGET_HAS_SCHEME UINT32_C(0x00000001)

/* Target 包含双斜杠引入的 authority。 */
#define XHTTP_TARGET_HAS_AUTHORITY UINT32_C(0x00000002)

/* Target 包含问号引入的 query，包括显式空 query。 */
#define XHTTP_TARGET_HAS_QUERY UINT32_C(0x00000004)



/* Target 借用原始方法与 request-target，并只保留 HTTP 路径需要的 URI 组件。 */
typedef struct xhttptarget {
	xhttptargetform Form;
	uint32 Flags;
	xstrview Method;
	xstrview Text;
	xstrview Scheme;
	xstrview Authority;
	xstrview Path;
	xstrview Query;
	xhttpauthority Host;
} xhttptarget;

#endif



#if defined(XRT_FEATURE_HTTP_PARAM)

/* 参数值标志区分省略值、token 值和 quoted-string 值。 */
typedef enum xhttpparamflags {
	XHTTP_PARAM_NONE = 0,
	XHTTP_PARAM_HAS_VALUE = 0x01,
	XHTTP_PARAM_QUOTED = 0x02
} xhttpparamflags;



/* 参数名称和值借用原文本；quoted-string 值不含双引号，但保留反斜杠转义。 */
typedef struct xhttpparam {
	xstrview Name;
	xstrview Value;
	uint32 Flags;
} xhttpparam;



/* 参数语义值游标由初始化函数建立；Offset 是下一次读取的原始值偏移。 */
typedef struct xhttpparamvaluecursor {
	const void* Source;
	const void* Value;
	size_t ValueSize;
	size_t Offset;
	uint32 Flags;
	uint8 Validated;
} xhttpparamvaluecursor;

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_HTTP_HOST)

/*
	解析单个 HTTP Host 字段值并返回借用原输入的 authority 结构。
	不接受字段名称、分隔冒号、两端 OWS 或 userinfo。
	RFC 9112 要求的空字段值与 RFC 3986 空端口都会保留。
	任意长度十进制端口属于合法协议文本；PORT_VALUE 表示可用网络数值。
*/
XRT_API bool xrtHttpHostParse(
	xstrview Value,
	xhttpauthority* pHost
);



/*
	验证 Host 字段值是单个、无 userinfo 的 URI authority。
	该函数不接受字段名称、冒号或两端 OWS。
*/
XRT_API bool xrtHttpHostValid(xstrview Value);



/* 纯谓词：严格验证 RFC 3986 IPv4 文本，拒绝多段、越界值和前导零。 */
XRT_API bool xrtHttpIpv4Valid(xstrview Value);



/* 纯谓词：严格验证 IPv6 文本，支持压缩和嵌入式 IPv4，不接受 ZoneID。 */
XRT_API bool xrtHttpIpv6Valid(xstrview Value);



/* 按 ASCII 大小写不敏感规则比较两个已经拆分出的 Host 视图。 */
XRT_API bool xrtHttpHostEqual(xstrview Left, xstrview Right);



/*
	验证拆分后的 authority 字段、标志与端口数值保持一致。
	Text 仅作为可选的原始文本视图保存，不要求 Host 与 PortText 必须从中切分。
	手工构造数值端口时可以省略 PortText，解析结果仍会保留原始端口文本。
*/
XRT_API bool xrtHttpAuthorityValid(
	const xhttpauthority* pAuthority
);



/* 取得显式端口；省略或空端口使用调用方给出的默认值。 */
XRT_API bool xrtHttpAuthorityPort(
	const xhttpauthority* pAuthority,
	uint16 iDefaultPort,
	uint16* pPort
);

#endif



#if defined(XRT_FEATURE_HTTP_TARGET)

/*
	按方法严格解析 HTTP request-target。
	CONNECT 只接受带非空端口 authority，但协议解析不提前限制网络端口范围；
	OPTIONS 星号形式必须精确为 "*"。
	pTarget 可使用未对齐存储，但完整可写区间不得回绕或覆盖方法与 target。
*/
XRT_API bool xrtHttpTargetParse(
	xstrview Method,
	xstrview Text,
	xhttptarget* pTarget
);



/*
	解析请求的有效 authority。
	absolute 和 CONNECT 使用 target；origin 和星号形式使用 Host 字段值。
	pAuthority 可使用未对齐存储，但完整可写区间不得回绕或覆盖输入与借用视图。
*/
XRT_API bool xrtHttpTargetAuthority(
	const xhttptarget* pTarget,
	xstrview Host,
	xhttpauthority* pAuthority
);

#endif



#if defined(XRT_FEATURE_HTTP)

/*
	返回已注册状态码的标准原因短语；未知、临时或未分配状态返回空视图。
	原因短语只用于人类可读输出，协议逻辑不得依赖它。
*/
XRT_API xstrview xrtHttpStatusText(uint16 iStatus);



/* 判断文本是否是非空 HTTP token。 */
XRT_API bool xrtHttpTokenValid(xstrview Text);



/* 按 ASCII 大小写不敏感规则比较两个 token。 */
XRT_API bool xrtHttpTokenEqual(xstrview Left, xstrview Right);



/*
	按大小写敏感规则分类 HTTP 方法。
	合法扩展方法返回 OTHER；空值或非法 token 返回 INVALID。
*/
XRT_API xhttpmethod xrtHttpMethodParse(xstrview Method);



/* 按 HTTP 大小写敏感规则比较两个合法方法名。 */
XRT_API bool xrtHttpMethodEqual(
	xstrview Left,
	xstrview Right
);



/* 判断方法是否只读取资源语义；GET、HEAD、OPTIONS 和 TRACE 属于安全方法。 */
XRT_API bool xrtHttpMethodSafe(xstrview Method);



/* 判断方法是否允许重复执行而不改变预期效果；安全方法、PUT 和 DELETE 属于幂等方法。 */
XRT_API bool xrtHttpMethodIdempotent(xstrview Method);



/*
	判断最终响应是否允许携带内容。
	HEAD、1xx、204、205、304 和成功 CONNECT 响应返回 false。
	方法名按 HTTP 规则区分大小写；无效方法或 100 到 999 之外的状态返回 false。
*/
XRT_API bool xrtHttpResponseContentAllowed(
	xstrview Method,
	uint16 iStatus
);



/* 删除文本两端的可选横向空白，返回借用原文本的视图。 */
XRT_API xstrview xrtHttpOwsTrim(xstrview Text);



/* 按 RFC 接收方规则读取 token-list，并忽略逗号产生的空元素；Offset 初始为零。 */
XRT_API xhttpnext xrtHttpTokenNext(
	xstrview List,
	size_t* pOffset,
	xstrview* pToken
);



/* 判断完整 token-list 是否包含指定 token；非空元素语法错误仍返回 false 并设置错误。 */
XRT_API bool xrtHttpTokenListHas(xstrview List, xstrview Token);



/* 统计 token-list 非空条目；空列表成功返回零，非空元素语法错误返回 false。 */
XRT_API bool xrtHttpTokenListCount(xstrview List, size_t* pCount);



/* 规范写出逗号空格分隔的 token-list；空输出可精确查询长度。 */
XRT_API bool xrtHttpTokenListWrite(
	const xstrview* pTokens,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 token-list，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpTokenListBuild(
	const xstrview* pTokens,
	size_t iCount,
	size_t* pSize
);



/* 初始化可重复使用的同名字段 token-list 游标。 */
XRT_API void xrtHttpFieldTokenCursorInit(
	xhttpfieldtokencursor* pCursor
);



/*
	跨重复同名字段读取 token-list 条目，并保持字段与条目线路顺序。
	第一次发布条目前完整验证全部同名字段并绑定输入；输入在游标结束前必须保持不变。
*/
XRT_API xhttpnext xrtHttpFieldTokenNext(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xhttpfieldtokencursor* pCursor,
	xstrview* pToken
);



/* 完整验证并统计全部重复同名字段中的非空 token 条目。 */
XRT_API bool xrtHttpFieldTokenCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t* pTokenCount
);



/* 完整验证并查找重复同名字段中的 token，返回值区分找到、未找到和错误。 */
XRT_API xhttpnext xrtHttpFieldTokenFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	xstrview Token
);



/*
	严格解析 RFC qvalue，接受两端 OWS，结果范围为 0 到 1000。
	语法错误时 Quality 保持为零；参数错误不修改输出。
	输出支持未对齐存储，但不得与 Text 重叠。
*/
XRT_API bool xrtHttpQualityParse(
	xstrview Text,
	uint16* pQuality
);



/*
	迭代 token [ weight ] 列表并忽略空成员；缺省 Quality 为 1000。
	该形式可直接用于 Accept-Encoding、Accept-Charset 等字段。
	语法错误不推进 Offset 并清空 Item；参数错误不修改输出。
	游标和结果支持未对齐存储，二者及 List 不得相互重叠。
*/
XRT_API xhttpnext xrtHttpWeightedTokenNext(
	xstrview List,
	size_t* pOffset,
	xhttpweightedtoken* pItem
);



/*
	解析 Content-Length 字段值。
	逗号分隔的重复值只有完全一致时才成功，失败时输出保持为零。
*/
XRT_API bool xrtHttpContentLengthParse(
	xstrview Value,
	uint64* pLength
);



/* 判断合法连续文本是否能安全作为 HTTP 字段值或 reason-phrase；空视图允许为 NULL/0。 */
XRT_API bool xrtHttpFieldValueValid(xstrview Value);



/* 严格解析一行不含 CRLF 的 HTTP 字段；未对齐输出在返回前一次性发布。 */
XRT_API bool xrtHttpFieldParse(xstrview Line, xhttpfield* pField);



/* 严格读取不含终止空行的字段块；游标和字段输出支持未对齐存储。 */
XRT_API xhttpnext xrtHttpFieldNext(
	xstrview Block,
	size_t* pOffset,
	xhttpfield* pField
);



/* 严格统计完整字段块；空字段块成功返回零，计数输出支持未对齐存储。 */
XRT_API bool xrtHttpFieldBlockCount(
	xstrview Block,
	size_t* pCount
);



/* 写出单个字段行及 CRLF；字段和长度描述符支持未对齐存储。 */
XRT_API bool xrtHttpFieldWrite(
	const xhttpfield* pField,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出字段数组及最终空行；描述符数组和长度输出支持未对齐存储。 */
XRT_API bool xrtHttpFieldBlockWrite(
	const xhttpfield* pFields,
	size_t iCount,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 按 ASCII 大小写不敏感规则比较字段名称。 */
XRT_API bool xrtHttpFieldNameEqual(xstrview Left, xstrview Right);



/* 从指定位置查找字段；描述符数组可未对齐，未找到返回 XRT_NPOS。 */
XRT_API size_t xrtHttpFieldFind(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	size_t iStart
);



/* 返回原数组中第一个同名字段的借用地址，未找到返回空指针。 */
XRT_API const xhttpfield* xrtHttpFieldGet(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);



/* 返回原数组中唯一同名字段的借用地址；指针输出支持未对齐存储。 */
XRT_API xhttpnext xrtHttpFieldGetUnique(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name,
	const xhttpfield** ppField
);



/* 统计同名字段数量。 */
XRT_API size_t xrtHttpFieldCount(
	const xhttpfield* pFields,
	size_t iCount,
	xstrview Name
);

#endif



#if defined(XRT_FEATURE_HTTP_PARAM)

/* 严格读取分号参数；游标和结果可未对齐，错误不推进游标并清空结果。 */
XRT_API xhttpnext xrtHttpParamNext(
	xstrview Parameters,
	size_t* pOffset,
	xhttpparam* pParam
);



/* 严格统计完整参数列表；计数可未对齐，空列表或失败分别发布零。 */
XRT_API bool xrtHttpParamCount(
	xstrview Parameters,
	size_t* pCount
);



/* 严格查找参数并验证全部后缀；结果可未对齐，未命中或错误时清空。 */
XRT_API xhttpnext xrtHttpParamFind(
	xstrview Parameters,
	xstrview Name,
	xhttpparam* pParam
);



/*
	读取逗号分隔 name[=value] 指令的下一项。
	空列表项被忽略；值可以是 token 或 quoted-string；输出可未对齐。
*/
XRT_API xhttpnext xrtHttpDirectiveNext(
	xstrview Directives,
	size_t* pOffset,
	xhttpparam* pDirective
);



/* 严格统计完整指令列表；空项不计数，计数可未对齐且失败发布零。 */
XRT_API bool xrtHttpDirectiveCount(
	xstrview Directives,
	size_t* pCount
);



/* 查找首个指令并验证全部后缀；结果可未对齐，未命中或错误时清空。 */
XRT_API xhttpnext xrtHttpDirectiveFind(
	xstrview Directives,
	xstrview Name,
	xhttpparam* pDirective
);



/* 判断文本是否是一段完整、合法的 HTTP quoted-string。 */
XRT_API bool xrtHttpQuotedValid(xstrview Quoted);



/* 解码完整 quoted-string；长度可未对齐，空输出查询长度且不附加零字符。 */
XRT_API bool xrtHttpQuotedRead(
	xstrview Quoted,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出带引号和必要转义的 quoted-string；长度可未对齐且不附加零字符。 */
XRT_API bool xrtHttpQuotedWrite(
	xstrview Value,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾 quoted-string；可选长度可未对齐，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpQuotedBuild(
	xstrview Value,
	size_t* pSize
);



/*
	判断参数是否带值，且解开 quoted-string 转义后的语义值是非空 token。
	描述符可未对齐；函数不修改线程错误，可用于要求 token 语义的协议参数。
*/
XRT_API bool xrtHttpParamTokenValid(const xhttpparam* pParam);



/* 按 ASCII 大小写不敏感规则比较参数的解码 token 值；纯谓词不修改错误槽。 */
XRT_API bool xrtHttpParamTokenEqual(
	const xhttpparam* pParam,
	xstrview Token
);



/* 初始化参数语义值游标；游标支持未对齐存储。 */
XRT_API void xrtHttpParamValueCursorInit(
	xhttpparamvaluecursor* pCursor
);



/*
	逐字节读取参数的解码语义值；首次调用完整验证并绑定参数描述符。
	Offset 保留原始值偏移，输入在迭代结束前必须保持不变。
*/
XRT_API xhttpnext xrtHttpParamValueNext(
	const xhttpparam* pParam,
	xhttpparamvaluecursor* pCursor,
	uint8* pByte
);



/* 解码参数值；描述符和长度可未对齐，token 复制，quoted-string 删除转义。 */
XRT_API bool xrtHttpParamValueWrite(
	const xhttpparam* pParam,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 写出单个参数；长度可未对齐，QUOTED 转义正文，NONE 省略等号和值。 */
XRT_API bool xrtHttpParamWrite(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pSize
);



/* 构建零结尾单个参数；可选长度可未对齐，返回值由 xrtFree 释放。 */
XRT_API str xrtHttpParamBuild(
	xstrview Name,
	xstrview Value,
	uint32 iFlags,
	size_t* pSize
);

#endif



#if defined(XRT_FEATURE_HTTP_PARAM_HOST)

/* 无分配验证参数解码后的语义值是 HTTP Host authority。 */
XRT_API bool xrtHttpParamHostValid(const xhttpparam* pParam);

#endif



XRT_EXTERN_C_END

#endif
