# XRT 教学文档

> 面向第一次系统学习 XRT 的读者。这里回答“什么时候该用什么能力、为什么要这样拆层”，函数签名和返回值细节请回到 `api/`。

[返回文档中心](../README.md)

---

## 建议起步路线

1. [从零开始写第一个 XRT 程序](first-xrt-program.md)
2. [时间、路径与文件系统入门：先把程序落到真实世界](time-path-file-intro.md)
3. [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)
4. [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
5. [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
6. [Queue 入门：什么时候该用消息交接，而不是共享状态](queue-intro.md)
7. [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
8. [XURL 入门：什么时候先把 URL 拆对，比直接发请求更重要](xurl-intro.md)
9. [HTTP Util 入门：Header、Query、Cookie 和 multipart 不是一堆字符串](http-util-intro.md)
10. [XWS 入门：什么时候该让 WebSocket 回调负责协议边界，什么时候该把业务交给 Queue 和 Coroutine](xws-intro.md)
11. [范例入口](../case/README.md)


## 按主题阅读

### 基础运行时与数据

- [从零开始写第一个 XRT 程序](first-xrt-program.md)
- [Charset 入门：程序里的字符串、文件编码和平台宽字符不是一回事](charset-intro.md)
- [OS 入门：什么时候只要启动一个程序，什么时候已经该上 subprocess](os-intro.md)
- [Logger 入门：替代临时 printf，统一控制台、文件和滚动日志](logger-intro.md)
- [XID 入门：什么时候该直接生成唯一 ID，什么时候还不够](xid-intro.md)
- [Math 与 Hash 入门：随机数、约等于和稳定指纹不是一回事](math-hash-intro.md)
- [xvalue、JSON 与 XSON 入门](xvalue-json-intro.md)
- [JNUM 入门：什么时候该直接解析数字文本，而不是把所有输入都先塞进 `atoi / strtod / sprintf`](jnum-intro.md)
- [XSON 入门：什么时候该从 JSON 升级到完整 `xvalue` 序列化](xson-intro.md)
- [Template 入门：什么时候该用模板，而不是拼字符串](template-intro.md)
- [Regex 入门：什么时候该用正则做验证、提取和路由，而不是手搓字符串扫描](regex-intro.md)
- [Crypto 入门：摘要、密钥派生、AEAD 和签名验证不是同一件事](crypto-intro.md)

### 容器、内存与结构

- [Buffer 入门](buffer-intro.md)
- [Array 入门](array-intro.md)
- [PtrArray 入门](ptrarray-intro.md)
- [Stack 入门](stack-intro.md)
- [DynStack 入门](dynstack-intro.md)
- [Dict 入门](dict-intro.md)
- [List 入门](list-intro.md)
- [AVLTree 入门](avltree-intro.md)
- [BSMM 入门](bsmm-intro.md)
- [MemUnit 入门](memunit-intro.md)
- [FSMemPool 入门](mempool-fs-intro.md)
- [MemPool 入门](mempool-intro.md)

### 并发与异步

- [多任务总论：线程、队列、协程与 Future 怎么选](multitask-overview.md)
- [线程入门：什么时候该开线程，什么时候不该](thread-intro.md)
- [Queue 入门：什么时候该用消息交接，而不是共享状态](queue-intro.md)
- [协程、Future 与 Task 入门](coroutine-future-task-intro.md)
- [Wait-Source 入门：把 Future 和网络等待说成同一种语言](wait-source-intro.md)
- [Task Group 入门：从统一等待走到结构化收口](task-group-intro.md)
- [XRT 运行时与线程附加入门](runtime-thread-attach.md)

### 系统能力

- [XRT 子进程与工具执行入门](subprocess-intro.md)
- [XRT 异步文件与目录操作入门](file-async-intro.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)

### 网络与服务

- [xnet-v2 Stream Wait-Source 入门](xnet-stream-wait-source-intro.md)
- [xnet-v2 与 TLS session 入门](xnet-v2-tls-intro.md)
- [XURL 入门](xurl-intro.md)
- [HTTP Util 入门](http-util-intro.md)
- [Proxy 入门](proxy-intro.md)
- [XWS 入门](xws-intro.md)
- [用 XRT 写最小 HTTP 服务入门](minimal-http-service-intro.md)
- [用 XRT 调用流式 LLM API 入门](streaming-llm-api-intro.md)
- [XRT HTTPD 异步处理入门](httpd-async-intro.md)


## 什么时候看 API、什么时候看案例

- 想理解模块的用途、边界和选型顺序，先看这里。
- 已经知道模块名，只想查函数、结构和返回值，去 [API 索引](../api/README.md)。
- 想看多个模块怎么拼成完整程序，去 [范例入口](../case/README.md)。


