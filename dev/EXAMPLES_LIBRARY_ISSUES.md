# 范例工程问题复核与修复台账

## 2026-09-06 复核结论与已完成修正

复核基线：`c2063aea` 及当前工作区。以下结论来自当前公开头文件、实现、
已有测试和独立运行核对；**原报告的 10 项不能直接当作 10 个已确认库缺陷**。
原始发现记录保留在本文后半部，作为历史材料，不再代表当前处理状态。

### 核心库清单逐项结案

| 编号 | 当前结论 | 处理与证据 |
| --- | --- | --- |
| A-1 | 撤回 ABI 不一致判断 | `file_async.h` 已明确声明 `DirStatsAsync` 发布 `xwalkstats`，实现及目录树测试一致；范例删去 `xdirquery` 强转和错误注释，直接校验 Files/Bytes。 |
| A-2 | 当前实现正常 | Set 后 Get 返回新值 11；槽位表范例补上此断言。 |
| A-3 | 当前实现正常 | Remove 回传最近一次 Set 后的值 11；范例恢复出参断言，保留陈旧句柄检查。 |
| A-4 | 未复现，不据此改库 | 默认 TCP 写高水位 256 KiB，原范例的 64 字节不能证明触发背压；`Pending` 包含传输待发量，`Paused` 只表示应用暂停。改正范例后，默认及 32 字节高水位均无需补发 Resume 即能回显；显式 Pause/Resume 单独验证通过。 |
| A-5 | 未复现，不据此改库 | 双端默认 deflate 参数可正常收发。原压缩小节仅 Attach 客户端、从主线程调用同步发送、忽略返回值，并重复释放已被接管的 TCP 引用；现改为所属 Worker 上发送，双端真实解压回显并逐字节核对。 |
| A-6 | 当前实现正常 | 成功 Seek 会清除 EOF；范例改为 ReadAll 后复用同一个 Reader，验证 CopyN=5、CopyLimit=10、超限时已复制=4。 |
| B-1 | 撤回注释不符判断 | `websocket.h` 对 DeflaterSize 已写明线路字节数；语义字节数属于 InflaterSize。仅更正范例注释。 |
| B-2 | **确认，已修复文档并补测试** | 空控制/零 Flags 的普通发送退化路径已存在，UDP 上层兼容相应终态；保留行为，补全 SendMsg/SendMsgVec 注释、网络 API 文档及单头快照中的相同注释。 |
| C-1 | 调用方未满足初始化前提 | SetInit 缺少 delta 会失败，不能对失败后仍未初始化的输出调用 Check。零 Set 的 Check 返回结构化错误；范例明确拆开两条负路径，不把“清零”当成有效构造。任意野指针无法由通用 C API 安全判活，不承诺将任意未初始化对象转成结构化错误。 |
| C-2 | 撤回“没有 Init”判断 | 已有 `xrtHttp1MessageInit` 统一初始化 Head/Trailers 槽；范例改用该接口，解析通过。 |

### B-2 修正后的终态契约

下表同时适用于标量 `xrtNetPortSendMsg` 和向量 `xrtNetPortSendMsgVec`，
前提是提交受理成功并最终提取其完成事件。

| 控制参数 | 目标地址 | 终态事件类型 |
| --- | --- | --- |
| 非空且 Flags 非零 | 有地址，或无地址但 Socket 已连接 | `SEND_MSG` |
| NULL 或 Flags 为零 | 有 pRemote | `SEND_TO` |
| NULL 或 Flags 为零 | 无 pRemote，Socket 已连接 | `SEND` |

修正位置：[公开头文件](../include/xrt/net.h)、[网络 API 文档](../docs/api/net.md)、
[port_tour](../examples/network/port_tour/main.c)。
新增 [共享终态矩阵](../tests/network/test_net_port_send_msg_cases.h)，由 IOCP 和
io_uring 测试入口调用，覆盖 2 种发送形态 × 2 种目标地址 × 3 种控制参数，
逐项校验结果、事件类型、字节数、负载、Id/User 和显式目标地址。

核心单头文件重新生成；5 个扩展的单头/声明快照仅同步相同注释。
扩展快照本来还落后于其他核心源码变更，本次没有顺带升级这些无关运行代码。

### 已修复的真实范例问题

- [核心 WebSocket Stream](../examples/websocket/stream_tour/main.c)：同步操作放到所属 Worker，所有返回值显式检查；跨线程信号改为原子发布；接管后不重复释放 TCP；去掉“背压后额外 Resume”的绕行；压缩用双端连接核验；等待异步关闭并检查 Engine 收尾。
- [xws Connection](../extlibs/xws/examples/websocket/connection_tour/main.c)：修复作用域过短的 Post 用法，任务上下文留到 Worker 退出；修复 `OK=0` 时按位 `&=` 吞掉错误；客户端绑定实际事件表与上下文，检查回显/Pong/Close，不能只打印成功；从 Call 的实际请求提取握手 Key；补齐 Response、TcpRef、Connection、Call、Future 和 Ref 负载释放；显式停止 HTTP Server 后再停止 Engine。
- [IO Stream](../examples/io/stream_tour/main.c)：除 EOF 绕行外，还修复 Writer 错用 `examplemem` 的 Close 回调。Writer 实际上下文是 `examplesink`，原回调按另一种结构布局写字段，在 64 位构建中越过该对象末尾。改用匹配的回调并校验关闭计数，同时保留并释放 TakeFile 返回的 Reader。
- [异步文件](../examples/file/async_tour/main.c)：保留最新 Future 以维持借用值有效期，替换时释放旧 Future；Take/Close 在受理后立即转移所有权，等待失败不再二次释放；释放回调使用原子计数并实际断言；纠正 size=22 的输出。
- [SlotMap](../examples/containers/slot_map_tour/main.c)、[HTTP1](../examples/http1/head_tour/main.c)、[CRL](../examples/x509/crl_tour/main.c)、[压缩扩展](../examples/websocket/extension_tour/main.c)：修正断言、初始化方式、夹具包含路径和错误语义说明。

### xws 原附注的处理

原附注 1–3 的 Pause/Paused/回调内关闭现象未复现；新版范例分别验证主线程
Pause 和消息回调内 Pause，暂停期间后续消息不交付，Resume 后继续接收。
原附注 4 的“回显后无事件关闭”同样未复现：当前实际核对 17 条消息、17 次回显、
零错误，以及主动 Close 后双方完整的干净关闭事件。没有证据据此修改
`xrtNetStreamPause` 或认定它是共享根因。

### 验证记录

- Windows/GCC：相关 8 个核心范例及 xws 范例编译、运行退出码均为 0；范例翻译单元使用 `-Wall -Wextra -Werror`。
- 23 个回归可执行程序通过：IOCP 终态测试 1 个；SlotMap/IO/目录树/HTTP1/Deflater 模块化与单头测试 18 个；WebSocket Stream 模块化测试 4 个。
- 核心 Stream：Select/IOCP × 1/2 Worker × 默认/32 字节写高水位，默认 deflate 双端配置共 8 组；另测关闭 deflate 的基础/ref 两种裁剪形态 × 两后端，共 4 组。
- xws：Select/IOCP × 1/2 Worker 共 4 组，覆盖真实 HTTP Upgrade、同步/异步发送、两种暂停位置和干净关闭。
- 核心 `amalgamate.py --check` 与 API 参考生成检查；Linux/io_uring 分支已接入共享测试，但本机未进行 Linux 运行验证。

没有修改 TLS 协议、证书验证规则、SlotMap、Reader 或 WebSocket 的库侧运行逻辑；
本次确认的库问题是 B-2 的契约文档缺失，其余运行代码修改均位于范例。

---

## 原始发现记录（历史待核验陈述，以前述复核结论为准）

> 来源：补齐全部核心库 API 范例的过程（内核最终 2958/2958 = 100% 覆盖）。
> 每条问题都由"编写范例 → 编译运行 → 断言失败 → 独立探针复现"的流程抓出，
> 并附发现时的范例位置。台账原件见 `dev/EXAMPLES_PROGRESS.md` 各批次条目。
> 日期：2026-09-06 整理。

分类说明：

- **A. 疑似行为缺陷** —— 实测行为与头文件声明的契约不符，或行为明显反常，需库侧确认并修复。
- **B. 文档/注释与实测不符** —— 行为本身可能是设计如此，但头文件注释与实测矛盾，需改文档或改行为二选一。
- **C. 健壮性建议** —— 调用方误用会得到段错误而非结构化错误，属于防御性不足，非功能错误。

注意区分：范例过程中记录的大量"语义澄清"（如 `CrlStatus` 未吊销返回
`VALUE+GOOD`、`PeerAlert` 在干净关闭时返回真、Windows 无数据报错误队列等）
是**正确但不易推断**的行为，不属于问题，不列本清单——它们已记录在各批次
台账与范例注释里。

---

## A. 疑似行为缺陷（6 项）

### A-1 `xrtDirStatsAsync` 出参 ABI 与头文件不符 【严重度：高——ABI 级】

- 模块：file_async
- 声明：`include/xrt/file_async.h`（标注出参类型为 `xdirquery`，即只含
  `Empty` 一个字段的结构）
- 实测：实现实际发布的计数器是 `xwalkstats`（含 Items/Files/Directories/
  Bytes 等字段）。按头文件声明的 `xdirquery` 读出的是 `Items` 首字节的
  垃圾值。
- 影响：任何按头文件写的调用方都会读到错误数据——这是 ABI 级不一致，
  无法用"行为澄清"解释。
- 发现：`examples/file/async_tour`（范例最终按 walkstats 判读并注明）。

### A-2 `xrtSlotMapSet` 受理成功但值不更新 【严重度：中】

- 模块：slot_map
- 实测：`xrtSlotMapSet(pMap, Slot, (ptr)11)` 返回成功，随后
  `xrtSlotMapGet(pMap, Slot)` 仍返回插入时的原值 `(ptr)1`。
- 期望：Set 语义应替换槽值；若设计为"只作占位确认"则应改名或文档说明。
- 发现：`examples/containers/slot_map_tour`（当时按实测行为记录，范例
  断言与直觉相反）。

### A-3 `xrtSlotMapRemove` 出参写 NULL 不回传值 【严重度：低】

- 模块：slot_map
- 实测：Remove 的出参（用于取回被删值）在成功删除时写入的是 NULL，
  不回传槽内原值。
- 期望：出参应回传删除前的值；否则该出参没有意义。
- 发现：`examples/containers/slot_map_tour`。

### A-4 WebSocket 流层背压读取门不自动恢复，且 `Paused()` 不反映 【严重度：中】

- 模块：websocket_stream
- 实测：突发发送经 TCP 高水位折叠触发背压后，即使发送队列已排空，
  读取门不自动恢复，需要 settle 后显式 `Resume`；同时 `Paused()` 在
  此期间不反映真实暂停状态。
- 影响：对端表现为"发送成功但对端收不到"，且无任何可查询的状态线索。
- 发现：`examples/websocket/stream_tour`。
- 关联：xws 扩展的 `xrtWsConnPause/Paused` 问题（见扩展库附注）疑似
  同一根因在上一层的表现。

### A-5 `DeflateEnabled` + 默认参数直连时客户端接收完全停滞 【严重度：中】

- 模块：websocket_stream
- 实测：启用 permessage-deflate 且使用默认压缩参数、双端直连回环时，
  客户端侧接收完全停滞（服务端发送正常受理）。
- 发现：`examples/websocket/stream_tour`。

### A-6 流式 `ReadAll` 后 EOF 标志粘滞，seek 不清除 【严重度：低】

- 模块：io（stream Reader）
- 实测：`ReadAll` 触发 EOF 后，对同一 Reader 执行 seek 回起始位置再读，
  仍得到 EOF（静默零拷贝），必须新建 Reader。
- 影响：复用 Reader 的常规写法会静默读到空，无错误提示。
- 发现：`examples/io/stream_tour`。

---

## B. 文档/注释与实测不符（2 项）

### B-1 `xrtWsDeflaterSize` 语义表述

- 模块：websocket（压缩变换层）
- 头文件注释：返回"语义字节数"。
- 实测：返回压缩后的线路字节数。
- 发现：`examples/websocket/extension_tour`。

### B-2 `xrtNetPortSendMsgVec` 终态事件类型注释

- 模块：net_port
- 头文件注释：终态事件类型为 `SEND_MSG`。
- 实测：零标志控制（`xnetdgramcontrol` 全零）提交时，终态事件类型为
  `SEND_TO`。
- 发现：`examples/network/port_tour`。

---

## C. 健壮性建议（2 项）

### C-1 `xrtX509CrlSetCheck` 对未清零的 Set 解引用野指针

- 模块：x509_crl_policy
- 实测：栈上未初始化的 `xx509crlset` 直接传给 `CrlSetCheck` 会段错误
  （结构含指针槽，API 未校验即解引用）。清零后同调用返回结构化错误。
- 建议：入口校验指针域，返回参数错误而非崩溃。
- 发现：`examples/x509/crl_tour`（当时以段错误定位，后按清零复现）。

### C-2 `xhttp1message` 未清零的 Head/Trailers 槽野指针

- 模块：http1_message
- 实测：栈上未 memset 的 `xhttp1message` 直接传给
  `RequestMessageParse` 会段错误；清零后容量不足能正确返回
  `XHTTP1_FIELDS`。
- 说明：该结构没有 Init 函数，指针槽（Head.Fields/Trailers）只能靠
  调用方 memset——与 C-1 同类。
- 发现：`examples/http1/head_tour`。

---

## 附注：xws 扩展库的 4 项疑似问题（不在核心库范围）

为完整起见列出（详见 `dev/EXAMPLES_PROGRESS.md` 2026-09-06 扩展库第一刀
条目），全部位于 `extlibs/xws` 的 websocket_runtime 连接层：

1. `xrtWsConnPause` 主线程调用不生效；
2. 消息回调内 Pause 导致连接静默转 CLOSED（无 Error/Close 事件）；
3. `Paused()` 快照在暂停确实生效时仍返回假；
4. 服务端连接在回显流量后无任何事件转 CLOSED。

其中 1–3 的机制经过核心库的 `xrtNetStreamPause`，与 A-4 可能共享根因，
定责需库侧排查。
