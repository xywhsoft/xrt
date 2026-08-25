# XRT 旧版与新版代码及功能对应审计

审计日期：2026-08-24

本报告回答两个问题：旧版每组实现迁移到了哪里；新版增加的代码究竟换来了哪些
功能、契约和验证。它不以总代码量代替质量判断，也不把拆文件、公共注释和测试代码
误算成运行时功能。


## 1. 统计口径

- 旧版发布实现以 `dev/ver1/lib` 为准，共 85 份 C/H 文件、134,599 行代码；目录中另有
  2 份 miniz 许可证/说明，共 37 行。
- 旧根 `xrt.h` 和 `singlehead/xrt.h` 是声明或聚合生成物，与 `lib` 重复，不计入旧实现行数。
- 新版核心实现以 `src` 为准，共 477 份 C/H 文件、247,583 行代码；目录中另有 4 份
  第三方许可证/说明，共 92 行。公共头 `include/xrt` 另有 89 份文件、38,617 行。
- 迁出核心但直接承接旧功能的 `xruntime`、`xhttp`、`xws`、`xregex` 单独统计，不把
  它们重新算进 XRT 核心。
- `xmail`、`xssh` 是恢复并扩展的历史扩展产品，不是 `dev/ver1/lib` 的直接替换项，
  只在总量说明中列出。
- 行数使用 UTF-8 容错读取后的物理行，不扣除空行、注释和平台条件分支。旧实现型头
  同时包含声明与实现，新版声明已经移到公共头，因此表中把新版公共头单列。
- 功能归属按 `dev/refactor/baseline.json` 的逐行审计、当前模块清单和实际源码目录交叉核对。

旧版 134,599 行代码中约 12,830 行同时落入多个当前体系，占 9.5%。典型交叉包括网络与
Future/Task、TLS 与 HTTP 客户端、HTTP 与 WebSocket。新版通过独立底座和显式桥接消除了
这类物理混放；这个比例本身就是旧版耦合程度的量化证据。


## 2. 总量变化

| 范围 | 旧版 | 新版 | 说明 |
|---|---:|---:|---|
| XRT 发布实现 | 134,599 | 247,583 | 核心实现增长 83.9% |
| XRT 公共头 | 旧 `xrt.h` 10,564 | 38,617 | 新版包含 4,453 行自动生成 Feature 闭包 |
| 旧能力的当前完整落点 | 134,599 | 410,039 | XRT 加 xruntime/xhttp/xws/xregex，增长 204.6% |
| 全部活动产品实现 | 134,599 | 465,826 | 再包含 xmail、xssh；两者不属于直接同比 |
| XRT 测试 | 47,199 | 182,151 | 核心测试约为旧版 3.86 倍 |
| 旧能力完整落点测试 | 47,199 | 356,785 | 加四个直接迁移扩展，约为旧版 7.56 倍 |
| 全部活动产品测试 | 47,199 | 396,937 | 再包含 xmail、xssh |

新版总代码明显增加。主要原因不是单头生成方式，而是安全传输、网络后端、严格协议、
并发契约和扩展库完整对象层都从局部实现变成了正式产品能力。实际应用不需要编译这些
总代码；正向模块选择和扩展产品边界决定最终闭包。


## 3. 功能组代码对应

旧版行数按旧文件的主要功能归类；新版实现不含公共头，公共头与当前测试另列。HTTP、
WebSocket 的新版实现分别写成“核心 + 扩展”，从而同时显示 XRT 快速路径和完整高级层。

| 功能组 | 旧实现 | 新实现 | 新公共头 | 新测试 | 主要变化 |
|---|---:|---:|---:|---:|---|
| Core 与内存 | 4,988 | 9,468 | 1,522 | 3,518 | 删除可变 `xCore`，拆出结构化错误、堆、Arena、调试与统计 |
| 容器 | 3,597 | 12,623 | 2,600 | 10,190 | 拥有式存储、独立迭代器、Move/Take、失败原子性和精确 OOM |
| 字符串、数字、Codec | 7,332 | 15,703 | 2,531 | 9,192 | View/Builder 分层、严格 Unicode、正确舍入数字和流式 Codec |
| Time | 1,650 | 3,288 | 406 | 985 | 历史时区、DST gap/fold、ISO 周、严格文本和协议时间 |
| 系统、文件、IO、进程 | 12,435 | 31,038 | 4,375 | 17,395 | 句柄化文件、原子操作、原生异步、管道、PTY 与日志 Ring |
| 并发、协程、任务 | 6,281 | 16,105 | 3,856 | 16,151 | 统一 wait/cancel/Future/Task，增加结构化任务组和工作窃取 Executor |
| Value、JSON、XSON | 10,078 | 12,004 | 1,870 | 8,074 | 值与运行时对象分离，COW 容器，JSON/XSON 共用严格流式底座 |
| Template | 5,526 | 6,558 | 441 | 2,944 | 整体替换为有界解析、编译、Writer 和明确错误模型 |
| XID | 85 | 335 | 115 | 366 | 24 字节稳定 ABI、批量、并发、溢出和严格文本 |
| xruntime | 4,793 | 21,358 | 4,139 | 19,554 | 类型、对象、调用、对象图和 typed 容器独立成扩展产品 |
| xregex | 6,257 | 8,957 | 699 | 1,909 | BBRE 内核修订，增加 matcher、replace、split、set 和完整错误位置 |
| Crypto、ASN.1、X.509、TLS | 14,255 | 57,068 | 7,853 | 38,412 | 从 TLS 私有代码扩展为完整安全传输体系 |
| 网络底座、TCP、UDP | 25,844 | 41,762 | 4,379 | 32,387 | Port/Engine/Transport 分层、动态缓冲、硬背压和完整同步异步路径 |
| HTTP 全部能力 | 22,734 | 11,469 + 117,497 | 2,418 + 19,945 | 8,259 + 130,657 | 核心收敛，完整对象和框架能力进入 xhttp |
| WebSocket 全部能力 | 5,782 | 11,730 + 14,644 | 1,798 + 2,242 | 14,963 + 22,514 | 核心补全 RFC 链路，客户端/服务器和对象层进入 xws |
| 内部与第三方支撑 | 2,962 | 18,432 | - | 共享门禁 | 公共算法内核、平台 ABI、第三方封装和跨文件私有契约集中管理 |

代码增量最大的直接迁移项依次是：HTTP 完整体系增加约 106,232 行，安全传输增加
42,813 行，WebSocket 增加 20,592 行，系统 IO 增加 18,603 行，xruntime 增加
16,565 行，网络增加 15,918 行。HTTP 的主要增长发生在可选 `xhttp`，不进入核心闭包。


## 4. Time 逐项审计

Time 是最能说明本轮重构性质的模块。旧 `lib/time.h` 共 1,650 行，其中台账判定为：

| 决策 | 行数 | 比例 |
|---|---:|---:|
| replace | 1,623 | 98.4% |
| retire | 26 | 1.6% |
| refine | 1 | 0.1% |

当前落点为：

| 文件 | 行数 | 职责 |
|---|---:|---|
| `src/system/time.c` | 1,195 | 时钟、Gregorian、构造分解、算术与范围 |
| `src/system/time_local.c` | 390 | 操作系统历史时区、DST gap/fold |
| `src/system/time_text.c` | 1,703 | 自定义格式、严格解析、RFC 3339、HTTP-date |
| `include/xrt/time.h` | 406 | 58 个公开函数及完整中文契约 |
| 六个 Time 测试 | 985 | 基础、本地时区、属性、文本、OOM 与文本属性 |

旧版约有 55 个公开 Time 函数，新版有 58 个。实现和公共契约约从 1,650 行增加到
3,694 行，但 API 数量只增加 3 个，说明增长主要来自语义压实，而不是堆积入口。

| 旧能力 | 新能力 | 处理结果 |
|---|---|---|
| `xrtTimeSerial`、`xrtDateTimeSerial`、`xrtDecodeSerial` | `xrtDate`、`xrtDateTime`、`xrtTimeMake`、`xrtTimeSplit` | 构造与分解分开，失败不再用零值表示 |
| `xrtDateAdd`、`xrtDateDiff` | `xrtTimeAdd`、`xrtTimeDiff` | 固定时长与月/季度/年日历运算统一 |
| `xrtIsSameDay/Month/Year` | `xrtTimeSameDay/Month/Year` | 保留 Helper，修复负时间和极值 |
| `xrtWeekOfYear` | `xrtISOWeek` | 替换错误的非 ISO 周算法 |
| `xrtWeekOfMonth` | `xrtWeekRange` | 退役含义不唯一的序号，返回显式范围 |
| `xrtTimeToStr`、`xrtStrToTime` 和重复解析器 | `Write`、`Format`、`Parse`、`ParseAny` | 合并状态机，删除 64 token/256 字节固定上限 |
| `xrtUTCToLocal`、`xrtLocalToUTC` | `xrtTimeLocal`、`xrtTimeFromLocal`、`xrtTimeSplitAt` | 系统时区与固定偏移分层 |
| `xrtTimezoneOffset` | `xdatetime.Offset` | 使用目标时刻的真实历史偏移 |
| `xrtTimeApprox` | `xrtTimeNear` | 容差由调用显式传入，删除可变全局状态 |
| 无专用协议层 | RFC 3339、三种 HTTP-date | 新增严格协议时间读写 |

旧版最严重的问题包括：负时间使用 `abs` 镜像、`INT64_MIN` 未定义行为、把当前 UTC
偏移套到历史日期、不能表达 DST 不存在/重复时间、解析未完整消费、共享格式缓冲和零值
兼作失败。Time 因此几乎全部替换，但保留了微秒 `xtime`、`xrtNow`、`xrtTimer`、
`xrtSleep` 和 Gregorian 400 年周期等成熟使用手感。


## 5. Core、内存与容器

旧 `base.h` 和 `memglobal.h` 把应用路径、错误字符串、分配器、线程状态、临时内存和
小块缓存集中在可变全局 `xCore`。新版 Core 不要求 `xrtInit/xrtUnit`，只保留固定宽度
类型、View、引用计数和资源限制；错误、堆、Arena、调试和统计各自独立裁剪。

内存算法保留 16 字节尺寸类、span 复用、线程缓存和同类原位 realloc，但替换了公开
函数指针、必须 attach 的线程缓存和跨分配器释放风险。`BSMM`、`FSMemPool` 和 `MemUnit`
合并为单页、固定池、变长池三层；大对象不再机械预分配 256 槽。

容器代码从 3,597 行增加到 12,623 行。主要不是增加容器名称，而是补齐稳定地址、独立
迭代器、外部存储、显式所有权、Move/Take、访问期间修改检测、重叠范围、溢出、OOM 和
多线程外部同步契约。旧容器内置 local/shared 分支及隐式锁被删除，热路径不再永久承担
同步成本。


## 6. 文本、数字与数据

旧 `string.h` 同时包含字节字符串、Unicode、HEX、Base64、数字格式和随机文本。新版把
它拆到 text、codec、number、charset 和 random，并公开无分配 View、调用方缓冲 Writer、
精确分配 Helper 三层。

Unicode 现在严格拒绝 overlong、代理区码点和超过 `U+10FFFF` 的输入；数字解析不再把
非法输入和零折叠为同一结果，`INT64_MIN` 不再取负溢出。浮点转换只复用 yyjson 中经过
审计的 Eisel-Lemire、固定栈 BigInt 和 Schubfach 数字内核，不引入 yyjson DOM。

Value 核心公开函数反而有所收敛：运行时类型、对象、动态调用和 typed 容器迁到
`xruntime`。核心只保留标量、句柄、COW 容器和对象图基础。JSON、XSON 共用字符串、数字、
事件访问、DOM 和 Writer，不再维护两套扫描与转义实现；XSON 只增加 bytes、time、int-map、
set、非有限浮点和自定义标签。


## 7. 文件、并发与任务

系统 IO 从 12,435 行增加到 31,038 行。文件体系保留 64 位定位、整文件、目录、链接、
FIFO、复制移动和遍历能力，并补齐句柄锚定、临时资源排他创建、原子写、映射、锁、异步
目录/文件操作和统一 Writer。Windows IOCP 与 Linux io_uring 提供原生异步文件路径，其他
平台使用明确的任务池 fallback；明文 TCP 另有 TransmitFile/sendfile/splice 文件发送。

旧并发组只有线程、Queue、Channel 和 Coroutine，Future/Task 的一部分实际藏在 8,710 行
的 `xnet_sync.h` 中。新版先建立与网络无关的 wait、deadline、cancel、Future/Promise 和
Task，再由协程、线程池和网络 Engine 组合。

TaskPool 保留有界 FIFO、公平和可预测语义；新的工作窃取 Executor 提供本地双端队列、
外部 Injection Queue、批量提交和 Detached Task。TaskGroup 提供结构化作用域、嵌套组、
活动硬上限和失败取消策略。协程补齐保护页、清理栈、Join、取消确认、跨线程 Post、
ASan/MSan/TSan Fiber 接口、x86 CET 以及 LoongArch64 ABI。


## 8. 网络

旧网络共 25,844 行，其中 Port 后端分别执行 recv/send、创建事件 Chain 并持有固定 8 KiB
缓冲；同步层又重复 Future、任务和隐藏 Engine。新版 41,762 行分为 Socket、Buffer、Port、
Engine、TCP、UDP、Resolver 和 Proxy，每层有独立公共合同。

关键变化包括：

- 空闲 TCP Stream 固定正文缓冲从 8 KiB 降为 0；数据块按需从 Worker 的
  512/2048/8192/32768 尺寸类池取得。
- 后端直接向 `xnetbuf` Reserve 的 Span 收取数据，不再经过固定缓冲后二次复制。
- IOCP、io_uring、epoll、kqueue、select 共用终态和所有权语义；Select 是完整 Tier C fallback。
- TCP 提供 Copy/Vec/Ref/Refs/Take/Buffer、文件区间、硬写上限、高低水位、Drain 和半关闭。
- Listener 增加并发 Accept、跨 Worker 分发和有界拉取队列；TCP Server 聚合多 Listener。
- UDP 增加连接式/未连接式、Batch、收发并发、包/字节双重上限、丢弃策略、元数据、
  错误队列、组播和广播。
- Resolver 增加有界线程、同查询合并、正负缓存、TTL/LRU 和 Happy Eyeballs Dial。
- 同步、Future、协程只改变等待方式，不复制 Stream 或 UDP 状态机。


## 9. Crypto、ASN.1、X.509 与 TLS

这是核心增长最大的体系：旧 `crypto.h + nettls.h` 为 14,255 行，新版为 57,068 行。
旧版很多 DER、证书、密钥和 TLS 代码只服务 `nettls` 或 HTTP 客户端，不能独立复用。

新版首先公开零分配严格 ASN.1 DER 和 PEM，再分别建立密码原语、X.509 语义与验证、TLS
记录/握手/会话/身份/Verifier/Stream。TLS 只支持 1.2 和 1.3；SSL 和 TLS 1.0/1.1 不进入
兼容路径。

成熟算术内核被保留，但高风险契约被替换：ChaCha20 计数器回绕、AES 全局 T 表、X25519
低阶点、随机失败不可见、ECDSA 宽松 DER、私钥位分支、RSA 忽略 CRT 和失败前写输出等均有
专项修复。新增 X.509 名称规范化、约束、CRL、路径验证、系统根、主机名验证，以及完整
TLS 客户端、服务器、恢复和流式传输测试。代码增长主要是把“TLS 能跑”提升为可独立使用的
安全基础设施。


## 10. HTTP 与 WebSocket

旧 HTTP 的 22,734 行把 URL、Header、Cookie、Form、客户端、服务器、路由、静态文件和
协议解析放在同一发布体。新版不是简单扩张为 128,966 行，而是拆成两个产品边界：

- XRT HTTP 核心 11,469 行，只保留借用式 HTTP/1 解析、正文边界、调用方写出、Upgrade、
  TCP/TLS 块链适配和流式 gzip/deflate。
- xhttp 117,497 行提供 URL、Query、Form、Multipart、Cookie、认证、缓存、客户端、服务器、
  连接池、代理、路由、中间件、静态文件、SSE 和结构化对象。

因此，只需要直接接收报文和发送预构造响应的程序，实际核心实现比旧 HTTP 全集减少约
49.5%；选择完整 xhttp 时，功能和代码量都远超旧版。xhttp 是当前最大的可选扩展，也是
后续最值得继续审视 API 密度和尺寸 profile 的产品。

旧 WebSocket 只有 5,782 行，但帧、消息、握手、网络连接和高级对象边界并不稳定。新版
核心 11,730 行完整覆盖 RFC 6455、RFC 7692、严格 Upgrade、`ws/wss` 轻量 Stream、关闭
握手、硬背压和流式 permessage-deflate；xws 14,644 行再提供客户端、服务器、Future、
Writer、连接组和路由。高级层不复制帧和压缩状态机。


## 11. 旧文件到新体系的完整分组

以下分组覆盖 `dev/ver1/lib` 的全部 79 个顶层文件；`third_party/miniz` 的 8 份资产另列，
没有未归档文件。

| 当前体系 | 旧文件 |
|---|---|
| Core/内存 | `base.h`, `bsmm.h`, `memglobal.h`, `mempool.h`, `mempool_fs.h`, `memunit.h`, 三个 memdebug macro 头 |
| 容器 | `array.h`, `array_point.h`, `avltree.h`, `avltree_base.h`, `buffer.h`, `dict.h`, `list.h`, `set.h`, `stack.h`, `stack_dyn.h` |
| 文本/数字/Codec | `charset.h`, `hash.h`, `jnum.h`, `math.h`, `string.h`, `xdeflate.h`, `xinflate.h` |
| Time | `time.h` |
| 系统 IO | `file.h`, `file_async.h`, `logger.h`, `os.h`, `path.h`, `signal.h`, `stream.h`, `subprocess.h` |
| 并发 | `channel.h`, `coroutine.h`, `queue.h`, `thread.h`；`xnet_sync.h` 中的通用异步部分也迁入此层 |
| 数据 | `json.h`, `value.h`, `xson.h` |
| Template/XID | `template.h`, `xid.h` |
| xruntime | `type.h`, `typed_container.h`, `typed_special.h`，以及 `value.h` 中的运行时对象部分 |
| xregex | `regex.h` |
| 安全传输 | `crypto.h`, `nettls.h` |
| 网络 | `network.h`, `xcodec.h`, 全部 `xnet_*` 与五个 Port 后端 |
| HTTP/xhttp | `xcodec_http1.h`, `xhttp*.h`, `xhttpd.h`, `xurl.h`, `xweb.h` |
| WebSocket/xws | `xcodec_ws.h`, `xws.h` |
| 实现支撑 | `suplib.h`, `third_party/miniz/*` |

逐行决策仍以 `dev/refactor/baseline.json` 为权威。按主要功能组看，Time 的 98.4%、安全
传输的 84.0%、数据 Value/JSON/XSON 的 78.7%、容器的 65.1%、网络的 54.5% 被替换；
网络另有 41.5% 保留主体后修订。Regex 是复用程度最高的复杂模块，约 82.3% 旧实现经过
修订继续使用，12.3% 原样保留。


## 12. 结论

本轮重构真正完成了五类工作：

1. 把旧大文件中的隐式模块拆成公开、可裁剪、可独立测试的层。
2. 保留经过验证的算法，但替换错误模型、所有权、边界、平台和并发契约。
3. 合并字符串编码、JSON/XSON、内存池、Future/Task 和协议解析中的重复实现。
4. 补齐网络、TLS、X.509、HTTP/WS 和结构化并发原来只有局部场景、没有完整底座的能力。
5. 把每个实现同时绑定到模块测试、OOM/竞争/模糊测试、单头、裁剪、文档和发布消费者。

因此，“新版代码约为旧版两到三倍”是事实，但不能简单解释为膨胀。XRT 核心本身增长
83.9%；迁移后的完整 HTTP 和 runtime 对象层被隔离为可选扩展。最值得继续控制的是 xhttp、
安全传输和内部支撑的长期代码增速；最能体现重构收益的是 Time、网络缓冲、统一异步契约
和 HTTP/WS 快速路径，它们的 API 数量变化不大，但语义、边界和可验证性发生了根本变化。
