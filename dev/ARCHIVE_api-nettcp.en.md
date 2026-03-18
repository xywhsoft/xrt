# NetTCP TCP Client/Server

> TCP client/server encapsulation based on Socket + EventLoop + IOCP/io_uring, supports TLS and event-driven

[English](api-nettcp.en.md) | [中文](api-nettcp.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [TCP 服务端 API](#tcp-服务端-api)
- [TCP 客户端 API](#tcp-客户端-api)
- [使用示例](#使用示例)

---

## 功能概述

**NetTCP** 是一个功能完整的 TCP 客户端/服务端库，主要特性：

- 🚀 **事件驱动架构**：基于 IOCP（Windows）和 io_uring（Linux）
- 🔄 **双模式 API**：简易版（内部线程）+ 高级版（共享事件循环）
- 🔐 **TLS 支持**：集成 TLS 1.3/1.2 协议
- 📦 **高效连接管理**：客户端槽位空闲栈 O(1) 分配/回收
- 💾 **环形接收缓冲区**：优化接收性能
- ⚡ **异步握手**：TLS 握手事件驱动化
- 🌐 **跨平台**：支持 Windows 和 Linux
- 🔧 **TCP_NODELAY**：可选禁用 Nagle 算法

**依赖模块**：
- `netsock.h` - Socket 基础
- `netpoll.h` - IO 多路复用
- `netloop.h` - 事件循环
- `nettls.h` - TLS 协议

---

## 数据类型

### xtcpserver

TCP 服务端对象。

**定义**：
```c
typedef struct xrt_tcp_server* xtcpserver;
```

**内存管理**：
- 创建：`xrtTcpServerCreate()` 或 `xrtTcpServerCreateEx()`
- 销毁：`xrtTcpServerDestroy()`

### xtcpclient

TCP 客户端对象。

**定义**：
```c
typedef struct xrt_tcp_client* xtcpclient;
```

**内存管理**：
- 创建：`xrtTcpClientCreate()` 或 `xrtTcpClientCreateEx()`
- 销毁：`xrtTcpClientDestroy()`

### xnetconfig

网络配置结构体。

**定义**：
```c
typedef struct xnetconfig {
    int iRecvBufSize;       // 接收缓冲区大小（默认 8192）
    int iSendBufSize;       // 发送缓冲区大小（默认 8192）
    int iMaxClients;        // 最大客户端数量（默认 256）
    int iPollTimeoutMs;     // 轮询超时毫秒（默认 10）
    int iConnectTimeoutMs;  // 连接超时毫秒（默认 5000）
    bool bNoDelay;          // 是否启用 TCP_NODELAY（默认 false）
} xnetconfig;
```

### xnetevents

网络事件回调结构体。

**定义**：
```c
typedef struct xnetevents {
    // 服务端回调
    void (*OnAccept)(xtcpserver* pServer, xnetconn* pConn);
    void (*OnRecv)(xtcpserver* pServer, xnetconn* pConn, const char* pData, size_t iLen);
    void (*OnSend)(xtcpserver* pServer, xnetconn* pConn, size_t iLen);
    void (*OnClose)(xtcpserver* pServer, xnetconn* pConn);

    // 客户端回调
    void (*OnConnect)(xtcpclient* pClient, xnetconn* pConn, bool bSuccess);
    void (*OnError)(xtcpclient* pClient, int iError);
} xnetevents;
```

### xtlsconfig

TLS 配置结构体。

**定义**：
```c
typedef struct xtlsconfig {
    const char* sCertFile;      // 证书文件路径（可选）
    const char* sKeyFile;       // 私钥文件路径（可选）
    bool bVerifyPeer;           // 是否验证对端证书（默认 false）
    bool bVerifyHost;           // 是否验证主机名（默认 false）
} xtlsconfig;
```

---

## 常量定义

### 默认配置

| 常量 | 值 | 说明 |
|------|-----|------|
| 默认接收缓冲区 | 8192 字节 | 接收缓冲区大小 |
| 默认发送缓冲区 | 8192 字节 | 发送缓冲区大小 |
| 默认最大客户端数 | 256 | 服务端最大连接数 |
| 默认轮询超时 | 10 毫秒 | 事件轮询超时 |
| 默认连接超时 | 5000 毫秒 | 客户端连接超时 |

---

## TCP 服务端 API

### 函数：xrtTcpServerCreate

**函数原型**：
```c
XXAPI xtcpserver* xrtTcpServerCreate(const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 TCP 服务端（简易模式，内部创建事件循环和线程）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sIP` | `const char*` | 监听 IP 地址（如 "0.0.0.0"） | 否 |
| `iPort` | `uint16` | 监听端口 | - |
| `pConfig` | `const xnetconfig*` | 网络配置（NULL 使用默认值） | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 服务端对象（需使用 `xrtTcpServerDestroy` 释放） |
| `NULL` | 创建失败 |

### 函数：xrtTcpServerCreateEx

**函数原型**：
```c
XXAPI xtcpserver* xrtTcpServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 TCP 服务端（高级模式，共享事件循环）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pLoop` | `xeventloop*` | 事件循环对象 | 否 |
| `sIP` | `const char*` | 监听 IP 地址 | 否 |
| `iPort` | `uint16` | 监听端口 | - |
| `pConfig` | `const xnetconfig*` | 网络配置 | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 服务端对象 |
| `NULL` | 创建失败 |

### 函数：xrtTcpServerDestroy

**函数原型**：
```c
XXAPI void xrtTcpServerDestroy(xtcpserver* pServer);
```

**功能**：
销毁 TCP 服务端，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 是 |

### 函数：xrtTcpServerStart

**函数原型**：
```c
XXAPI xnet_result xrtTcpServerStart(xtcpserver* pServer);
```

**功能**：
启动 TCP 服务端，开始监听连接。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 启动成功 |
| `XRT_NET_ERROR` | 启动失败 |

### 函数：xrtTcpServerStop

**函数原型**：
```c
XXAPI void xrtTcpServerStop(xtcpserver* pServer);
```

**功能**：
停止 TCP 服务端，关闭所有客户端连接。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 是 |

### 函数：xrtTcpServerSend

**函数原型**：
```c
XXAPI xnet_result xrtTcpServerSend(xtcpserver* pServer, int iClientId,
    const char* pData, size_t iLen);
```

**功能**：
向指定客户端发送数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 否 |
| `iClientId` | `int` | 客户端 ID（由 `pConn->iId` 获取） | - |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 发送成功 |
| `XRT_NET_ERROR` | 发送失败 |

### 函数：xrtTcpServerDisconnect

**函数原型**：
```c
XXAPI void xrtTcpServerDisconnect(xtcpserver* pServer, int iClientId);
```

**功能**：
断开指定客户端连接。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 是 |
| `iClientId` | `int` | 客户端 ID | - |

### 函数：xrtTcpServerEnableTLS

**函数原型**：
```c
XXAPI xnet_result xrtTcpServerEnableTLS(xtcpserver* pServer, const xtlsconfig* pConfig);
```

**功能**：
为服务端启用 TLS（必须在 Start 之前调用）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 否 |
| `pConfig` | `const xtlsconfig*` | TLS 配置 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 启用成功 |
| `XRT_NET_ERROR` | 启用失败 |

### 函数：xrtTcpServerGetClientCount

**函数原型**：
```c
XXAPI int xrtTcpServerGetClientCount(xtcpserver* pServer);
```

**功能**：
获取当前连接的客户端数量。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 是 |

**返回值**：
当前客户端数量

### 函数：xrtTcpServerSetUserData

**函数原型**：
```c
XXAPI void xrtTcpServerSetUserData(xtcpserver* pServer, ptr pData);
```

**功能**：
设置服务端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 否 |
| `pData` | `ptr` | 用户数据指针 | 是 |

### 函数：xrtTcpServerGetUserData

**函数原型**：
```c
XXAPI ptr xrtTcpServerGetUserData(xtcpserver* pServer);
```

**功能**：
获取服务端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xtcpserver*` | 服务端对象 | 是 |

**返回值**：
用户数据指针

---

## TCP 客户端 API

### 函数：xrtTcpClientCreate

**函数原型**：
```c
XXAPI xtcpclient* xrtTcpClientCreate(const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 TCP 客户端（简易模式，内部创建事件循环和线程）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `sIP` | `const char*` | 服务器 IP 地址 | 否 |
| `iPort` | `uint16` | 服务器端口 | - |
| `pConfig` | `const xnetconfig*` | 网络配置 | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 客户端对象（需使用 `xrtTcpClientDestroy` 释放） |
| `NULL` | 创建失败 |

### 函数：xrtTcpClientCreateEx

**函数原型**：
```c
XXAPI xtcpclient* xrtTcpClientCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 TCP 客户端（高级模式，共享事件循环）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pLoop` | `xeventloop*` | 事件循环对象 | 否 |
| `sIP` | `const char*` | 服务器 IP 地址 | 否 |
| `iPort` | `uint16` | 服务器端口 | - |
| `pConfig` | `const xnetconfig*` | 网络配置 | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 客户端对象 |
| `NULL` | 创建失败 |

### 函数：xrtTcpClientDestroy

**函数原型**：
```c
XXAPI void xrtTcpClientDestroy(xtcpclient* pClient);
```

**功能**：
销毁 TCP 客户端，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 是 |

### 函数：xrtTcpClientConnect

**函数原型**：
```c
XXAPI xnet_result xrtTcpClientConnect(xtcpclient* pClient);
```

**功能**：
连接到服务器。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 连接成功 |
| `XRT_NET_AGAIN` | TLS 握手进行中（Ex 模式） |
| `XRT_NET_TIMEOUT` | 连接超时 |
| `XRT_NET_ERROR` | 连接失败 |

### 函数：xrtTcpClientDisconnect

**函数原型**：
```c
XXAPI void xrtTcpClientDisconnect(xtcpclient* pClient);
```

**功能**：
断开与服务器的连接。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 是 |

### 函数：xrtTcpClientSend

**函数原型**：
```c
XXAPI xnet_result xrtTcpClientSend(xtcpclient* pClient, const char* pData, size_t iLen);
```

**功能**：
向服务器发送数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 否 |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 发送成功 |
| `XRT_NET_ERROR` | 发送失败 |

### 函数：xrtTcpClientEnableTLS

**函数原型**：
```c
XXAPI xnet_result xrtTcpClientEnableTLS(xtcpclient* pClient, const xtlsconfig* pConfig);
```

**功能**：
为客户端启用 TLS（必须在 Connect 之前调用）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 否 |
| `pConfig` | `const xtlsconfig*` | TLS 配置 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 启用成功 |
| `XRT_NET_ERROR` | 启用失败 |

### 函数：xrtTcpClientIsConnected

**函数原型**：
```c
XXAPI bool xrtTcpClientIsConnected(xtcpclient* pClient);
```

**功能**：
检查客户端是否已连接。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 已连接 |
| `false` | 未连接 |

### 函数：xrtTcpClientSetUserData

**函数原型**：
```c
XXAPI void xrtTcpClientSetUserData(xtcpclient* pClient, ptr pData);
```

**功能**：
设置客户端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 否 |
| `pData` | `ptr` | 用户数据指针 | 是 |

### 函数：xrtTcpClientGetUserData

**函数原型**：
```c
XXAPI ptr xrtTcpClientGetUserData(xtcpclient* pClient);
```

**功能**：
获取客户端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xtcpclient*` | 客户端对象 | 是 |

**返回值**：
用户数据指针

---

## 使用示例

### 示例1：TCP 服务端（简易模式）

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

static void on_accept(xtcpserver* pServer, xnetconn* pConn) {
    printf("Client connected: ID=%d\n", pConn->iId);
}

static void on_recv(xtcpserver* pServer, xnetconn* pConn, const char* pData, size_t iLen) {
    printf("Received from %d: %.*s\n", pConn->iId, (int)iLen, pData);

    // 回显数据
    xrtTcpServerSend(pServer, pConn->iId, pData, iLen);
}

static void on_close(xtcpserver* pServer, xnetconn* pConn) {
    printf("Client disconnected: ID=%d\n", pConn->iId);
}

int main() {
    xrtInit();

    xnetconfig config = {
        .iRecvBufSize = 8192,
        .iSendBufSize = 8192,
        .iMaxClients = 256,
        .iPollTimeoutMs = 10,
    };

    xnetevents events = {
        .OnAccept = on_accept,
        .OnRecv = on_recv,
        .OnClose = on_close,
    };

    xtcpserver* server = xrtTcpServerCreate("0.0.0.0", 8080, &config, &events);
    if (!server) {
        printf("Failed to create server\n");
        return 1;
    }

    xrtTcpServerStart(server);
    printf("Server started on port 8080\n");

    getchar();

    xrtTcpServerStop(server);
    xrtTcpServerDestroy(server);
    xrtUnit();
    return 0;
}
```

### 示例2：TCP 服务端（TLS）

```c
int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnAccept = on_accept,
        .OnRecv = on_recv,
        .OnClose = on_close,
    };

    xtcpserver* server = xrtTcpServerCreate("0.0.0.0", 8443, &config, &events);

    // 启用 TLS
    xtlsconfig tls_config = {
        .sCertFile = "server.crt",
        .sKeyFile = "server.key",
        .bVerifyPeer = false,
    };
    xrtTcpServerEnableTLS(server, &tls_config);

    xrtTcpServerStart(server);
    printf("TLS Server started on port 8443\n");

    getchar();

    xrtTcpServerStop(server);
    xrtTcpServerDestroy(server);
    xrtUnit();
    return 0;
}
```

### 示例3：TCP 客户端（简易模式）

```c
static void on_connect(xtcpclient* pClient, xnetconn* pConn, bool bSuccess) {
    if (bSuccess) {
        printf("Connected to server\n");
        const char* msg = "Hello, Server!";
        xrtTcpClientSend(pClient, msg, strlen(msg));
    } else {
        printf("Failed to connect\n");
    }
}

static void on_recv(xtcpclient* pClient, xnetconn* pConn, const char* pData, size_t iLen) {
    printf("Received: %.*s\n", (int)iLen, pData);
}

static void on_close(xtcpclient* pClient, xnetconn* pConn) {
    printf("Connection closed\n");
}

int main() {
    xrtInit();

    xnetconfig config = {
        .iConnectTimeoutMs = 5000,
    };

    xnetevents events = {
        .OnConnect = on_connect,
        .OnRecv = on_recv,
        .OnClose = on_close,
    };

    xtcpclient* client = xrtTcpClientCreate("127.0.0.1", 8080, &config, &events);
    if (!client) {
        printf("Failed to create client\n");
        return 1;
    }

    xrtTcpClientConnect(client);

    sleep(2);

    xrtTcpClientDisconnect(client);
    xrtTcpClientDestroy(client);
    xrtUnit();
    return 0;
}
```

### 示例4：TCP 客户端（TLS）

```c
int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnConnect = on_connect,
        .OnRecv = on_recv,
        .OnClose = on_close,
    };

    xtcpclient* client = xrtTcpClientCreate("127.0.0.1", 8443, &config, &events);

    // 启用 TLS
    xtlsconfig tls_config = {
        .bVerifyPeer = false,
        .bVerifyHost = false,
    };
    xrtTcpClientEnableTLS(client, &tls_config);

    xrtTcpClientConnect(client);

    sleep(2);

    xrtTcpClientDisconnect(client);
    xrtTcpClientDestroy(client);
    xrtUnit();
    return 0;
}
```

### 示例5：高级模式（共享事件循环）

```c
int main() {
    xrtInit();

    // 创建事件循环
    xeventloop* loop = xrtEventLoopCreate();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;
    xnetevents events = { /* ... */ };

    // 创建服务端（绑定到共享事件循环）
    xtcpserver* server = xrtTcpServerCreateEx(loop, "0.0.0.0", 8080, &config, &events);
    xrtTcpServerStart(server);

    // 创建客户端（绑定到共享事件循环）
    xtcpclient* client = xrtTcpClientCreateEx(loop, "127.0.0.1", 8080, &config, &events);
    xrtTcpClientConnect(client);

    // 运行事件循环
    xrtEventLoopRun(loop);

    xrtTcpClientDestroy(client);
    xrtTcpServerDestroy(server);
    xrtEventLoopDestroy(loop);
    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **双模式选择**：
   - **简易模式**：适合单连接场景，内部自动管理线程
   - **高级模式**：适合多连接/多服务端场景，共享事件循环提升性能

2. **TLS 使用**：
   - 必须在 `Start`（服务端）或 `Connect`（客户端）之前调用 `EnableTLS`
   - 简易模式：TLS 握手是同步阻塞的
   - 高级模式：TLS 握手是事件驱动的，通过 `OnConnect` 回调通知

3. **内存管理**：
   - 服务端和客户端对象必须使用对应的 `Destroy` 函数释放
   - 客户端 ID 在连接期间保持不变
   - 回调函数中可以使用 `pConn->iId` 获取客户端 ID

4. **性能优化**：
   - 对于高并发场景，建议使用高级模式（共享事件循环）
   - 设置合适的缓冲区大小以平衡内存和性能
   - 启用 `TCP_NODELAY` 可降低延迟（`config.bNoDelay = true`）

5. **跨平台兼容**：
   - Windows 使用 IOCP
   - Linux 使用 io_uring
   - API 接口完全一致

---

## 相关函数

- [xrtSockCreate](api-netsock.md) - 创建 Socket
- [xrtTlsCreate](api-nettls.md) - 创建 TLS 上下文
- [xrtEventLoopCreate](api-eventloop.md) - 创建事件循环
- [xrtPollCreate](api-netpoll.md) - 创建 Poller

---

<div align="center">

**XRT TCP Client/Server** | 版本 2.0 | 最后更新: 2025-01

</div>
