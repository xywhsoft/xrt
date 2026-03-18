# NetUDP UDP 客户端/服务端

> 基于 Socket + EventLoop + IOCP/io_uring 的 UDP 客户端/服务端封装，支持事件驱动

[English](api-netudp.en.md) | [中文](api-netudp.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [UDP 服务端 API](#udp-服务端-api)
- [UDP 客户端 API](#udp-客户端-api)
- [使用示例](#使用示例)

---

## 功能概述

**NetUDP** 是一个功能完整的 UDP 客户端/服务端库，主要特性：

- 🚀 **事件驱动架构**：基于 IOCP（Windows）和 io_uring（Linux）
- 🔄 **双模式 API**：简易版（内部线程）+ 高级版（共享事件循环）
- 📡 **无连接协议**：UDP 无连接管理，通过远端地址传递
- 📦 **支持 recvfrom**：获取数据报来源地址
- ⚡ **高性能轮询**：简易模式使用 recvfrom 轮询，更好的 UDP 远端地址支持
- 🌐 **跨平台**：支持 Windows 和 Linux
- 🎯 **绑定到随机端口**：客户端自动绑定系统分配端口

**依赖模块**：
- `netsock.h` - Socket 基础
- `netpoll.h` - IO 多路复用
- `netloop.h` - 事件循环

---

## 数据类型

### xudpserver

UDP 服务端对象。

**定义**：
```c
typedef struct xrt_udp_server* xudpserver;
```

**内存管理**：
- 创建：`xrtUdpServerCreate()` 或 `xrtUdpServerCreateEx()`
- 销毁：`xrtUdpServerDestroy()`

### xudpclient

UDP 客户端对象。

**定义**：
```c
typedef struct xrt_udp_client* xudpclient;
```

**内存管理**：
- 创建：`xrtUdpClientCreate()` 或 `xrtUdpClientCreateEx()`
- 销毁：`xrtUdpClientDestroy()`

### xnetaddr

网络地址结构体。

**定义**：
```c
typedef struct xnetaddr {
    char sIP[64];      // IP 地址字符串
    uint16 iPort;      // 端口号
} xnetaddr;
```

### xnetconfig

网络配置结构体。

**定义**：
```c
typedef struct xnetconfig {
    int iRecvBufSize;       // 接收缓冲区大小（默认 8192）
    int iSendBufSize;       // 发送缓冲区大小（默认 8192）
    int iMaxClients;        // 最大客户端数量（UDP 不使用）
    int iPollTimeoutMs;     // 轮询超时毫秒（默认 10）
    int iConnectTimeoutMs;  // 连接超时毫秒（UDP 不使用）
    bool bNoDelay;          // 是否启用 TCP_NODELAY（UDP 不使用）
} xnetconfig;
```

### xnetevents

网络事件回调结构体。

**定义**：
```c
typedef struct xnetevents {
    // UDP 服务端/客户端回调
    void (*OnRecvFrom)(void* pOwner, xnetconn* pConn, const xnetaddr* pAddr, const char* pData, size_t iLen);
    void (*OnRecv)(void* pOwner, xnetconn* pConn, const char* pData, size_t iLen);
    void (*OnError)(void* pOwner, xnetconn* pConn, int iError);
} xnetevents;
```

---

## 常量定义

### 默认配置

| 常量 | 值 | 说明 |
|------|-----|------|
| 默认接收缓冲区 | 8192 字节 | 接收缓冲区大小 |
| 默认发送缓冲区 | 8192 字节 | 发送缓冲区大小 |
| 默认轮询超时 | 10 毫秒 | 事件轮询超时 |

---

## UDP 服务端 API

### 函数：xrtUdpServerCreate

**函数原型**：
```c
XXAPI xudpserver* xrtUdpServerCreate(const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 UDP 服务端（简易模式，内部创建轮询线程）。

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
| `非NULL` | 服务端对象（需使用 `xrtUdpServerDestroy` 释放） |
| `NULL` | 创建失败 |

### 函数：xrtUdpServerCreateEx

**函数原型**：
```c
XXAPI xudpserver* xrtUdpServerCreateEx(xeventloop* pLoop, const char* sIP, uint16 iPort,
    const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 UDP 服务端（高级模式，共享事件循环）。

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

### 函数：xrtUdpServerDestroy

**函数原型**：
```c
XXAPI void xrtUdpServerDestroy(xudpserver* pServer);
```

**功能**：
销毁 UDP 服务端，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 是 |

### 函数：xrtUdpServerStart

**函数原型**：
```c
XXAPI xnet_result xrtUdpServerStart(xudpserver* pServer);
```

**功能**：
启动 UDP 服务端，开始接收数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 启动成功 |
| `XRT_NET_ERROR` | 启动失败 |

### 函数：xrtUdpServerStop

**函数原型**：
```c
XXAPI void xrtUdpServerStop(xudpserver* pServer);
```

**功能**：
停止 UDP 服务端。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 是 |

### 函数：xrtUdpServerSendTo

**函数原型**：
```c
XXAPI xnet_result xrtUdpServerSendTo(xudpserver* pServer, const xnetaddr* pAddr,
    const char* pData, size_t iLen);
```

**功能**：
向指定地址发送 UDP 数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 否 |
| `pAddr` | `const xnetaddr*` | 目标地址 | 否 |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 发送成功 |
| `XRT_NET_ERROR` | 发送失败 |

### 函数：xrtUdpServerSetUserData

**函数原型**：
```c
XXAPI void xrtUdpServerSetUserData(xudpserver* pServer, ptr pData);
```

**功能**：
设置服务端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 否 |
| `pData` | `ptr` | 用户数据指针 | 是 |

### 函数：xrtUdpServerGetUserData

**函数原型**：
```c
XXAPI ptr xrtUdpServerGetUserData(xudpserver* pServer);
```

**功能**：
获取服务端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pServer` | `xudpserver*` | 服务端对象 | 是 |

**返回值**：
用户数据指针

---

## UDP 客户端 API

### 函数：xrtUdpClientCreate

**函数原型**：
```c
XXAPI xudpclient* xrtUdpClientCreate(const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 UDP 客户端（简易模式，内部创建轮询线程）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pConfig` | `const xnetconfig*` | 网络配置 | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 客户端对象（需使用 `xrtUdpClientDestroy` 释放） |
| `NULL` | 创建失败 |

### 函数：xrtUdpClientCreateEx

**函数原型**：
```c
XXAPI xudpclient* xrtUdpClientCreateEx(xeventloop* pLoop, const xnetconfig* pConfig, const xnetevents* pEvents);
```

**功能**：
创建 UDP 客户端（高级模式，共享事件循环）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pLoop` | `xeventloop*` | 事件循环对象 | 否 |
| `pConfig` | `const xnetconfig*` | 网络配置 | 是 |
| `pEvents` | `const xnetevents*` | 事件回调 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | 客户端对象 |
| `NULL` | 创建失败 |

### 函数：xrtUdpClientDestroy

**函数原型**：
```c
XXAPI void xrtUdpClientDestroy(xudpclient* pClient);
```

**功能**：
销毁 UDP 客户端，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 是 |

### 函数：xrtUdpClientStart

**函数原型**：
```c
XXAPI xnet_result xrtUdpClientStart(xudpclient* pClient);
```

**功能**：
启动 UDP 客户端，开始接收数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 启动成功 |
| `XRT_NET_ERROR` | 启动失败 |

### 函数：xrtUdpClientStop

**函数原型**：
```c
XXAPI void xrtUdpClientStop(xudpclient* pClient);
```

**功能**：
停止 UDP 客户端。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 是 |

### 函数：xrtUdpClientSendTo

**函数原型**：
```c
XXAPI xnet_result xrtUdpClientSendTo(xudpclient* pClient, const xnetaddr* pAddr,
    const char* pData, size_t iLen);
```

**功能**：
向指定地址发送 UDP 数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 否 |
| `pAddr` | `const xnetaddr*` | 目标地址 | 否 |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 发送成功 |
| `XRT_NET_ERROR` | 发送失败 |

### 函数：xrtUdpClientSetUserData

**函数原型**：
```c
XXAPI void xrtUdpClientSetUserData(xudpclient* pClient, ptr pData);
```

**功能**：
设置客户端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 否 |
| `pData` | `ptr` | 用户数据指针 | 是 |

### 函数：xrtUdpClientGetUserData

**函数原型**：
```c
XXAPI ptr xrtUdpClientGetUserData(xudpclient* pClient);
```

**功能**：
获取客户端用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pClient` | `xudpclient*` | 客户端对象 | 是 |

**返回值**：
用户数据指针

---

## 使用示例

### 示例1：UDP 服务端（简易模式）

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

static void on_recv_from(void* pOwner, xnetconn* pConn, const xnetaddr* pAddr,
                         const char* pData, size_t iLen) {
    printf("Received from %s:%d: %.*s\n", pAddr->sIP, pAddr->iPort, (int)iLen, pData);

    // 回显数据
    xudpserver* server = (xudpserver*)pOwner;
    xrtUdpServerSendTo(server, pAddr, pData, iLen);
}

static void on_error(void* pOwner, xnetconn* pConn, int iError) {
    printf("Error: %d\n", iError);
}

int main() {
    xrtInit();

    xnetconfig config = {
        .iRecvBufSize = 8192,
        .iSendBufSize = 8192,
        .iPollTimeoutMs = 10,
    };

    xnetevents events = {
        .OnRecvFrom = on_recv_from,
        .OnError = on_error,
    };

    xudpserver* server = xrtUdpServerCreate("0.0.0.0", 8080, &config, &events);
    if (!server) {
        printf("Failed to create server\n");
        return 1;
    }

    xrtUdpServerStart(server);
    printf("UDP Server started on port 8080\n");

    getchar();

    xrtUdpServerStop(server);
    xrtUdpServerDestroy(server);
    xrtUnit();
    return 0;
}
```

### 示例2：UDP 客户端（简易模式）

```c
static void on_recv(void* pOwner, xnetconn* pConn, const char* pData, size_t iLen) {
    printf("Received: %.*s\n", (int)iLen, pData);
}

static void on_error(void* pOwner, xnetconn* pConn, int iError) {
    printf("Error: %d\n", iError);
}

int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnRecv = on_recv,
        .OnError = on_error,
    };

    xudpclient* client = xrtUdpClientCreate(&config, &events);
    if (!client) {
        printf("Failed to create client\n");
        return 1;
    }

    xrtUdpClientStart(client);

    // 发送数据
    xnetaddr addr;
    xrtNetAddrInit(&addr, "127.0.0.1", 8080);
    const char* msg = "Hello, UDP Server!";
    xrtUdpClientSendTo(client, &addr, msg, strlen(msg));

    sleep(1);

    xrtUdpClientStop(client);
    xrtUdpClientDestroy(client);
    xrtUnit();
    return 0;
}
```

### 示例3：高级模式（共享事件循环）

```c
int main() {
    xrtInit();

    // 创建事件循环
    xeventloop* loop = xrtEventLoopCreate();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;
    xnetevents events = { /* ... */ };

    // 创建服务端（绑定到共享事件循环）
    xudpserver* server = xrtUdpServerCreateEx(loop, "0.0.0.0", 8080, &config, &events);
    xrtUdpServerStart(server);

    // 创建客户端（绑定到共享事件循环）
    xudpclient* client = xrtUdpClientCreateEx(loop, &config, &events);
    xrtUdpClientStart(client);

    // 运行事件循环
    xrtEventLoopRun(loop);

    xrtUdpClientDestroy(client);
    xrtUdpServerDestroy(server);
    xrtEventLoopDestroy(loop);
    xrtUnit();
    return 0;
}
```

### 示例4：广播消息

```c
int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;
    xnetevents events = { /* ... */ };

    xudpclient* client = xrtUdpClientCreate(&config, &events);
    xrtUdpClientStart(client);

    // 启用广播
    xudpclient* clientImpl = (xudpclient*)client;
    int broadcast = 1;
    setsockopt(clientImpl->tConn.hSocket, SOL_SOCKET, SO_BROADCAST,
               (char*)&broadcast, sizeof(broadcast));

    // 发送广播消息
    xnetaddr addr;
    xrtNetAddrInit(&addr, "255.255.255.255", 8080);
    const char* msg = "Broadcast message";
    xrtUdpClientSendTo(client, &addr, msg, strlen(msg));

    sleep(1);

    xrtUdpClientStop(client);
    xrtUdpClientDestroy(client);
    xrtUnit();
    return 0;
}
```

### 示例5：使用 OnRecv 回调（不含远端地址）

```c
static void on_recv(void* pOwner, xnetconn* pConn, const char* pData, size_t iLen) {
    // 不关心数据来源，只处理数据
    printf("Received data: %.*s\n", (int)iLen, pData);
}

int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnRecv = on_recv,  // 使用 OnRecv 而不是 OnRecvFrom
    };

    xudpserver* server = xrtUdpServerCreate("0.0.0.0", 8080, &config, &events);
    xrtUdpServerStart(server);

    getchar();

    xrtUdpServerStop(server);
    xrtUdpServerDestroy(server);
    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **UDP 特性**：
   - UDP 是无连接协议，不保证数据到达和顺序
   - 数据包可能丢失、重复或乱序
   - 没有连接状态管理

2. **远端地址**：
   - 使用 `OnRecvFrom` 回调可以获取数据来源地址
   - 使用 `OnRecv` 回调只获取数据，不关心来源
   - `OnRecvFrom` 优先于 `OnRecv`

3. **简易模式**：
   - 简易模式使用 `recvfrom` 轮询接收
   - 可以更好地获取远端地址信息
   - 适合单连接场景

4. **高级模式**：
   - 高级模式使用事件循环（IOCP/io_uring）
   - 适合多连接/多服务端场景
   - 性能更高

5. **广播和多播**：
   - 需要手动设置 `SO_BROADCAST` 选项启用广播
   - 多播需要使用 `IP_ADD_MEMBERSHIP` 选项
   - 参考系统文档了解详细配置

6. **内存管理**：
   - 服务端和客户端对象必须使用对应的 `Destroy` 函数释放
   - 回调函数中的指针生命周期由调用者保证

7. **跨平台兼容**：
   - Windows 使用 IOCP
   - Linux 使用 io_uring
   - API 接口完全一致

---

## 相关函数

- [xrtSockCreate](api-netsock.md) - 创建 Socket
- [xrtNetAddrInit](api-netsock.md) - 初始化网络地址
- [xrtEventLoopCreate](api-eventloop.md) - 创建事件循环
- [xrtPollCreate](api-netpoll.md) - 创建 Poller

---

<div align="center">

**XRT UDP Client/Server** | 版本 2.0 | 最后更新: 2025-01

</div>
