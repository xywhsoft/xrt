# XRT 常见问题解答

> 面向当前主线的常见问题。这里不再解释已经退役的旧网络/TLS/API 表面，只回答当前源码主线下最常见的使用、并发、异步和网络问题。

[English Version](FAQ.en.md) | [返回索引](README.md)

---

## 目录

- [构建与集成](#构建与集成)
- [运行时与线程附加](#运行时与线程附加)
- [内存与临时内存](#内存与临时内存)
- [Value 与 shared root](#value-与-shared-root)
- [协程与 future](#协程与-future)
- [网络与 TLS](#网络与-tls)
- [文档与主线判断](#文档与主线判断)

---

## 构建与集成

### 1. 当前主线应该如何编译？

当前推荐方式是直接使用源码树：

```c
#include "xrt.h"
```

再编译：

```bash
gcc main.c xrt.c -o app
```

或：

```bash
tcc main.c xrt.c -o app.exe
```

当前不再推荐把源码树当成“`#define XRT_IMPLEMENTATION` 的单头示例”来解释主线。

### 2. 为什么会出现 `undefined reference` 或 `undefined symbol`？

最常见原因：

- 只编译了 `main.c`，没有把 [xrt.c](../xrt.c) 一起编译
- 只包含了 [xrt.h](../xrt.h)，但没链接实现
- 单头文件和源码树用法混在一起

优先检查：

```bash
gcc main.c xrt.c -o app
```

### 3. 当前主线还需要 `XRT_IMPLEMENTATION` 吗？

源码树主线不需要。  
它属于单头文件分发形态，不是当前主线文档默认解释模型。

---

## 运行时与线程附加

### 4. 为什么有些 API 在主线程能用，换到别的线程就不对？

因为当前主线已经把运行时拆成：

- 进程级状态
- 线程级状态

下面这些能力都依赖“当前线程已附加到 XRT 运行时”：

- `xrtGetError()`
- `xrtTempMemory()`
- `xrtRand32()` / `xrtRand64()`
- future / coroutine / wait-source 相关能力

### 5. 哪些线程会自动附加到运行时？

- 主线程：`xrtInit()` 后自动附加
- `xrtThreadCreate()` 创建的线程：自动附加
- 宿主自己创建的线程：需要手工 `xrtThreadAttachCurrent()`

推荐模式：

```c
xrtInit();

/* 主线程直接使用 XRT API */

xrtUnit();
```

或在宿主线程里：

```c
xrtThreadAttachCurrent();
/* ... 使用运行时绑定 API ... */
xrtThreadDetachCurrent();
```

### 6. `xrtThreadDestroy()` 会终止线程吗？

不会。

当前正式合同是：

- `xrtThreadWait()`：负责等待线程结束
- `xrtThreadDestroy()`：负责释放线程管理对象

正确顺序：

```c
xrtThreadWait(pThread);
xrtThreadDestroy(pThread);
```

如果线程仍在运行，`Destroy` 会拒绝执行，而不是偷偷 `detach` 或强杀。

---

## 内存与临时内存

### 7. `xrtTempMemory()` 适合做什么？

适合：

- 当前线程里的短期临时字符串
- 当前线程里的格式化结果
- 当前线程里的中间缓冲

不适合：

- 跨线程共享
- 跨函数长期保存
- 保存到全局结构、容器、异步回调之后再长期使用

### 8. 为什么 `xrtTempMemory()` 返回的内容后面会变掉？

因为它本质上是“线程级临时内存”，不是长期所有权内存。

如果你要长期保存，请改用：

- `xrtMalloc / xrtFree`
- `xrtCopyStr`
- `xvalue`
- 容器托管

### 9. XRT 现在的内存调试重点是什么？

当前主线的内存调试目标不只是“统计数量”，还包括尽量暴露危险操作，例如：

- 内存泄露定位到文件和行号
- 重复释放
- 非法释放
- 越界写导致的块破坏
- 已释放内存继续使用
- 错线程访问线程本地对象

对于基础设施开发，建议在测试和调试构建里优先打开这类检查，而不是只在最终崩溃后回溯。

---

## Value 与 shared root

### 10. 为什么跨线程共享容器会失败？

因为 phase-2 之后，容器默认都是本线程本地对象。

下面这些默认构造器创建的都是 local root：

- `xvoCreateArray()`
- `xvoCreateList()`
- `xvoCreateColl()`
- `xvoCreateTable()`

如果要跨线程共享，必须显式创建 shared root：

```c
xvalue pTable = xvoCreateTableEx(XRT_OBJMODE_SHARED);
xvalue pTags = xvoCreateCollEx(XRT_OBJMODE_SHARED);
```

### 11. shared root 是否等于“天然完全无锁”？

不是。

shared root 解决的是：

- 这个对象可不可以跨线程共享

它不自动意味着：

- walk / iterator / pointer view 可以无锁长期稳定读取

复杂遍历或需要一致视图时，仍应显式使用 root lock。

### 12. 为什么把 local 容器塞进 shared 容器会失败？

这是当前主线刻意做的边界收紧。

正确写法是：

- shared root 里嵌套的容器，如果也要跨线程参与共享，就必须自己也是 shared root

错误写法会被拒绝，并设置当前线程错误，而不是继续静默混用。

---

## 协程与 future

### 13. 协程和线程是什么关系？

当前主线的建议是：

- 线程：负责 CPU 并行
- 协程：负责单线程内的协作式并发

协程不是线程替代品，也不能跨线程迁移运行。

### 14. 协程现在应该优先等什么？

优先等：

- `future`
- `wait-source`
- `xcoevent`

不建议自己手写轮询逻辑。

推荐思路：

- 协程负责挂起与恢复
- future 负责结果与终态
- wait-source 负责把网络对象统一成等待面

### 15. `ThenCurrent / CatchCurrent / FinallyCurrent` 为什么有时不立刻执行？

因为它们是 `current-thread deferred continuation`。

当前主线下：

- 如果当前线程进入 `xFutureWait*()`，会自动 pump
- 如果当前线程没有走 future wait 主路径，也可以手工调用：

```c
xFuturePumpCurrentContinuations(0);
```

### 16. `task group` 现在只是 future 容器吗？

不是。

当前主线里的 `task group` 已经有明确的 scope 语义：

- `Close`
- `ReapCompleted`
- `Destroy => Close + Cancel pending children`
- `JoinFuture / Join / JoinTimeout / JoinUntil`
- `CreateChild`

如果你需要的是“一组异步任务作为一个作用域统一收口”，优先用 `task group`，不要自己手工拼一堆 future 列表。

---

## 网络与 TLS

### 16. 当前正式网络主线是什么？

当前正式主线是：

- `xnet-v2`
- `xtlssession`
- `xhttp`
- `xhttpd`
- `xws`

旧网络模块族和旧 public TLS 已经不是当前推荐主线。

### 17. 当前 TLS 正式 public surface 是什么？

当前正式 public surface 是：

- `xtlssession`
- `xrtNetTlsSession*`

旧：

- `xtlsctx`
- `xrtTls*`

已经不再属于正式对外主线。

### 18. 网络等待应该优先用哪套接口？

当前更推荐统一理解为：

- `future`
- `wait-source`
- thread wait
- coroutine wait

例如：

- `xrtNetFutureWait*`
- `xrtNetWaitSourceWait*`
- `xrtNetWaitSourceWaitCo*`

这样比单独记忆每个对象各自的等待 helper 更稳定。

### 19. listener / stream / dgram 这类等待接口已经完全生产级了吗？

主线已经成型，但近期刚做过较大重构，仍建议继续做：

- 更重的压力验证
- 更长时间的 soak test
- destroy / cancel / timeout 竞争回归

所以当前更准确的说法是：

- 主线能力已经正式化
- 但仍在继续朝全面生产级收口

---

## 文档与主线判断

### 20. 为什么有些旧文档找不到了？

因为本轮文档重建已经把旧网络/TLS 主线文档从 `docs/` 迁出，统一归档到 `dev/`。

这样做是为了避免：

- 旧主线继续和新主线并列误导使用者

### 21. 现在应该从哪里开始读？

推荐顺序：

1. [项目简介](../README.md)
2. [架构设计](ARCHITECTURE.md)
3. [API 索引](api/README.md)
4. [示例说明](EXAMPLES.md)

如果准备直接写代码，优先看：

1. [Base](api/api-base.md)
2. [Value](api/api-value.md)
3. [Thread](api/api-thread.md)
4. [Coroutine](api/api-coroutine.md)
5. [Future / Task / Promise](api/api-future-task-promise.md)
6. [XNet V2](api/api-xnet-v2.md)

如果你想快速建立“互联网 + AI 主线”的整体理解，建议再看：

1. [runtime 与线程附加](guide/runtime-thread-attach.md)
2. [协程、future、task 入门](guide/coroutine-future-task-intro.md)
3. [xnet-v2 与 TLS 主线](guide/xnet-v2-tls-intro.md)
4. [流式 LLM API 教学](guide/streaming-llm-api-intro.md)

---

## 相关文档

- [项目简介](../README.md)
- [架构设计](ARCHITECTURE.md)
- [API 索引](api/README.md)
- [最佳实践](BEST_PRACTICES.md)
- [迁移指南](MIGRATION.md)
- [示例说明](EXAMPLES.md)

---

**XRT FAQ 文档版本：当前主线重建版**

