# 性能基准与发布规则

XRT 的性能目标是以可控内存维持稳定吞吐、延迟和尾延迟，同时保留完整的错误、
取消、背压和所有权契约。单个微基准数字不能替代边界测试，也不能证明另一个平台、
编译器或负载下的表现。

## 证据等级

性能资料分为三个等级：

1. **当前基线**：基准直接编译当前模块化源码或 `single/xrt.h`，记录源码版本、系统、
   CPU、编译器、优化参数、输入矩阵、原始样本和统计方法。
2. **开发基准**：入口使用当前 API，但尚未形成完整跨平台样本或稳定阈值，只用于定位
   热点和观察趋势。
3. **历史对比**：来自 `dev/ver1`、旧 API 或已经被替换的实现，只能说明历史问题和
   测试场景，不能作为当前发布结论。

交叉编译成功、一次 smoke、`.tmp-build` 或 `out` 中遗留的可执行文件都不是性能
证据。报告必须能够从已登记的源码和命令重新生成。

## 当前 XRT 2 基准

以下入口已经使用当前 API，并拥有当前实现的基准报告：

| 体系 | 基准入口 | 当前报告 |
|---|---|---|
| Fixed Stack | `dev/bench/fixed_stack/bench_fixed_stack.c` | `dev/bench/fixed_stack/FIXED_STACK_BENCH_20260728.md` |
| Block Stack | `dev/bench/block_stack/bench_block_stack.c` | `dev/bench/block_stack/BLOCK_STACK_BENCH_20260728.md` |
| Map | `dev/bench/map/bench_map.c` | `dev/bench/map/MAP_BENCH_20260728.md` |
| Set | `dev/bench/set/bench_set.c` | `dev/bench/set/SET_BENCH_20260728.md` |
| Queue | `dev/bench/queue/bench_queue_pointer.c` | `dev/bench/QUEUE_BENCH_20260728_XRT2.md` |
| Value | `dev/bench/value/bench_value.c` | `dev/bench/value/VALUE_BENCH_20260729.md` |
| Value Container | `dev/bench/value_container/bench_value_container.c` | `dev/bench/value_container/VALUE_CONTAINER_BENCH_20260729.md` |
| Value Collection | `dev/bench/value_collection/bench_value_collection.c` | `dev/bench/value_collection/VALUE_COLLECTION_BENCH_20260729.md` |
| Value Graph | `dev/bench/value_graph/bench_value_graph.c` | `dev/bench/value_graph/VALUE_GRAPH_BENCH_20260729.md` |
| JSON | `dev/bench/json/bench_json.c` | `dev/bench/json/JSON_BENCH_20260731.md` |
| XSON | `dev/bench/xson/bench_xson.c` | `dev/bench/xson/XSON_BENCH_20260731.md` |
| Regex | `dev/bench/regex/bench_regex.c` | `dev/bench/regex/REGEX_BENCH_20260731.md` |
| XID | `dev/bench/xid/bench_xid.c` | `dev/bench/xid/XID_BENCH_20260801.md` |
| Task Pool | `dev/bench/task/bench_task_pool.c` | `dev/bench/task/TASK_POOL_BENCH_20260808.md` |
| Executor | `dev/bench/task/bench_executor.c` | 自动 runner 开发基准 |
| Coroutine | `dev/bench/coroutine/*.c` | 自动 runner 开发基准 |
| Async File | `dev/bench/file/bench_file_async.c` | 自动 runner 开发基准 |
| Network loopback | `dev/bench/network/bench_network_loopback.c` | `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md` |
| HTTP/1.1 core | `dev/bench/http/bench_http_core.c` | `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md` |
| WebSocket core | `dev/bench/websocket/bench_websocket_core.c` | `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md` |
| TLS 1.3 loopback | `dev/bench/tls/bench_tls_loopback.c` | `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md` |
| Logger Ring | `dev/bench/logging/bench_logger_ring.c` | 自动 runner 开发基准 |
| AES-GCM | `dev/bench/crypto/bench_aes_gcm.c` | 自动 runner 开发基准 |
| Crypto Primitives | `dev/bench/crypto/bench_crypto_primitives.c` | 自动 runner 开发基准 |
| X.509 | `dev/bench/x509/bench_x509.c` | 自动 runner 开发基准 |

Channel 与协程入口已经迁移到当前 API 和统一 runner；线程、Stack 和部分队列延迟
基准仍需重新核对构建命令、样本来源和版本后才能提升为当前发布基线。报告名称带日期
不自动获得当前证据等级。

## 自动性能门禁

当前自动门禁由 `config/performance_profiles.json` 和 `tools/measure_performance.py` 驱动，
覆盖任务、协程、Channel、队列、运行时、模板、数据、标识、文件、网络、Web 协议、
TLS、日志、密码和 X.509 等当前 API 组合。Windows GCC 16 x64 的机器
基线是 `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json`，评审摘要
是 `dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.md`。已签入的上一份
JSON 保存 66 个受控指标的全部原始样本、中位数、MAD、完整范围、中央范围、方向和
阈值。当前 HTTP 与 WebSocket profile 只测协议核心；旧客户端/服务器回环基准已经随
高级层归档，不能继续作为 XRT 核心比较对象。

```text
python tools/measure_performance.py --compiler gcc --arch x64 --smoke
python tools/measure_performance.py --compiler E:/software/w64devkit/bin/gcc.exe --arch x64 --baseline dev/bench/performance/PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
```

正式报告默认先预热一次，再串行运行五个独立进程样本。完整范围保留所有异常值供审计；
质量门禁使用去掉一个最高值和一个最低值后的中央范围。MAD 超过 20% 或中央范围超过
30% 会拒绝报告：一次外部抢占不会污染中位数，至少两个慢/快样本形成的双峰仍会失败。
尾延迟等指标可以登记专用中央范围，该值属于指标契约，改变后旧基线不能继续比较。吞吐默认下降 10% 失败，TCP/UDP 回环按
实测噪声使用 15%；TCP p50 和 p99 分别限制上升 20% 和 25%。

SPSC queue 在当前 Windows 未固定线程拓扑时呈双峰分布，暂时只保留原始输出，不进入
自动阈值。不得为了得到绿色结果放宽全局阈值或覆盖基线；应先建立拓扑感知 runner。

`file_io` 测量任务池型便利异步文件，包括 Future、拥有型读取结果和定位 I/O 成本；
它不等价于 `xrtNetFileRead/Write` 的 IOCP/io_uring 原生完成式路径。后者必须按网络
后端单独建立基准。文件写吞吐计时不包含 flush，因此只用于同机运行时回归，不能解释
为磁盘耐久性吞吐。

## 历史网络基准边界

`dev/bench/XNET_COMPARE_20260314.md`、`TLS_BENCH_20260315.md`、
`COROUTINE_BENCH_20260314.md` 及相邻 sweep 是旧主线的重要历史资产，保存了 TCP
echo、TLS echo、UDP burst、发送队列压力、握手和协程切换等负载设计。

这些报告和原 runner 仍引用旧根 `xrt.c`、旧 `xnetstream`/`xnet-v2` 或已经退役的
运行时模型，不能证明当前 `xrtNetTcp*`、`xrtTls*`、HTTP 或 WebSocket 实现的性能。
其中的绝对数字不得出现在当前发布说明中。有效场景应迁移到当前 API 后重新采样；
旧实现只作为同机历史对照，不参与当前阈值。

当前 TCP 同步 ping-pong、UDP 批量回环、TCP Ref 大流、HTTP/1 协议核心、WebSocket
帧核心、TLS 完整握手、票据恢复和加密流已经迁移到新 API。完整网络发布矩阵仍需要
继续覆盖：

- TCP client/server：连接建立、echo、单向大流、慢读端、慢写端、半关闭和大量空闲连接。
- UDP client/server：小包 burst、批量收发、队列溢出和不同 receive concurrency。
- TLS：大记录吞吐、长证书链、不同密码套件、并发握手和慢端。
- HTTP/1.1 核心：字段规模、任意分块、chunked、压缩层数和输出上限。
- WebSocket 核心：大帧、分片、压缩、mask 和慢连接背压。
- xhttp/xws 扩展层：连接池、重试、应用级客户端/服务器、广播和长连接心跳；不混入
  XRT 核心体积与性能结论。
- Select、IOCP、epoll、kqueue、io_uring 后端分别报告，不能把一个后端外推到其他后端。

## 测量规则

发布级基准必须满足以下条件：

- 使用优化构建，同时保留正常发布的安全检查和特性宏。
- 在同一主机上串行运行基线与候选，记录系统、CPU、逻辑核、内存、编译器版本、
  电源策略、CPU 绑定、后端和源码提交。
- 预热不计入样本；至少取得三次独立样本并报告中位数。容易受调度影响的并发基准
  应增加样本数并保留原始结果。
- 吞吐与延迟分开计时；延迟至少给出 p50、p95、p99 和最大值，不能用平均值掩盖
  尾延迟。
- 同时记录峰值已分配字节、稳定态常驻内存、操作结束后的未释放对象和队列高水位。
- setup、连接、握手与 steady-state 分开报告；只有明确标为 end-to-end 的场景才能
  合并。
- 基准必须验证处理数量、结果、错误计数和资源归零。校验失败的快速样本是测试失败，
  不是性能数据。

CPU 固定可以作为诊断路径，但不能与未固定样本混入同一统计集合。localhost 数据只
证明本机协议栈和运行时开销，真实网络结果应另建场景。

## 回归判断

不同机器之间不设置统一绝对吞吐承诺。发布比较以同机、同编译器、同参数的最近有效
基线为准，并同时检查吞吐、p99、峰值内存和最终资源计数。任何超过已登记阈值的回归
必须定位原因、修复或在报告中说明经过评审的功能代价，不能通过更新基线隐藏。

若尚未为某条基准建立稳定噪声区间，默认把吞吐中位数下降超过 10%、p99 或峰值内存
上升超过 20% 视为需要调查，而不是自动宣布失败或通过。正式阈值应根据同一 runner
的多轮方差收紧，并和 runner 一起版本化。

## 体积门禁

体积门禁由 `config/size_profiles.json` 和 `tools/measure_size.py` 驱动。目前覆盖
`core`、`console`、`foundation`、`concurrency`、`runtime_core`、`runtime_full`、
`data`、`system_io`、`security_transport`、`identifier`、`template`、`network`、
`http_websocket` 和 `all`。每组分别测量：

- 单头实现对象的 `text`、`data`、`bss` 和对象文件大小。
- 静态归档全部成员的节区总量和归档文件大小。
- 动态库的节区总量和动态库文件大小。

当前 Windows GNU 开发基线由机器可读的
`dev/bench/size/SIZE_BASELINE_WINDOWS_GCC16_X64.json` 和供评审使用的
`dev/bench/size/SIZE_BASELINE_WINDOWS_GCC16_X64.md` 共同记录。环境是 Windows x64、
GCC 16.1.0、GNU Binutils 2.47.20260726、`-O2`、未 strip，共包含 14 组 profile、三类
产物和 42 个测量项。JSON 是自动比较的唯一权威输入；发布候选必须从干净工作树重新
生成。Linux、macOS、MSVC 和其他固定工具链必须各自建立基线，不能与这个 Windows
GNU 基线交叉比较。

默认回归阈值是 `text` 10%、`data`/`bss` 20%、文件大小 15%。阈值用于阻止意外增长，
不表示小于阈值的增长无需解释。除此之外：

- 每个正向模块根都要能只编译自己的依赖闭包；新增模块不得增加未选择组合的符号或
  目标体积。
- 编译器与 `size` 版本、调试符号、链接器、LTO、优化和 strip 策略必须记录；任一
  口径不同都拒绝比较。
- 为功能完备必须付出的体积应记录到对应模块，不得用重复 helper、兼容别名或测试源
  混入发布物解释增长。
- 更新基线必须附带功能或实现变动原因，不得用更新基线掩盖未定位回归。

## 发布边界

性能基准是最终发布矩阵的一部分，但不进入每次模块单元回归。日常修改运行目标模块
测试、裁剪和必要的局部基准；模块稳定、性能敏感实现改变或发布候选生成时再执行完整
基准矩阵。当前已经建立 Windows IOCP 上的并发基础、TCP/UDP、HTTP/1 与 WebSocket
协议核心、TLS 握手/恢复/加密流基线。慢端、协议扩展场景、其他网络后端、ARMv8
AES/PMULL、Logger Ring 以及非 Windows GNU 工具链的正式机器基线仍属于
待完成发布门禁，完成前不能把
整个 XRT 标记为 `stable`。
