# XRT 迁移指南

> 面向当前主线的迁移说明。本文不再把旧网络/TLS 公共表面继续当作长期兼容目标，而是帮助你迁移到当前正式主线。

[English Version](MIGRATION.en.md) | [返回索引](README.md)

---

## 目录

- [当前迁移原则](#当前迁移原则)
- [源码树与单头文件](#源码树与单头文件)
- [运行时迁移](#运行时迁移)
- [线程迁移](#线程迁移)
- [容器与 shared root 迁移](#容器与-shared-root-迁移)
- [协程与异步主线迁移](#协程与异步主线迁移)
- [网络与 TLS 迁移](#网络与-tls-迁移)
- [迁移检查清单](#迁移检查清单)

---

## 当前迁移原则

本轮迁移遵循 3 条原则：

1. 以当前源码主线为唯一事实来源
2. 不为尚未大规模使用的旧表面长期背兼容包袱
3. 能收成单一主线的地方，就不继续保留“双轨 public surface”

因此，当前迁移的重点不是“旧接口都继续保留”，而是：

- 帮你尽快转到正式主线
- 让未来的 API 冻结更干净

---

## 源码树与单头文件

### 1. 源码树是当前主线解释模型

推荐：

```bash
gcc main.c xrt.c -o app
```

或：

```bash
tcc main.c xrt.c -o app.exe
```

当前文档和示例默认围绕源码树解释主线能力。

### 2. 单头文件仍支持，但不再作为默认迁移目标

如果你的旧代码还在使用：

```c
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

并不代表马上不能用；但如果你在迁移一条长期维护的主线，建议优先切到源码树。

---

## 运行时迁移

### 1. 从“全局错误/临时状态”迁移到线程运行时

旧习惯经常把错误和临时状态理解成全局共享。

当前主线已经拆成：

- 进程级状态
- 线程级状态

迁移重点：

- 使用 `xrtGetError()`，不再直接依赖 `xCore.LastError`
- 理解 `xrtTempMemory()` 是线程级临时内存
- 默认随机数也属于当前线程状态

### 2. 初始化 / 结束仍然是正式入口

```c
xrtInit();
/* ... */
xrtUnit();
```

任何依赖运行时线程状态的程序主线，都应保留这对入口。

---

## 线程迁移

### 1. 主线程和 XRT 创建线程自动附加

当前正式合同：

- 主线程：`xrtInit()` 后自动附加
- `xrtThreadCreate()` 创建的线程：自动附加

### 2. 宿主线程需要显式附加

如果线程不是由 `xrtThreadCreate()` 创建，而是宿主自己创建的，那么迁移后应改成：

```c
xrtThreadAttachCurrent();
/* ... 使用运行时绑定 API ... */
xrtThreadDetachCurrent();
```

### 3. `Destroy` 语义收紧

旧习惯：

```c
xrtThreadDestroy(pThread);
```

当前推荐：

```c
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

`Destroy` 只负责释放管理对象，不再承担“隐式 detach / 终止线程”的角色。

---

## 容器与 shared root 迁移

这是当前最重要的一类迁移。

### 1. 默认容器现在都是 local root

下面这些默认构造器创建的都是本线程本地对象：

- `xvoCreateArray()`
- `xvoCreateList()`
- `xvoCreateColl()`
- `xvoCreateTable()`

### 2. 跨线程共享改为显式 shared root

旧写法：

```c
xvalue pTable = xvoCreateTable();
xvalue pTags = xvoCreateColl();
```

新写法：

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
xvalue pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
xvoCollSetText(pTags, "ai", 0, FALSE);
xvoTableSetValue(pTable, "tags", 4, pTags, FALSE);
```

### 3. nested shared 也必须是 shared

如果把 array/list/coll/table 作为子值写入 shared 容器，那么这个子容器本身也必须是 real shared root。

否则当前主线会拒绝该操作，而不是继续静默混用。

### 4. shared 不等于“全部无锁”

shared root 解决的是“能否跨线程共享”，不等于：

- walk
- iterator
- pointer view

这些路径天然完全无锁安全。复杂遍历仍然应显式使用 root lock。

---

## 协程与异步主线迁移

### 1. 协程高层抽象已稳定

迁移时建议统一理解为：

- 协程：执行与挂起
- scheduler：调度
- event / deadline：原生等待
- future / task / promise / wait-source：统一异步主线

### 2. 优先迁移到 future / wait-source

如果旧代码在协程里自己写轮询等待，建议迁移到：

- `xFutureWaitCo*`
- `xrtNetFutureWaitCo*`
- `xrtNetWaitSourceWaitCo*`

这样可以让协程、线程和网络等待都建立在同一套异步模型上。

### 3. 当前线程 deferred continuation 会自动 pump

如果你已经迁移到：

- `ThenCurrent`
- `CatchCurrent`
- `FinallyCurrent`

那么当前线程调用 `xFutureWait*()` 时会自动推进这类 continuation，不再必须手工 pump 每一步。

### 4. 新异步主线不只是一组 future helper

当前正式迁移目标应统一理解为：

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`
- `task group`
- `nested scope`

如果旧代码里已经出现：

- 线程任务一套回调
- 协程任务一套回调
- 网络异步一套等待

建议尽量收口到这条统一主线上，而不是继续维护多套结果模型。

### 5. 作用域型并发优先迁移到 `task group`

如果旧代码存在：

- 一组子任务并发执行
- 父任务等待全部结束
- 中途取消时要传播到子任务

那么迁移后应优先考虑：

- `xTaskGroupRun*`
- `xTaskGroupJoin*`
- `xTaskGroupCreateChild`

而不是继续手工管理 future 数组和取消链。

---

## 网络与 TLS 迁移

### 1. 旧网络主线已退役

当前不再推荐继续围绕：

- `nettcp`
- `netudp`
- `nethttp`
- `netpoll`

做长期迁移。

应优先转向：

- `xnet-v2`
- `xtlssession`
- `xhttp`
- `xhttpd`
- `xws`

### 2. TLS 已收成单一正式 public surface

旧 public surface：

- `xtlsctx`
- `xrtTls*`

当前已不再作为正式对外主线。

迁移目标应统一为：

- `xtlssession`
- `xrtNetTlsSession*`

### 3. 推荐迁移顺序

如果你要迁移旧网络逻辑，建议按下面顺序做：

1. 先迁移到底层 `xnet-v2`
2. 再迁移到 `future / wait-source`
3. 再接 TLS session
4. 最后再迁移到 `xhttp / xhttpd / xws`

这样风险最小，也更接近当前正式架构。

### 4. 网络应用层不要继续围绕旧 public surface 迁移

如果你要迁移的是：

- HTTP 客户端
- HTTP 服务端
- WebSocket 服务

那么应直接围绕：

- `xhttp`
- `xhttpd`
- `xws`

建立新主线，而不是再把旧 TCP / HTTP 表面包一层继续延长历史链路。

---

## 迁移检查清单

### 代码入口

- [ ] 主线构建已切到源码树
- [ ] 保留 `xrtInit()` / `xrtUnit()`

### 运行时

- [ ] 不再直接依赖 `xCore.LastError`
- [ ] 线程绑定 API 已改成 `xrtGetError()` / `xrtTempMemory()` 思维
- [ ] 宿主线程已显式 `AttachCurrent / DetachCurrent`

### 并发

- [ ] `xrtThreadDestroy()` 使用顺序已调整为 `Wait -> Destroy`
- [ ] 协程等待已尽量迁移到 future / wait-source

### 容器

- [ ] 跨线程容器已切到 `xvoCreate*Ex(XRT_OBJMODE_SHARED)`
- [ ] shared root 的 nested 容器也已显式 shared
- [ ] 复杂 shared 遍历已加 root lock

### 网络

- [ ] 新逻辑优先建立在 `xnet-v2`
- [ ] TLS 已切到 `xtlssession`
- [ ] 不再继续新增对旧网络 public surface 的依赖

---

## 相关文档

- [项目简介](../../README.md)
- [架构设计](ARCHITECTURE.md)
- [API 索引](api/README.md)
- [Thread](api/api-thread.md)
- [Coroutine](api/api-coroutine.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [Value](api/api-value.md)
- [XNet V2](api/api-xnet-v2.md)
- [Network TLS](api/api-network-tls.md)

---

**XRT 迁移指南版本：当前主线重建版**
