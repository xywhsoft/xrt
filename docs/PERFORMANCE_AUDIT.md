# XRT 全库高性能路径评估

日期：2026-08-12

本报告评估当前 XRT 2.0 重构工作树的性能架构、高性能 API 路径、实现上限、
裁剪结果和验证缺口。范围只包括 XRT 自身，不包括 xlang 适配，也不把 HTTP/2、
HTTP/3 纳入当前发布目标。

本报告不是一次吞吐排行榜。基础库的“极致性能”应解释为：公共契约不强迫分配、
复制、加锁或构建重型对象；调用方能直接选择引用、所有权转移、批处理、流式处理和
平台原生路径；所有队列和内存都有硬上限；高负载下不存在无法绕开的全局瓶颈。

> **实施状态（2026-08-13）**
>
> 本文第 1 至 17 节保留 2026-08-12 的实施前审计快照，用于解释优化决策，不能再
> 当作当前缺口列表。本轮已经完成以下高性能路径：
>
> - 单头实现按模块闭包裁剪，并增加正向依赖、缺失依赖、符号和体积门禁；
> - 新增工作窃取 Executor、批量提交和 Detached Task；
> - HTTP 客户端请求 Lease 以 Ref/Take 进入 TCP 队列，连接池按分片管理；
> - HTTP 服务端完整 wire/向量响应支持 Copy、Ref、Refs 和 Take；
> - Windows IOCP 与 Linux io_uring 原生文件读写，Windows `TransmitFile`、Linux
>   io_uring `splice` 和 readiness `sendfile` 文件发送；TLS 保持显式加密 fallback；
> - io_uring SQE 延迟批量发布，正确处理部分提交、取消前发布和高并发预投递；
> - WebSocket 核心收敛为无网络对象依赖的帧、消息、握手、扩展和流式压缩原语；
> - TLS 服务端握手临时 Arena，以及 AArch64 AES/PMULL 运行时硬件路径；
> - 预分配 Logger Ring，预热后生产者不请求堆内存、不等待生产者互斥；
> - 网络统计支持 `OFF`、`BASIC`、`FULL` 编译成本等级。
> - Runtime Model 已补齐类型操作、对象引用和动态调用基准，并分别测量核心与完整闭包；
>   它按层裁剪且不复制基础容器实现，现已从 `review` 转为 `retained`。
> - 2026-08-14，HTTP 客户端、服务器、路由、拥有型报文，以及 WebSocket 连接、Writer、
>   Future/TLS 适配、连接组和客户端/服务器均已退出 XRT 核心；此前完成的高层资产保存在
>   `extlibs/xhttp/archive` 与 `extlibs/xws/archive`，供扩展库阶段按需复用。
> - 当前 HTTP 核心只冻结借用式 HTTP/1.1 解析、正文边界、调用方输出、Upgrade 和流式
>   gzip/deflate 解码。后文涉及旧客户端/服务器规模与瓶颈的内容只保留为历史决策依据。
>
> 当前公开网络所有权仍坚持“一次提交、一个缓冲所有者、一个终态”。io_uring
> multishot receive、provided buffer ring、registered resources 和 `SEND_ZC` 没有
> 强行塞入这一契约：它们需要可跨多个 CQE 移交缓冲所有权的新模型，且必须先有真实
> Linux 负载收益证据。现阶段后端已具备批量 SQ 提交、原生文件 I/O 和文件发送能力，
> Linux io_uring 与 ARM64 硬件路径由 CI 专项门禁执行，本机 Windows 只验证其构建边界。



## 1. 结论

当前 XRT 已经具备一批真正可用的高性能底座：临时内存、内存池、字符串视图、
调用方缓冲区、流式 Codec、无锁有界队列、Channel、TCP 引用发送、UDP 批量接口、
HTTP 流式协议解析、HTTP 服务端引用正文、WebSocket 服务端引用发送、TLS 到 TCP 的
缓冲区所有权转移。这些实现不是只有易用性包装，底层能力确实已经公开。

但是，当前版本还不能整体宣称“所有模块均达到极致性能”。最重要的原因不是个别
循环慢，而是仍存在以下结构性上限：

1. 单头实现没有按功能宏隔离源码，最小 Core 配置仍会编入 BBRE、miniz 等未选实现。
   这直接破坏裁剪、编译速度和最小制品体积，是发布阻断项。
2. 当前 TaskPool 是严格有界共享 FIFO，适合粗粒度、可预测任务，但细粒度任务随
   Worker 增加会受共享队列和同步竞争限制。可扩展执行器路径尚未形成。
3. HTTP 服务端已有接近 TCP 的高性能路径，但完整 raw 响应仍有复制型入口；HTTP
   客户端会把已准备的请求片段再次复制进 TCP 发送队列，连接池还有全局锁热点。
4. 异步文件由 TaskPool 执行阻塞文件操作，缺少 IOCP/io_uring 原生文件完成路径，
   也没有 `sendfile`、`TransmitFile`、`splice` 等文件到 Socket 的零拷贝路径。
5. Linux io_uring 当前是可用后端，不是高级极限后端：尚无 multishot、buffer ring、
   registered file/buffer、`SEND_ZC` 等能力，也缺少真实 Linux 发布基线。
6. TLS 稳态数据路径合理，x86 AES-GCM 有硬件加速；完整握手仍有较多临时分配，
   ARMv8 AES/PMULL 快速路径和跨平台密码套件基线尚未补齐。
7. Runtime Model 仍处于 `review`，注册表、反射、对象图和类型容器的热点性能没有形成
   发布证据，不能与已经压实的基础容器同等评价。

因此，当前整体判断是：**高性能能力大部分已经存在，但还没有在所有关键体系中形成
可长期冻结的完整契约。** 网络与服务端协议底座最接近目标；单头裁剪、细粒度任务、
HTTP 客户端、原生文件异步和跨平台证据是下一阶段的主要工作。



## 2. 评估口径

### 2.1 等级

| 等级 | 含义 |
| --- | --- |
| 可用 | 存在公开、可组合且没有明显结构性上限的高性能路径 |
| 有条件可用 | 快速路径存在，但受平台、负载、所有权或验证范围限制 |
| 不足 | 只能使用便利路径，或热点中存在无法绕开的分配、复制、共享锁 |
| 阻断 | 会破坏核心性能承诺或裁剪承诺，发布前必须修复 |

“可用”不表示所有便利 API 都是零开销。XRT 应允许同一能力具有三层入口：

- 基础层：视图、调用方缓冲区、迭代器、Sink、引用和所有权转移。
- 组合层：拥有型对象、Builder、Future、自动解析和自动资源管理。
- 手感层：一行完成常见操作，允许为简单性支付有限且明确的成本。

只有基础层不能绕开成本时，才属于性能架构问题。

### 2.2 每条高性能路径必须记录的指标

- 每次操作的堆分配次数，以及预热后是否可以做到零分配。
- 应用数据复制字节数；协议或加密强制转换应单独统计。
- 每条消息、每个请求和每批数据的系统调用次数。
- 每次操作的锁、原子读改写和跨核缓存线迁移。
- 吞吐、p50、p99、最大延迟和队列等待时间。
- 每个空闲对象、连接、Worker 和活跃请求的常驻内存。
- 高水位、拒绝、丢弃、超时、取消和回收后的残留对象数。
- 裁剪后的 `.text`、数据段、文件体积和编译时间。

### 2.3 证据等级

本报告把证据分为三类：

- **实测**：当前工作树在本机实际运行所得。
- **代码审计**：由公开 API、所有权规则和具体实现路径确认。
- **待验证**：架构看起来合理，但还没有目标平台或目标负载证据。

一次回环 Smoke 只能用于发现明显异常，不能证明跨平台生产性能。



## 3. 当前实测快照

环境为 Windows x64、AMD Ryzen 5 5600、GCC 16.1.0、`-O2`、IOCP。当前工作树
有未提交修改。2026-08-12 的 Smoke 只运行一次、没有预热，结果仅用于诊断：

| 路径 | 当前结果 |
| --- | ---: |
| TaskPool windowed | 1,717,770 tasks/s |
| Channel local | 28,442,000 items/s |
| Channel buffered | 18,857,000 items/s |
| Coroutine Channel buffered | 6,956,810 items/s |
| MPSC batch 32 | 87,981,700 items/s |
| MPMC batch 32 | 71,576,800 items/s |
| TCP 64 B 回环 | 40,065 round-trips/s，p99 38.6 us |
| UDP 256 B 批量回环 | 226,978 packets/s |
| HTTP Reply keep-alive | 23,547 requests/s，p99 79.9 us |
| HTTP raw keep-alive | 29,653 requests/s，p99 54.8 us |
| WebSocket 64 B 回环 | 47,150 messages/s，p99 29.3 us |
| TLS 1.3 完整握手 | 140 handshakes/s，p99 7,432 us |
| TLS 1.3 恢复 | 913 handshakes/s，p99 1,192 us |
| TLS 加密流回环 | 57,516 round-trips/s，p99 32.9 us |

五样本开发基线见
[`dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md`](../dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md)。
该基线同样来自脏工作树，只能作为重构期回归基线。正式发布必须在干净提交上按编译器、
CPU、后端和平台分别建立基线。

当前 raw HTTP 比结构化 Reply 快约 26%，证明保留直接响应路径是必要的；这不是要求
删除 Reply，而是要求 raw/ref/take 路径始终独立、完整且更接近 TCP 成本。



## 4. 体积与裁剪

### 4.1 当前单头实测

当前 `single/xrt.h` 为 11,233,769 字节。GCC `-O2` 编译后的单对象结果如下：

| 功能配置 | `.text` | 对象文件 |
| --- | ---: | ---: |
| Core | 108,284 B | 142,152 B |
| Console | 111,584 B | 147,654 B |
| Foundation | 216,536 B | 290,320 B |
| Concurrency | 191,720 B | 267,970 B |
| Network | 395,272 B | 529,201 B |
| HTTP + WebSocket | 768,320 B | 1,017,898 B |
| All | 3,374,112 B | 4,406,473 B |

Core 的模块化静态对象 `.text` 约 11 KB，而单头 Core 为 108 KB。最小单头对象中可以
找到 BBRE 和 miniz 符号，证明这不是链接器显示误差，而是实现源码已被编译。

### 4.2 根因

`tools/amalgamate.py` 当前把所有内部头和所有源文件集中放入全局
`XRT_IMPLEMENTATION` 区域，没有按拥有该源文件的功能宏生成条件编译边界。公共声明
虽然可以裁剪，未选模块的实现却仍进入编译单元。

### 4.3 必须修复的契约

1. 每个内部头和源文件必须被其所属功能宏保护。
2. 多个模块共享的源文件使用所有所有者功能宏的逻辑或，不复制实现。
3. BBRE、miniz、数字转换内核等第三方实现必须有精确边界。
4. 每个最小配置增加负向符号测试，确认未选模块的符号不存在。
5. 体积门禁同时测模块化库和单头，并新增冷编译、增量编译时间门禁。
6. 单头仍保持一个规范发行文件；按 Profile 生成更小单头可以作为附加制品，不能掩盖
   规范单头的裁剪错误。

**结论：阻断。** 在修复前，XRT 最经典的轻量集成方式不满足自身裁剪承诺。



## 5. 基础、内存与文本

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| 全局分配器 | 有条件可用 | 小块分级、线程缓存、批量回填和 Span 复用 | 缺跨线程释放、碎片和中央竞争矩阵 |
| Temp Arena | 可用 | 每线程/协程 Arena、线性分配、批量重置 | 需把峰值和 Spill 纳入统一指标 |
| Pool/Page/MemPool | 可用 | 无隐藏锁、Reserve/Trim、固定页复用 | 缺正式发布基准 |
| Buffer/Array | 可用 | 连续存储、显式 Reserve、Take/Detach | 需统一统计分配和扩容次数 |
| String/View/Builder | 可用 | View、调用方缓冲区、Builder、零分配迭代 | Unicode 大文本基准不足 |
| Number | 可用 | 使用高性能数字转换内核，不引入完整 yyjson DOM | 需补全格式和极值性能矩阵 |
| Hash | 可用 | nmhash、rapidhash、SipHash、流式状态 | 缺 ARM SIMD 证据 |
| Codec | 可用 | HEX/Base64/Percent 的 size、to、in-place、stream | SIMD Base64 可按基准决定 |
| Regex | 有条件可用 | 可复用 Matcher/Set，避免回溯爆炸 | 无 JIT；编译与大集合门禁不足 |
| Compress | 有条件可用 | miniz 流式、Reset/Reuse、使用时才分配 | 标量吞吐不是极限，缺级别/块大小矩阵 |

当前内存底座已经解决“每个网络对象固定 8K 缓冲”的旧问题。网络缓冲按
512/2048/8192/32768 分级、按 Worker 缓存；空闲连接不预占固定 8K。默认缓存上限约为
每个 Worker 2 MiB，具体内存只随活跃流量增长。

网络缓冲池目前在每次分配和释放时更新实时、峰值、分配和复用统计。它位于 Worker
本地，不需要原子操作，但极限负载下仍会增加指令和写缓存线。应先用硬件计数器测量，
若成本显著，再提供 `OFF/BASIC/FULL` 可观测级别；不能为了理论性能直接删除诊断能力。

Regex 不应为了追求单一峰值而强制引入大型 JIT。对标准库而言，可预测时间和无回溯
拒绝服务风险更重要。只有确有工作负载证据时，才考虑可裁剪 JIT 后端。

miniz 的引入是合理的：HTTP gzip、WebSocket deflate 依赖它，公开压缩 API 可以复用
已有容量。但是，应以压缩级别、数据类型、块大小、压缩比和内存峰值共同评估；若
HTTP 压缩成为瓶颈，可设计可选后端接口，而不是直接再内置一个大型压缩库。



## 6. 容器、数据与通用运行时

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| Queue | 可用 | 外部缓冲、有界 SPSC/MPSC/MPMC、批量接口 | SPSC 需拓扑固定基线 |
| Map/Set | 有条件可用 | Reserve、平均 O(1)、稳定地址和插入顺序 | 每条目分配影响批量装载和局部性 |
| Stack/Fixed/Block Stack | 可用 | 连续或分块存储，可预留容量 | 尚未进入统一发布基线 |
| Value | 有条件可用 | COW、Take/Ref、快速 Scalar、Typed Container | 不能替代静态类型热路径 |
| JSON | 有条件可用 | Valid 零分配、Visit 事件流、Writer/Sink、DOM 可选 | 标量结构扫描仍有 SIMD 提升空间 |
| XSON | 有条件可用 | 与 JSON 相同的 Visit/Writer 分层 | 仍需审计扫描器和写出逻辑复用 |
| Runtime Model | 不足 | 公开动态类型、反射、对象图和类型容器 | 注册、查找、调用和回收均缺发布证据 |

MPSC/MPMC 队列的批量路径在本次 Smoke 中分别达到约 8,798 万和 7,158 万 items/s，
说明减少原子竞争的接口确实有效。需要保留文档中的进度语义：生产者预留位置后若长时间
停顿，后续项仍可能受 FIFO 发布顺序阻塞，不能把它宣传为所有意义上的无等待队列。

Map 的稳定地址和插入顺序是有价值的通用契约，但每条目紧凑分配仍会限制极端批量装载
和缓存局部性。不要破坏当前 Map 来追逐微基准。只有真实数据库、编译器符号表或路由表
基准证明必要时，才增加独立、可裁剪、允许元素移动的 Packed Map。

JSON 已经有正确的性能分层：简单校验和事件访问不需要 DOM；拥有型树只在调用方选择时
产生。历史开发基准中，小文档 Visit 明显快于 DOM。下一步应在不改变 API 的前提下评估
可裁剪 SIMD 结构扫描器，并用真实大小和转义比例语料验证，不应因 yyjson 名气而替换整套
JSON 体系。当前 yyjson 资产主要用于优质数字转换内核，这一选择合理。

Runtime Model 约有 2 万行实现，且注册表与对象图存在共享同步。它是 xlang 和动态数据
体系的重要能力，但不应成为基础容器的隐式依赖。发布前必须测量：类型注册/查找、字段
访问、动态调用、Typed 与基础容器差值、引用操作和环回收停顿。注册阶段结束后，应允许
冻结注册表，使读路径使用不可变快照，避免热点查询经过全局写锁。



## 7. 并发、协程与任务

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| Atomic/Thread/Sync | 有条件可用 | 薄平台封装、明确内存序 | 缺跨平台争用和公平性门禁 |
| 无锁 Queue | 可用 | 有界、外部存储、批处理 | 见队列进度语义 |
| Channel | 可用 | Local、Buffered、Rendezvous、Select、协程等待 | 缺多拓扑 p99 和高争用矩阵 |
| Future | 有条件可用 | 线程/协程统一完成与继续 | 对每包、每项细粒度操作偏重 |
| Coroutine | 有条件可用 | 单所有者调度、跨线程 Post、有保护栈 | 旧基准未迁移，非 x86 证据不足 |
| TaskPool | 不足 | 严格有界共享 FIFO、背压、Future | 细粒度任务扩展性有结构上限 |

当前 TaskPool 的设计适合“行为可预测、队列有界、任务相对较粗”的通用任务池。历史
Worker 扩展测试显示，极小任务在 1 Worker 时约 280 万 tasks/s，增加到 4 和 12 Worker
后反而下降到约 119 万和 80 万，说明共享 FIFO、条件变量和池锁已成为扩展上限。

不能通过破坏当前 FIFO、公平性和容量契约来偷偷把它改成工作窃取。正确方案是增加一个
独立且可裁剪的高吞吐 Executor：

1. 每个 Worker 使用 Chase-Lev 本地双端队列。
2. 外部提交进入全局有界 MPSC Injection Queue。
3. Worker 先执行本地任务，再批量拉取外部任务，最后窃取。
4. 提供 Batch Submit 和 Detached Task，热路径不强制创建 Future。
5. Future/TaskGroup 作为可选组合层，不能污染 Detached 热路径。
6. 明确亲和、阻塞任务隔离、Shutdown、取消和饥饿边界。

Future 适合跨线程结果和结构化异步，但每个极小操作都创建 Future 会产生对象、锁和等待器
管理成本。网络内部应继续使用 Completion/Wait Source；公开层可增加池化 Promise 或
侵入式等待节点，但只能在生命周期清晰且基准证明收益后实施。

协程架构本身合理：Windows Fiber，POSIX `mmap` 保护栈和延迟提交，Scheduler 单线程
拥有，跨线程操作通过队列进入。当前性能 profile 已覆盖上下文切换、创建/销毁、Timer
和跨线程 Post。LoongArch64 LP64D 已在龙芯 3A5000LL 上取得当前 ABI、功能和性能 smoke
证据，ARM64 与 RISC-V 仍没有对应真机运行证据。



## 8. 文件、I/O、进程与日志

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| Reader/Writer | 可用 | 流式回调，不强制中间整块缓冲 | Wrapper 创建仍可能分配 |
| File/Map | 可用 | 原始读写、定位读写、内存映射、整文件 Helper | 大文件和随机 I/O 基线不足 |
| Async File | 不足 | 有界 TaskPool Fallback | 阻塞 Worker，不是原生完成 I/O |
| Path/Time/Environment | 可用 | 薄封装、调用方缓冲和文本转换分层 | 缺统一微基准，优先级低 |
| Process/Pipeline | 有条件可用 | 流式管道、Capture 可选 | Spawn 和大输出背压证据不足 |
| 同步 Logger | 可用 | 流式格式化、文件 Sink 复用缓冲 | 多 Sink 吞吐矩阵不足 |
| 异步 Logger | 不足 | 有界队列和明确丢弃统计 | 每记录深拷贝、互斥链表队列 |

异步文件当前的“异步”语义是把阻塞文件操作提交给 TaskPool。它是可靠的跨平台兼容层，
但在大量并发文件请求下会占满任务线程，也不能与网络 Engine 统一完成和取消。建议增加
可裁剪的原生文件端口：Windows IOCP、Linux io_uring；其余平台继续使用任务池 Fallback。

仓库目前没有 `sendfile`、`TransmitFile`、`splice`、`copy_file_range` 或等价的文件到
Socket 传输路径。这是 HTTP 静态文件、代理和大文件服务最明显的系统级缺口。应在网络
传输层公开文件区间发送，再由 HTTP 组合；TLS 因加密必须退回用户态读取和加密，接口应
明确报告实际采用的路径。

异步 Logger 当前对每条记录深拷贝，并经过互斥链表队列。可靠性和有界行为是完整的，
但高频多生产者日志会成为热点。可选高吞吐路径应使用每线程 SPSC Lane 或有界 MPSC Ring、
批量 Drain 和复用记录块；可靠队列继续保留，二者通过策略选择。



## 9. 网络底座、TCP、UDP 与 DNS

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| Socket 基础 | 可用 | 标量/向量单系统调用、调用方缓冲 | 缺文件区间和内核零拷贝发送 |
| Engine/Port | 有条件可用 | Worker、MPSC Command、Timer Heap、完成/就绪统一 | 缺跨后端规模基线 |
| Select | 可用作 Fallback | 非 Windows/Linux/BSD 的完整后备端口 | 仅应作为 Tier C，不追求极限吞吐 |
| IOCP | 可用 | AcceptEx、重叠收发、零字节读探针、Scatter/Gather | 需更多真实高连接数门禁 |
| epoll/kqueue | 有条件可用 | Readiness 批量 Drain 和统一对象模型 | 与 IOCP 功能/性能矩阵不完整 |
| io_uring | 有条件可用 | 原生 SQE/CQE 后端 | 缺高级能力和真实 Linux 证据 |
| TCP | 可用 | SendRef/Refs/Take/Buffer、硬背压、Drain、并发 Accept | 缺文件发送和内核零拷贝 |
| UDP | 有条件可用 | Batch、Recv Concurrency、队列/丢弃/统计 | Windows 无批量 syscall，uring 未用 multishot |
| Resolver | 有条件可用 | 有界线程、合并查询、正负缓存、TTL/LRU | `getaddrinfo` 不能真正取消，缓存锁需压测 |

TCP 是当前最完整的高性能体系之一。发送层同时具备 Copy、Ref、Refs、Take 和 Worker
Buffer；发送队列有硬上限、高低水位和 Drain；接收有自适应、直接和 Probe 模式；服务端
有 Accept 并发度和队列限制。`SendRef` 能避免应用数据进入发送队列时再次复制，底层再用
`WSASend`/`writev` 类向量发送。这满足标准库应有的应用层零拷贝契约。

这里的“零拷贝”只指应用缓冲到 XRT 发送队列不复制，普通 Socket 发送仍会复制到内核。
大文件优先补 `sendfile`/`TransmitFile`；Linux `MSG_ZEROCOPY`/io_uring `SEND_ZC` 会引入
完成通知和页固定成本，应在大块负载基准证明收益后作为可选路径，不能替换普通发送。

UDP 已具备高层批量 API。Linux 模块化构建可使用 `recvmmsg`/`sendmmsg`；Windows IOCP
通过多个并发 `WSARecvFrom` 保持管线。当前单头在 Linux 上若没有在系统头之前定义所需
GNU 声明，可能静默退回逐包路径。规范单头必须在最前端建立平台特性宏，或者内部使用
稳定 syscall 封装，不能让包含顺序改变性能能力。

io_uring 应分成两个等级：

- 基础模式：保持当前简单、稳健、可回退的 SQE/CQE 实现。
- 高吞吐模式：能力探测后启用 multishot accept/recv、provided buffer ring、registered
  file/buffer 和适用场景下的 zero-copy send。

Timer 当前使用最小堆和 ID Hash，插入/删除为 O(log n)，取消可定位，不是低质量链表。
只有百万级短 Timer 的基准证明堆成为瓶颈时，才增加分层时间轮；时间轮不应仅因理论
复杂度更低而替换当前精确、清晰的实现。

Select Fallback 路径确实存在。自动选择为 Windows IOCP、Linux epoll、BSD/macOS
kqueue，其余平台使用 select；显式指定后端失败时不应静默换后端。Select 的目标是完整
兼容和正确退化，不是与 IOCP/io_uring 争夺极限性能。



## 10. HTTP 协议核心

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| URL/Query/Form | 可用 | View、迭代器、调用方缓冲、流式解析 | 需统一大输入和恶意边界基准 |
| HTTP Parser/Writer | 可用 | 借用 View、分段输入、调用方输出、流式 Body | 需更完整的切片/字段规模矩阵 |
| Content Decode | 可用 | 增量 gzip/deflate、回调输出、硬上限、Reset 复用 | 需持续压测多层编码与小分块 |
| Upgrade | 可用 | 借用字段、调用方输出、保留 TCP 余量 | 需与 WebSocket/TLS 组合回归 |

当前 HTTP 核心不构建拥有型请求或响应，也不管理连接池、路由和业务状态。接收路径直接在
调用方提供的报文切片上解析借用 View，并由正文状态机消费任意 TCP 分块；发送路径把状态行、
字段向量和正文所有权直接交给通用 TCP/TLS API。固定响应可以直接提交预构造 wire bytes，
动态响应可以使用栈字段数组和调用方缓冲，不需要经过 Response 对象或字符串拼接器。

gzip/deflate 解码器独立于客户端和服务器对象，支持嵌套编码、增量输出、Reset 复用和硬
输出上限。未知编码默认拒绝；显式 raw 模式允许扩展层接管完整 representation，不产生
协议死角。

旧客户端、服务器、拥有型 Header、Cookie、MIME、Multipart、认证、缓存、SSE 与 Builder
已迁至 `extlibs/xhttp/archive`。这些资产不参与核心编译、单头生成和裁剪闭包；后续 xhttp
开发应在公开协议原语上恢复需要的便利层，不能把高级对象模型反向塞回核心。

`src/http` 已由约 205 个文件收敛为 18 个实现文件，核心实现约 223 KiB。单头文件由约
10.95 MiB 降至约 7.96 MiB。该结果说明主要体积来自高级客户端/服务器与语义体系，而不是
HTTP/1.1 线协议本身。



## 11. WebSocket

| 模块 | 结论 | 高性能路径 | 发布边界 |
| --- | --- | --- | --- |
| Frame | 稳定 | 栈上最多 14 字节帧头、调用方 payload、分片 mask | 不持有网络发送节点 |
| Message | 稳定 | 借用网络缓冲、增量状态机、无完整消息聚合 | 不自动回复或管理连接 |
| Handshake | 稳定 | 借用 HTTP 字段、调用方输出、无客户端/服务器对象 | 仅依赖 HTTP 字段原语 |
| Deflate | 稳定 | 流式输入输出、容量规划、上下文复用和硬上限 | 不拥有网络压缩队列 |

发送端用 `xrtWsFrameWrite` 在栈上生成帧头，再把帧头与 payload 直接交给 TCP/TLS 向量写入；
接收端借用网络缓冲逐片执行 Frame/Message 状态机。核心不产生每帧 Header 节点，也不复制、
排队或聚合 payload。客户端 mask 与 TLS 密文属于协议强制变换，不能承诺消除。

连接、Writer、Future/TLS 适配、连接级压缩和 Group/Broadcast 已全部迁入
`extlibs/xws/archive`。这些能力的连接吞吐、慢连接和广播基准归 `xws` 扩展库维护；XRT
只冻结协议原语及其解析、写出、mask 和压缩性能。



## 12. Crypto、TLS、ASN.1 与 X.509

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| AES/AES-GCM x86 | 可用 | AES-NI、PCLMUL 运行时路径 | 需按 CPU 能力分别建基线 |
| AES/AES-GCM ARM | 不足 | 标量 Fallback | 缺 ARMv8 AES/PMULL/NEON |
| Hash/ChaCha/Curve/RSA | 有条件可用 | 流式、调用方输出、协议可组合 | 缺跨平台密码套件吞吐矩阵 |
| ASN.1/PEM/X.509 | 有条件可用 | 独立解析层和限制 | Trust Store、长证书链性能证据不足 |
| TLS Record | 有条件可用 | xnetbuf、Feed Ref/Take、Ciphertext SendBuffer | 大记录和并发流吞吐未进入门禁 |
| TLS Handshake | 不足 | TLS 1.3、恢复、票据均已完整 | 临时分配多，完整握手成本高 |

TLS 数据链路没有明显的多余复制：明文进入 TLS 后必须生成密文，密文使用网络 Buffer 并
通过 `SendBuffer` 转给 TCP。需要优化的是记录批处理、Scratch 复用和硬件路径，而不是
承诺不可能实现的“明文零拷贝加密”。

当前 RSA 证书完整握手约 141 次/秒，票据恢复约 936 次/秒。握手代码存在多个短生命周期
对象，应引入 Session/Worker 级 Handshake Arena，复用转录 Hash、证书消息和临时密钥空间；
不可变证书链可预编码，Trust Store 可建立只读索引。任何缓存都必须保持证书、SNI、ALPN、
票据密钥和验证策略隔离。

ARMv8 AES/PMULL 是必要的跨平台性能补齐，不能把 x86 结果外推到服务器 ARM。发布矩阵
还应比较 AES-128-GCM 与 ChaCha20-Poly1305，在无 AES 加速 CPU 上允许 ChaCha 成为优先路径。



## 13. XID 与 Template

| 模块 | 结论 | 高性能路径 | 主要缺口 |
| --- | --- | --- | --- |
| XID | 可用 | 值类型、调用方文本缓冲、批量安全随机 | 需要进入统一发布 Profile |
| Template | 有条件可用 | Compile Once、不可变 Program、Writer 流式渲染 | 缺正式模板类型矩阵和分配统计 |

XID 是低成本、独立且适合 xlang 的基础设施。历史批量生成基准已经达到很高吞吐，API 也
不强制分配；它不构成当前风险。

Template 的架构方向正确：编译与渲染分离，编译结果不可变并可并发复用，输出走 Writer，
无需先形成完整字符串。应测量 Literal、路径查找、循环、条件、Include 和扩展函数，分别
记录编译成本、渲染 bytes/s、分配次数和最大 Frame 深度。热模板可以预解析路径段并复用
渲染栈，但不能把全局 Cache 强制塞进基础模块。



## 14. 问题优先级

### P0：冻结 API 和宣称高性能前必须完成

1. 修复单头实现裁剪，增加未选符号和编译时间门禁。
2. 固定 HTTP/1 解析、正文边界、直接写入和内容解码的规模与延迟基线。
3. 建立干净提交的 Windows IOCP、Linux epoll/io_uring、macOS kqueue 性能与内存基线。
4. 把仍处于 developing 的通用网络/TLS io_uring 测试转为真实 Linux 证据，或者明确
   降级该后端的发布等级。
5. 迁移协程核心基准到当前 API，并补 ARM64 运行证据；未验证架构不能标为同等 Tier。
6. 为 Runtime Model 建立性能边界，决定冻结读快照、分片或继续保持 review。

### P1：形成完整高负载底座

1. 增加独立可裁剪的工作窃取 Executor、Batch Submit 和 Detached Task。
2. 增加原生异步文件端口与文件到 Socket 传输 API。
3. 增加 io_uring multishot、buffer ring 和注册资源的能力探测模式。
4. 在 `xws` 扩展库评估连接级 Header 节点池、压缩缓冲复用和共享广播 Frame。
5. TLS Handshake Arena、证书消息预编码和 ARMv8 Crypto。
6. 异步 Logger 增加无生产者互斥的可选 Ring/Lane 路径。
7. 为统计、Trace 和调试计数建立 `OFF/BASIC/FULL` 成本等级。

### P2：由基准决定是否实施

1. JSON SIMD 结构扫描器。
2. Base64、Hash 和 Unicode 的更多 SIMD 后端。
3. 可移动 Packed Map。
4. 压缩库可选后端。
5. 大规模 Timer Wheel。
6. Linux 通用 `MSG_ZEROCOPY`/`SEND_ZC`。
7. Regex JIT。

P2 项目不能只凭“理论更快”进入 XRT。它们会增加代码量、平台分支和测试矩阵，必须先有
真实工作负载、可重复瓶颈和明确收益阈值。



## 15. 发布性能契约

### 15.1 每个核心能力必须公开三种所有权

- Copy：最安全、最容易使用，允许一次明确复制。
- Ref/Borrow：调用方或引用对象保证生命周期，热路径不复制 Payload。
- Take/Owned Buffer：把所有权交给 XRT，完成或取消后精确释放一次。

TCP、UDP、HTTP Body、WebSocket、TLS Feed、文件输出和 Codec 应使用一致术语与完成语义。

### 15.2 高性能路径目标

| 路径 | 发布目标 |
| --- | --- |
| TCP SendRef | 应用 Payload 进入队列零复制；有硬队列上限；完成时释放一次 |
| UDP Batch | Linux 一批尽量一个 syscall；Windows 保持多请求在途；无隐式无界队列 |
| HTTP/1 解析与写出 | 借用输入、调用方缓冲、正文独立所有权；目标零通用堆分配 |
| HTTP 固定报文 | 预构造 wire bytes 直接进入 TCP/TLS，不经过请求或响应对象 |
| WebSocket Frame | 调用方 payload，栈上 Frame Header，不使用通用堆分配 |
| Detached Task | 预热后提交不创建 Future，不经过全局共享锁热路径 |
| Async Logger Ring | 预热后 Producer 不分配、不互斥等待，批量消费 |
| 空闲 TCP/TLS 连接 | 不预留固定 8K；常驻内存有实测上限 |

### 15.3 背压和失败也是性能契约

任何高性能队列都必须有硬限制。达到限制后只允许：立即返回 AGAIN、等待 Writable/Drain、
按公开策略丢弃，或由调用方扩容。不得通过隐藏的无界链表把延迟问题变成内存问题。

取消、超时、对端关闭和提交失败后，Ref/Take 的释放次数必须可证明，统计中的 Pending
Bytes、Buffer、Future、Timer 和 Engine Object 必须回到零。



## 16. 基准与门禁矩阵

### 16.1 分层执行

- 每次改动：模块边界、正确性、裁剪宏和 1 至 3 秒 Micro Smoke。
- 相关体系改动：该体系 5 样本基线、泄漏和尾延迟门禁。
- 每日或主分支：Foundation、Data、Concurrency、I/O、Network、HTTP/WS/TLS Profile。
- 发布候选：全平台、全编译器、长稳态、故障注入、体积和编译时间。

### 16.2 必须新增的 Profile

| Profile | 关键工作负载 |
| --- | --- |
| Allocator | 16 B 至 1 MiB、冷热缓存、跨线程释放、碎片、RSS |
| Text/Codec/Hash | 16 B 至 16 MiB、ASCII/UTF-8、合法/非法、Streaming |
| Containers | Reserve/No Reserve、随机/顺序、命中/未命中、删除再插入 |
| JSON/XSON | 64 B 至 10 MiB、转义、数字、Valid/Visit/DOM/Write |
| Runtime | 注册、冻结查找、字段、动态调用、类型容器、环回收 |
| Task/Coroutine | 0/100/1000 ns 任务、多 Worker、Steal、阻塞隔离、Timer |
| File/I/O | 顺序/随机、4 KiB 至 1 GiB、Map、原生 Async、取消 |
| Logger | 多生产者、长短消息、多 Sink、Drop、Flush p99 |
| TCP | 1/100/1K/10K 连接、短包/大流、慢读写、半关闭、RST |
| UDP | 64/256/1400 B、10 万/100 万 Burst、Batch、Drop Policy |
| DNS | Cache Hit/Miss、同名合并、多域名、超时、缓存竞争 |
| HTTP | Parse/Write、0/64 B/4 KiB/1 MiB、分块、字段规模、gzip/deflate |
| HTTP Decode | Identity/gzip/deflate/嵌套编码、小分块、输出上限、Reset |
| WebSocket | 7/16/64 位长度、Fragment、控制帧、mask、deflate 和畸形输入 |
| TLS | 完整/恢复、并发握手、长证书链、16 KiB Record、Cipher Matrix |
| Template/XID | 编译/渲染分离、复杂控制流、批量生成、分配次数 |

### 16.3 平台矩阵

- Windows x64/ARM64：MSVC、GCC 或 Clang；IOCP。
- Linux x64/ARM64：GCC、Clang；epoll 与 io_uring 分开建立基线。
- macOS x64/ARM64：Clang；kqueue。
- Select：至少一个真实 Fallback 平台或强制后端测试，作为 Tier C 正确性门禁。

不同后端的结果不能混在一个基线中。CPU、逻辑核、亲和、电源策略、编译参数、
Feature Profile 和后端必须成为报告身份的一部分。



## 17. 建议实施顺序

### 阶段 A：先消除结构性阻断

修复单头裁剪；补编译时间和负向符号测试；收敛 HTTP/1 协议核心并固定直接收发与
内容解码契约；在干净提交上重建当前 Windows 基线。

### 阶段 B：压实跨平台网络与协议

建立 Linux/macOS Runner；完成 io_uring 证据；修复单头 Linux UDP 批量能力；补 HTTP、
WebSocket 协议分块/压缩矩阵和 TLS 大消息、高连接数矩阵。

### 阶段 C：补齐系统级高吞吐路径

实现原生文件异步、文件到 Socket、工作窃取 Executor 和 TLS ARM 硬件加速。HTTP 连接池
与 WebSocket 连接缓冲复用分别属于 xhttp、xws 扩展层，不进入 XRT 核心。

### 阶段 D：按证据优化通用模块

建立 Foundation/Data/Runtime/Logger/Template 的完整 Profile，再决定 SIMD JSON、Packed
Map、压缩后端、Timer Wheel 等 P2 项目。没有证据的优化不进入实现。



## 18. 最终判断

XRT 当前不是“底层性能不可救”的状态。相反，视图、流式、Ref/Take、批量、背压、
Worker 本地内存和后端抽象这些最重要的方向基本正确，TCP 与服务端协议尤其扎实。

当前需要避免两个极端：一是只保留便利对象，把所有请求都变成分配和拼接；二是为了
峰值数字引入大量平台特化和第三方实现，让 XRT 失去小巧、清楚、可裁剪的价值。正确目标
是让高性能路径完整公开，让便利路径建立在同一底座上，并用分配、复制、锁、系统调用、
尾延迟、内存和体积共同守门。

完成 P0 后，可以认为核心高性能契约具备冻结条件；完成 P1 和跨平台发布矩阵后，才适合
把 XRT 网络、HTTP/1.1、WebSocket、协程和任务体系定义为未来长期稳定的标准库底座。
