# HTTP 静态表示

`http_static.h` 从协议计划开始构建静态资源体系。第一层
`XHTTP_FEATURE_HTTP_STATIC_PLAN` 不打开文件、不创建响应对象，也不依赖网络；
它只把方法、请求字段和当前表示验证器转换为稳定的 HTTP 状态与字节区间。

## 裁剪边界

| 宏 | 能力 | 依赖 |
|---|---|---|
| `XHTTP_FEATURE_HTTP_STATIC_PLAN` | 条件请求、Range 限额、解析、排序与合并 | `HTTP_PRECONDITION`、`HTTP_RANGE` |
| `XHTTP_FEATURE_HTTP_STATIC_RESPONSE` | 把计划转换为状态、精确长度和响应字段 | `HTTP_STATIC_PLAN`、`HTTP_RANGE_MULTIPART` |
| `XHTTP_FEATURE_HTTP_STATIC_PATH` | 挂载匹配、percent 解码、UTF-8 与安全相对路径 | `CODEC_PERCENT`、`PATH_SAFE` |
| `XHTTP_FEATURE_HTTP_STATIC_FILE` | 文件根内打开、同句柄元数据、默认验证器与异步文件资源 | `ATOMIC`、`FILE_ROOT`、`HTTP_BODY_FILE`、`HTTP_PRECONDITION` |
| `XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY` | 单文件多区间异步 Body 与静态文件便捷采用 | `HTTP_RANGE_MULTIPART`、`HTTP_STATIC_FILE` |
| `XRT_FEATURE_MIME_TYPES` | 零分配扩展名与路径媒体类型查询 | core |

## 公共分类

路径映射标志和结果保持协议错误与正常路由未命中可区分：

```c
typedef enum xhttpstaticpathflag {
	XHTTP_STATIC_PATH_PORTABLE = UINT32_C(0x00000001),
	XHTTP_STATIC_PATH_ALLOW_HIDDEN = UINT32_C(0x00000002)
} xhttpstaticpathflag;

typedef enum xhttpstaticpathstatus {
	XHTTP_STATIC_PATH_ERROR = -1,
	XHTTP_STATIC_PATH_NO_MATCH = 0,
	XHTTP_STATIC_PATH_MATCH = 1
} xhttpstaticpathstatus;
```

静态文件错误保留打开、元数据、区间、采用和引用阶段：

```c
typedef enum xhttpstaticfileerror {
	XHTTP_STATIC_FILE_ERROR_SUBMIT = 1,
	XHTTP_STATIC_FILE_ERROR_OPEN,
	XHTTP_STATIC_FILE_ERROR_STAT,
	XHTTP_STATIC_FILE_ERROR_TYPE,
	XHTTP_STATIC_FILE_ERROR_SIZE,
	XHTTP_STATIC_FILE_ERROR_RANGE,
	XHTTP_STATIC_FILE_ERROR_ADOPT,
	XHTTP_STATIC_FILE_ERROR_BODY,
	XHTTP_STATIC_FILE_ERROR_REFERENCE,
	XHTTP_STATIC_FILE_ERROR_CONSUMED
} xhttpstaticfileerror;

typedef enum xhttpstaticmultipartbodyerror {
	XHTTP_STATIC_MULTIPART_BODY_ERROR_ADOPT = 1
} xhttpstaticmultipartbodyerror;
```

静态响应最多发布 `XHTTP_STATIC_RESPONSE_MAX_FIELDS` 个协议字段；业务字段仍由
调用方在更高层追加。

安全文件根打开、异步文件 Body 和 MIME 类型映射都已作为独立层提供。服务器连接仍由
后续上层组合，不会让只需要缓存或对象存储策略的程序被迫链接文件与网络模块。

## 响应计划

```c
xhttpstaticplanconfig config;
xhttpbyterange ranges[16];
xhttpstaticplan plan;

xrtHttpStaticPlanConfigInit(&config);
if ( !xrtHttpStaticPlanBuild(
	requestMethod,
	fields,
	fieldCount,
	&current,
	completeLength,
	ranges,
	16,
	&config,
	&plan
) ) {
	return false;
}
```

默认最多解析 16 个范围项，并合并重叠或直接相邻的闭区间。调用方提供的
`ranges` 容量必须至少为 `MaxRanges`；没有范围能力的裁剪路径可以把
`MaxRanges` 和容量同时设为零。`MergeGap` 可以进一步合并间距很小的范围，
用于限制 multipart 放大。

结果覆盖：

- `200`：完整 GET，HEAD，或 Range 被忽略。
- `206`：至少一个范围可满足；前 `RangeCount` 项已排序、裁剪和合并。
- `304`：GET/HEAD 的验证器命中。
- `405`：静态表示只接受 GET 与 HEAD。
- `412`：If-Match 或 If-Unmodified-Since 等前置条件失败。
- `416`：语法有效的 byte-range-set 没有任何可满足项。

`SendBody` 只在 GET 的 200/206 路径为真。`CompleteLength` 是完整表示长度，
`SelectedLength` 是单段或多段实际文件负载字节之和，不包含
`multipart/byteranges` 的边界和分段字段。

## Range 策略

Range 只对 GET 生效，HEAD 按完整表示生成元数据但不发送正文。未知范围单位、
畸形 Range、重复 Range 字段和超过 `MaxRanges` 的集合都会被安全忽略并退化
为 200，且不会留下线程错误。这样既符合服务器可以忽略 Range 的协议空间，
也避免攻击者用复杂字段强迫昂贵解析或制造错误响应。

唯一 If-Range 必须强匹配；弱标签、无效值、重复字段或不匹配都会忽略 Range。
零长度表示上的非零 suffix-range 按语义可满足但不选择字节，计划采用完整的
200 空响应。普通全部越界或 `-0` 集合返回 416。

有效多范围先解析到完整长度，再按起点排序。重叠、相邻和 `MergeGap` 覆盖的
小间隔会合并，因此 `SelectedLength` 不会重复计算同一字节。单个范围也走
完全相同的路径，没有另一套容易漂移的专用解析器。

## 条件请求顺序

方法检查后，计划直接复用 `xrtHttpPreconditionsEvaluate` 的 RFC 固定顺序。
304、412 总是在 Range 之前完成。表示由调用方通过 `xhttprepresentation`
提供，因此文件系统、数据库、对象存储和生成内容可以共享同一协议层。

## 内存与错误

计划不分配内存。配置、字段、验证器、范围输出和计划输出必须相互独立；
别名、容量和结构错误返回 `false` 并设置统一错误。线路上合法但应忽略的
Range 不是库错误。成功后只有前 `RangeCount` 项有意义。

配置、字段描述符数组、当前表示、范围数组和计划输出都可以使用未对齐存储，
但每段固定结构或数组必须完整且地址不回绕。构建器先快照输入，失败时不会发布
半个计划，也不会修改计数之外的范围存储。

完整示例位于 `examples/http/static_plan/main.c`。

## 响应字段

`XHTTP_FEATURE_HTTP_STATIC_RESPONSE` 是计划后的纯协议层。它不打开文件、不创建
Body、Reply 或服务器连接，只把 `xhttpstaticplan` 转换为状态码、正文长度和
最多七个标准字段。结果既可以交给 HTTP/1 写入器，也可以由 HTTP/2、自定义
封包、对象存储网关或上层便捷函数直接使用。

```c
xhttpstaticresponseconfig config;
xhttpstaticresponse response;
char workspace[256];
size_t required;

xrtHttpStaticResponseConfigInit(&config);
config.ContentType = xrtMimeByPath(relativePath);

if ( !xrtHttpStaticResponseBuild(
	&plan,
	ranges,
	&current,
	&config,
	NULL,
	0,
	&required,
	NULL
) || (required > sizeof(workspace)) ) {
	return false;
}
if ( !xrtHttpStaticResponseBuild(
	&plan,
	ranges,
	&current,
	&config,
	workspace,
	sizeof(workspace),
	&required,
	&response
) ) {
	return false;
}
```

`pResponse` 为空是严格的长度查询模式，此时不能传工作区。实际构建不分配内存；
字段名和固定值借用静态存储，媒体类型、缓存策略与边界借用配置文本，十进制
长度、日期、ETag、Content-Range 和 multipart 类型写入调用方工作区。使用
`response.Fields` 期间，配置文本、验证器文本和工作区都必须继续存活。输入、
输出结构和工作区不得相互覆盖。

计划、当前表示、配置、范围数组、长度输出和响应输出均可未对齐。构建器对每个
固定输入只取一次快照，并在全部测量和写入成功后一次性发布长度与响应结构；
任何地址回绕、容量或别名错误都不会留下部分响应。

字段集合按状态固定：

- `200`：Content-Type、完整 Content-Length，并按配置附加 Accept-Ranges、
  ETag、Last-Modified 和 Cache-Control；HEAD 保留相同长度但 `SendBody` 为假。
- 单范围 `206`：发送所选范围长度，并增加顶层 Content-Range。
- 多范围 `206`：Content-Type 改为 `multipart/byteranges; boundary=...`，
  Content-Length 使用完整 multipart 正文长度，顶层不发送 Content-Range。
- `304`：不发送 Content-Length，只保留适用的表示元数据。
- `405`：发送零长度和 `Allow: GET, HEAD`，不泄漏表示元数据。
- `412`：发送零长度，并保留适用的缓存与验证器元数据。
- `416`：发送零长度和 `Content-Range: bytes */完整长度`。

多范围边界必须是 1 到 70 字节的 HTTP token。响应层与正文层复用同一个
canonical 线缆测量器，自动计算全部 Part 字段、CRLF、边界和文件负载，不再
接收容易与实际正文漂移的手填长度。响应层仍不创建正文或依赖文件、网络。

完整示例位于 `examples/http/static_response/main.c`。

## 多范围线缆

多范围格式不属于静态文件专用能力。响应字段和文件 Body 共同复用公共
`XHTTP_FEATURE_HTTP_RANGE_MULTIPART` 的 `xrtHttpRangeMultipartLength`、
`HeadWrite`、`EndWrite` 与 `CloseWrite`，因此缓存、内存表示、对象存储和静态
文件不会各自维护一套线缆算法。完整 API 与线缆格式见 `http_semantics.md`，
独立示例位于 `examples/http/range_multipart/main.c`。

## 多范围正文

`XHTTP_FEATURE_HTTP_STATIC_MULTIPART_BODY` 把同一线缆格式适配为不可重放的异步
`xhttpbody`：

```c
xhttpbody* body = xrtHttpStaticFileTakeMultipartBody(
	file,
	ranges,
	rangeCount,
	contentType,
	boundary
);
```

工厂先验证并复制范围，预生成全部固定 Part 头和关闭边界，再原子取走静态
文件的异步句柄。格式、范围、OOM 或 Body 构造失败都不会消费 `file`，调用方
可以修正后重试。需要绕过静态文件资源时，低层
`xrtHttpStaticMultipartBodyAdopt` 直接采用可读 `xasyncfile`；成功后 Body
独占关闭文件，失败时所有权仍归调用方。

正文只分配一个与范围数和固定元数据成正比的工厂块，以及 Reader。文件负载
不聚合、不复制，也没有每对象固定 8 KiB 缓冲；每段复用 `http_body_file`
已经压实的异步 cursor，由消费者当前 `MaxBytes` 决定读取大小。固定元数据
Chunk 通过工厂引用延长生命周期，文件 Chunk 通过读取 Future 延长生命周期，
两者都可以晚于 Reader 和 Body 释放。文件缩短会稳定返回
`XHTTP_BODY_FILE_ERROR_READ`，不会把截断正文伪装成合法 EOF。

完整示例位于 `examples/http/static_multipart_body/main.c`。

## 历史资产

旧 `xweb` 的静态服务已经覆盖 200、206、304、416、固定范围、开放范围与
后缀范围。这些场景被保留，但旧实现的单范围字符串解析、先命中即停止的
验证器比较和路径层内联状态分支没有继续复制。新计划建立在公共 ETag、
前置条件与 Range 层之上，并补齐 HEAD、405、If-Range 强比较、64 位多范围、
工作上限和放大控制。

## URL 路径映射

`XHTTP_FEATURE_HTTP_STATIC_PATH` 是独立于文件系统和网络的第二层能力。它依赖
`XRT_FEATURE_CODEC_PERCENT` 与 `XRT_FEATURE_PATH_SAFE`，负责把原始 URL path 安全映射为文件根内的
UTF-8 相对路径，但不会打开文件，也不会自行拼接系统绝对路径。

```c
xhttpstaticpathconfig config;
xhttpstaticpath path;

xrtHttpStaticPathConfigInit(&config);
config.Mount = XRT_STR_LITERAL("/assets");
xrtHttpStaticPathInit(&path);

if ( !xrtHttpStaticPathMap(requestPath, &config, &path) ) {
	return false;
}
if ( path.Matched ) {
	/* path.Path 只能继续交给受文件根约束的文件 API。 */
}
xrtHttpStaticPathFree(&path);
```

挂载点使用已经解码的规范 origin 路径。根挂载写作 `/`；其他挂载必须以斜杠开头且不能以斜杠结尾。
匹配区分大小写，并要求完整路径段边界。请求 `/%61ssets/app.js` 可以命中 `/assets`，
但 `/assets-old/app.js` 只会得到正常的 `XHTTP_STATIC_PATH_NO_MATCH`，不会留下线程错误。

原始请求路径执行以下固定检查：

- 必须以字面斜杠开头，不接受 query 或 fragment 混入。
- percent 转义必须完整，`+` 保持普通加号，不采用表单语义。
- 编码斜杠、反斜杠、NUL、控制字节和非法 UTF-8 一律拒绝。
- 重复斜杠、空段、`.`、`..` 及其 percent 编码变体一律拒绝。
- 尾斜杠不写入文件路径，而是通过 `TrailingSlash` 单独保留。
- 挂载根本身映射为 `.`，避免产生空文件路径。

默认 `XHTTP_STATIC_PATH_PORTABLE` 复用路径模块的跨 Windows/POSIX 安全段规则，拒绝 Windows
禁用字符、尾部点或空格以及 `CON`、`COM1`、`CONIN$`、`CONOUT$` 等设备名。默认也拒绝任意
点开头的隐藏段。确实只服务 POSIX 文件名时可以清除 `XHTTP_STATIC_PATH_PORTABLE`；
需要 `.well-known` 等目录时可以显式加入 `XHTTP_STATIC_PATH_ALLOW_HIDDEN`。无论策略如何放宽，
路径结构、percent、UTF-8 和穿越检查都不能关闭。

`xrtHttpStaticPathWrite` 是零分配基础 API。空输出查询所需长度，实际输出额外需要一个末尾零字节；
容量不足和输入输出重叠都不会修改文本输出。`xrtHttpStaticPathMap` 是分配型易用 API，
正常未命中返回 `true` 且 `Matched` 为 false。完整示例位于
`examples/http/static_path/main.c`。

旧 `xweb` 的挂载段边界、空相对路径、点文件开关和点段拒绝语义都已保留。旧路径先分配 percent
解码文本，再执行字符串拼接和词法规范化；新基础 API 改为流式多遍扫描，热路径零分配，并直接复用
`path_safe` 已经压实的跨平台设备名规则。旧的反斜杠放行开关没有保留，因为反斜杠会改变 Windows
路径结构，无法与受根约束的跨平台文件打开契约安全组合。

## 静态文件资源

`XHTTP_FEATURE_HTTP_STATIC_FILE` 不接受系统绝对路径，也不执行字符串前缀形式的目录检查。它只通过
`xroot` 逐分量解析相对路径，安全的根内相对链接可以继续工作，绝对链接、父目录逃逸和链接环会由文件根拒绝。
最终文件只打开一次，随后直接对同一文件句柄执行 `xrtFileStat`，再把该句柄交给异步文件层。这样不会重现旧
`xweb` 中“按路径检查、按路径查询、稍后再按路径打开”造成的 TOCTOU 窗口。

```c
xhttpstaticfile* file = xrtHttpStaticFileOpen(pool, root, relativePath);
if ( file == NULL ) {
	return false;
}

const xhttprepresentation* current =
	xrtHttpStaticFileRepresentation(file);
uint64 length = xrtHttpStaticFileSize(file);

if ( !xrtHttpStaticPlanBuild(
	method,
	fields,
	fieldCount,
	current,
	length,
	ranges,
	16,
	&config,
	&plan
) ) {
	xrtHttpStaticFileDestroy(file);
	return false;
}

if ( plan.SendBody && (plan.RangeCount == 0) ) {
	body = xrtHttpStaticFileTakeBodyAll(file);
} else if ( plan.SendBody && (plan.RangeCount == 1) ) {
	body = xrtHttpStaticFileTakeBody(
		file,
		ranges[0].First,
		ranges[0].Last - ranges[0].First + 1
	);
}
xrtHttpStaticFileDestroy(file);
```

```c
const xfileinfo* xrtHttpStaticFileInfo(
	const xhttpstaticfile* pFile
);
```

返回的 `xfileinfo` 借用静态文件对象，只在对应对象引用存活期间有效。

异步静态文件入口在提交任务前复制路径文本，因此调用返回后可以立即修改或释放原路径；文件根借用到 Future 终态，任务池则必须存活到静态文件资源以及从中取出的所有 Body 全部释放。Body 工厂发生内存不足时，静态文件仍保留同一个异步文件，可以在内存条件恢复后直接重试。

`xrtHttpStaticMultipartBodyAdopt` 和 `xrtHttpStaticFileTakeMultipartBody` 接受未对齐的完整范围数组，并在返回前复制所有范围；数组地址、元素数量形成的字节范围不得回绕。采用成功后 Body 独占异步文件并负责关闭；采用失败时文件仍归调用方。静态文件便捷入口也只在整个 multipart 工厂和 Body 构造成功后才取走文件，因此非法范围和内存不足都不会破坏重试能力。

`xrtHttpStaticFileOpen` 是明确的同步底层入口，可能阻塞。事件循环和 HTTP 服务器应使用
`xrtHttpStaticFileFuture`，其成功值由 Future 持有；需要超过 Future 生命周期时调用
`xrtHttpStaticFileRef`。任务执行期间 `xroot` 由调用方借用，必须保持存活；任务池必须一直存活到静态文件
以及从它取出的所有 Body 都完成关闭。

资源对象保存普通文件类型、完整大小和可用时间、身份元数据。默认 ETag 由大小、修改时间以及平台可用时的
设备和文件身份组成，并明确标记为弱 ETag；Last-Modified 也来自同一文件句柄。文件系统元数据不能天然证明
字节级强一致，因此默认不会把修改时间标记为强验证器。需要强 ETag 的应用应使用内容摘要或自己的版本号覆盖
协议规划输入。

正文资源只能取走一次。无正文的 HEAD、304、405、412 和 416 路径直接销毁资源即可，不会创建无用 Reader。
单范围使用 `xrtHttpStaticFileTakeBody`，完整文件使用 `xrtHttpStaticFileTakeBodyAll`。多范围、自定义压缩或
平台发送优化可以通过 `xrtHttpStaticFileTakeFile` 取得底层 `xasyncfile`，而不必绕过文件根和同句柄元数据
契约。区间参数或 Body 构造失败不会消费资源，修正后可以重试。

完整示例位于 `examples/http/static_file/main.c`。
