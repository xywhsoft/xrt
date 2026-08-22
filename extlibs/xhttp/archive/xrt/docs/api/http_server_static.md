# HTTP 静态文件服务

`http_server_static` 是静态协议、受限文件根、异步文件正文与 HTTP Server Reply 的组合层。它不重新实现 URL、Range、条件请求、MIME、文件读取或 HTTP/1 序列化，也不要求其他 HTTP 响应必须经过对象构建器。

## 分层

静态文件链路按能力由浅入深：

- `xrtHttpStaticPathMap`：只做 URL path 到安全相对路径的映射。
- `xrtHttpStaticPlanBuild`：只计算 GET/HEAD、条件请求和 Range 结果。
- `xrtHttpStaticResponseBuild`：只生成状态、精确长度和协议字段。
- `xrtHttpStaticFileOpen`：从受限根内打开同一文件句柄并取得稳定元数据。
- `xrtHttpReplyFromStatic`：把纯协议响应桥接为 Reply。
- `xrtHttpReplyStatic`：组合已经打开的静态文件。
- `xrtHttpReplyStaticFuture`：映射、异步打开并返回 Reply Future。
- `xrtHttpConnStatic`：在服务端请求回调中一次完成常用路径。

这些层都公开。调用方可以在任意层插入路由、鉴权、虚拟资源、自定义正文、压缩变体或缓存，而不必复制协议解析代码。

## 默认行为

`xrtHttpStaticServeConfigInit` 提供：

- 挂载点 `/`。
- 可移植安全文件名。
- 拒绝隐藏路径段。
- 目录索引 `index.html`。
- 最多 16 个 Range，并合并重叠或相邻范围。
- 按扩展名推断内置 MIME，未知类型使用 `application/octet-stream`。
- 多范围响应使用 128 位系统安全随机数生成 boundary。

路径未命中、被路径安全策略拒绝、资源不存在、无权读取或不是普通文件时，Future 成功生成 `404` Reply。语法无效的 request-target 生成 `400`。这些是正常 HTTP 结果，不污染线程错误。任务提交、内存、随机源、文件系统和协议内部错误保留结构化错误并使 Future 失败。

## 一次调用

```c
static void onRequest(
	xhttpserver* pServer,
	xhttpconn* pConnection,
	const xhttpserverrequest* pRequest,
	ptr pData
)
{
	app* pApp = (app*)pData;

	(void)pServer;
	(void)pRequest;
	if ( !xrtHttpConnStatic(
		pConnection,
		pApp->Files,
		pApp->PublicRoot,
		NULL
	) ) {
		(void)xrtHttpConnReply(
			pConnection,
			XHTTP_STATUS_SERVICE_UNAVAILABLE,
			XRT_STR_LITERAL("application/json; charset=utf-8"),
			XRT_BYTES_LITERAL("{\"code\":503}")
		);
	}
}
```

`true` 表示 Reply Future 已交给连接的唯一最终响应门，不表示文件已打开或字节已发送。`Root` 必须存活到 Reply Future 终态；任务池还必须覆盖响应 Body、Reader 和异步文件关闭的完整生命周期。

## 挂载与策略

```c
xhttpstaticserveconfig Config;
static const xstrview Indexes[] = {
	XRT_STR_LITERAL("home.html"),
	XRT_STR_LITERAL("index.html")
};

xrtHttpStaticServeConfigInit(&Config);
Config.Path.Mount = XRT_STR_LITERAL("/assets");
Config.Indexes = Indexes;
Config.IndexCount = sizeof(Indexes) / sizeof(Indexes[0]);
Config.Reply.CacheControl =
	XRT_STR_LITERAL("public, max-age=3600");

(void)xrtHttpConnStatic(
	pConnection,
	pFiles,
	PublicRoot,
	&Config
);
```

`Indexes` 借用 `IndexCount` 个可移植文件名，并按顺序返回首个可见的普通文件；把 `IndexCount` 设为零可关闭目录索引。候选数组和文本只在 `xrtHttpReplyStaticFuture` 的同步调用期间借用。`ContentType` 非空时覆盖 MIME 推断；`Boundary` 非空时覆盖自动随机 boundary，主要用于确定性协议测试或外部 multipart 策略。

配置文本和请求引用在 `xrtHttpReplyStaticFuture` 返回前被独立保留，调用方可以立即修改或释放原配置存储，并可释放自己的请求引用。`Root` 和任务池本身采用借用生命周期，避免每个请求增加全局对象引用和锁。`Root` 只需覆盖文件准备和 Reply Future 终态；任务池还承载 Body 读取及异步关闭，因此必须活到 Reply、Body 和 Reader 全部释放且关闭完成。

两个配置初始化器和所有静态服务入口都接受未对齐但完整、地址不回绕的配置结构。入口只快照一次固定结构；Future 路径随后深复制 `ContentType`、`CacheControl` 和 `Boundary` 文本，因此返回后原结构和这些文本都不再参与异步工作。

对返回 Future 请求取消后，取消请求会传播到这条静态响应链独占的文件 Future；尚未开始的任务不会执行文件打开过程，已经运行的任务在既有检查点协作收尾。Future 进入终态后才可以结束 `Root` 生命周期，任务池则仍需遵守上述正文关闭边界。普通 Future 延续仍保持共享源所需的单向取消语义。

## 已打开文件

```c
xhttpstaticfile* pFile = xrtHttpStaticFileOpen(
	pFiles,
	PublicRoot,
	"asset.bin"
);
xhttpreply* pReply = xrtHttpReplyStatic(
	pRequest,
	pFile,
	XRT_STR_LITERAL("asset.bin"),
	NULL
);
```

该路径适合自定义路由、授权和元数据缓存。函数先完成所有 Range、字段和 Reply 分配，最后才一次性取走文件正文。构建失败时，静态文件通常仍可重试；无论成功失败，`pFile` 对象都由调用方销毁。成功 Reply 独立持有正文引用。

## 纯协议桥

```c
xhttpreply* pReply = xrtHttpReplyFromStatic(
	&pResponse,
	pCustomBody
);
```

该函数复制状态和字段并保留正文引用。`SendBody` 与正文是否存在必须一致；正文必须声明精确长度并等于 `BodyLength`。静态响应已经生成确定的 `Content-Length`，因此未知长度的生成器正文会在桥接边界失败，不会留下一个只能在 HTTP/1 序列化阶段才报错的 Reply。应用仍可以使用纯协议层生成字段，再选择文件正文、内存正文、定长生成器正文或自己的传输策略；未知长度流应绕过静态 Reply 桥并使用普通流式响应。

`xhttpstaticresponse` 可以来自未对齐存储。桥接层先快照完整描述符并验证所有借用字段，再创建 Reply；地址回绕、非法字段名/值和无效 multipart boundary 会在分配 Reply 前失败。字段借用字节只需覆盖本次同步调用，成功返回的 Reply 已拥有自己的字段副本。

## 协议范围

组合层完整承接：

- GET 与 HEAD。
- `If-Match`、`If-Unmodified-Since`、`If-None-Match`、`If-Modified-Since`。
- `Range` 与 `If-Range`。
- `200`、`206`、`304`、`405`、`412`、`416`。
- ETag、Last-Modified、Accept-Ranges、Content-Range、Allow。
- 单范围和 canonical `multipart/byteranges` 流式正文。

通用 `http_server_file` 仍保留为“发送指定文件或严格区间”的低层 Helper，不自动解释上述静态资源语义。两者没有重复文件读取实现。

## 裁剪

`XRT_FEATURE_HTTP_SERVER_STATIC` 只在需要完整静态服务时启用。它依赖静态协议、受限文件根、异步文件 Body、Server Future、MIME、HEX 和安全随机源。只需要协议计算、路径映射或文件 Body 时，应单独启用对应低层特性。

完整可运行示例见 `examples/http/server_static/main.c`。
