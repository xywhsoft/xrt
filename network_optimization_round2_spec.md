# XRT 网络库深度优化 Round 2 Spec

## 背景
这是在 Round 1 全面事件驱动重构基础上的第二轮优化，解决剩余的 16 个关键问题。

## 当前架构回顾
- **事件驱动核心**: `xeventloop` + `xnetpoller` (IOCP/io_uring)
- **事件路由**: `xnetconn->pUserData` -> `__xrt_conn_handler` -> 回调分发
- **双模式 API**: 简易版(内部线程) + 高级版(共享事件循环)
- **TCP**: 客户端槽位空闲栈 + 环形缓冲区 + TLS 握手事件驱动
- **UDP**: 简易模式 recvfrom 轮询 + Ex 模式事件驱动

---

## 问题分类与优先级

### P0 - 关键问题 (影响正确性或严重性能)

#### P0.1 io_uring Wakeup 机制无效 (netpoll.h L724, L1004)
- **现状**: `eventfd` 创建但未注册到 io_uring，`xrtEventLoopStop()` 无法唤醒
- **后果**: Linux 下只能等超时自然醒，无法及时响应停止指令
- **修复**: 用 `IORING_OP_READ` 将 eventfd 注册到 io_uring，收到 wakeup 信号后重新投递 read

#### P0.2 IOCP 0 字节完成事件误判 (netpoll.h L432)
- **现状**: UDP 0 字节数据报被错误判定为连接关闭
- **后果**: UDP 通信异常终止
- **修复**: 仅对 TCP RECV (`iOpType==RECV && iType==0`) 才触发 CLOSE

#### P0.3 操作池巨额内存浪费 (netpoll.h L164)
- **现状**: 固定 `calloc(4096, sizeof(op))` ≈ 32MB，无论实际并发数
- **后果**: 内存浪费 + 页面触碰开销
- **修复**: 按需扩容，初始 64，满时 2x 扩容至最大 4096

#### P0.4 TLS 客户端握手 Sleep(10) 轮询 (nettcp.h L774)
- **现状**: 最坏等 5 秒，每 10ms 轮询一次
- **后果**: 握手延迟高，CPU 空转
- **修复**: 用 `select` 等 socket 可读再推进状态机

---

### P1 - 性能优化

#### P1.5 PollRemove 线性扫描 O(n) (netpoll.h L225, L782)
- **现状**: 遍历所有 op 查找匹配 conn
- **修复**: 删除扫描，直接调 `CancelIoEx` / `IORING_OP_ASYNC_CANCEL`

#### P1.6 TCP 未默认启用 NODELAY (nettcp.h)
- **现状**: Nagle 算法导致小包延迟 ~200ms
- **修复**: `xnetconfig` 加 `bNoDelay` 字段，默认 true

#### P1.7 xnetbuf Consume memmove 瓶颈 (netsock.h L544)
- **现状**: TLS 每次解密后 Consume 触发 memmove
- **备注**: 已用 `xnetringbuf` 解决 TCP 明文路径，TLS 仍用 `xnetbuf`。大改风险高，本轮暂不处理

#### P1.8 io_uring 超时兼容性 (netpoll.h L928)
- **现状**: `io_uring_enter` 带 timeout 需 kernel 5.11+
- **修复**: 改用 `IORING_OP_TIMEOUT` SQE 实现超时

---

### P2 - 健壮性增强

#### P2.9 Stop 未先 PollRemove (nettcp.h L478)
- **问题**: 直接 close socket 可能产生错误完成事件
- **修复**: close 前先 `xrtPollRemove`

#### P2.10 Windows KeepAlive 参数缺失 (netsock.h L221)
- **问题**: 仅设 `SO_KEEPALIVE=1`，未配置 idle/interval
- **修复**: 用 `SIO_KEEPALIVE_VALS` 配置完整参数

#### P2.11 连接超时硬编码 (nettcp.h L752)
- **问题**: 固定 5 秒，不可配置
- **修复**: `xnetconfig` 加 `iConnectTimeoutMs`

#### P2.12 无 Graceful Shutdown (nettcp.h)
- **问题**: 直接 closesocket，可能丢数据
- **修复**: shutdown(SD_SEND) 半关闭后等 FIN-ACK

#### P2.13 UDP Ex 模式无远端地址 (netudp.h L64)
- **问题**: IOCP/io_uring recv 不提供发送者地址
- **修复**: IOCP 用 `WSARecvFrom`，io_uring 用 `IORING_OP_RECVMSG`

---

### P3 - 增强项

#### P3.14 操作池无动态扩容上限
- **问题**: 固定 4096，满了就失败
- **修复**: Step 2 已含动态扩容

#### P3.15 SO_RCVBUF/SNDBUF 未配置
- **问题**: `iRecvBufSize` 仅用于应用层，未设 OS socket 缓冲区
- **修复**: `setsockopt(SOL_SOCKET, SO_RCVBUF/SO_SNDBUF)`

#### P3.16 HTTP Client 同步阻塞模型
- **问题**: nethttp.h 仍用阻塞 socket
- **备注**: 范围过大，需独立项目

---

## 实施计划 (12 步)

### Step 1: xrt.h — 扩展配置 + TCC 兼容
**文件**: `xrt.h`
**改动**:
```c
// L1426-1431 xnetconfig 扩展
typedef struct {
    size_t iRecvBufSize;
    size_t iSendBufSize;
    int iMaxClients;
    int iPollTimeoutMs;
    int iConnectTimeoutMs;  // 新增: 连接超时(ms), 默认 5000
    bool bNoDelay;          // 新增: TCP_NODELAY, 默认 true
} xnetconfig;

// L131 TCC 兼容区新增
#ifndef SIO_KEEPALIVE_VALS
    #define SIO_KEEPALIVE_VALS 0x98000004
#endif
struct tcp_keepalive {
    ULONG onoff;
    ULONG keepalivetime;
    ULONG keepaliveinterval;
};
```

---

### Step 2: netpoll.h — 操作池动态扩容 (P0.3)
**文件**: `lib/netpoll.h`
**改动**:
- 新增 `#define __XRT_POLL_INIT_OPS 64`
- `xrtPollCreate`: 初始分配 64 个 op
- `__xrt_iocp_alloc_op` / `__xrt_uring_alloc_op`: 
  - 空闲链表为空且 `iOpCount == iOpCapacity` 时
  - `xrtRealloc` 扩容 2x (上限 4096)
  - 新增槽位串入空闲链表

---

### Step 3: netpoll.h — IOCP 0 字节修复 (P0.2)
**文件**: `lib/netpoll.h` (IOCP 部分 L430-440)
**改动**:
```c
if ( iBytesTransferred == 0 && pOp->iOpType == __XRT_POLL_OP_RECV ) {
    if ( pConn && pConn->iType == 0 ) {  // 仅 TCP 判 close
        pPoller->pfnCallback(pPoller, pConn, XRT_POLL_CLOSE, NULL, 0);
        __xrt_iocp_free_op(pPoller, pOp);
        continue;
    }
    // UDP 0 字节是合法数据，走正常 READ 分发
}
```

---

### Step 4: netpoll.h — PollRemove 去线性扫描 (P1.5)
**文件**: `lib/netpoll.h`
**改动**:

**IOCP (L220-236)**:
```c
XXAPI xnet_result xrtPollRemove(xnetpoller* pPoller, xnetconn* pConn)
{
    if ( !pPoller || !pConn ) return XRT_NET_ERROR;
    CancelIoEx((HANDLE)pConn->hSocket, NULL);
    return XRT_NET_OK;
}
```

**io_uring (L777-798)**:
```c
XXAPI xnet_result xrtPollRemove(xnetpoller* pPoller, xnetconn* pConn)
{
    if ( !pPoller || !pConn ) return XRT_NET_ERROR;
    struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
    if ( pSQE ) {
        pSQE->opcode = IORING_OP_ASYNC_CANCEL;
        pSQE->fd = pConn->hSocket;
        #ifdef IORING_ASYNC_CANCEL_FD  // kernel 5.19+
            pSQE->cancel_flags = IORING_ASYNC_CANCEL_FD;
        #endif
        __xrt_uring_submit_sqe(pPoller);
        __xrt_uring_flush(pPoller);
    }
    return XRT_NET_OK;
}
```

---

### Step 5: netpoll.h — io_uring Wakeup 修复 (P0.1)
**文件**: `lib/netpoll.h` (Linux 部分)
**改动**:

**新增结构成员 (L555)**:
```c
uint64 iWakeupVal;  // eventfd 读缓冲区
```

**新增内部函数**:
```c
static void __xrt_uring_post_wakeup_read(xnetpoller* pPoller)
{
    struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
    if ( !pSQE ) return;
    pSQE->opcode = IORING_OP_READ;
    pSQE->fd = pPoller->iWakeupFd;
    pSQE->addr = (unsigned long long)(uintptr_t)&pPoller->iWakeupVal;
    pSQE->len = sizeof(uint64);
    pSQE->user_data = 0;  // 特殊标记
    __xrt_uring_submit_sqe(pPoller);
}
```

**PollCreate 末尾调用**:
```c
__xrt_uring_post_wakeup_read(pPoller);
```

**PollWait CQ 处理 (L950)**:
```c
if ( pCQE->user_data == 0 ) {
    __xrt_uring_post_wakeup_read(pPoller);  // 重新投递
    iHead++;
    continue;
}
```

---

### Step 6: netpoll.h — io_uring 超时兼容 (P1.8)
**文件**: `lib/netpoll.h` (Linux 部分 L912-935)
**改动**:
```c
XXAPI xnet_result xrtPollWait(xnetpoller* pPoller, int iTimeoutMs)
{
    __xrt_uring_flush(pPoller);
    
    // 投递 TIMEOUT SQE
    if ( iTimeoutMs >= 0 ) {
        struct io_uring_sqe* pSQE = __xrt_uring_get_sqe(pPoller);
        if ( pSQE ) {
            pSQE->opcode = IORING_OP_TIMEOUT;
            pSQE->addr = (unsigned long long)(uintptr_t)&tTimeoutSpec;
            pSQE->len = 1;  // 等到 1 个事件或超时
        }
    }
    
    // 不带超时调 enter
    int iRet = __xrt_io_uring_enter(pPoller->iFd, 0, 1, IORING_ENTER_GETEVENTS, NULL);
    
    // CQ 处理中忽略 -ETIME
    if ( pCQE->res == -ETIME ) {
        return XRT_NET_TIMEOUT;
    }
}
```

---

### Step 7: netsock.h — Windows KeepAlive 完整配置 (P2.10)
**文件**: `lib/netsock.h` (L212-237)
**改动**:
```c
#if defined(_WIN32) || defined(_WIN64)
    setsockopt(..., SO_KEEPALIVE, ...);
    if ( iIdleSec > 0 || iIntervalSec > 0 ) {
        struct tcp_keepalive tKA;
        tKA.onoff = 1;
        tKA.keepalivetime = (iIdleSec > 0 ? iIdleSec : 7200) * 1000;
        tKA.keepaliveinterval = (iIntervalSec > 0 ? iIntervalSec : 75) * 1000;
        DWORD dwRet;
        WSAIoctl(pConn->hSocket, SIO_KEEPALIVE_VALS,
            &tKA, sizeof(tKA), NULL, 0, &dwRet, NULL, NULL);
    }
#endif
```

---

### Step 8: nettcp.h — TCP 增强 (P1.6, P2.9, P2.11, P2.12)
**文件**: `lib/nettcp.h`
**改动**:

**Accept 后设 NoDelay (L246)**:
```c
if ( pServer->tConfig.bNoDelay ) {
    xrtSockSetNoDelay(&pSlot->tConn);
}
```

**连接超时可配置 (L752)**:
```c
int iTimeoutMs = pClient->tConfig.iConnectTimeoutMs > 0 ? 
    pClient->tConfig.iConnectTimeoutMs : 5000;
tTimeout.tv_sec = iTimeoutMs / 1000;
tTimeout.tv_usec = (iTimeoutMs % 1000) * 1000;
```

**Graceful Shutdown (L842)**:
```c
#if defined(_WIN32) || defined(_WIN64)
    shutdown(pConn->hSocket, SD_SEND);
#else
    shutdown(pConn->hSocket, SHUT_WR);
#endif
// 等待 FIN-ACK 或超时后再 closesocket
```

**Stop 前 PollRemove (L478-492)**:
```c
xrtPollRemove(pPoller, &pSlot->tConn);  // 新增
xrtSockClose(&pSlot->tConn);
```

---

### Step 9: nettcp.h — TLS 握手去 Sleep (P0.4)
**文件**: `lib/nettcp.h` (L774-795)
**改动**:
```c
while ( (iTlsRes = xrtTlsHandshake(...)) == XRT_NET_AGAIN ) {
    // 发送 TLS 输出
    if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
        size_t iSent = 0;
        xrtSockSend(&pClient->tConn, ..., &iSent);
        xrtNetBufConsume(&pClient->pTlsCtx->tSendBuf, iSent);
    }
    
    // select 等待事件驱动，替代 Sleep(10)
    fd_set tReadSet, tWriteSet;
    FD_ZERO(&tReadSet); FD_ZERO(&tWriteSet);
    FD_SET(pClient->tConn.hSocket, &tReadSet);
    if ( pClient->pTlsCtx->tSendBuf.iSize > 0 ) {
        FD_SET(pClient->tConn.hSocket, &tWriteSet);
    }
    struct timeval tSelTimeout = {0, 100000};  // 100ms
    select((int)pClient->tConn.hSocket + 1, &tReadSet, 
        pClient->pTlsCtx->tSendBuf.iSize > 0 ? &tWriteSet : NULL, NULL, &tSelTimeout);
    
    iRetries++;
    if ( iRetries > iMaxRetries ) { /* timeout */ }
}
```

---

### Step 10: netudp.h — Ex 模式远端地址支持 (P2.13)
**文件**: `lib/netudp.h`
**说明**: 需要先在 netpoll.h 中为 UDP 提供 `xrtPollPostRecvFrom`，然后 netudp.h Ex 模式改用它。

由于涉及 netpoll.h 大改，且 UDP 简易模式已能正常工作，此项可选。

---

### Step 11: netloop.h + netsock.h — 小修
**文件**: `lib/netloop.h`, `lib/netsock.h`
**改动**:

**EventLoop 防 busy-loop (L87)**:
```c
while ( pLoop->bRunning ) {
    xnet_result iRes = xrtPollWait(...);
    if ( iRes == XRT_NET_ERROR ) {
        #if defined(_WIN32) || defined(_WIN64)
            Sleep(1);
        #else
            usleep(1000);
        #endif
    }
}
```

---

### Step 12: 编译验证 + 测试
**命令**:
```bash
gcc -m64 -DDEBUG_TRACE test.c xrt.c -lWs2_32 -lIPHLPAPI -O0 -o test_run.exe
tcc -m64 -shared xrt.c -std=c99 -DBUILD_DLL -o release/x64/xrt.dll

# 运行测试
./test_run.exe
```

**重点测试**:
- `Test_TLS_Comprehensive()` - 确认 TLS 握手改造后正常
- `Test_NetTCPUDP()` - 确认 TCP/UDP 功能正常
- Linux 下 `xrtEventLoopStop()` 响应速度

---

## 依赖关系图

```
Step 1 (xrt.h 类型扩展)
  |
  ├── Step 2 (操作池动态扩容) ──┐
  ├── Step 3 (UDP 0 字节修复)   │
  ├── Step 4 (PollRemove 优化)  │
  ├── Step 5 (io_uring wakeup)  ├── 全部依赖 Step 1
  ├── Step 6 (io_uring timeout) │
  ├── Step 7 (KeepAlive Win)    │
  │                             │
  ├── Step 8 (TCP 增强) ────────┘
  ├── Step 9 (TLS 去 Sleep) ────┘
  ├── Step 10 (UDP Ex 地址) ────┘ (可选)
  ├── Step 11 (小修)
  │
  └── Step 12 (编译验证) ←─ 依赖全部
```

---

## 不实施项 (本轮范围外)

### P1.7 TLS xnetbuf 改 ringbuf
- **原因**: 涉及 `nettls.h` 2700 行内部逻辑，改动风险远大于收益
- **替代**: TLS 1.3 已用 ringbuf，TLS 1.2 暂维持现状

### P3.16 HTTP Client 异步化
- **原因**: 范围过大，需独立项目重新设计 API

---

## 预期收益

| 优化点 | 收益 |
|--------|------|
| P0.1 io_uring wakeup | Linux 事件循环可及时停止，不再卡顿 |
| P0.2 UDP 0 字节修复 | UDP 通信恢复正常 |
| P0.3 操作池扩容 | 内存节省 30MB+，冷启动更快 |
| P0.4 TLS 去 Sleep | 握手延迟从 5s 降至秒级，CPU 利用率提升 |
| P1.5 PollRemove 优化 | 高并发下 Remove 性能提升 O(n) → O(1) |
| P1.6 TCP NoDelay | 小包延迟从 200ms 降至微秒级 |
| P2.9-P2.13 健壮性 | 网络稳定性显著提升 |

---

## 备注

- 所有改动均向后兼容
- 保持现有 API 不变
- 重点关注 Linux 平台兼容性 (kernel 5.1+)
- Windows 平台 TCC 兼容性已保障
