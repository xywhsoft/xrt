# NetPoll Network Poller

> Async IO abstraction layer based on IOCP (Windows) and io_uring (Linux), provides unified async interface

[English](api-netpoll.en.md) | [中文](api-netpoll.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [Poller 管理 API](#poller-管理-api)
- [连接管理 API](#连接管理-api)
- [IO 操作 API](#io-操作-api)
- [使用示例](#使用示例)

---

## 功能概述

**NetPoll** 是一个高性能的异步 IO 抽象层，主要特性：

- 🚀 **高性能架构**：Windows 使用 IOCP，Linux 使用 io_uring
- 🔄 **操作池优化**：O(1) 操作分配/释放
- 📦 **批量操作**：支持批量收割完成事件
- 💾 **动态缓冲区**：大消息（>8KB）自动使用动态缓冲区
- 🔒 **线程安全**：内置锁机制保护共享数据
- ⚡ **异步 Accept**：Windows 支持 AcceptEx 异步接受
- 🌐 **跨平台**：Windows 和 Linux 统一接口
- 📡 **UDP 支持**：支持 RECVFROM 获取远端地址

**依赖模块**：
- `netsock.h` - Socket 基础
- `buffer.h` - 缓冲区管理

---

## 数据类型

### xnetpoller

网络轮询器对象。

**定义**：
```c
typedef struct xrt_net_poller* xnetpoller;
```

**内存管理**：
- 创建：`xrtPollCreate()`
- 销毁：`xrtPollDestroy()`

### xnetconn

网络连接对象。

**定义**：
```c
typedef struct xnetconn {
    xsocket hSocket;         // Socket 句柄
    int iType;               // 类型（0=TCP, 1=UDP）
    int iId;                 // 连接 ID
    bool bTlsEnabled;        // 是否启用 TLS
    xnetaddr tRemoteAddr;    // 远端地址
    ptr pUserData;           // 用户数据
} xnetconn;
```

### xpoll_fn

事件回调函数类型。

**定义**：
```c
typedef void (*xpoll_fn)(xnetpoller* pPoller, xnetconn* pConn,
                          int iEvent, const char* pData, size_t iLen);
```

**参数说明**：
- `pPoller` - Poller 对象
- `pConn` - 连接对象
- `iEvent` - 事件类型（见常量定义）
- `pData` - 数据指针（接收数据时有效）
- `iLen` - 数据长度

---

## 常量定义

### 事件类型

| 常量 | 值 | 说明 |
|------|-----|------|
| `XRT_POLL_ACCEPT` | 1 | 接受新连接（服务端） |
| `XRT_POLL_READ` | 2 | 可读事件 |
| `XRT_POLL_WRITE` | 3 | 可写事件 |
| `XRT_POLL_CLOSE` | 4 | 连接关闭 |
| `XRT_POLL_ERROR` | 5 | 错误事件 |

### 操作类型

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_POLL_OP_RECV` | 1 | 接收操作 |
| `__XRT_POLL_OP_SEND` | 2 | 发送操作 |
| `__XRT_POLL_OP_ACCEPT` | 3 | 接受操作 |
| `__XRT_POLL_OP_RECVFROM` | 4 | UDP 接收操作 |

### 默认配置

| 常量 | 值 | 说明 |
|------|-----|------|
| 默认操作数 | 64 | 初始操作池大小 |
| 最大操作数 | 4096 | 操作池最大容量 |
| 默认接收大小 | 8192 字节 | 单次接收缓冲区大小 |
| IOCP 批量大小 | 64 | Windows 批量收割数量 |
| io_uring 条目数 | 256 | Linux SQ/CQ 条目数 |

---

## Poller 管理 API

### 函数：xrtPollCreate

**函数原型**：
```c
XXAPI xnetpoller* xrtPollCreate(xnetconfig* pConfig, xpoll_fn pfnCallback, ptr pUserData);
```

**功能**：
创建网络轮询器。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pConfig` | `xnetconfig*` | 网络配置（NULL 使用默认值） | 是 |
| `pfnCallback` | `xpoll_fn` | 事件回调函数 | 否 |
| `pUserData` | `ptr` | 用户数据指针 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | Poller 对象（需使用 `xrtPollDestroy` 释放） |
| `NULL` | 创建失败 |

**示例**：
```c
static void on_event(xnetpoller* pPoller, xnetconn* pConn,
                     int iEvent, const char* pData, size_t iLen) {
    switch (iEvent) {
        case XRT_POLL_READ:
            printf("Received: %.*s\n", (int)iLen, pData);
            break;
        case XRT_POLL_WRITE:
            printf("Sent: %zu bytes\n", iLen);
            break;
        case XRT_POLL_CLOSE:
            printf("Connection closed\n");
            break;
    }
}

xnetpoller* poller = xrtPollCreate(NULL, on_event, NULL);
```

### 函数：xrtPollDestroy

**函数原型**：
```c
XXAPI void xrtPollDestroy(xnetpoller* pPoller);
```

**功能**：
销毁网络轮询器，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 是 |

---

## 连接管理 API

### 函数：xrtPollAdd

**函数原型**：
```c
XXAPI xnet_result xrtPollAdd(xnetpoller* pPoller, xnetconn* pConn, int iEvents);
```

**功能**：
将连接添加到轮询器。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `pConn` | `xnetconn*` | 连接对象 | 否 |
| `iEvents` | `int` | 事件掩码（READ/WRITE/ACCEPT） | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 添加成功 |
| `XRT_NET_ERROR` | 添加失败 |

**说明**：
- Windows：将 Socket 关联到 IOCP
- Linux：不需要显式注册（io_uring 在提交操作时自动注册）

### 函数：xrtPollRemove

**函数原型**：
```c
XXAPI xnet_result xrtPollRemove(xnetpoller* pPoller, xnetconn* pConn);
```

**功能**：
从轮询器中移除连接，取消所有待处理的 IO 操作。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `pConn` | `xnetconn*` | 连接对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 移除成功 |
| `XRT_NET_ERROR` | 移除失败 |

**说明**：
- Windows：调用 `CancelIoEx` 取消所有 IO
- Linux：提交 `IORING_OP_ASYNC_CANCEL` 操作

---

## IO 操作 API

### 函数：xrtPollPostRecv

**函数原型**：
```c
XXAPI xnet_result xrtPollPostRecv(xnetpoller* pPoller, xnetconn* pConn);
```

**功能**：
投递接收操作。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `pConn` | `xnetconn*` | 连接对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 投递成功 |
| `XRT_NET_ERROR` | 投递失败 |

**说明**：
- TCP：投递 `WSARecv` 或 `IORING_OP_RECV`
- UDP：投递 `WSARecvFrom` 或 `IORING_OP_RECVMSG`（获取远端地址）
- 接收完成后会触发 `XRT_POLL_READ` 事件

### 函数：xrtPollPostSend

**函数原型**：
```c
XXAPI xnet_result xrtPollPostSend(xnetpoller* pPoller, xnetconn* pConn,
                                  const char* pData, size_t iLen);
```

**功能**：
投递发送操作。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `pConn` | `xnetconn*` | 连接对象 | 否 |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 投递成功 |
| `XRT_NET_ERROR` | 投递失败 |

**说明**：
- 大消息（>8KB）自动使用动态分配缓冲区
- 发送完成后会触发 `XRT_POLL_WRITE` 事件
- `iLen` 参数表示实际发送的字节数

### 函数：xrtPollPostAccept

**函数原型**：
```c
XXAPI xnet_result xrtPollPostAccept(xnetpoller* pPoller, xnetconn* pServer, xnetconn* pClient);
```

**功能**：
投递接受连接操作（服务端）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `pServer` | `xnetconn*` | 监听连接对象 | 否 |
| `pClient` | `xnetconn*` | 预分配的客户端连接对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 投递成功 |
| `XRT_NET_ERROR` | 投递失败 |

**说明**：
- Windows：使用 `AcceptEx` 异步接受（如果可用）
- Linux：使用 `IORING_OP_ACCEPT`
- 接受完成后会触发 `XRT_POLL_ACCEPT` 事件
- 客户端连接的 `hSocket` 会被填充

### 函数：xrtPollWait

**函数原型**：
```c
XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs);
```

**功能**：
等待并处理完成的事件。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 否 |
| `iTimeoutMs` | `int` | 超时时间（毫秒，-1 表示无限等待） | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 事件处理完成 |
| `XRT_NET_TIMEOUT` | 超时 |
| `XRT_NET_ERROR` | 错误 |

**说明**：
- Windows：使用 `GetQueuedCompletionStatusEx` 批量收割
- Linux：使用 `io_uring_enter` 提交并等待
- 所有完成事件都会通过回调函数通知

### 函数：xrtPollWakeup

**函数原型**：
```c
XXAPI void xrtPollWakeup(xnetpoller* pPoller);
```

**功能**：
唤醒正在等待的 `xrtPollWait`。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 是 |

**说明**：
- Windows：使用 `PostQueuedCompletionStatus`
- Linux：使用 `eventfd` 写入唤醒值

### 函数：xrtPollGetUserData

**函数原型**：
```c
XXAPI ptr xrtPollGetUserData(xnetpoller* pPoller);
```

**功能**：
获取 Poller 的用户数据指针。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pPoller` | `xnetpoller*` | Poller 对象 | 是 |

**返回值**：
用户数据指针

---

## 使用示例

### 示例1：TCP Echo 服务器

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

static void on_event(xnetpoller* pPoller, xnetconn* pConn,
                     int iEvent, const char* pData, size_t iLen) {
    switch (iEvent) {
        case XRT_POLL_ACCEPT: {
            printf("New client accepted\n");
            // 投递接收操作
            xrtPollPostRecv(pPoller, pConn);
            break;
        }
        case XRT_POLL_READ: {
            printf("Received: %.*s\n", (int)iLen, pData);
            if (iLen == 0) {
                // 连接关闭
                xrtPollRemove(pPoller, pConn);
                xrtSockClose(pConn);
            } else {
                // 回显数据
                xrtPollPostSend(pPoller, pConn, pData, iLen);
                // 继续接收
                xrtPollPostRecv(pPoller, pConn);
            }
            break;
        }
        case XRT_POLL_WRITE: {
            printf("Sent: %zu bytes\n", iLen);
            // 继续接收
            xrtPollPostRecv(pPoller, pConn);
            break;
        }
        case XRT_POLL_CLOSE:
            printf("Connection closed\n");
            break;
    }
}

int main() {
    xrtInit();

    // 创建 poller
    xnetpoller* poller = xrtPollCreate(NULL, on_event, NULL);
    if (!poller) {
        printf("Failed to create poller\n");
        return 1;
    }

    // 创建监听 socket
    xnetconn listenConn;
    xrtSockCreate(&listenConn, 0);  // 0 = TCP
    xrtSockSetReuseAddr(&listenConn);
    xrtSockSetNonBlock(&listenConn);

    xnetaddr addr;
    xrtNetAddrInit(&addr, "0.0.0.0", 8080);
    xrtSockBind(&listenConn, &addr);
    xrtSockListen(&listenConn, 128);

    // 添加到 poller
    xrtPollAdd(poller, &listenConn, XRT_POLL_ACCEPT);
    xrtPollPostAccept(poller, &listenConn, &gClientConn);

    printf("Server started on port 8080\n");

    // 事件循环
    while (1) {
        xrtPollWait(poller, 100);
    }

    xrtSockClose(&listenConn);
    xrtPollDestroy(poller);
    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **操作池管理**：
   - 操作池自动管理，支持 O(1) 分配/释放
   - 初始大小为 64，最大可扩展到 4096
   - 大消息（>8KB）自动使用动态缓冲区

2. **事件循环**：
   - 必须在单独的线程中调用 `xrtPollWait`
   - `xrtPollWait` 是阻塞调用，建议使用超时
   - 使用 `xrtPollWakeup` 可以从其他线程唤醒

3. **连接生命周期**：
   - 调用 `xrtPollAdd` 后才能投递 IO 操作
   - 连接关闭前调用 `xrtPollRemove` 取消所有待处理 IO
   - `XRT_POLL_READ` 事件中收到 0 字节数据表示连接关闭

4. **跨平台差异**：
   - Windows：使用 IOCP，支持批量收割
   - Linux：使用 io_uring，性能更高
   - API 接口完全一致

5. **线程安全**：
   - Poller 内部使用锁保护共享数据
   - 可以从多个线程投递 IO 操作
   - 回调函数在调用 `xrtPollWait` 的线程中执行

6. **UDP 远端地址**：
   - UDP 的 `XRT_POLL_READ` 事件会填充 `pConn->tRemoteAddr`
   - 可以使用此地址回复数据

---

## 相关函数

- [xrtSockCreate](api-netsock.md) - 创建 Socket
- [xrtEventLoopCreate](api-eventloop.md) - 创建事件循环
- [xrtTcpServerCreate](api-nettcp.md) - 创建 TCP 服务端
- [xrtUdpServerCreate](api-netudp.md) - 创建 UDP 服务端

---

<div align="center">

**XRT Network Poller** | 版本 2.0 | 最后更新: 2025-01

</div>
