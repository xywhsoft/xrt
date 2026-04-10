# XURL URL 与查询构建层

> 面向当前网络主线的 URL / authority / target 解析与构建工具。

[返回索引](README.md)

---

## 1. 定位

`xurl` 负责解决这些问题：

- 解析完整 URL
- 解析 authority（`userinfo@host:port`）
- 解析 target（`/path?query#fragment`）
- 判断 scheme、默认端口、安全 scheme
- 构建 `Host` 头
- 归一化路径
- 基于 base URL 解析相对引用

这层能力主要给：

- `xhttp`
- `xhttpd`
- `xws`
- `xnet-v2`

以及你自己的上层业务代码使用。


## 2. 核心类型

### `xrtstrview`

```c
typedef struct {
	const char* sPtr;
	size_t iLen;
} xrtstrview;
```

这是只读字符串视图。

它的特点是：

- 不负责内存分配
- 不要求输入字符串必须以 `\0` 结尾
- 适合解析器返回“借用视图”


### `xrturlview`

`xrturlview` 是 URL 解析结果视图，内部通过多个 `xrtstrview` 字段表达：

- `tScheme`
- `tAuthority`
- `tUserInfo`
- `tHost`
- `tPath`
- `tQuery`
- `tFragment`
- `iPort`
- `iFlags`

这类 API 的共同特点是：

- 解析结果默认**借用原始输入缓冲区**
- 如果需要长期持有，应自行复制


### URL 标志位

当前主线会用到这些标志：

```c
#define XRT_URL_F_NONE
#define XRT_URL_F_ABSOLUTE
#define XRT_URL_F_TARGET_ONLY
#define XRT_URL_F_HAS_AUTHORITY
#define XRT_URL_F_HAS_USERINFO
#define XRT_URL_F_HAS_HOST
#define XRT_URL_F_HAS_PORT
#define XRT_URL_F_HAS_PATH
#define XRT_URL_F_HAS_QUERY
#define XRT_URL_F_HAS_FRAGMENT
#define XRT_URL_F_SECURE
```

常见理解方式：

- `ABSOLUTE`：完整 URL
- `TARGET_ONLY`：只有 target，没有 scheme / authority
- `SECURE`：`https` 或 `wss`


## 3. 解析 API

### 3.1 解析 authority

```c
XXAPI bool xrtUrlParseAuthorityN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParseAuthority(const char* sText, xrturlview* pOut);
```

适用：

- 只处理 `userinfo@host:port`
- 例如 HTTP `Host`、代理 authority、手工拼接的连接目标


### 3.2 解析 target

```c
XXAPI bool xrtUrlParseTargetN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParseTarget(const char* sText, xrturlview* pOut);
```

适用：

- 只处理 `/path?query#fragment`
- 不要求包含 scheme / authority


### 3.3 解析完整 URL 视图

```c
XXAPI bool xrtUrlParseViewN(const char* sText, size_t iLen, xrturlview* pOut);
XXAPI bool xrtUrlParseView(const char* sText, xrturlview* pOut);
```

适用：

- 解析完整 URL
- 保留所有子视图，供后续按字段使用


### 3.4 固定缓冲区解析

```c
XXAPI bool xrtUrlParseFixedTo(
	const char* sURL,
	const char* sSchemeA,
	const char* sSchemeB,
	bool* pSchemeB,
	char* sHost,
	size_t iHostCap,
	uint16* pPort,
	char* sTarget,
	size_t iTargetCap
);

XXAPI bool xrtUrlParse(const char* sURL, xurl pOut);
```

适用：

- 快速把 URL 解析到固定结构
- 适合客户端连接路径、配置读取、脚本式快速开发

`xrtUrlParseFixedTo()` 的典型场景：

- 限定只接受 `http/https`
- 直接拿到 `host/port/target`


## 4. 判断与查询 API

### 默认端口与安全 scheme

```c
XXAPI uint16 xrtUrlDefaultPort(xrtstrview tScheme);
XXAPI bool xrtUrlIsSecureScheme(xrtstrview tScheme);
XXAPI bool xrtUrlIsDefaultPort(const xrturlview* pURL);
```

用途：

- 自动补端口
- 判断是否应进入 TLS
- 构建 `Host` 头时决定是否省略端口


### scheme 判断

```c
XXAPI bool xrtUrlViewIsScheme(const xrturlview* pURL, const char* sScheme);
XXAPI bool xrtUrlViewMatchesScheme2(const xrturlview* pURL, const char* sSchemeA, const char* sSchemeB);
```

典型用法：

```c
if ( xrtUrlViewMatchesScheme2(&tURL, "http", "https") ) {
	/* HTTP 系列协议 */
}
```


## 5. 视图复制 API

当前主线提供了多组“视图复制到固定缓冲区”的函数：

```c
XXAPI bool xrtUrlViewCopySchemeTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyAuthorityTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyHostTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyPathTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyQueryTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyFragmentTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlViewCopyTargetTo(const xrturlview* pURL, char* sOut, size_t iOutCap);
```

推荐场景：

- 日志输出
- 构建网络请求
- 从 view 转到你自己的固定结构


## 6. 构建与规范化 API

### 构建 authority / target / 完整 URL

```c
XXAPI bool xrtUrlBuildTarget(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
XXAPI bool xrtUrlBuildAuthority(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
XXAPI bool xrtUrlBuild(const xrturlview* pURL, char* sOut, size_t iOutCap, size_t* pOutLen);
```

用途：

- 从解析视图重新构建 URL
- 对外输出统一格式
- 作为 normalize / resolve 后的最终串化步骤


### 构建 `Host` 头

```c
XXAPI bool xrtUrlMakeHostHeader(const xrturlview* pURL, char* sOut, size_t iOutCap);
XXAPI bool xrtUrlMakeHostHeaderFixed(const char* sScheme, const char* sHost, uint16 iPort, char* sOut, size_t iOutCap);
```

这组 API 的意义很直接：

- 自动处理默认端口省略
- 自动处理 IPv6 host 的方括号形式


### 路径归一化

```c
XXAPI bool xrtUrlNormalizePathTo(const char* sPath, size_t iLen, char* sOut, size_t iOutCap, size_t* pOutLen);
```

用途：

- 清理多余分隔符
- 归一化 `.` / `..`
- 让路径拼接与比较更稳定


### 基于 base URL 解析相对引用

```c
XXAPI bool xrtUrlResolveTo(
	const xrturlview* pBase,
	const char* sRef,
	size_t iRefLen,
	char* sOut,
	size_t iOutCap,
	size_t* pOutLen
);

XXAPI bool xrtUrlResolve(
	const xrturlview* pBase,
	const char* sRef,
	char* sOut,
	size_t iOutCap,
	size_t* pOutLen
);
```

适用：

- HTTP 跳转
- 页面资源相对地址补全
- WebSocket / API 客户端的相对路径解析


## 7. 常见用法

### 7.1 解析完整 URL 并提取 host / target

```c
xrturlview tURL;
char sHost[256];
char sTarget[2048];

if ( !xrtUrlParseView("https://example.com:8443/api/v1?a=1", &tURL) ) {
	return;
}

xrtUrlViewCopyHostTo(&tURL, sHost, sizeof(sHost));
xrtUrlViewCopyTargetTo(&tURL, sTarget, sizeof(sTarget));
```


### 7.2 生成 Host 头

```c
char sHostHeader[320];

if ( xrtUrlMakeHostHeader(&tURL, sHostHeader, sizeof(sHostHeader)) ) {
	/* Host: example.com:8443 */
}
```


### 7.3 解析并限制只允许 HTTP / HTTPS

```c
bool bHTTPS = false;
char sHost[256];
char sTarget[2048];
uint16 iPort = 0;

if ( !xrtUrlParseFixedTo(
	"https://example.com/path?q=1",
	"http",
	"https",
	&bHTTPS,
	sHost, sizeof(sHost),
	&iPort,
	sTarget, sizeof(sTarget)
) ) {
	return;
}
```


## 8. 使用建议

- 解析函数返回的是**视图**，不是独立副本
- 需要长期保存字段时，使用 `Copy*To`
- 如果你已经站在 `xhttp / xws / xnet_stream` 之上，优先让上层模块管理 URL 生命周期
- 如果只是做客户端连接、路由、重定向处理，`xurl` 已经足够，不需要自己再写一套 URL 解析器


## 9. 当前边界

当前 `xurl` 文档覆盖的是：

- URL / authority / target 解析
- 基础构建
- Host 头生成
- 相对引用解析

它的定位是：

- 互联网主线的通用 URL 基础设施
- 不是浏览器级 URL 标准兼容层

