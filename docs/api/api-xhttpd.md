# XHTTPD HTTP/1.1 服务端主线

> 基于 `xnet-v2 + xnet_stream + xtlssession` 的当前 HTTP/1.1 服务端主线。

[返回索引](README.md)

---

## 1. 定位

`xhttpd` 是当前主线 HTTP 服务端。

它负责：

- 监听端口
- 接受连接
- 解析 HTTP/1.1 请求
- 物化 request / response 对象
- 支持明文 HTTP 和 TLS 服务路径
- 与 `xnetengine`、协程、future 主线配合

它的设计目标是：

- 轻量但可直接用于服务开发
- 保留基础设施层的可控性
- 不把路由、静态文件、模板系统强行耦合进核心服务端


## 2. 核心类型

### `xhttpdconfig`

核心字段包括：

- `tBindAddr`
- `iFlags`
- `iBacklog`
- `iRecvLimit`
- `pTlsConfig`

用途：

- 设置监听地址与端口
- 调整 backlog
- 控制接收上限
- 开启 TLS 服务


### `xhttpdrequest`

当前请求对象包含：

- `iFlags`
- `iHeaderCount`
- `iContentLength`
- `sMethod`
- `sTarget`
- `sPath`
- `sQuery`
- `sVersion`
- `arrHeaders`
- `pBody / iBodyLen`


### `xhttpdresponse`

当前响应对象包含：

- `iStatusCode`
- `iFlags`
- `iHeaderCount`
- `sReason`
- `arrHeaders`
- `pBody / iBodyLen`


### `xhttpdevents`

当前服务端事件模型：

```c
typedef struct {
	void (*OnOpen)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn);
	bool (*OnRequest)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp);
	void (*OnClose)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, xnet_result iReason);
	void (*OnError)(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, int iSysErr);
} xhttpdevents;
```

其中最关键的是：

- `OnRequest`

它的职责是：

- 读取请求
- 填充响应
- 返回是否已成功产出响应


## 3. 当前正式 API

### 配置与请求/响应对象

```c
XXAPI void xrtHttpdConfigInit(xhttpdconfig* pCfg);
XXAPI void xrtHttpdRequestInit(xhttpdrequest* pReq);
XXAPI void xrtHttpdRequestUnit(xhttpdrequest* pReq);
XXAPI void xrtHttpdResponseInit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseUnit(xhttpdresponse* pResp);
XXAPI void xrtHttpdResponseSetStatus(xhttpdresponse* pResp, uint32 iStatusCode, const char* sReason);
XXAPI bool xrtHttpdResponseSetHeader(xhttpdresponse* pResp, const char* sName, const char* sValue);
XXAPI bool xrtHttpdResponseSetBodyCopy(xhttpdresponse* pResp, const void* pData, size_t iLen, const char* sContentType);
XXAPI const char* xrtHttpdRequestHeader(const xhttpdrequest* pReq, const char* sName);
XXAPI const char* xrtHttpdResponseHeader(const xhttpdresponse* pResp, const char* sName);
```


### 服务端生命周期

```c
XXAPI xhttpdserver* xrtHttpdCreate(xnetengine* pEngine, const xhttpdconfig* pCfg, const xhttpdevents* pEvents, ptr pUserData);
XXAPI uint16 xrtHttpdBoundPort(const xhttpdserver* pServer);
XXAPI xnet_result xrtHttpdStart(xhttpdserver* pServer);
XXAPI void xrtHttpdStop(xhttpdserver* pServer);
XXAPI void xrtHttpdDestroy(xhttpdserver* pServer);
```

推荐流程：

1. `ConfigInit`
2. 填 bind 地址、TLS 配置、接收上限
3. 准备 `xhttpdevents`
4. `xrtHttpdCreate`
5. `xrtHttpdStart`
6. 停止时：
	- `xrtHttpdStop`
	- `xrtHttpdDestroy`


## 4. 常见用法

### 4.1 最小 HTTP 服务

```c
static bool procOnRequest(ptr pOwner, xhttpdserver* pServer, xhttpdconn* pConn, const xhttpdrequest* pReq, xhttpdresponse* pResp)
{
	xrtHttpdResponseSetStatus(pResp, 200u, "OK");
	xrtHttpdResponseSetBodyCopy(pResp, "hello", 5u, "text/plain; charset=utf-8");
	return true;
}
```


### 4.2 设置 JSON 响应

```c
xrtHttpdResponseSetStatus(pResp, 200u, "OK");
xrtHttpdResponseSetHeader(pResp, "Cache-Control", "no-store");
xrtHttpdResponseSetBodyCopy(pResp, sJson, strlen(sJson), "application/json");
```


### 4.3 读取请求头

```c
const char* sAuth = xrtHttpdRequestHeader(pReq, "Authorization");
```


## 5. 与其他模块的关系

### 与 `xnet_stream`

- 每个 HTTP 连接建立在 stream 上
- keep-alive、close、TLS、收发链都来自 stream 主线

### 与 `xtlssession`

- `pTlsConfig != NULL` 时进入 TLS 服务路径

### 与 `xhttp_util`

- header / token / target / multipart 等底层能力可直接复用

### 与模板 / JSON / Value

- `xhttpd` 本身不强耦合这些模块
- 但它是这些上层能力最自然的承载服务端

这意味着：

- 如果返回 JSON，推荐先组织 `xvalue`，再序列化输出
- 如果返回 HTML，推荐先组织 `xvalue`，再交给 template 渲染

## 6. 当前最推荐的使用方式

当前更推荐把 `xhttpd` 当成：

- 最小 HTTP 服务的正式入口
- JSON API 的正式入口
- 后续模板渲染、AI 结果展示、后台管理页面等能力的上层承载者

而不是继续从底层 socket / HTTP 解析手工搭服务。

## 7. 当前边界

当前 `xhttpd` 文档覆盖的是：

- 当前 HTTP/1.1 服务端主线
- request / response 物化
- 监听与生命周期

当前仍明确 deferred 的能力包括：

- 通用路由表
- 静态文件服务
- 泛型 upgrade 分发
- 更完整的流式 request / response body

## 8. 建议继续阅读

- [XNet V2](api-xnet-v2.md)
- [Value](api-value.md)
- [JSON](api-json.md)
- [Template](api-template.md)
