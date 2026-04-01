# XRT 文档中心

> XRT 当前正式文档入口。`api/` 已经可以承担源码主线的合同说明；`guide/` 与 `case/` 正在按“从最小 DEMO 到完整项目”的方式重建。

[English](README.en.md) | [项目简介](../README.md)

---

## 当前状态

- `docs/api/`：当前最适合直接查合同、类型和模块边界。
- `docs/guide/`：现有页面大多还是主题短讲，正在重建为循序渐进教程。
- `docs/case/`：现有页面大多还是专题说明，正在重建为完整案例拆解。
- [文档审计](DOCS_AUDIT.md)：整理哪些文档已过期、哪些链接有问题、哪些模块尚未覆盖。
- [教学重建路线图](guide/ROADMAP.md)：定义后续正式教学主线、模块覆盖范围和案例梯度。


## 文档结构

当前 `docs/` 目录分为 4 层：

### 1. 文档中心

- 当前页 [README.md](README.md)
- [文档审计](DOCS_AUDIT.md)

负责：

- 说明文档结构
- 标记当前状态
- 提供统一入口
- 跟踪重建进度

### 2. API 参考

- [API 索引](api/README.md)

负责：

- 类型与公共结构
- 基础运行时
- 线程、协程、队列与异步
- 内存、容器、文本与数据
- 当前网络主线 API

### 3. 教学文档

- [教学入口](guide/README.md)
- [教学重建路线图](guide/ROADMAP.md)

负责：

- 从零到一教学
- 概念解释
- 方案比较
- 从最小 DEMO 到组合示例的渐进式学习路线

### 4. 案例文档

- [案例入口](case/README.md)

负责：

- 完整问题拆解
- 多模块协作方式
- 从“能跑”走向“能落地”的组合示例


## 当前推荐入口

### 源码与定位

- 根目录 [README.md](../README.md)
- [架构设计](ARCHITECTURE.md)
- [裁剪宏定义说明](裁剪宏定义说明.md)

### API 主线

- [API 索引](api/README.md)
- [Base 基础运行时](api/api-base.md)
- [Value 动态类型系统](api/api-value.md)
- [JNUM 数值转换库](api/api-jnum.md)
- [JSON 处理库](api/api-json.md)
- [XSON 处理库](api/api-xson.md)
- [Thread 线程管理库](api/api-thread.md)
- [Queue 队列模块](api/api-queue.md)
- [Coroutine 协程运行时](api/api-coroutine.md)
- [Future / Task / Promise](api/api-future-task-promise.md)
- [Subprocess](api/api-subprocess.md)
- [Async File](api/api-file-async.md)
- [XURL](api/api-xurl.md)
- [HTTP Util](api/api-http-util.md)
- [XNet v2 网络主线](api/api-xnet-v2.md)
- [Network TLS](api/api-network-tls.md)
- [XHTTP](api/api-xhttp.md)
- [XHTTPD](api/api-xhttpd.md)
- [XWS](api/api-xws.md)

### 教学与案例主线

- [教学入口](guide/README.md)
- [教学重建路线图](guide/ROADMAP.md)
- [案例入口](case/README.md)
- [示例说明](EXAMPLES.md)


## 当前文档重建规则

- 中文版本优先完整和校准。
- 英文版本在中文稳定后统一同步。
- API 文档优先保证“和代码一致”。
- 教学文档优先保证“解释为什么、何时用、如何一步步写出来”。
- 案例文档优先保证“一个完整问题如何把多个模块串起来”。
- 已淘汰的旧主线或内部设计稿，不应继续作为正式入口文档保留在 `docs/`。


## 建议阅读顺序

如果你是第一次接触 XRT，建议按下面顺序阅读：

1. [项目简介](../README.md)
2. [架构设计](ARCHITECTURE.md)
3. [API 索引](api/README.md)
4. [文档审计](DOCS_AUDIT.md)
5. [教学重建路线图](guide/ROADMAP.md)

如果你准备直接写代码，建议优先阅读：

1. [Base](api/api-base.md)
2. [Value](api/api-value.md)
3. [JSON](api/api-json.md)
4. [Thread](api/api-thread.md)
5. [Queue](api/api-queue.md)
6. [Coroutine](api/api-coroutine.md)
7. [Future / Task / Promise](api/api-future-task-promise.md)
8. [Subprocess](api/api-subprocess.md)
9. [Async File](api/api-file-async.md)
10. [XURL](api/api-xurl.md)
11. [HTTP Util](api/api-http-util.md)
12. [XNet v2](api/api-xnet-v2.md)

如果你想先看当前已有的快速教学页，可以继续阅读：

1. [从零开始写第一个 XRT 程序](guide/first-xrt-program.md)
2. [时间、路径与文件系统入门：先把程序落到真实世界](guide/time-path-file-intro.md)
3. [Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事](guide/charset-intro.md)
4. [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](guide/os-intro.md)
5. [本机网络信息入门：什么时候该拿主机名、IP、MAC，什么时候不该](guide/local-network-intro.md)
6. [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](guide/xid-intro.md)
7. [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](guide/math-hash-intro.md)
8. [Buffer 入门：什么时候你需要一块会长大的字节区，而不是数组和字符串](guide/buffer-intro.md)
9. [Array 入门：什么时候该用连续结构体数组，而不是 buffer 和指针数组](guide/array-intro.md)
10. [PtrArray 入门：什么时候该管理对象指针，而不是搬运整个结构体](guide/ptrarray-intro.md)
11. [Stack 入门：什么时候该用固定深度工作栈，而不是数组或动态栈](guide/stack-intro.md)
12. [DynStack 入门：什么时候该用可增长工作栈，而不是固定深度栈](guide/dynstack-intro.md)
13. [Dict 入门：什么时候该用字典存键值，而不是数组、列表或手搓二叉树](guide/dict-intro.md)
14. [List 入门：什么时候该用整数键列表，而不是数组、字典或连续索引](guide/list-intro.md)
15. [AVLTree 入门：什么时候该直接用通用有序树，而不是 Dict、List 或数组](guide/avltree-intro.md)
16. [BSMM 入门：什么时候该用固定大小块池，而不是 `malloc/free`、数组或更重的对象池](guide/bsmm-intro.md)
17. [MemUnit 入门：什么时候该直接控制单个 256 槽内存单元，而不是上 `bsmm`、`fsmempool` 或 `mempool`](guide/memunit-intro.md)
18. [FSMemPool 入门：什么时候该用正式固定大小对象池，而不是 `bsmm`、`memunit` 或通用 `mempool`](guide/mempool-fs-intro.md)
19. [MemPool 入门：什么时候该用通用变长池，而不是 `fsmempool`、`memunit` 或直接 `malloc/free`](guide/mempool-intro.md)
20. [xvalue、JSON 与 XSON 入门](guide/xvalue-json-intro.md)
21. [JNUM 入门：什么时候该直接解析数字文本，而不是把所有输入都先塞进 `atoi / strtod / sprintf`](guide/jnum-intro.md)
22. [XSON 入门：什么时候该从 JSON 升级到完整 `xvalue` 序列化](guide/xson-intro.md)
23. [Template 入门：什么时候该用模板，而不是拼字符串](guide/template-intro.md)
24. [Regex 入门：什么时候该用正则做验证、提取和路由，而不是手搓字符串扫描](guide/regex-intro.md)
25. [Crypto 入门：摘要、密钥派生、AEAD 和签名验证不是同一件事](guide/crypto-intro.md)
26. [多任务总论：线程、队列、协程与 Future 怎么选](guide/multitask-overview.md)
27. [线程入门：什么时候该开线程，什么时候不该](guide/thread-intro.md)
28. [Queue 入门：什么时候该用消息交接，而不是共享状态](guide/queue-intro.md)
29. [协程、Future 与 Task 入门](guide/coroutine-future-task-intro.md)
30. [Wait-Source 入门：把 Future 和网络等待说成同一种语言](guide/wait-source-intro.md)
31. [Task Group 入门：从统一等待走到结构化收口](guide/task-group-intro.md)
32. [XRT 运行时与线程附加入门](guide/runtime-thread-attach.md)
33. [XRT 子进程与工具执行入门](guide/subprocess-intro.md)
34. [XRT 异步文件与目录操作入门](guide/file-async-intro.md)
35. [XURL 入门：什么时候先把 URL 拆对，比直接发请求更重要](guide/xurl-intro.md)
36. [HTTP Util 入门：Header、Query、Cookie 和 multipart 不是一堆字符串](guide/http-util-intro.md)
37. [Proxy 入门：什么时候代理只是一个共享对象，什么时候它已经进入连接时序](guide/proxy-intro.md)
38. [XWS 入门：什么时候该让 WebSocket 回调负责协议边界，什么时候该把业务交给 Queue 和 Coroutine](guide/xws-intro.md)

如果你想直接看当前已有的组合案例，可以优先阅读：

1. [用 xvalue + json 构造配置系统](case/json-config-system.md)
2. [最小 HTTP 服务](case/minimal-http-service.md)
3. [用 Queue + Future 写一个多生产者 Worker](case/queue-worker-future.md)
4. [用 Subprocess + File Async 写一个工具链流水线](case/subprocess-file-async-pipeline.md)
5. [用 XHTTP 走完 URL、代理与 TLS 客户端调用链](case/xhttp-client-proxy-tls.md)
6. [用 XWS + Queue + Coroutine 写一个双向会话服务骨架](case/xws-session-queue-coroutine.md)
7. [线程、协程与 Future 协同模型](case/thread-coroutine-future.md)
8. [流式 LLM API](case/streaming-llm-api.md)
9. [HTTP + JSON + Template 完整链路](case/http-json-template-chain.md)
10. [xnet-v2 Stream Wait-Source 实战](case/xnet-stream-wait-source.md)
11. [用 MemPool + AVLTree + List 组织一个会话索引与回收池](case/session-registry-pool-index.md)

