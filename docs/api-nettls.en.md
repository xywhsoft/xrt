# NetTLS TLS Protocol

> Built-in TLS 1.3 implementation with zero external dependencies, supports client/server

[English](api-network-tls.en.md) | [中文](api-nettls.md) | [返回索引](README.md)

---

## 📑 目录

- [功能概述](#功能概述)
- [架构设计](#架构设计)
- [数据类型](#数据类型)
- [常量定义](#常量定义)
- [TLS 管理 API](#tls-管理-api)
- [TLS 1.3 握手 API](#tls-13-握手-api)
- [TLS 1.2 握手 API](#tls-12-握手-api)
- [数据读写 API](#数据读写-api)
- [使用示例](#使用示例)

---

## 功能概述

**NetTLS** 是一个零依赖的 TLS 1.3 实现，主要特性：

- 🔐 **TLS 1.3 完整支持**：完整的握手和数据加密
- 🚀 **零外部依赖**：无需外部 TLS 库
- 🌐 **跨平台**：支持 Windows 和 Linux
- ⚡ **高性能**：针对游戏网络优化
- 🔧 **多种后端**：内置 + OpenSSL/mbedTLS/WolfSSL（可选）
- 📦 **完整加密套件**：ChaCha20-Poly1305 和 AES-GCM
- 🔑 **密钥交换**：X25519（椭圆曲线 Diffie-Hellman）
- 🛡️ **TLS 1.2 支持**：向后兼容 TLS 1.2

**依赖模块**：
- `crypto/sha256.h` - SHA-256 哈希
- `crypto/chacha20.h` - ChaCha20 流加密
- `crypto/poly1305.h` - Poly1305 MAC
- `crypto/x25519.h` - X25519 密钥交换
- `buffer.h` - 缓冲区管理

---

## 架构设计

```
┌─────────────────────────────────────────┐
│          Application Layer               │
├─────────────────────────────────────────┤
│    TCP Server/Client                  │
│    UDP Server/Client                  │
├─────────────────────────────────────────┤
│         TLS Abstraction Layer          │
├─────────────────────────────────────────┤
│  BUILTIN │  OpenSSL │ mbedTLS │ WolfSSL │
└─────────────────────────────────────────┘
```

---

## 数据类型

### xtlsctx

TLS 上下文对象（不透明结构）。

**定义**：
```c
typedef struct xrt_tls_context* xtlsctx;
```

**内存管理**：
- 创建：`xrtTlsCreate()`
- 销毁：`xrtTlsDestroy()`

### xtlsconfig

TLS 配置结构体。

**定义**：
```c
typedef struct xtlsconfig {
    const char* sCertFile;      // 证书文件路径（可选）
    const char* sKeyFile;       // 私钥文件路径（可选）
    const char* sCaFile;        // CA 证书文件路径（可选）
    bool bVerifyPeer;           // 是否验证对端证书（默认 false）
    bool bVerifyHost;           // 是否验证主机名（默认 false）
    const char* sHostname;      // 主机名（用于 SNI 和验证）
} xtlsconfig;
```

### xtls_backend

TLS 后端类型枚举。

**定义**：
```c
typedef enum {
    XRT_TLS_BACKEND_NONE = 0,
    XRT_TLS_BACKEND_MBEDTLS = 1,
    XRT_TLS_BACKEND_OPENSSL = 2,
    XRT_TLS_BACKEND_WOLFSSL = 3,
    XRT_TLS_BACKEND_BUILTIN = 4,      // 内置实现
} xtls_backend;
```

### xtls_state

TLS 状态枚举。

**定义**：
```c
typedef enum {
    // TLS 1.3 客户端状态
    XRT_TLS_CLIENT_START = 0,
    XRT_TLS_CLIENT_WAIT_SH,
    XRT_TLS_CLIENT_WAIT_EE,
    XRT_TLS_CLIENT_WAIT_CERT,
    XRT_TLS_CLIENT_WAIT_CV,
    XRT_TLS_CLIENT_WAIT_FINISH,
    XRT_TLS_CLIENT_CONNECTED,

    // TLS 1.2 客户端状态
    XRT_TLS12_CLIENT_WAIT_CERT,
    XRT_TLS12_CLIENT_WAIT_SKE,
    XRT_TLS12_CLIENT_WAIT_SHD,
    XRT_TLS12_CLIENT_WAIT_CCS,
    XRT_TLS12_CLIENT_WAIT_FINISH,
    XRT_TLS12_CLIENT_CONNECTED,

    // 服务端状态
    XRT_TLS_SERVER_START,
    XRT_TLS_SERVER_NEGOTIATED,
    XRT_TLS_SERVER_CONNECTED,
} xtls_state;
```

---

## 常量定义

### TLS 记录类型

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS_CHANGE_CIPHER` | 20 | 更改密码规范 |
| `__XRT_TLS_ALERT` | 21 | 警报 |
| `__XRT_TLS_HANDSHAKE` | 22 | 握手 |
| `__XRT_TLS_APP_DATA` | 23 | 应用数据 |

### TLS 1.3 握手消息类型

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS_CLIENT_HELLO` | 1 | 客户端问候 |
| `__XRT_TLS_SERVER_HELLO` | 2 | 服务端问候 |
| `__XRT_TLS_ENCRYPTED_EXTENSIONS` | 8 | 加密扩展 |
| `__XRT_TLS_CERTIFICATE` | 11 | 证书 |
| `__XRT_TLS_CERTIFICATE_VERIFY` | 15 | 证书验证 |
| `__XRT_TLS_FINISHED` | 20 | 完成 |

### TLS 1.2 握手消息类型

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS_SERVER_KEY_EXCHANGE` | 12 | 服务端密钥交换 |
| `__XRT_TLS_SERVER_HELLO_DONE` | 14 | 服务端问候完成 |
| `__XRT_TLS_CLIENT_KEY_EXCHANGE` | 16 | 客户端密钥交换 |
| `__XRT_TLS_CHANGE_CIPHER` | 20 | 更改密码规范 |

### TLS 1.3 密码套件

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS_AES_128_GCM_SHA256` | 0x1301 | AES-128-GCM-SHA256 |
| `__XRT_TLS_CHACHA20_POLY1305_SHA256` | 0x1303 | ChaCha20-Poly1305-SHA256 |

### TLS 1.2 密码套件

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS12_ECDHE_ECDSA_AES128_GCM_SHA256` | 0xC02B | ECDHE-ECDSA-AES128-GCM-SHA256 |
| `__XRT_TLS12_ECDHE_RSA_AES128_GCM_SHA256` | 0xC02F | ECDHE-RSA-AES128-GCM-SHA256 |
| `__XRT_TLS12_ECDHE_ECDSA_AES256_GCM_SHA384` | 0xC02C | ECDHE-ECDSA-AES256-GCM-SHA384 |
| `__XRT_TLS12_ECDHE_RSA_AES256_GCM_SHA384` | 0xC030 | ECDHE-RSA-AES256-GCM-SHA384 |
| `__XRT_TLS12_RSA_AES128_GCM_SHA256` | 0x009C | RSA-AES128-GCM-SHA256 |
| `__XRT_TLS12_RSA_AES256_GCM_SHA384` | 0x009D | RSA-AES256-GCM-SHA384 |

### TLS 版本

| 常量 | 值 | 说明 |
|------|-----|------|
| `__XRT_TLS_VERSION_1_2` | 0x0303 | TLS 1.2 |
| `__XRT_TLS_VERSION_1_3` | 0x0304 | TLS 1.3 |

### 默认配置

| 常量 | 值 | 说明 |
|------|-----|------|
| 最大记录大小 | 16384 字节 | TLS 记录最大大小 |
| 默认缓冲区 | 8192 字节 | 发送/接收缓冲区大小 |

---

## TLS 管理 API

### 函数：xrtTlsCreate

**函数原型**：
```c
XXAPI xtlsctx* xrtTlsCreate(const xtlsconfig* pConfig, bool bIsServer);
```

**功能**：
创建 TLS 上下文。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pConfig` | `const xtlsconfig*` | TLS 配置 | 否 |
| `bIsServer` | `bool` | 是否为服务端模式 | - |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `非NULL` | TLS 上下文对象（需使用 `xrtTlsDestroy` 释放） |
| `NULL` | 创建失败 |

**示例**：
```c
// 服务端配置
xtlsconfig server_config = {
    .sCertFile = "server.crt",
    .sKeyFile = "server.key",
    .bVerifyPeer = false,
};
xtlsctx* server_ctx = xrtTlsCreate(&server_config, true);

// 客户端配置
xtlsconfig client_config = {
    .bVerifyPeer = false,
    .bVerifyHost = false,
    .sHostname = "example.com",
};
xtlsctx* client_ctx = xrtTlsCreate(&client_config, false);
```

### 函数：xrtTlsDestroy

**函数原型**：
```c
XXAPI void xrtTlsDestroy(xtlsctx* pCtx);
```

**功能**：
销毁 TLS 上下文，释放所有资源。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 是 |

### 函数：xrtTlsClose

**函数原型**：
```c
XXAPI void xrtTlsClose(xtlsctx* pCtx);
```

**功能**：
关闭 TLS 连接，发送关闭通知。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 是 |

---

## TLS 1.3 握手 API

### 函数：xrtTlsHandshake

**函数原型**：
```c
XXAPI xnet_result xrtTlsHandshake(xtlsctx* pCtx, xnetconn* pConn);
```

**功能**：
执行 TLS 握手（支持 TLS 1.3 和 TLS 1.2）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 否 |
| `pConn` | `xnetconn*` | 网络连接对象 | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 握手完成 |
| `XRT_NET_AGAIN` | 握手进行中，需要更多数据 |
| `XRT_NET_ERROR` | 握手失败 |

**说明**：
- 自动协商 TLS 1.3 或 TLS 1.2
- 需要多次调用直到返回 `XRT_NET_OK`
- 客户端和服务端都使用相同的 API

### 函数：xrtTlsRead

**函数原型**：
```c
XXAPI xnet_result xrtTlsRead(xtlsctx* pCtx, char* pOut, size_t iMaxLen, size_t* pOutLen);
```

**功能**：
读取解密后的应用数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 否 |
| `pOut` | `char*` | 输出缓冲区 | 否 |
| `iMaxLen` | `size_t` | 最大读取长度 | - |
| `pOutLen` | `size_t*` | 实际读取长度（输出） | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 读取成功 |
| `XRT_NET_AGAIN` | 需要更多数据 |
| `XRT_NET_ERROR` | 读取失败 |

### 函数：xrtTlsWrite

**函数原型**：
```c
XXAPI xnet_result xrtTlsWrite(xtlsctx* pCtx, const char* pData, size_t iLen, size_t* pWritten);
```

**功能**：
加密并写入应用数据。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 否 |
| `pData` | `const char*` | 数据指针 | 否 |
| `iLen` | `size_t` | 数据长度 | - |
| `pWritten` | `size_t*` | 实际写入长度（输出） | 否 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `XRT_NET_OK` | 写入成功 |
| `XRT_NET_AGAIN` | 需要发送缓冲区 |
| `XRT_NET_ERROR` | 写入失败 |

---

## TLS 1.2 握手 API

TLS 1.2 握手与 TLS 1.3 使用相同的 API（`xrtTlsHandshake`），系统会自动协商协议版本。

### TLS 1.2 特性

- **密钥交换**：支持 ECDHE（secp256r1）和 RSA
- **密码套件**：支持 AES-128-GCM-SHA256 和 AES-256-GCM-SHA384
- **PRF**：支持 HMAC-SHA256 和 HMAC-SHA384
- **签名算法**：支持 RSA-PKCS1、RSA-PSS 和 ECDSA

---

## 数据读写 API

### 函数：xrtTlsGetSendBuffer

**函数原型**：
```c
XXAPI xnetbuf* xrtTlsGetSendBuffer(xtlsctx* pCtx);
```

**功能**：
获取 TLS 发送缓冲区（包含加密后的数据）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 否 |

**返回值**：
发送缓冲区指针

**说明**：
- 调用 `xrtTlsWrite` 后，加密数据会存入发送缓冲区
- 需要将缓冲区数据发送到网络
- 发送后调用 `xrtNetBufConsume` 消耗已发送数据

### 函数：xrtTlsGetRecvBuffer

**函数原型**：
```c
XXAPI xnetbuf* xrtTlsGetRecvBuffer(xtlsctx* pCtx);
```

**功能**：
获取 TLS 接收缓冲区（用于存放网络接收的加密数据）。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 否 |

**返回值**：
接收缓冲区指针

**说明**：
- 从网络接收的加密数据需要先存入接收缓冲区
- 调用 `xrtTlsRead` 从接收缓冲区解密数据

### 函数：xrtTlsIsHandshakeDone

**函数原型**：
```c
XXAPI bool xrtTlsIsHandshakeDone(xtlsctx* pCtx);
```

**功能**：
检查握手是否完成。

**参数**：
| 参数 | 类型 | 说明 | 是否可为NULL |
|------|------|------|--------------|
| `pCtx` | `xtlsctx*` | TLS 上下文对象 | 是 |

**返回值**：
| 返回值 | 说明 |
|--------|------|
| `true` | 握手完成 |
| `false` | 握手未完成 |

---

## 使用示例

### 示例1：TLS 服务端（使用 TCP 服务端）

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"

static void on_recv(xtcpserver* pServer, xnetconn* pConn, const char* pData, size_t iLen) {
    printf("Received: %.*s\n", (int)iLen, pData);

    // 回显数据
    xrtTcpServerSend(pServer, pConn->iId, pData, iLen);
}

static void on_close(xtcpserver* pServer, xnetconn* pConn) {
    printf("Client disconnected\n");
}

int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnRecv = on_recv,
        .OnClose = on_close,
    };

    // 创建服务端
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

### 示例2：TLS 客户端（使用 TCP 客户端）

```c
static void on_connect(xtcpclient* pClient, xnetconn* pConn, bool bSuccess) {
    if (bSuccess) {
        printf("Connected and handshake completed\n");
        const char* msg = "Hello, Secure Server!";
        xrtTcpClientSend(pClient, msg, strlen(msg));
    } else {
        printf("Failed to connect or handshake\n");
    }
}

static void on_recv(xtcpclient* pClient, xnetconn* pConn, const char* pData, size_t iLen) {
    printf("Received: %.*s\n", (int)iLen, pData);
}

int main() {
    xrtInit();

    xnetconfig config = XRT_NET_CONFIG_DEFAULT;

    xnetevents events = {
        .OnConnect = on_connect,
        .OnRecv = on_recv,
    };

    // 创建客户端
    xtcpclient* client = xrtTcpClientCreate("127.0.0.1", 8443, &config, &events);

    // 启用 TLS
    xtlsconfig tls_config = {
        .bVerifyPeer = false,
        .bVerifyHost = false,
        .sHostname = "localhost",
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

### 示例3：手动 TLS 握手（高级用法）

```c
int main() {
    xrtInit();

    // 创建 TLS 上下文
    xtlsconfig tls_config = {
        .bVerifyPeer = false,
        .sHostname = "example.com",
    };
    xtlsctx* ctx = xrtTlsCreate(&tls_config, false);

    // 创建 socket 并连接
    xnetconn conn;
    xrtSockCreate(&conn, 0);
    xnetaddr addr;
    xrtNetAddrInit(&addr, "example.com", 443);
    xrtSockConnect(&conn, &addr);

    // TLS 握手循环
    while (!xrtTlsIsHandshakeDone(ctx)) {
        // 执行握手
        xnet_result res = xrtTlsHandshake(ctx, &conn);

        // 发送握手数据
        xnetbuf* send_buf = xrtTlsGetSendBuffer(ctx);
        if (send_buf->iSize > 0) {
            size_t sent = 0;
            xrtSockSend(&conn, send_buf->pData, send_buf->iSize, &sent);
            xrtNetBufConsume(send_buf, sent);
        }

        // 接收握手数据
        char recv_buf[8192];
        size_t recvd = 0;
        xnet_result sock_res = xrtSockRecv(&conn, recv_buf, sizeof(recv_buf), &recvd);
        if (sock_res == XRT_NET_OK && recvd > 0) {
            // 将接收数据存入 TLS 接收缓冲区
            xnetbuf* tls_recv_buf = xrtTlsGetRecvBuffer(ctx);
            xrtNetBufAppend(tls_recv_buf, recv_buf, recvd);
        }
    }

    printf("TLS handshake completed\n");

    // 发送应用数据
    const char* msg = "Hello, TLS!";
    size_t written = 0;
    xrtTlsWrite(ctx, msg, strlen(msg), &written);

    // 发送加密数据
    xnetbuf* send_buf = xrtTlsGetSendBuffer(ctx);
    if (send_buf->iSize > 0) {
        size_t sent = 0;
        xrtSockSend(&conn, send_buf->pData, send_buf->iSize, &sent);
        xrtNetBufConsume(send_buf, sent);
    }

    // 接收应用数据
    char app_buf[8192];
    size_t app_len = 0;
    xrtTlsRead(ctx, app_buf, sizeof(app_buf), &app_len);
    if (app_len > 0) {
        printf("Received: %.*s\n", (int)app_len, app_buf);
    }

    xrtTlsClose(ctx);
    xrtTlsDestroy(ctx);
    xrtSockClose(&conn);
    xrtUnit();
    return 0;
}
```

### 示例4：TLS 1.2 连接

```c
int main() {
    xrtInit();

    xtlsconfig tls_config = {
        .bVerifyPeer = false,
        .sHostname = "example.com",
    };
    xtlsctx* ctx = xrtTlsCreate(&tls_config, false);

    // 创建连接并握手
    // ... 同上 ...

    // 系统会自动协商 TLS 1.2（如果服务器不支持 TLS 1.3）
    while (!xrtTlsIsHandshakeDone(ctx)) {
        // 握手循环
        // ...
    }

    printf("TLS %s handshake completed\n",
           ctx->bIsTls12 ? "1.2" : "1.3");

    // 正常读写
    // ...

    xrtTlsDestroy(ctx);
    xrtUnit();
    return 0;
}
```

---

## 注意事项

1. **零依赖**：
   - 内置 TLS 实现无需外部库
   - 所有加密原语已在 XRT 中实现
   - 支持的加密算法：SHA-256、ChaCha20-Poly1305、AES-GCM、X25519

2. **TLS 1.3 vs TLS 1.2**：
   - TLS 1.3：更安全、更快、更简单
   - TLS 1.2：向后兼容，支持旧系统
   - 系统自动协商协议版本

3. **证书管理**：
   - 服务端需要证书和私钥文件
   - 客户端可以验证服务器证书
   - 生产环境建议启用证书验证

4. **内存管理**：
   - TLS 上下文必须使用 `xrtTlsDestroy` 释放
   - 发送和接收缓冲区由 TLS 上下文管理
   - 不要直接修改 TLS 缓冲区

5. **性能特征**：
   - 握手时间：约 2-3 毫秒
   - 加密开销：每条记录约 16 字节
   - CPU 使用：低（ChaCha20-Poly1305 高度优化）
   - 内存使用：每连接约 1KB

6. **安全注意事项**：
   - 生产环境启用 `bVerifyPeer` 和 `bVerifyHost`
   - 使用有效的 CA 证书
   - 定期更新证书
   - 禁用不安全的密码套件

7. **跨平台兼容**：
   - Windows 和 Linux API 完全一致
   - 内置实现保证跨平台一致性

---

## 性能特征

### 内置 TLS 性能

- **握手时间**：约 2-3 毫秒
- **加密开销**：每条记录约 16 字节
- **CPU 使用**：低（ChaCha20-Poly1305 高度优化）
- **内存使用**：每连接约 1KB

### 与外部库对比

| 特性 | 内置 | OpenSSL | mbedTLS | WolfSSL |
|------|------|---------|---------|---------|
| TLS 1.3 | ✅ | ✅ | ✅ | ✅ |
| 零依赖 | ✅ | ❌ | ❌ | ❌ |
| 二进制大小 | 小 | 大 | 中 | 中 |
| 性能 | 高 | 高 | 高 | 高 |
| 证书生成 | ✅ | ❌ | ❌ | ❌ |

---

## 限制

- **证书验证**：基本的 DER 解析，无完整的 X.509 验证
- **密码套件**：仅支持 ChaCha20-Poly1305 和 AES-GCM
- **TLS 1.2/1.1**：仅 TLS 1.3 完整支持（TLS 1.2 部分支持）
- **ALPN**：无应用层协议协商
- **会话恢复**：未实现（未来增强）

---

## 未来增强

- 完整 X.509 证书解析和验证
- 证书链验证
- 会话票据恢复
- 0-RTT（零往返时间）握手
- 支持更多密码套件
- 完美前向保密（PFS）优化

---

## 相关函数

- [xrtTcpServerEnableTLS](api-nettcp.md) - 启用服务端 TLS
- [xrtTcpClientEnableTLS](api-nettcp.md) - 启用客户端 TLS
- [xrtSockSend](api-netsock.md) - 发送 Socket 数据
- [xrtSockRecv](api-netsock.md) - 接收 Socket 数据
- [xrtNetBufAppend](api-buffer.md) - 追加缓冲区数据

---

## 参考资料

- [RFC 8446: TLS 1.3](https://datatracker.ietf.org/doc/html/rfc8446)
- [RFC 8439: ChaCha20-Poly1305](https://datatracker.ietf.org/doc/html/rfc8439)
- [RFC 6234: SHA-256](https://datatracker.ietf.org/doc/html/rfc6234)
- [RFC 7748: X25519](https://datatracker.ietf.org/doc/html/rfc7748)
- [Mongoose TLS Implementation](https://github.com/cesanta/mongoose)

---

<div align="center">

**XRT TLS Protocol** | 版本 1.0 | 最后更新: 2025-01

</div>
