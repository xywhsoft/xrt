# XRT 教学重建路线图

> 目标：把 `guide/` 从“主题说明集合”重建为“从第一个 DEMO 到完整项目”的课程体系，并最终覆盖 XRT 全部公开模块。

[返回教学文档](README.md) | [返回文档中心](../README.md)

---

## 1. 写作原则

后续每一篇正式教程都应固定回答下面这些问题：

1. 这个模块解决什么问题
2. 如果只有一条主线程，程序会卡在哪里
3. 它和相邻方案的区别是什么
4. 最小 DEMO 该怎么写
5. 什么时候该升级到第二个、更完整的 DEMO
6. 常见错误和边界是什么
7. Windows / Linux 跨平台时要注意什么
8. 下一篇应该学什么

因此，正式教学页不再只是 API 摘要，而至少要包含：

- 场景动机
- 方案比较
- 最小可运行示例
- 逐步扩展
- 常见误区
- 练习或改造任务


## 2. 分阶段路线

### 第 0 阶段：认识 XRT 与跑通第一个程序

- 覆盖模块：整体项目结构、`xrt.h`、`xrt.c`
- 目标：理解源码树、构建方式、`xrtInit()` / `xrtUnit()` 的边界
- 计划文档：
1. 第一个 XRT 程序
2. 源码树、单头分发与文档结构
3. Windows / Linux 最小构建脚本

### 第 1 阶段：基础运行时与工具模块

- 覆盖模块：`base`、`string`、`charset`、`time`、`path`、`file`、`os`、`math`、`hash`、`xid`、`network`
- 目标：让读者先具备“写一个正常跨平台小程序”的基础能力
- 计划文档：
1. 运行时初始化、错误槽与临时内存
2. 字符串与编码处理
3. 时间、路径与文件系统
4. 系统调用、进程打开与本机网络信息
5. 常用工具函数：随机数、哈希、唯一 ID

### 第 2 阶段：内存模型、容器与数据结构

- 覆盖模块：`bsmm`、`memunit`、`mempool_fs`、`mempool`、`buffer`、`array`、`ptrarray`、`stack`、`dynstack`、`llist`、`avltree`、`dict`、`list`
- 目标：让读者理解“什么时候该继续用普通分配，什么时候该切换到池、缓冲区或容器”
- 计划文档：
1. 为什么 XRT 不只包一层 `malloc/free`
2. 固定块、小对象、通用对象池的选择方法
3. 顺序容器与链式容器
4. 字典、AVL、列表与查找结构

### 第 3 阶段：结构化数据与文本处理

- 覆盖模块：`value`、`jnum`、`json`、`xson`、`template`、`regex`、`crypto`
- 目标：让读者能把原始字符串升级成“配置、模板、协议、校验、加密”这类真实数据链路
- 计划文档：
1. `xvalue` 的类型系统与所有权
2. `json` 与 `xson`
3. `template` 的表达式、控制流与组合
4. `regex` 在验证、提取和路由中的用法
5. `crypto` 在签名、摘要和编码场景中的边界

### 第 4 阶段：多任务与异步主线

- 覆盖模块：`thread`、`queue`、`coroutine`、`future/task/promise`、`wait-source`、`task group`
- 目标：把“为什么需要多任务、哪些方式可选、什么时候换模型”讲完整
- 计划文档：
1. 为什么需要多任务：一条主线程的瓶颈是什么
2. 线程专题：创建、附加、同步、适用场景与成本
3. 队列专题：生产者/消费者、背压、关闭与 drain
4. 协程专题：单线程编排、等待、deadline、cleanup
5. Future / Task / Promise：统一异步结果、组合等待、continuation
6. Wait-Source：把 future、stream、listener、dgram 收敛到同一等待模型
7. Task Group 与结构化并发：关闭、回收、父子取消传播
8. 线程 / 协程 / future 的混合分工与选型表

### 第 5 阶段：系统能力与工具链

- 覆盖模块：`subprocess`、`file_async`
- 目标：让读者能写出工具程序、批处理任务和 agent 式工作流
- 计划文档：
1. 子进程创建、stdout/stderr 管道与 future 等待
2. 异步文件句柄、整文件 helper 与目录任务
3. `subprocess + file_async + future` 组合实例

### 第 6 阶段：网络主线

- 覆盖模块：`xurl`、`xhttp_util`、`xnet-v2`、`network-tls`、`xhttp`、`xhttpd`、`xws`、`xnet_proxy`
- 目标：把底层网络等待一路讲到应用层协议
- 计划文档：
1. URL、header、query、cookie 等基础拼装
2. `xnet-v2` 的 engine、stream、dgram、listener
3. TLS session 的位置、边界和生命周期
4. HTTP client
5. HTTP server
6. WebSocket
7. 代理对象与代理握手

### 第 7 阶段：完整项目梯度

- 目标：从小型功能逐步升级到多模块组合项目
- 计划案例：
1. 配置系统：`file + path + value + json`
2. 模板渲染：`value + template + file`
3. 多任务 worker：`thread + queue + future`
4. 工具链流水线：`subprocess + file_async + future`
5. HTTP 客户端调用链：`xurl + xhttp + TLS + proxy`
6. HTTP 服务：`xhttpd + json + template`
7. 流式 LLM API：`xhttp/xws + future + coroutine`


## 3. 多任务专题的正式拆法

用户最关心、也是最容易讲浅的一条线，是多任务。

这条线后续必须按下面顺序展开，而不是直接跳到函数列表：

1. 为什么单主线程不够用
2. 哪些工作适合线程
3. 哪些工作适合协程
4. 为什么需要 `future / task / promise`
5. `wait-source` 为什么能统一等待语义
6. `queue` 在 worker、背压和跨线程交接里扮演什么角色
7. 什么叫“线程负责执行，协程负责编排，future 负责结果”
8. 什么场景下要混合使用它们

每一页都应至少给出：

- 一个最小例子
- 一个升级例子
- 一个“错误示范为什么不好”的对照


## 4. 现有文档如何衔接

当前已有页面不会直接废弃，而是按下面方式衔接：

- [first-xrt-program.md](first-xrt-program.md)：保留为第 0 阶段入口
- [time-path-file-intro.md](time-path-file-intro.md)：作为第 1 阶段 `time / path / file` 正式第一页
- [charset-intro.md](charset-intro.md)：作为第 1 阶段 `charset` 正式第一页
- [os-intro.md](os-intro.md)：作为第 1 阶段 `os` 正式第一页
- [local-network-intro.md](local-network-intro.md)：作为第 1 阶段 `network(local info)` 正式第一页
- [xid-intro.md](xid-intro.md)：作为第 1 阶段 `xid` 正式第一页
- [math-hash-intro.md](math-hash-intro.md)：作为第 1 阶段 `math / hash` 正式第一页
- [buffer-intro.md](buffer-intro.md)：作为第 2 阶段 `buffer` 正式第一页
- [array-intro.md](array-intro.md)：作为第 2 阶段 `array` 正式第一页
- [ptrarray-intro.md](ptrarray-intro.md)：作为第 2 阶段 `ptrarray` 正式第一页
- [stack-intro.md](stack-intro.md)：作为第 2 阶段 `stack` 正式第一页
- [dynstack-intro.md](dynstack-intro.md)：作为第 2 阶段 `dynstack` 正式第一页
- [dict-intro.md](dict-intro.md)：作为第 2 阶段 `dict` 正式第一页
- [list-intro.md](list-intro.md)：作为第 2 阶段 `list` 正式第一页
- [avltree-intro.md](avltree-intro.md)：作为第 2 阶段 `avltree` 正式第一页
- [bsmm-intro.md](bsmm-intro.md)：作为第 2 阶段 `bsmm` 正式第一页
- `llist`：当前源码树暂无独立 public header，先保留历史 API 页并阻止其误入正式教学主线
- [multitask-overview.md](multitask-overview.md)：作为第 4 阶段正式前导页
- [thread-intro.md](thread-intro.md)：作为第 4 阶段线程专题正式第一页
- [queue-intro.md](queue-intro.md)：作为第 4 阶段队列专题正式第一页
- [wait-source-intro.md](wait-source-intro.md)：作为第 4 阶段 wait-source 专题正式第一页
- [task-group-intro.md](task-group-intro.md)：作为第 4 阶段结构化并发与 scope 收口专题正式第一页
- [runtime-thread-attach.md](runtime-thread-attach.md)：并入第 4 阶段线程专题前导页
- [xvalue-json-intro.md](xvalue-json-intro.md)：并入第 3 阶段
- [template-intro.md](template-intro.md)：作为第 3 阶段 `template` 专题正式第一页
- [coroutine-future-task-intro.md](coroutine-future-task-intro.md)：并入第 4 阶段
- [subprocess-intro.md](subprocess-intro.md)：并入第 5 阶段
- [file-async-intro.md](file-async-intro.md)：并入第 5 阶段
- 网络相关页面：逐步并入第 6 阶段


## 5. 完成标准

当下面 4 条同时满足时，才算教学主线重建完成：

1. XRT 全部公开模块都有对应入口
2. 每个阶段都有“最小 DEMO -> 升级 DEMO -> 完整案例”
3. 多任务专题已经把线程、协程、future、queue、wait-source 讲成一条完整主线
4. 中文主线稳定后，再同步英文版本
