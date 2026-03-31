# XRT 教学文档

> 面向零基础、循序渐进和“从概念到代码”的教学入口。这里与 `docs/api/` 的区别是：`api/` 负责合同和边界，`guide/` 负责学习顺序、方案比较和写代码时的心智模型。

[返回文档中心](../README.md)

---

## 目录

- [教学文档的定位](#教学文档的定位)
- [当前状态](#当前状态)
- [正式教学路线](#正式教学路线)
- [现有页面如何使用](#现有页面如何使用)
- [当前空缺](#当前空缺)
- [与 API 文档的关系](#与-api-文档的关系)

---

## 教学文档的定位

`guide/` 目录后续主要承载：

- 面向第一次接触 XRT 的读者
- 面向希望快速搭起可运行程序的开发者
- 面向想理解“为什么这样设计、什么时候该换方案”的读者

因此，这里的文档不应该只是：

- 列 API
- 给一个片段
- 说一句“更多看源码”

而应该系统回答：

1. 这个模块解决什么问题
2. 如果只有一条主线程，会卡在哪里
3. 它和相邻方案的区别是什么
4. 最小 DEMO 该怎么写
5. 什么时候该升级到更完整的版本
6. 常见错误和边界是什么


## 当前状态

当前 `guide/` 已有若干可读页面，但整体仍处于重建中：

- 现有页面大多是“主题短讲”或“快速上手页”
- 适合先扫概念、确认主线方向
- `time / path / file` 已补出基础模块正式教学第一页
- `charset` 已补出基础模块正式教学第一页
- `os` 已补出基础模块正式教学第一页
- `network(local info)` 已补出基础模块正式教学第一页
- `xid` 已补出基础模块正式教学第一页
- `math / hash` 已补出基础模块正式教学第一页
- `buffer` 已补出第 2 阶段正式教学第一页
- `array` 已补出第 2 阶段正式教学第一页
- `ptrarray` 已补出第 2 阶段正式教学第一页
- `stack` 已补出第 2 阶段正式教学第一页
- `dynstack` 已补出第 2 阶段正式教学第一页
- `dict` 已补出第 2 阶段正式教学第一页
- `list` 已补出第 2 阶段正式教学第一页
- `avltree` 已补出第 2 阶段正式教学第一页
- `template` 已补出正式教学第一页
- 还不等于完整课程体系
- 尚未覆盖 XRT 全部公开模块

因此，当前入口分成两层理解：

- [ROADMAP.md](ROADMAP.md)：正式教学重建路线图
- 现有专题页：可作为过渡材料和草稿种子


## 正式教学路线

后续正式中文教学主线按下面顺序重建：

1. [教学重建路线图](ROADMAP.md)
2. [从零开始写第一个 XRT 程序](first-xrt-program.md)
3. [时间、路径与文件系统入门：先把程序落到真实世界](time-path-file-intro.md)
4. [Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事](charset-intro.md)
5. [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](os-intro.md)
6. [本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该](local-network-intro.md)
7. [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](xid-intro.md)
8. [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
9. [Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串](buffer-intro.md)
10. [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
11. [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](ptrarray-intro.md)
12. [Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈](stack-intro.md)
13. [DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈](dynstack-intro.md)
14. [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](dict-intro.md)
15. [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](list-intro.md)
16. [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](avltree-intro.md)
17. [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
18. [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
19. [Queue 入门：什么时候该用消息交接，而不是共享状态](queue-intro.md)
20. [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)
21. [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)
22. 内存、容器与数据结构专题
23. `xvalue / json / xson / template / regex / crypto` 专题
24. 多任务专题：线程、队列、协程、future/task/promise、wait-source、task group
25. 系统能力专题：子进程、异步文件
26. 网络主线专题：`xurl / http util / xnet-v2 / TLS / xhttp / xhttpd / xws / proxy`
27. 完整案例梯度：从配置系统、HTTP 服务到流式 LLM API

如果你要先看“课程蓝图”，直接打开：

- [ROADMAP.md](ROADMAP.md)


## 现有页面如何使用

当前已经存在的页面，可以这样理解：

### 1. 入门种子页

- [从零开始写第一个 XRT 程序](first-xrt-program.md)
- [时间、路径与文件系统入门：先把程序落到真实世界](time-path-file-intro.md)
- [Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事](charset-intro.md)
- [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](os-intro.md)
- [本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该](local-network-intro.md)
- [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](xid-intro.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
- [Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串](buffer-intro.md)
- [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](array-intro.md)
- [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](ptrarray-intro.md)
- [Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈](stack-intro.md)
- [DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈](dynstack-intro.md)
- [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](dict-intro.md)
- [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](list-intro.md)
- [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](avltree-intro.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)
- [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
- [xvalue 与 JSON 入门](xvalue-json-intro.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)

### 2. 并发与异步主线页

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](queue-intro.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
- [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)
- [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)

### 3. 网络与应用页

- [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](../case/xws-session-queue-coroutine.md)
- [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](../case/xhttp-client-proxy-tls.md)
- [用 XRT 写最小 HTTP 服务入门](minimal-http-service-intro.md)
- [用 XRT 调用流式 LLM API 入门](streaming-llm-api-intro.md)
- [XRT HTTPD 异步处理入门](httpd-async-intro.md)

### 4. 系统能力页

- [XRT 子进程与工具执行入门](subprocess-intro.md)
- [XRT 异步文件与目录操作入门](file-async-intro.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)

这些页面当前适合：

- 快速了解一个模块是否已进入主线
- 先拿到最短路径的可运行片段
- 在等待正式课程页重写时作为过渡参考


## 当前空缺

目前还没有成体系教学的模块主要包括：

- `llist`（当前源码树暂无独立 public header，正式教学页暂缓，先保留历史 API 页提示状态）
- `bsmm / memunit / mempool_fs / mempool`
- `jnum / xson / regex / crypto`
- `xurl / xhttp_util / xws / proxy`

这也是后续优先补齐的部分。


## 与 API 文档的关系

建议把 `guide/` 和 `api/` 的关系理解成：

- `api/`：告诉你 API 是什么、合同是什么、边界是什么
- `guide/`：告诉你应该先学什么、为什么这样写、怎样一步步组合成可运行程序

如果你已经知道自己要找哪个函数，优先去：

- [API 索引](../api/README.md)

如果你还不确定应该先学哪条主线，优先从这里开始，并先打开：

- [ROADMAP.md](ROADMAP.md)

