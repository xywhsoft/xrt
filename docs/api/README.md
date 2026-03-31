# XRT API 索引

> 当前正式 API 参考入口。这里优先收录和当前源码主线一致的 API 文档；少量历史页和待补齐页已在索引中显式标注状态。

[返回文档中心](../README.md)

---

## 1. 基础类型与运行时

| 模块 | 文档 | 说明 |
|------|------|------|
| Types | [types.md](types.md) | 基础类型、公共结构、运行时核心类型 |
| Base | [api-base.md](api-base.md) | 运行时初始化、内存分配、线程级错误与临时内存 |
| Charset | [api-charset.md](api-charset.md) | UTF-8/16/32 编码转换 |
| OS | [api-os.md](api-os.md) | 系统与平台封装 |
| Math | [api-math.md](api-math.md) | 数学工具与默认随机数 |
| String | [api-string.md](api-string.md) | 字符串与格式化 |
| Path | [api-path.md](api-path.md) | 路径处理 |
| Time | [api-time.md](api-time.md) | 时间与日期 |
| File | [api-file.md](api-file.md) | 文件与目录操作 |
| Async File | [../guide/file-async-intro.md](../guide/file-async-intro.md) | 当前先参考教学页，独立 API 文档待补齐 |
| Subprocess | [../guide/subprocess-intro.md](../guide/subprocess-intro.md) | 当前先参考教学页，独立 API 文档待补齐 |
| Network (local info) | [api-network.md](api-network.md) | 本机网络信息 |
| Hash | [api-hash.md](api-hash.md) | 哈希函数 |
| XID | [api-xid.md](api-xid.md) | 分布式唯一 ID |


## 2. 并发与异步

| 模块 | 文档 | 说明 |
|------|------|------|
| Thread | [api-thread.md](api-thread.md) | 线程、附加运行时、cleanup、同步原语 |
| Queue | [api-queue.md](api-queue.md) | 有界并发队列与 MPSC wait 包装 |
| Coroutine | [api-coroutine.md](api-coroutine.md) | 协程运行时、调度器、事件等待 |
| Future / Task / Promise | [api-future-task-promise.md](api-future-task-promise.md) | 统一异步模型、continuation、task group |


## 3. 内存与数据结构

| 模块 | 文档 | 说明 |
|------|------|------|
| Buffer | [api-buffer.md](api-buffer.md) | 动态缓冲区 |
| BSMM | [api-bsmm.md](api-bsmm.md) | 块结构内存管理 |
| MemUnit | [api-memunit.md](api-memunit.md) | 页式内存单元 |
| FSMemPool | [api-mempool-fs.md](api-mempool-fs.md) | 固定大小内存池 |
| MemPool | [api-mempool.md](api-mempool.md) | 通用内存池 |
| Array | [api-array.md](api-array.md) | 结构体数组 |
| PtrArray | [api-ptrarray.md](api-ptrarray.md) | 指针数组 |
| Stack | [api-stack.md](api-stack.md) | 静态栈 |
| DynStack | [api-dynstack.md](api-dynstack.md) | 动态栈 |
| LList | [api-llist.md](api-llist.md) | 历史文档；当前源码树暂无独立 public header 或 `xrtLList*` 公开声明 |
| LList-Base | [api-llist-base.md](api-llist-base.md) | 历史文档；当前源码树暂无独立 public header 或 `xrtLLB*` 公开声明 |
| AVLTree | [api-avltree.md](api-avltree.md) | AVL 树 |
| AVLTree-Base | [api-avltree-base.md](api-avltree-base.md) | AVL 底层操作 |
| Dict | [api-dict.md](api-dict.md) | 字典 |
| List | [api-list.md](api-list.md) | 整数键列表 |


## 4. 高层数据与文本

| 模块 | 文档 | 说明 |
|------|------|------|
| Value | [api-value.md](api-value.md) | 16 种动态数据类型、shared root、引用管理 |
| JNUM | [api-jnum.md](api-jnum.md) | 数值与字符串转换 |
| JSON | [api-json.md](api-json.md) | JSON 解析与生成 |
| XSON | [api-xson.md](api-xson.md) | `xvalue` 完整序列化，兼容 JSON 并扩展 `time / list / set / class` |
| Template | [api-template.md](api-template.md) | 模板引擎 |
| Crypto | [api-crypto.md](api-crypto.md) | 加密基础能力 |
| Regex | [api-regex.md](api-regex.md) | 正则表达式与多模式匹配 |


## 5. 当前网络主线

> 当前正式网络主线是 `xnet-v2 + xtlssession + xhttp/xhttpd/xws`。

| 模块 | 文档 | 状态 |
|------|------|------|
| XNet V2 | [api-xnet-v2.md](api-xnet-v2.md) | 已建立总览文档，包含共享代理对象 |
| Network TLS | [api-network-tls.md](api-network-tls.md) | 当前 TLS session 主线 |
| XURL | [api-xurl.md](api-xurl.md) | URL / authority / target 解析与构建 |
| HTTP Util | [api-http-util.md](api-http-util.md) | header/query/cookie/media type/multipart 工具 |
| XHTTP | [api-xhttp.md](api-xhttp.md) | 当前 HTTP/1.1 客户端主线，支持代理透传 |
| XHTTPD | [api-xhttpd.md](api-xhttpd.md) | 当前 HTTP/1.1 服务端主线 |
| XWS | [api-xws.md](api-xws.md) | 当前 WebSocket client / server 主线，客户端支持代理 |


## 6. 当前说明

- 当前 API 文档优先维护中文版本
- 英文版本将在中文审阅通过后统一同步
- 已淘汰的旧网络/TLS API 文档已迁入 `dev/` 归档
- 当前主线优先围绕“运行时 / 并发 / 队列 / 异步 / xnet-v2 / TLS session / HTTP / WebSocket”组织
- `file_async` 与 `subprocess` 目前先以教学页承载，后续补独立 API 文档
- `llist / llist-base` 相关页面当前仅保留为历史参考；当前源码树暂无独立 public header，恢复正式入口前不应视为现行合同


## 7. 推荐阅读顺序

如果你希望快速理解当前主线，建议按下面顺序阅读：

1. [types.md](types.md)
2. [api-base.md](api-base.md)
3. [api-thread.md](api-thread.md)
4. [api-queue.md](api-queue.md)
5. [api-coroutine.md](api-coroutine.md)
6. [api-future-task-promise.md](api-future-task-promise.md)
7. [api-value.md](api-value.md)
8. [api-xnet-v2.md](api-xnet-v2.md)
9. [api-network-tls.md](api-network-tls.md)
10. [api-xhttp.md](api-xhttp.md)
11. [api-xhttpd.md](api-xhttpd.md)
12. [api-xws.md](api-xws.md)


## 8. 与教学 / 案例文档的边界

本目录只负责：

- API 合同
- 模块能力
- 当前主线行为
- 线程 / 协程 / 异步 / 网络的正式语义

如果你想看更偏教学和组合使用方式，请继续阅读：

- [教学文档](../guide/README.md)
- [范例解析](../case/README.md)
