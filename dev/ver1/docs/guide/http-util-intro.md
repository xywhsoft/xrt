# HTTP Util 入门：Header、Query、Cookie 和 multipart 不是一堆字符串

> 目标：把 `xrtQueryNext()`、`xrtHttpHeaderNextLine()`、`xrtCookieNext()`、`xrtSetCookieParse()`、`xrtHttpMediaTypeParse()`、`xrtMultipartValidate()`、`xrtMultipartStream*` 讲成第 6 阶段里“HTTP 文本块处理”这条正式主线。读完这页后，你应该明确知道：什么时候只需要逐项扫描，什么时候必须先做限制校验，什么时候 multipart 已经不该再按整包字符串处理。

[返回教学文档](README.md)

---

## 1. 为什么 `http util` 要单独讲

很多 HTTP 代码刚开始都会写成：

- `strstr("Content-Type:")`
- `strtok(query, "&")`
- `strncmp(cookie, "sid=", 4)`

短期能跑，长期一定会越来越脆。

因为 HTTP 里真正要处理的不是“一大坨字符串”，而是一组不同层次的文本块：

- token
- header line / header block
- query pair
- cookie pair
- `Set-Cookie`
- media type
- multipart

而这些块的风险和边界也完全不同：

- 有的适合逐项迭代
- 有的要先做 size limit
- 有的需要结构化 view
- 有的已经必须用流式解析


## 2. 先把 8 个边界分清楚

### 2.1 `xhttp_util` 不是 HTTP client/server，而是文本工具层

你可以先把它理解成：

- `xhttp` / `xhttpd` / `xws`
	- 真正跑协议
- `xhttp_util`
	- 帮你拆协议里的文本块

它不负责：

- 建连接
- 发请求
- 跑状态机

它负责：

- 解析、校验、遍历、构建 HTTP 相关文本片段


### 2.2 先切段，再逐项解析

这条线和 `xurl` 是连着的。

先用：

- `xurl`

把：

- query
- target

这些大段切出来。

再用：

- `xhttp_util`

逐项处理：

- query pair
- header line
- cookie pair


### 2.3 先校验限制，再进入业务解析

当前这组 API 里最容易被忽略的一层是：

- `xrthttputillimits`

和一整组 `Validate*()`。

这意味着当前正式主线不是：

- “先解析，出事再说”

而更接近：

1. 设限制
2. 先校验
3. 再逐项解析

这对下面这些边界尤其重要：

- 超大 header block
- 超长 token
- 过多 pair
- 过大的 multipart


### 2.4 Query、Header、Cookie 的扫描模式是统一的 offset 迭代

这组 API 的手感很接近：

- `xrtQueryNext*()`
- `xrtHttpHeaderNextLine*()`
- `xrtCookieNext*()`
- `xrtHttpParamNext*()`

统一模式都是：

```c
size_t iOffset = 0u;
while ( xrtQueryNextN(sText, iLen, &iOffset, &tPair) ) {
	/* 处理一项 */
}
```

这很好，因为你不需要为每一种文本块记三套不同解析习惯。


### 2.5 `Set-Cookie` 不是普通 cookie pair

这一点非常关键。

普通请求头里的：

- `Cookie: a=1; b=2`

适合：

- `xrtCookieNext*()`

但响应头里的：

- `Set-Cookie: sid=...; HttpOnly; Secure; SameSite=Lax`

应该走：

- `xrtSetCookieParse*()`

因为这里已经不只是 name/value，而是带属性的结构化 cookie 定义。


### 2.6 `Content-Type` 不只是字符串匹配，应该先进 `xrtmediatypeview`

很多代码会直接写：

- `if ( strstr(sType, "application/json") )`

这不稳。

更正式的路径是：

- `xrtHttpMediaTypeParse*()`
- `xrtHttpMediaTypeFindParam*()`

这样你才能明确知道：

- `type/subtype`
- `suffix`
- `charset`
- `boundary`

这些字段分别是什么。


### 2.7 multipart 小包可以整体验证，大包要尽早切到流式

当前这组能力分两层：

| 能力 | 适合什么 |
|------|----------|
| `xrtMultipartValidate*()` | 小体积、整包字符串、先做合法性判断 |
| `xrtmultipartstream*` | 大 body、上传、流式喂入 |

也就是说：

- 你能一次性拿完整 body
	- 可以先整体验证
- 你在 HTTP server 里边收边处理
	- 就别再假设自己总能先把整包拼好


### 2.8 视图类型默认也是借用原始缓冲区

像这些类型：

- `xrtheaderpair`
- `xrtquerypair`
- `xrtcookiepair`
- `xrthttpparam`
- `xrtsetcookieview`
- `xrtmediatypeview`
- `xrtmultipartpartview`

本质上也都是：

- 借原始文本
- 不自动复制

所以同样的规则还成立：

- 如果要长期持有，就自己复制


## 3. 最小 DEMO：先把 query 和 header block 扫干净

第一步先别碰 multipart。  
先把最常见的两件事跑通：

1. 扫 query
2. 扫 header block

```c
size_t iOffset = 0u;
xrtquerypair tQuery;
xrtheaderpair tHeader;

while ( xrtQueryNext("model=gpt-5&stream=true", &iOffset, &tQuery) ) {
	/* 处理每个 query pair */
}

iOffset = 0u;
while ( xrtHttpHeaderNextLine("Host: api.example.com\r\nAccept: application/json\r\n", &iOffset, &tHeader) ) {
	/* 处理每个 header line */
}
```

这一步的重点是建立 offset 迭代心智，而不是一上来就手搓分隔符。


## 4. 升级 DEMO：限制、Media Type、`Set-Cookie` 和 multipart

第二步再把更真实的边界接上：

1. 初始化 limits
2. 校验 header block
3. 解析 `Content-Type`
4. 解析 `Set-Cookie`
5. 需要上传时切到 multipart validate / stream

```c
xrthttputillimits tLimits;
xrtmediatypeview tType;
xrtsetcookieview tCookie;

xrtHttpUtilLimitsInit(&tLimits);
tLimits.iMaxHeaderBytes = 32u * 1024u;
tLimits.iMaxMultipartBytes = 16u * 1024u * 1024u;

if ( xrtHttpHeaderBlockValidate("Content-Type: multipart/form-data; boundary=abc\r\n", &tLimits) ) {
	xrtHttpMediaTypeParse("multipart/form-data; boundary=abc", &tType);
}

xrtSetCookieParse("sid=abc; Path=/; HttpOnly; SameSite=Lax", &tCookie);
```

如果后面 body 已经是大文件上传，下一步就不该再停在整包字符串模型，而应该切到：

- `xrtmultipartstream`


## 5. 什么时候该用哪一层

| 场景 | 优先入口 |
|------|----------|
| 想逐项扫 query | `xrtQueryNext*()` |
| 想逐项扫 header line | `xrtHttpHeaderNextLine*()` |
| 想逐项扫 Cookie | `xrtCookieNext*()` |
| 想处理 `Set-Cookie` 属性 | `xrtSetCookieParse*()` |
| 想处理 `Content-Type` / boundary / charset | `xrtHttpMediaTypeParse*()` |
| 想先挡住过大 header / multipart | `Validate*()` + `xrthttputillimits` |
| 小 multipart body 先验合法性 | `xrtMultipartValidate*()` |
| 大上传或边收边处理 | `xrtmultipartstream*` |


## 6. 常见错误

### 6.1 把 `Set-Cookie` 当普通 cookie 列表

这会直接丢掉：

- `HttpOnly`
- `Secure`
- `SameSite`
- `Path`
- `Domain`


### 6.2 完全不设 limits

文本工具层最怕的就是：

- 大输入
- 多 pair
- 特殊构造 header bomb


### 6.3 用 `strstr()` 判 `Content-Type`

你真正需要的是结构化 media type，而不只是“里面好像出现了 json”。


### 6.4 把 multipart 一律当整包字符串

小 demo 能跑，不代表上传场景能撑住。


## 7. 下一步阅读

建议按这条线继续：

- [Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序](proxy-intro.md)
- [HTTP Util API](../api/api-http-util.md)
- [XURL 入门：什么时候先把 URL 拆对，比直接发请求更重要](xurl-intro.md)
- [HTTP + JSON + Template 完整链路](../case/http-json-template-chain.md)
