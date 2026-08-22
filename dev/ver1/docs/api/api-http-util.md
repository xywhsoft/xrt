# HTTP Util HTTP 文本与 multipart 工具层

> 面向 HTTP / WebSocket / 上传场景的文本解析、校验、媒体类型与 multipart 流工具层。

[返回索引](README.md)

---

## 1. 定位

`xhttp_util` 负责解决这几类问题：

- HTTP token / header / query / cookie / parameter 的逐项解析
- 上述文本块的格式校验与限制控制
- `Set-Cookie` 解析
- `Content-Type` / media type 解析与构建
- `Content-Disposition` 解析
- multipart body 的整体验证
- multipart stream 的增量解析

这层能力主要服务于：

- `xhttp`
- `xhttpd`
- `xws`
- 文件上传
- AI / HTTP API 客户端中的 header、query、cookie 处理


## 2. 核心类型

### 2.1 成对视图类型

当前工具层会频繁返回这几类视图：

- `xrtheaderpair`
- `xrtquerypair`
- `xrtcookiepair`
- `xrthttpparam`

它们的共同特点是：

- 使用 `xrtstrview`
- 默认借用原始输入缓冲区
- 适合 parser / validator / builder 串联使用


### 2.2 `xrthttputillimits`

这是 HTTP 工具层的统一限制配置。

当前可限制的内容包括：

- name 最大字节数
- value 最大字节数
- pair 最大数量
- header 行最大字节数
- header block 最大字节数
- header 最大数量
- token 最大字节数
- boundary 最大长度
- multipart 头数量
- multipart part 数量
- multipart 总字节数

推荐用法：

```c
xrthttputillimits tLimits;
xrtHttpUtilLimitsInit(&tLimits);
tLimits.iMaxHeaderBytes = 32u * 1024u;
```


### 2.3 `xrtsetcookieview`

用于表示 `Set-Cookie` 解析结果。

可携带的信息包括：

- `name/value`
- `domain`
- `path`
- `expires`
- `max-age`
- `SameSite`
- `Secure`
- `HttpOnly`
- `Partitioned`
- `SameParty`
- `Priority`


### 2.4 `xrtmediatypeview`

用于表示 `Content-Type` / media type。

可覆盖：

- `type`
- `subtype`
- `suffix`
- `params`

例如：

- `application/json`
- `application/problem+json`
- `multipart/form-data; boundary=xxx`


### 2.5 `xrtmultipartpartview`

表示 multipart 的单个 part。

包含：

- `tHeaders`
- `tBody`
- `tContentDisposition`
- `tName`
- `tFileName`
- `tFileNameExt`
- `tContentType`
- `tTransferEncoding`


### 2.6 `xrtmultipartstream`

这是当前主线里非常重要的一层：

- 支持流式喂入 multipart body
- 不要求先把整个 body 一次性读完
- 适合 HTTP server / 大文件上传 / 分块接收场景

相关结果和错误：

- `xrtmultipartstreamresult`
- `XRT_MULTIPART_STREAM_RESULT_*`
- `XRT_MULTIPART_STREAM_ERR_*`


## 3. 限制与校验 API

### 初始化默认限制

```c
XXAPI void xrtHttpUtilLimitsInit(xrthttputillimits* pLimits);
XXAPI void xrtMultipartStreamConfigApplyLimits(xrtmultipartstreamconfig* pConfig, const xrthttputillimits* pLimits);
```

意义：

- 给 parser / validator 提供统一上限
- 避免 header bomb、过大 multipart、异常参数数量等问题


### 各类文本校验

```c
XXAPI bool xrtHttpTokenValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtHttpParamValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtQueryValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtFormUrlEncodedValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtHttpHeaderBlockValidateN(const char* sBlock, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtSetCookieValidateN(const char* sText, size_t iLen, const xrthttputillimits* pLimits);
XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits);
```

对应的非 `N` 版本会自动使用 `strlen()`。

推荐场景：

- 服务端入站请求预校验
- 代理/网关边界检查
- 上传接口的 multipart 边界防御
- 客户端构造前的参数检查


## 4. 逐项迭代解析 API

这组 API 适合“扫描一段文本，并一次取一个元素”。

### Query

```c
XXAPI bool xrtQueryNextN(const char* sText, size_t iLen, size_t* pOffset, xrtquerypair* pOut);
XXAPI bool xrtQueryNext(const char* sText, size_t* pOffset, xrtquerypair* pOut);
```

### HTTP token

```c
XXAPI bool xrtHttpTokenNextN(const char* sText, size_t iLen, size_t* pOffset, xrtstrview* pOut);
XXAPI bool xrtHttpTokenNext(const char* sText, size_t* pOffset, xrtstrview* pOut);
```

### Header 行

```c
XXAPI bool xrtHttpHeaderNextLineN(const char* sBlock, size_t iLen, size_t* pOffset, xrtheaderpair* pOut);
XXAPI bool xrtHttpHeaderNextLine(const char* sBlock, size_t* pOffset, xrtheaderpair* pOut);
```

### Cookie

```c
XXAPI bool xrtCookieNextN(const char* sText, size_t iLen, size_t* pOffset, xrtcookiepair* pOut);
XXAPI bool xrtCookieNext(const char* sText, size_t* pOffset, xrtcookiepair* pOut);
```

### HTTP 参数

```c
XXAPI bool xrtHttpParamNextN(const char* sText, size_t iLen, size_t* pOffset, xrthttpparam* pOut);
XXAPI bool xrtHttpParamNext(const char* sText, size_t* pOffset, xrthttpparam* pOut);
```

这些 API 的共同模式是：

```c
size_t iOffset = 0u;
xrtquerypair tPair;

while ( xrtQueryNextN(sQuery, iLen, &iOffset, &tPair) ) {
	/* 处理 tPair */
}
```


## 5. Cookie / Media Type / Content-Disposition

### Set-Cookie 解析

```c
XXAPI bool xrtSetCookieParseN(const char* sText, size_t iLen, xrtsetcookieview* pOut);
XXAPI bool xrtSetCookieParse(const char* sText, xrtsetcookieview* pOut);
XXAPI bool xrtSetCookieParseLineN(const char* sLine, size_t iLen, xrtsetcookieview* pOut);
XXAPI bool xrtSetCookieParseLine(const char* sLine, xrtsetcookieview* pOut);
```

适用：

- 处理响应头里的 `Set-Cookie`
- 做 cookie 策略判断和日志分析


### Media Type

```c
XXAPI bool xrtHttpMediaTypeParseN(const char* sText, size_t iLen, xrtmediatypeview* pOut);
XXAPI bool xrtHttpMediaTypeParse(const char* sText, xrtmediatypeview* pOut);
XXAPI bool xrtHttpMediaTypeBuildTo(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
XXAPI bool xrtHttpMediaTypeBuild(const xrtmediatypeview* pType, char* sOut, size_t iOutCap, size_t* pOutLen);
XXAPI bool xrtHttpMediaTypeFindParamN(const xrtmediatypeview* pType, const char* sName, size_t iNameLen, xrthttpparam* pOut);
XXAPI bool xrtHttpMediaTypeFindParam(const xrtmediatypeview* pType, const char* sName, xrthttpparam* pOut);
```

用途：

- 识别 `application/json`
- 读取 `charset`
- 读取 `multipart/form-data` 的 `boundary`
- 输出规范化的 media type


### Content-Disposition

当前主线也提供了对应解析视图，适合：

- multipart 上传字段
- 文件名提取
- `filename*` 扩展字段识别

这部分常与 `xrtmultipartpartview` 一起使用。


## 6. multipart 工具

### 整体验证

```c
XXAPI bool xrtMultipartValidateN(const char* sBody, size_t iLen, const char* sBoundary, size_t iBoundaryLen, const xrthttputillimits* pLimits);
XXAPI bool xrtMultipartValidate(const char* sBody, const char* sBoundary, const xrthttputillimits* pLimits);
```

适合：

- 小 body 的一次性检查
- 上传前/测试中的格式验证


### 流式 multipart 解析

```c
XXAPI void xrtMultipartStreamConfigInit(xrtmultipartstreamconfig* pConfig);
XXAPI bool xrtMultipartStreamInit(xrtmultipartstream* pStream, const char* sBoundary, size_t iBoundaryLen, const xrtmultipartstreamconfig* pConfig);
XXAPI void xrtMultipartStreamUnit(xrtmultipartstream* pStream);
XXAPI void xrtMultipartStreamReset(xrtmultipartstream* pStream);
XXAPI bool xrtMultipartStreamFeed(xrtmultipartstream* pStream, const void* pData, size_t iLen);
XXAPI void xrtMultipartStreamFinish(xrtmultipartstream* pStream);
XXAPI uint32 xrtMultipartStreamError(const xrtmultipartstream* pStream);
XXAPI xrtmultipartstreamresult xrtMultipartStreamNext(xrtmultipartstream* pStream, xrtmultipartstreamevent* pEvent);
```

这是当前工具层里最值得强调的一组 API。

它的优势是：

- 不要求一次拿到完整 body
- 适合大文件上传
- 可和 `xnet_stream / xhttpd` 的接收链自然衔接

常见流程：

1. `ConfigInit`
2. `ApplyLimits`
3. `StreamInit`
4. 每收到一段网络数据就 `Feed`
5. 循环 `Next`
6. 处理：
	- `PART_BEGIN`
	- `DATA`
	- `PART_END`
	- `END`
7. 结束时 `Finish` / `Unit`


## 7. 常见用法

### 7.1 校验 query 串

```c
xrthttputillimits tLimits;
xrtHttpUtilLimitsInit(&tLimits);

if ( !xrtQueryValidate("a=1&b=2", &tLimits) ) {
	return;
}
```


### 7.2 迭代 header block

```c
size_t iOffset = 0u;
xrtheaderpair tHeader;

while ( xrtHttpHeaderNextLine(sHeaders, &iOffset, &tHeader) ) {
	/* 处理头字段 */
}
```


### 7.3 解析 Content-Type

```c
xrtmediatypeview tType;
xrthttpparam tBoundary;

if ( xrtHttpMediaTypeParse("multipart/form-data; boundary=abc", &tType) ) {
	if ( xrtHttpMediaTypeFindParam(&tType, "boundary", &tBoundary) ) {
		/* 取得 boundary */
	}
}
```


### 7.4 流式解析 multipart 上传

```c
xrtmultipartstream tStream;
xrtmultipartstreamconfig tConfig;
xrtmultipartstreamevent tEvent;

xrtMultipartStreamConfigInit(&tConfig);
xrtMultipartStreamInit(&tStream, sBoundary, strlen(sBoundary), &tConfig);

xrtMultipartStreamFeed(&tStream, pData, iLen);

while ( xrtMultipartStreamNext(&tStream, &tEvent) > 0 ) {
	/* 按事件处理 */
}
```


## 8. 使用建议

- 面对不可信 HTTP 输入时，优先总是带上 `xrthttputillimits`
- `*Next*` 解析 API 返回的是视图，不复制内存
- 小请求可以用 `Validate*` / `Parse*` 一次性处理
- 大上传或流式接收，优先使用 `xrtMultipartStream*`
- `xhttp / xhttpd` 已经会复用这层能力，自定义协议或精细控制时再直接使用


## 9. 当前边界

当前 `xhttp_util` 文档覆盖的是：

- HTTP 文本级工具
- 媒体类型
- Cookie / Set-Cookie
- multipart 验证与流式解析

它的定位是：

- HTTP 语义零件层
- 不是完整 HTTP client / server

