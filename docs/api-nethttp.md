# NetHTTP HTTP客户端

> 基于 Socket + TLS 的 HTTP/1.1 客户端封装，支持 HTTPS、重定向、Chunked 传输

[English](api-nethttp.en.md) | [中文](api-nethttp.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [URL解析](#url解析)
- [极简API](#极简api)
- [完整API - 请求对象](#完整api---请求对象)
- [完整API - 响应对象](#完整api---响应对象)
- [Cookie管理](#cookie管理)
- [Multipart表单](#multipart表单)
- [使用示例](#使用示例)

---

## 功能概述

**NetHTTP** 是一个功能完整的 HTTP/1.1 客户端，主要特性：

- 🌐 **支持 HTTP/HTTPS**：基于 nettcp.h + nettls.h 实现
- 🔄 **自动重定向**：支持 301/302/303/307/308 重定向
- 📦 **Chunked 传输**：支持 Transfer-Encoding: chunked
- 🍪 **Cookie 管理**：自动处理 Set-Cookie 和发送 Cookie
- 📝 **Multipart 表单**：支持表单字段和文件上传
- 🎯 **进度回调**：支持数据接收进度回调
- ⚡ **同步阻塞模型**：简单易用的阻塞式 API
- 🔒 **可选 SSL 验证**：支持证书验证开关

**依赖模块**：
- `netsock.h` - Socket 基础
- `nettls.h` - TLS 协议
- `buffer.h` - 自增缓冲区

---

## 数据类型

### xhttpreq

HTTP 请求对象，用于构建和发送 HTTP 请求。

**定义**：
```c
typedef struct xhttpreq_struct* xhttpreq;
```

**内存管理**：
- 创建：`xrtHttpReqCreate()`
- 释放：`xrtHttpReqFree()`

### xhttpresp

HTTP 响应对象，包含响应状态、头部和正文。

**定义**：
```c
typedef struct xhttpresp_struct* xhttpresp;
```

**内存管理**：
- 释放：`xrtHttpRespFree()` - 必须调用

### xurl

URL 解析结果结构体。

**定义**：
```c
typedef struct xurl_struct {
    bool bHttps;          // 是否为 HTTPS
    char sHost[256];      // 主机名
    uint16 iPort;         // 端口号
    char sPath[2048];     // 路径（含查询参数）
} xurl_struct, *xurl;
```

### xhttp_method

HTTP 请求方法枚举。

**定义**：
```c
typedef enum {
    XHTTP_GET,     // GET 请求
    XHTTP_POST,    // POST 请求
    XHTTP_PUT,     // PUT 请求
    XHTTP_DELETE,  // DELETE 请求
    XHTTP_PATCH,   // PATCH 请求
    XHTTP_HEAD     // HEAD 请求
} xhttp_method;
```

### xhttp_proc

HTTP 数据接收进度回调函数类型。

**定义**：
```c
typedef bool (*xhttp_proc)(xbuffer pData, size_t iTotal, size_t iReceived);
```

**参数说明**：
- `pData` - 当前接收的数据缓冲区
- `iTotal` - 总数据大小（Content-Length，未知时为0）
- `iReceived` - 已接收的数据量

**返回值**：
- `true` - 继续接收
- `false` - 中止接收

---

## 常量定义

### 默认配置

| 常量 | 值 | 说明 |
|------|-----|------|
| 默认超时 | 10秒 | 连接和读取超时 |
| 默认重定向次数 | 5次 | 最大重定向次数 |
| 默认 SSL 验证 | true | 是否验证 SSL 证书 |

---

## URL解析

### 函数：xrtUrlParse

**函数原型**：
```c
XXAPI bool xrtUrlParse(str sURL, xurl pOut);
```

**功能**：
解析 URL 字符串为 xurl 结构体。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sURL` | `str` | URL 字符串 | 否 |
| `pOut` | `xurl` | 输出结构体指针 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 解析成功 |
| `false` | 解析失败 |

**示例**：
```c
xurl_struct tURL;
if (xrtUrlParse("https://example.com:443/api/v1/users?id=123", &tURL)) {
    printf("Host: %s\n", tURL.sHost);
    printf("Port: %d\n", tURL.iPort);
    printf("Path: %s\n", tURL.sPath);
    printf("HTTPS: %s\n", tURL.bHttps ? "yes" : "no");
}
```

---

## 极简API

### 函数：xrtHttpGet

**函数原型**：
```c
XXAPI xhttpresp xrtHttpGet(str sURL, str sHeaders, xhttp_proc pProc);
```

**功能**：
发送 HTTP GET 请求。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sURL` | `str` | 请求 URL | 否 |
| `sHeaders` | `str` | 自定义请求头 | 是 |
| `pProc` | `xhttp_proc` | 进度回调函数 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 响应对象（需使用 `xrtHttpRespFree` 释放） |
| `NULL` | 请求失败 |

**示例**：
```c
xhttpresp resp = xrtHttpGet("https://api.example.com/users", NULL, NULL);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
```

### 函数：xrtHttpPost

**函数原型**：
```c
XXAPI xhttpresp xrtHttpPost(str sURL, str sBody, str sHeaders, xhttp_proc pProc);
```

**功能**：
发送 HTTP POST 请求。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sURL` | `str` | 请求 URL | 否 |
| `sBody` | `str` | 请求正文 | 否 |
| `sHeaders` | `str` | 自定义请求头 | 是 |
| `pProc` | `xhttp_proc` | 进度回调函数 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 响应对象（需使用 `xrtHttpRespFree` 释放） |
| `NULL` | 请求失败 |

**示例**：
```c
str json = "{\"name\":\"Alice\",\"age\":25}";
xhttpresp resp = xrtHttpPost(
    "https://api.example.com/users",
    json,
    "Content-Type: application/json\r\n",
    NULL
);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
xrtFree(json);
```

### 函数：xrtHttpGetFile

**函数原型**：
```c
XXAPI bool xrtHttpGetFile(str sURL, str sFilePath, str sHeaders, xhttp_proc pProc);
```

**功能**：
下载文件到本地路径。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sURL` | `str` | 下载 URL | 否 |
| `sFilePath` | `str` | 保存路径 | 否 |
| `sHeaders` | `str` | 自定义请求头 | 是 |
| `pProc` | `xhttp_proc` | 进度回调函数 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 下载成功（HTTP 状态码 200-299） |
| `false` | 下载失败 |

**示例**：
```c
bool ok = xrtHttpGetFile(
    "https://example.com/file.zip",
    "file.zip",
    NULL,
    NULL
);
if (ok) {
    printf("Download successful!\n");
} else {
    printf("Download failed!\n");
}
```

### 函数：xrtHttpPostFile

**函数原型**：
```c
XXAPI bool xrtHttpPostFile(str sURL, str sBody,
    str sFilePath, str sHeaders, xhttp_proc pProc);
```

**功能**：
发送 POST 请求并保存响应到文件。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sURL` | `str` | 请求 URL | 否 |
| `sBody` | `str` | 请求正文 | 是 |
| `sFilePath` | `str` | 保存路径 | 否 |
| `sHeaders` | `str` | 自定义请求头 | 是 |
| `pProc` | `xhttp_proc` | 进度回调函数 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 请求成功（HTTP 状态码 200-299） |
| `false` | 请求失败 |

### 函数：xrtHttpRespFree

**函数原型**：
```c
XXAPI void xrtHttpRespFree(xhttpresp pResp);
```

**功能**：
释放响应对象及其关联的内存。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 是 |

---

## 完整API - 请求对象

### 函数：xrtHttpReqCreate

**函数原型**：
```c
XXAPI xhttpreq xrtHttpReqCreate(xhttp_method iMethod, str sURL);
```

**功能**：
创建 HTTP 请求对象。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `iMethod` | `xhttp_method` | HTTP 请求方法 | - |
| `sURL` | `str` | 请求 URL | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 请求对象（需使用 `xrtHttpReqFree` 释放） |
| `NULL` | 创建失败 |

### 函数：xrtHttpReqFree

**函数原型**：
```c
XXAPI void xrtHttpReqFree(xhttpreq pReq);
```

**功能**：
释放请求对象及其关联的内存。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 是 |

### 函数：xrtHttpReqSetHeader

**函数原型**：
```c
XXAPI void xrtHttpReqSetHeader(xhttpreq pReq, str sName, str sValue);
```

**功能**：
设置请求头。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sName` | `str` | 请求头名称 | 否 |
| `sValue` | `str` | 请求头值 | 否 |

**示例**：
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/data");
xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
xrtHttpReqSetHeader(req, "Accept", "application/json");
// ... 执行请求
xrtHttpReqFree(req);
```

### 函数：xrtHttpReqSetBody

**函数原型**：
```c
XXAPI void xrtHttpReqSetBody(xhttpreq pReq, str pData, size_t iLen, str sContentType);
```

**功能**：
设置请求正文和 Content-Type。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `pData` | `str` | 请求正文数据 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `sContentType` | `str` | Content-Type 值 | 是 |

**示例**：
```c
str json = "{\"name\":\"Alice\"}";
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/users");
xrtHttpReqSetBody(req, json, strlen(json), "application/json");
// ... 执行请求
xrtHttpReqFree(req);
xrtFree(json);
```

### 函数：xrtHttpReqAddField

**函数原型**：
```c
XXAPI void xrtHttpReqAddField(xhttpreq pReq, str sName, str sValue);
```

**功能**：
添加 URL 编码的表单字段（用于 x-www-form-urlencoded）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sName` | `str` | 字段名 | 否 |
| `sValue` | `str` | 字段值 | 否 |

**说明**：
- 字段名和值会自动进行 URL 编码
- 多个字段会自动用 `&` 连接
- 使用此函数后，Content-Type 会自动设置为 `application/x-www-form-urlencoded`

**示例**：
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/form");
xrtHttpReqAddField(req, "username", "alice@example.com");
xrtHttpReqAddField(req, "password", "secret123");
// ... 执行请求
xrtHttpReqFree(req);
```

### 函数：xrtHttpReqSetTimeout

**函数原型**：
```c
XXAPI void xrtHttpReqSetTimeout(xhttpreq pReq, int iTimeoutSec);
```

**功能**：
设置连接和读取超时时间。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `iTimeoutSec` | `int` | 超时时间（秒） | - |

### 函数：xrtHttpReqSetRedirect

**函数原型**：
```c
XXAPI void xrtHttpReqSetRedirect(xhttpreq pReq, int iMaxRedirects);
```

**功能**：
设置最大重定向次数。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `iMaxRedirects` | `int` | 最大重定向次数（0 = 不跟随重定向） | - |

### 函数：xrtHttpReqSetVerifySSL

**函数原型**：
```c
XXAPI void xrtHttpReqSetVerifySSL(xhttpreq pReq, bool bVerify);
```

**功能**：
设置是否验证 SSL 证书。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `bVerify` | `bool` | 是否验证证书 | - |

**说明**：
- 生产环境建议设置为 `true`
- 测试环境可设置为 `false` 以避免证书问题

### 函数：xrtHttpReqSetCallback

**函数原型**：
```c
XXAPI void xrtHttpReqSetCallback(xhttpreq pReq, xhttp_proc pProc);
```

**功能**：
设置数据接收进度回调函数。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `pProc` | `xhttp_proc` | 回调函数 | 是 |

**示例**：
```c
bool progress_cb(xbuffer pData, size_t iTotal, size_t iReceived) {
    printf("Progress: %zu/%zu (%.1f%%)\n", iReceived, iTotal, iTotal > 0 ? iReceived * 100.0 / iTotal : 0);
    return true;  // 继续接收
}

xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://example.com/file.zip");
xrtHttpReqSetCallback(req, progress_cb);
// ... 执行请求
xrtHttpReqFree(req);
```

### 函数：xrtHttpReqSetUserData

**函数原型**：
```c
XXAPI void xrtHttpReqSetUserData(xhttpreq pReq, ptr pData);
```

**功能**：
设置用户数据指针（在回调中使用）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `pData` | `ptr` | 用户数据指针 | 是 |

### 函数：xrtHttpReqExecute

**函数原型**：
```c
XXAPI xhttpresp xrtHttpReqExecute(xhttpreq pReq);
```

**功能**：
执行 HTTP 请求。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 响应对象（需使用 `xrtHttpRespFree` 释放） |
| `NULL` | 请求失败 |

**示例**：
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/users");
xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
xhttpresp resp = xrtHttpReqExecute(req);
if (resp) {
    printf("Status: %d\n", xrtHttpRespCode(resp));
    printf("Body: %s\n", xrtHttpRespBody(resp));
    xrtHttpRespFree(resp);
}
xrtHttpReqFree(req);
```

---

## 完整API - 响应对象

### 函数：xrtHttpRespCode

**函数原型**：
```c
XXAPI int xrtHttpRespCode(xhttpresp pResp);
```

**功能**：
获取 HTTP 状态码。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |

**返回值**：
HTTP 状态码（如 200, 404, 500 等）

### 函数：xrtHttpRespBody

**函数原型**：
```c
XXAPI str xrtHttpRespBody(xhttpresp pResp);
```

**功能**：
获取响应正文。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |

**返回值**：
响应正文字符串（无需释放）

### 函数：xrtHttpRespBodyLen

**函数原型**：
```c
XXAPI size_t xrtHttpRespBodyLen(xhttpresp pResp);
```

**功能**：
获取响应正文长度。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |

**返回值**：
响应正文长度（字节）

### 函数：xrtHttpRespHeader

**函数原型**：
```c
XXAPI str xrtHttpRespHeader(xhttpresp pResp, str sName);
```

**功能**：
获取响应头值（不区分大小写）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |
| `sName` | `str` | 响应头名称 | 否 |

**返回值**：
响应头值（临时缓冲区，无需释放）

**示例**：
```c
str ct = xrtHttpRespHeader(resp, "Content-Type");
str cl = xrtHttpRespHeader(resp, "Content-Length");
printf("Content-Type: %s\n", ct);
printf("Content-Length: %s\n", cl);
```

### 函数：xrtHttpRespCookie

**函数原型**：
```c
XXAPI str xrtHttpRespCookie(xhttpresp pResp, str sName);
```

**功能**：
获取响应中的 Cookie 值。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |
| `sName` | `str` | Cookie 名称 | 否 |

**返回值**：
Cookie 值（临时缓冲区，无需释放）

### 函数：xrtHttpRespContentType

**函数原型**：
```c
XXAPI str xrtHttpRespContentType(xhttpresp pResp);
```

**功能**：
获取响应 Content-Type。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pResp` | `xhttpresp` | 响应对象 | 否 |

**返回值**：
Content-Type 值（无需释放）

---

## Cookie管理

### 函数：xrtHttpReqEnableCookies

**函数原型**：
```c
XXAPI void xrtHttpReqEnableCookies(xhttpreq pReq, bool bEnable);
```

**功能**：
启用或禁用 Cookie 管理。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `bEnable` | `bool` | 是否启用 | - |

**说明**：
- 启用后，响应中的 Set-Cookie 会自动保存
- 下次请求会自动发送保存的 Cookie

### 函数：xrtHttpReqSetCookie

**函数原型**：
```c
XXAPI void xrtHttpReqSetCookie(xhttpreq pReq, str sName, str sValue);
```

**功能**：
设置请求 Cookie。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sName` | `str` | Cookie 名称 | 否 |
| `sValue` | `str` | Cookie 值 | 否 |

### 函数：xrtHttpReqRemoveCookie

**函数原型**：
```c
XXAPI void xrtHttpReqRemoveCookie(xhttpreq pReq, str sName);
```

**功能**：
移除请求 Cookie。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sName` | `str` | Cookie 名称 | 否 |

---

## Multipart表单

### 函数：xrtHttpReqAddFormField

**函数原型**：
```c
XXAPI void xrtHttpReqAddFormField(xhttpreq pReq, str sName, str sValue);
```

**功能**：
添加 Multipart 表单字段。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sName` | `str` | 字段名 | 否 |
| `sValue` | `str` | 字段值 | 否 |

**说明**：
- 使用此函数会自动启用 Multipart 模式
- Content-Type 会自动设置为 `multipart/form-data; boundary=...`

### 函数：xrtHttpReqAddFormFile

**函数原型**：
```c
XXAPI void xrtHttpReqAddFormFile(xhttpreq pReq, str sFieldName,
    str sFilePath, str sMimeType);
```

**功能**：
添加文件上传字段。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sFieldName` | `str` | 表单字段名 | 否 |
| `sFilePath` | `str` | 文件路径 | 否 |
| `sMimeType` | `str` | MIME 类型（可选） | 是 |

**说明**：
- 文件会自动读取到内存
- 如果不指定 MIME 类型，默认使用 `application/octet-stream`

**示例**：
```c
xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/upload");
xrtHttpReqAddFormField(req, "description", "A file");
xrtHttpReqAddFormFile(req, "file", "data.txt", "text/plain");
// ... 执行请求
xrtHttpReqFree(req);
```

### 函数：xrtHttpReqAddFormData

**函数原型**：
```c
XXAPI void xrtHttpReqAddFormData(xhttpreq pReq, str sFieldName,
    str sFileName, str pData, size_t iLen, str sMimeType);
```

**功能**：
添加内存中的数据作为文件上传。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pReq` | `xhttpreq` | 请求对象 | 否 |
| `sFieldName` | `str` | 表单字段名 | 否 |
| `sFileName` | `str` | 文件名 | 是 |
| `pData` | `str` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `sMimeType` | `str` | MIME 类型（可选） | 是 |

---

## 使用示例

### 示例1：RESTful API 调用

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

int main() {
    xrtInit();

    // GET 请求
    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/users/1");
    xrtHttpReqSetHeader(req, "Authorization", "Bearer token123");
    xrtHttpReqSetHeader(req, "Accept", "application/json");

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp && xrtHttpRespCode(resp) == 200) {
        printf("User Data: %s\n", xrtHttpRespBody(resp));
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### 示例2：文件下载

```c
bool progress_cb(xbuffer pData, size_t iTotal, size_t iReceived) {
    printf("\rDownloaded: %zu/%zu bytes", iReceived, iTotal);
    fflush(stdout);
    return true;
}

int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://example.com/file.zip");
    xrtHttpReqSetCallback(req, progress_cb);
    xrtHttpReqSetTimeout(req, 30);

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp && xrtHttpRespCode(resp) == 200) {
        // 保存文件
        FILE* fp = fopen("file.zip", "wb");
        if (fp) {
            fwrite(xrtHttpRespBody(resp), 1, xrtHttpRespBodyLen(resp), fp);
            fclose(fp);
            printf("\nDownload complete!\n");
        }
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### 示例3：Multipart 表单上传

```c
int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_POST, "https://api.example.com/upload");
    xrtHttpReqAddFormField(req, "username", "alice");
    xrtHttpReqAddFormField(req, "description", "Profile picture");

    // 上传文件
    xrtHttpReqAddFormFile(req, "avatar", "avatar.jpg", "image/jpeg");

    xhttpresp resp = xrtHttpReqExecute(req);
    if (resp) {
        printf("Status: %d\n", xrtHttpRespCode(resp));
        printf("Response: %s\n", xrtHttpRespBody(resp));
    }

    xrtHttpRespFree(resp);
    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

### 示例4：使用 Cookie

```c
int main() {
    xrtInit();

    xhttpreq req = xrtHttpReqCreate(XHTTP_GET, "https://api.example.com/profile");
    xrtHttpReqEnableCookies(req, true);

    // 首次请求，会自动保存响应中的 Set-Cookie
    xhttpresp resp1 = xrtHttpReqExecute(req);
    xrtHttpRespFree(resp1);

    // 第二次请求，会自动发送保存的 Cookie
    xhttpresp resp2 = xrtHttpReqExecute(req);
    if (resp2) {
        printf("Status: %d\n", xrtHttpRespCode(resp2));
        printf("Response: %s\n", xrtHttpRespBody(resp2));
        xrtHttpRespFree(resp2);
    }

    xrtHttpReqFree(req);
    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **内存管理**：
   - 请求对象和响应对象必须使用对应的 Free 函数释放
   - Cookie 字符串会在释放请求对象时自动释放
   - 响应头和 Cookie 返回值使用临时缓冲区，无需释放

2. **超时设置**：
   - 连接超时和读取超时使用相同的超时值
   - 超时时间过短可能导致大文件下载失败

3. **重定向**：
   - 301/302/303 会自动转换为 GET 请求
   - 307/308 会保持原请求方法和请求正文

4. **HTTPS**：
   - 默认启用 SSL 证书验证
   - 测试环境可使用 `xrtHttpReqSetVerifySSL(req, false)` 关闭验证

5. **性能**：
   - 同步阻塞模型，不适合高并发场景
   - 大文件下载建议使用进度回调

---

## 相关函数

- [xrtSockCreate](api-nettcp.md) - 创建 Socket
- [xrtTlsCreate](api-nettls.md) - 创建 TLS 上下文
- [xrtBufferInit](api-buffer.md) - 初始化缓冲区
- [xrtDictCreate](api-dict.md) - 创建字典（用于 Cookie 存储）

---

<div align="center">

**XRT HTTP Client** | 版本 1.0 | 最后更新: 2025-01

</div>
