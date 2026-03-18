# XRT 架构设计

> 当前源码主线的架构说明。本文档只描述已经进入正式主线的设计，不再保留旧网络/TLS 主线的历史叙述。

[返回索引](README.md)

---

## 1. 设计定位

XRT 的目标不是“函数集合”，而是：

**互联网 + AI 时代的 C 语言基础设施库**

它需要同时满足：

- 轻量化
- 高性能
- 跨平台
- 单头文件友好
- 功能完备且成体系
- 能支撑互联网程序开发链路
- 能支撑 AI Agent 的异步编排与网络交互链路

这决定了 XRT 的架构不是若干孤立模块堆叠，而是一条完整基础设施主线：

- 基础运行时
- 并发运行时
- 统一异步模型
- 网络主线
- 网络应用层
- 结构化数据与文本处理


## 2. 当前架构分层

### 2.1 运行时基础层

负责：

- 基础类型
- 进程级运行时
- 线程级运行时
- 内存分配入口
- 错误处理
- 临时内存
- 字符串、编码、路径、时间、文件、哈希、随机数

核心文件：

- [xrt.h](../../xrt.h)
- [xrt.c](../../xrt.c)
- [base.h](../../lib/base.h)
- [string.h](../../lib/string.h)
- [os.h](../../lib/os.h)
- [charset.h](../../lib/charset.h)
- [math.h](../../lib/math.h)
- [path.h](../../lib/path.h)
- [time.h](../../lib/time.h)
- [file.h](../../lib/file.h)
- [hash.h](../../lib/hash.h)

### 2.2 并发运行时层

负责：

- 线程
- 线程附加运行时
- 协程
- 协程调度器
- 协程事件等待

核心文件：

- [thread.h](../../lib/thread.h)
- [coroutine.h](../../lib/coroutine.h)

### 2.3 统一异步模型层

负责：

- `future`
- `promise`
- `task`
- `wait-source`
- continuation
- combinator
- task group
- nested scope

当前已形成正式主线。

核心文件：

- [xrt.h](../../xrt.h)
- [xnet_sync.h](../../lib/xnet_sync.h)

### 2.4 内存与容器层

负责：

- 块结构内存管理
- 固定大小内存池
- 通用内存池
- 数组、指针数组、栈、链表、AVLTree、字典、列表

这层当前已经进入：

- owner-thread 默认语义
- shared root 显式语义
- 调试与危险操作识别

核心文件：

- [bsmm.h](../../lib/bsmm.h)
- [memunit.h](../../lib/memunit.h)
- [mempool_fs.h](../../lib/mempool_fs.h)
- [mempool.h](../../lib/mempool.h)
- [array_point.h](../../lib/array_point.h)
- [array.h](../../lib/array.h)
- [stack.h](../../lib/stack.h)
- [stack_dyn.h](../../lib/stack_dyn.h)
- [llist.h](../../lib/llist.h)
- [avltree_base.h](../../lib/avltree_base.h)
- [avltree.h](../../lib/avltree.h)
- [dict.h](../../lib/dict.h)
- [list.h](../../lib/list.h)

### 2.5 结构化数据与文本层

负责：

- `xvalue`
- JSON
- 数值与文本转换
- 模板引擎
- 正则表达式

核心文件：

- [value.h](../../lib/value.h)
- [json.h](../../lib/json.h)
- [jnum.h](../../lib/jnum.h)
- [template.h](../../lib/template.h)
- [regex.h](../../lib/regex.h)

### 2.6 当前网络主线

当前正式网络主线是：

**xnet-v2 + xtlssession + xhttp/xhttpd/xws**

负责：

- URL
- HTTP 辅助
- 网络底座
- 端口抽象
- 编解码
- engine
- stream
- dgram
- sync/future/wait-source
- TLS session
- HTTP client
- HTTP server
- WebSocket

核心文件：

- [xurl.h](../../lib/xurl.h)
- [xhttp_util.h](../../lib/xhttp_util.h)
- [xnet_base.h](../../lib/xnet_base.h)
- [xnet_mem.h](../../lib/xnet_mem.h)
- [xnet_port.h](../../lib/xnet_port.h)
- [xnet_port_iocp.h](../../lib/xnet_port_iocp.h)
- [xnet_port_uring.h](../../lib/xnet_port_uring.h)
- [xcodec.h](../../lib/xcodec.h)
- [xcodec_http1.h](../../lib/xcodec_http1.h)
- [xcodec_ws.h](../../lib/xcodec_ws.h)
- [xnet_engine.h](../../lib/xnet_engine.h)
- [nettls.h](../../lib/nettls.h)
- [xnet_stream.h](../../lib/xnet_stream.h)
- [xnet_dgram.h](../../lib/xnet_dgram.h)
- [xnet_sync.h](../../lib/xnet_sync.h)
- [xhttp.h](../../lib/xhttp.h)
- [xhttpd.h](../../lib/xhttpd.h)
- [xws.h](../../lib/xws.h)


## 3. 运行时模型

### 3.1 全局态与线程态分离

XRT 当前运行时明确分为两类状态：

- `xrtGlobalData`
- `xrtThreadData`

设计目的：

- 进程级配置保持唯一
- 线程级错误、临时内存、随机数和后续线程内存上下文不再全局混放

### 3.2 进程级运行时

主要保存：

- `sNull`
- 应用路径
- 分配器函数表
- 全局错误回调
- 高频时钟等进程级环境

### 3.3 线程级运行时

主要保存：

- `LastError`
- 线程级临时内存
- 默认随机数状态
- 协程运行时
- current-thread deferred continuation 队列
- 后续线程内存上下文

### 3.4 attach 模型

当前主线采用：

- `xrtInit()` 自动附加当前线程
- `xrtThreadCreate()` 创建的线程自动附加
- 宿主线程如果要调用运行时绑定 API，需显式：
	- `xrtThreadAttachCurrent()`
	- `xrtThreadDetachCurrent()`


## 4. 线程与协程

### 4.1 线程

线程主线已重构为：

- 明确的 Create / Wait / Destroy 生命周期
- runtime attach/detach 自动接线
- 线程 cleanup 栈
- 线程级运行时状态自动释放

### 4.2 协程

协程当前是：

- 线程绑定
- 栈式
- 协作式
- scheduler 驱动

它支持：

- sleep
- deadline wait
- event wait
- cancel / join / close
- 结果槽
- cleanup 栈
- 与 future / xnet wait-source 集成

协程 backend 当前策略：

- production backend 优先走自有实现
- compat backend 仅作为兼容后端
- 正式支持矩阵按 ABI + 编译器 + 实测平台逐步扩展


## 5. 统一异步模型

XRT 当前已经建立统一异步主线：

- `xfuture`
- `xpromise`
- `xtask`
- `xwaitsrc`

并支持：

- `thread / coroutine / engine / delayed` 四类 task 执行入口
- `Then / Catch / Finally`
- `Current / Engine / Co` continuation 目标
- `WhenAny / WhenAll / Race`
- `task group`
- `nested scope`
- `dynamic join`

这条主线的目标是把：

- 线程任务
- 协程等待
- 网络异步结果
- 定时器与事件等待

统一成一套正式运行时语义。


## 6. 内存与 shared-mode

XRT 当前不再默认把所有容器都视为可无约束跨线程共享。

主线规则是：

- 默认对象属于本线程
- 跨线程共享必须显式创建 shared root
- shared root 的公开 root 操作必须遵守对应合同
- 违规跨线程修改会被拒绝并产生诊断

shared-mode 当前已经覆盖：

- root-level array / ptrarray
- dict / list / coll
- real shared `xvalue` 顶层引用计数


## 7. TLS 主线

TLS 当前已经收口为：

- 内部核心实现仍在 [nettls.h](../../lib/nettls.h)
- 对外正式 public surface 为：
	- `xtlssession`
	- `xrtNetTlsSession*`

旧 `xtlsctx / xrtTls*` 已不再属于正式 public 主线。

这意味着：

- `xnet_stream`
- `xhttp`
- `xhttpd`
- `xws`

全部围绕 session 层工作，而不是直接暴露 TLS core 上下文。


## 8. 单头文件与模块裁剪

XRT 仍然保持：

- 单头文件分发能力
- 模块裁剪能力

但裁剪主线已经围绕当前模块重新整理，不再保留旧网络模块的历史裁剪面。


## 9. 当前正式主线与历史归档的边界

当前文档与代码必须遵守：

- 正式主线只描述当前架构
- 历史旧网络/TLS 文档不再作为正式入口
- 历史内容如有保留价值，应迁入 `dev/` 归档

这条边界是为了避免：

- 旧 API 与新 API 并列
- 旧命名误导新用户
- 文档与源码主线持续分叉


## 10. 当前架构的关键优势

当前 XRT 架构相对于“零散工具库”有 5 个关键优势：

- 运行时、并发、异步、网络、数据处理是成体系的，不是外部拼装
- 线程、协程、future、xnet 已形成可互相接线的统一主线
- shared-mode 和 owner-thread 模型明确，减少跨线程误用
- TLS、HTTP、WebSocket 已并入统一网络主线
- 单头文件、轻量化和跨平台目标仍然保留


## 11. 后续架构演进方向

当前架构已经进入正式主线阶段，后续重点不是再增加平行体系，而是继续收口：

- 文档与主线同步
- 网络应用层正式文档补齐
- 更多平台的协程 backend 真机验证
- shared-mode 和内存调试能力继续完善
- 基于统一异步模型扩展更多 AI / Agent 场景能力

