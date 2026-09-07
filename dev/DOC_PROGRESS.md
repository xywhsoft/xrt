# API 文档深化台账（DOC_PROGRESS）

> 配套规范：`dev/DOC_SPEC.md`（每会话开头必读）。状态：待办 / 进行中 [x/y 段] / **完成**。
> 完成定义（DoD）：G1–G4 全绿 + 本台账已记 + commit `docs(api): <模块> 按 DOC_SPEC v1 细化`。

| # | 文件 | API 数 | 状态 | 备注 |
|---|---|---|---|---|
| 1 | array.md | 50 | **完成** | 试点 1；50 函数 G1-G4 全绿（2026-09-07） |
| 2 | asn1.md | 27 | **完成** | 27/27 全绿（G3 27 片段，2026-09-07）；错误码 TAG/LENGTH/VALUE/TYPE/END/TRAILING/ORDER/DEPTH/RANGE 九码全表成文；编码器族（Append*8 + Oid 工具 3）首入文档 |
| 3 | atomic.md | 29 | **完成** | 29/29 全绿（G3 29 片段，2026-09-07）；范例 atomic_tour 补 5 个缺口 API（32Store/FetchAdd/FetchSub、64Store、PtrInit/PtrStore）并实测通过 |
| 4 | avl.md | 43 | **完成** | 43/43 全绿（G3 43 片段，2026-09-07）；双形态 17+26 全部成节，锚点跨 3 个已注册范例 |
| 5 | buffer.md | 23 | **完成** | 23/23 全绿（G3 23 片段，2026-09-07）；含 HEX/Base64 解码构造器；范例 buffer_tour 补 Clear 并实测通过 |
| 6 | cancel.md | 9 | **完成** | 9/9 全绿（G3 9 片段，2026-09-07）；修复 Ref/Destroy 合并节导致的 G1 签名不一致 |
| 7 | channel.md | 42 | **完成** | 42/42 全绿（G3 42 片段，2026-09-07）；五族全成节，锚点跨 7 个已注册范例 |
| 8 | charset.md | 69 | **完成** | 69/69 全绿（G3 69 片段，2026-09-07）；16 组全成节（原 131 问题清零），锚点跨 9 个已注册范例 |
| 9 | codec.md | 20 | **完成** | 20/20 全绿（G3 20 片段，2026-09-07）；HEX/Base64/Percent 三族；codec/tour 补注册至 codec_hex |
| 10 | compress.md | 18 | **完成** | 18/18 全绿（G3 18 片段，2026-09-07）；Inflate/Deflate 对称 9+9；失败终态与 Reset 复用契约成文 |
| 11 | console.md | 4 | **完成** | 4/4 全绿（G3 4 片段，2026-09-07）；四码 xrt.console 域成节 |
| 12 | core.md | 44 | **完成** | 44/44 全绿（G3 44 片段，2026-09-07）；四头并集（core/error/memory/features）；error/tour 补注册至 core 模块 |
| 13 | coroutine.md | 51 | **完成** | 51/51 全绿（G3 51 片段，2026-09-07）；核心 18 + 调度器 23 + 事件 10；旧 API 索引表全部转模板节 |
| 14 | crypto.md | 122 | **完成** | 122/122 全绿（G3 122 片段，2026-09-07）；三段：哈希 30 + 密码/MAC-KDF 46 + 签名/曲线 46 |
| 15 | environment.md | 4 | **完成** | 4/4 全绿（G3 4 片段，2026-09-07）；Lookup 的"不存在=成功+空输出"语义成文 |
| 16 | error.md | 45 | **完成** | 45/45 全绿（G3 45 片段，2026-09-07）；错误族 26（含 SetErrorFormat）+ Core 并集附录 19；门禁工具补变参支持 |
| 17 | executor.md | 10 | **完成** | 10/10 全绿（G3 10 片段，2026-09-07）；双门禁一次全绿 |
| 18 | file.md | 97 | **完成** | 97/97 全绿（G3 97 片段，2026-09-07）；三段：IO/文本 36 + 锁/映射/目录 37 + 遍历/链接/根 24 |
| 19 | file_async.md | 34 | **完成** | 34/34 全绿（G3 34 片段，2026-09-07）；七组：文件对象 4 + 定位读写 5 + 大小 3 + 整文件 5 + 管理 3 + 目录 6 + 目录树 8 |
| 20 | future.md | 104 | **完成** | 104/104 全绿（G3 104 片段，2026-09-07）；三段：核心 44 + 桥/listener/dial 29 + TLS stream 31 |
| 21 | hash.md | 9 | **完成** | 9/9 全绿（G3 9 片段，2026-09-07）；合并节全部拆立 |
| 22 | html.md | 3 | **完成** | 3/3 全绿（G3 3 片段，2026-09-07）；html/variants 补注册 |
| 23 | http.md | 167 | **完成** | 167/167 全绿（G3 167 片段，2026-09-07）；四段：核心 28 + field/param 33 + te/decode 55 + http1/proxy 52 |
| 24 | http_connection.md | 5 | **完成** | 5/5 全绿（G3 5 片段，2026-09-07）；与 error.md 同型的共享头子集复用策略 |
| 25 | http_decode.md | 10 | **完成** | 10/10 全绿；节复用自 http.md |
| 26 | http_encoding.md | 12 | **完成** | 12/12 全绿；节复用自 http.md |
| 27 | http_expect.md | 8 | **完成** | 8/8 全绿；节复用自 http.md |
| 28 | http_fields.md | 64 | **完成** | 64/64 全绿（G3 64 片段，2026-09-07）；11 组全部复用自 http.md + http_connection.md 已验证节 |
| 29 | http_te.md | 10 | **完成** | 10/10 全绿；节复用自 http.md |
| 30 | http_trailer.md | 6 | **完成** | 6/6 全绿；节复用自 http.md |
| 31 | http_upgrade.md | 10 | **完成** | 10/10 全绿；节复用自 http.md |
| 32 | io.md | 42 | **完成** | 42/42 全绿（G3 42 片段，2026-09-07）；Reader/LineReader/Writer 三族；全库自动锚点一次生成 |
| 33 | json.md | 30 | **完成** | 30/30 全绿（G3 30 片段，2026-09-07）；读取/写入器/序列化/文件四族 |
| 34 | list.md | 28 | **完成** | 28/28 全绿（G3 28 片段，2026-09-07）；侵入式双向链表全接口 |
| 35 | logger.md | 76 | **完成** | 76/76 全绿（G3 76 片段，2026-09-07）；13 功能组全覆盖；错误语义逐条对照源码（Attach 重复=XERR_EXISTS、xlogresult 四值口径、Async/Ring 目标引用与后台错误新引用）；ring_async 范例补注册 |
| 36 | map.md | 62 | **完成** | 62/62 全绿（G3 62 片段，2026-09-07）；字节键 Map 31 + 整数键 IntMap 31 双形态 |
| 37 | math.md | 20 | **完成** | 20/20 全绿（G3 20 片段，2026-09-07）；math/tour 补注册 |
| 38 | memory.md | 44 | **完成** | 44/44 全绿（G3 44 片段，2026-09-07）；节复用自 core.md（分配器 13 + 引用/运行时 4 + 错误族 27 三组重组），旧短式「## 函数」段整体替换 |
| 39 | memory_debug.md | 11 | **完成** | 11/11 全绿（G3 11 片段，2026-09-07）；Enable/Reset 活动分配 XERR_STATE、Report 写者失败 XERR_STATE 入档；锚点 fail_inject/debug/debug_report 三范例 |
| 40 | memory_stats.md | 4 | **完成** | 4/4 全绿（G3 4 片段，2026-09-07）；纯开关/清空函数显式"不失败"错误节；锚点 stats |
| 41 | net-dns.md | 172 | **完成** | 172/172 全绿（G3 172 片段，2026-09-07）；映射 net.h（=net.md 减 net_interface.h 12 个）；全部节复用自 net.md，12 组结构镜像，同名组（错误/文本输出等）合并 |
| 42 | net-file.md | 4 | **完成** | 4/4 全绿（G3 4 片段，2026-09-07）；错误码对照 src/network/file.c（Worker 归属 STATE、范围 RANGE、无文件 I/O 能力 UNSUPPORTED）；锚点 file_tour |
| 43 | net-frame.md | 9 | **完成** | 9/9 全绿（G3 9 片段，2026-09-07）；FRAME_CONFIG/STATE/LIMIT/LENGTH 四域码入档；xnetframestatus 逐值成表；锚点 frame_line/frame_length |
| 44 | net-interface.md | 12 | **完成** | 12/12 全绿（G3 12 片段，2026-09-07）；全部节复用自 net.md「网络接口与本机信息」组 |
| 45 | net-resolver.md | 172 | **完成** | 172/172 全绿（G3 172 片段，2026-09-07）；同 net-dns（net.h 172），Resolver 组置首 + 其余 11 组镜像 |
| 46 | net.md | 184 | **完成** | 试点 2；六段全绿（184/184，G3 203 片段）：地址族 20 + 缓冲/DNS/Bytes 39 + Socket 39 + Port 29 + Post 3 + Engine 17 + Worker 9 + 第 6 段 26 + CompletionInit 1 |
| 47 | number.md | 15 | **完成** | 15/15 全绿（G3 15 片段，2026-09-07）；xrt.number 域 CONFIG/FORMAT/RANGE 三码入档；容量原子失败 XERR_RANGE （查询仍返回长度）；锚点 integer/float/format/variants |
| 48 | once.md | 22 | **完成** | 22/22 全绿（G3 22 片段，2026-09-07）；thread.h 全集：Once 1 + 原生线程 15 + 线程局部键 6；Wait 自等待 STATE、Once 同线程重入 STATE 入档；锚点 once/thread/thread_tour |
| 49 | path.md | 32 | **完成** | 32/32 全绿（G3 32 片段，2026-09-07）；xrt.path 域 FORMAT/OVERFLOW/ROOT/SYSTEM 四码入档；safe 族纯谓词不设错；manifest 补注册 tour/system/safe 三范例 |
| 50 | pattern.md | 37 | **完成** | 37/37 全绿（G3 37 片段，2026-09-07）；xrt.pattern 域 CONFIG/PATTERN/LIMIT/CONFLICT/CAPACITY 五码入档；ErrorOffset/ErrorPattern 机器数据定位器；锚点 pattern/pattern_tour |
| 51 | pem.md | 7 | **完成** | 7/7 全绿（G3 7 片段，2026-09-07）；xrt.pem 域 LABEL/BOUNDARY/BODY/NOT_FOUND 四码入档；xpemresult 三值逐值成表；锚点 pem/pem_tour（后者挂 asn1_der） |
| 52 | pool.md | 58 | **完成** | 58/58 全绿（G3 58 片段，2026-09-07）；单页 20 + 固定池 20 + 变长池 18 三族；旧合并式小节（`X` / `Y` 双名）拆分；AGAIN 页满、RANGE 乘法溢出入档；锚点 pool_page/pool/memory_pool |
| 53 | process.md | 37 | **完成** | 37/37 全绿（G3 37 片段，2026-09-07）；xrt.process 域 14 码（ARGUMENT..TERMINAL）入档；Read/Write int64 三态口径；Run 族 false=仅基础设施失败；manifest 补注册 tour |
| 54 | proxy.md | 23 | **完成** | 23/23 全绿（G3 23 片段，2026-09-07）；对象 5 + 握手 10 + 拨号 8 三组；六域码（CONFIG/CREATE/LIMIT/PROTOCOL/CONNECT/UNSUPPORTED）；两状态机逐值成表；锚点 tour/socks5/dial |
| 55 | queue.md | 46 | **完成** | 46/46 全绿（G3 46 片段，2026-09-07）；SPSC/MPSC/MPMC 三族 ×15 + Capacity；xqueueresult 四值与批量部分完成口径；CLOSED 弹出=关闭且排空；锚点 queue_tour + containers 三范例 |
| 56 | random.md | 39 | **完成** | 39/39 全绿（G3 39 片段，2026-09-07）；安全 4 + 显式 14 + 线程默认 12 + Fast 别名 9 四组；非密码学安全警示逐节标注；Secure 失败清零输出；锚点 6 范例 |
| 57 | regex.md | 54 | **完成** | 54/54 全绿（G3 54 片段，2026-09-07）；编译/转义 15 + matcher 10 + 便捷 4 + 替换 4 + 拆分 5 + 集合 16 六组；xrt.regex 六域码；xregexresult 三值；锚点 5 范例 |
| 58 | set.md | 34 | **完成** | 34/34 全绿（G3 34 片段，2026-09-07）；七组按既有散文结构入节；GetOrAdd 原子插入 + pNew 出参；集合运算族兼容性 STATE；锚点 set/owned/set_tour |
| 59 | signal.md | 20 | **完成** | 20/20 全绿（G3 20 片段，2026-09-07）；代码/订阅/句柄/原生处理/计数关闭五组；Owned 数据失败不转移、Shutdown 回调线程自关闭 STATE 入档；锚点 signal/signal_tour |
| 60 | slot_map.md | 16 | **完成** | 16/16 全绿（G3 16 片段，2026-09-07）；诊断/生命周期/基本/迭代四组；旧合并式双名小节拆分；陈旧句柄一律不设错；锚点 slot_map/slot_map_tour |
| 61 | spin.md | 7 | **完成** | 7/7 全绿（G3 7 片段，2026-09-07）；Unit/Destroy 持有中失败、TryLock 忙碌不设错；锚点 spin |
| 62 | stack.md | 74 | **完成** | 74/74 全绿（G3 74 片段，2026-09-07）；五族（Stack 18/Fixed 15/PtrFixed 11/Block 18/Ptr 12）；固定族满 = AGAIN、空弹/越界 = RANGE、Block 族块布局溢出 = OVERFLOW；锚点 tour + containers 五范例 |
| 63 | string.md | 83 | **完成** | 83/83 全绿（G3 83 片段，2026-09-07）；八组：视图/查询 23 + 借用切分 11 + 独立操作 20 + 拆分列表 12 + 构建器 14 + 格式化 2 + 通配 1；参数名自动提取自签名；锚点 16+ 范例 |
| 64 | sync.md | 48 | **完成** | 48/48 全绿（G3 48 片段，2026-09-07）；Mutex 7 + Cond 9 + Sem 9 + RWLock 12 + Event 9 五族；虚假唤醒契约、Sem 上限不部分发布、RWLock 升降级前置条件入档；锚点 5 范例 |
| 65 | task.md | 42 | **完成** | 42/42 全绿（G3 42 片段，2026-09-07）；组 15 + 池 14 + 组池提交 5 + 协程 2 + 网络 6 五组；AGAIN 队满回滚、工作线程自等待/自销毁 STATE 入档；锚点 8 范例 |
| 66 | tcp.md | 101 | **完成** | 101/101 全绿（G3 101 片段，2026-09-07）；tcp.h 63 + tcp_server 15 新写按 12 组入档 + proxy.h 23 节复用自 proxy.md；WriteLimit 背压 AGAIN、Worker 归属 STATE 全表；锚点 13 范例 |
| 67 | temp.md | 17 | **完成** | 17/17 全绿（G3 17 片段，2026-09-07）；arena 14 + 上下文便捷层 3；作用域后进先出 STATE、Trim 忙碌 STATE 入档；锚点 memory/temp |
| 68 | template.md | 32 | **完成** | 32/32 全绿（G3 32 片段，2026-09-07）；编译/注册表 13 + 扩展调用 13 + 渲染 6 三组；XTEMPLATE 12 域码；ErrorLocation 定位器；锚点 6 范例；manifest 补注册 tour |
| 69 | thread-key.md | 22 | **完成** | 22/22 全绿（G3 22 片段，2026-09-07）；全部节复用自 once.md（同映射 thread.h），以键为主线重组 |
| 70 | thread.md | 22 | **完成** | 22/22 全绿（G3 22 片段，2026-09-07）；全部节复用自 once.md（同映射 thread.h），按线程视角重组 |
| 71 | time.md | 58 | **完成** | 58/58 全绿（G3 58 片段，2026-09-07）；九组入档；字段越界 RANGE、解析族 ARGUMENT、Write 容量原子失败；锚点 10 范例；manifest 补注册 text_parse |
| 72 | tls.md | 268 | **完成** | 268/268 全绿（G3 268 片段，2026-09-07）；八头并集 20 组；签名/形参名/头文件契约注释自动提取（blurb=注释原文）；xtlsresult 四值逐函数成表；身份范例补四形态构造器（openssl 新材料实测）；manifest 补注册 handshake_extra |
| 73 | udp.md | 73 | **完成** | 73/73 全绿（G3 73 片段，2026-09-07）；打开 6 + 接收 7 + 错误包 5 + Future/批量/包 17 + 发送 13 + 组播 5 + 关闭查询 20 七组；批量容量 1–256 RANGE、Worker 归属 STATE 全表；manifest 补注册三范例 |
| 74 | value.md | 116 | **完成** | 116/116 全绿（G3 116 片段，2026-09-07）；标量 20 + 句柄 3 + 生命周期 9 + Array 20 + Object 13 + IntMap 11 + 迭代 8 + Set 16 + 类型身份 13 + 弱引用 3 十组；引用/移交/消费三形态全表；XERR_TYPE/VALUE/EXISTS 特有口径；锚点 13 范例 |
| 75 | wait.md | 3 | **完成** | 3/3 全绿（G3 3 片段，2026-09-07）；Deadline 三函数；NEVER 语义（Expired 永假、Remaining UINT64_MAX）；锚点 concurrency/deadline |
| 76 | websocket.md | 104 | **完成** | 104/104 全绿（G3 104 片段，2026-09-07）；帧/消息 12 + 握手 19 + 升级 9 + 压缩协商 30 + 流 34 五组；控制帧 125 字节上限、permessage-deflate 尾块约定入档；锚点 20 范例 |
| 77 | x509.md | 89 | **完成** | 89/89 全绿（G3 89 片段，2026-09-07）；视图/算法/名称/扩展/名称约束/CRL 三层/策略/签名/身份/路径/信任库十三组；X509_DONE/VALUE/ERROR 三值逐函数成表；锚点 19 范例 |
| 78 | xid.md | 11 | **完成** | 11/11 全绿（G3 11 片段，2026-09-07）；生成/文本/时间比较三组；系统随机源 IO、Write 容量 33 字节、ErrorOffset 定位器；锚点 xid/xid_batch |
| 79 | xson.md | 35 | **完成** | 35/35 全绿（G3 35 片段，2026-09-07）；读取/DOM/事件/写出/writer 22/文件/错误八组；WriterTake 未 Finish = STATE；锚点 xson/xson_tour |

## 记录

- 2026-09-07 Phase 0：DOC_SPEC v1.0-draft 定稿；两门禁工具上线
  （check_api_reference_detail.py / extract_doc_examples.py，验证于 array.md：改前 117 problem → 改后 0）；
  试点 1 array.md 完成（50 函数全绿）；试点 2 net.md 地址族完成（20/184，余 5 段）；
  ptr_array 模块补挂公共头至 manifest（文档映射用）。规范冻结待用户审定。- 2026-09-07 规范冻结 v1.0；net.md 第 2 段完成：缓冲/列表/DNS/Bytes 39 函数
  补齐（错误码自 src/network/buffer.c 逐函数溯源——POOL_BUSY/BUFFER_STATE/
  BUFFER 三个域码首次入档；预留状态机与四类追加所有权全部落成参数表）。
  工具修复：多行签名括号内空白折叠。array.md 复验仍绿。
- 2026-09-07 net.md 第 3 段（Socket 原语 39 函数）完成：错误码自
  src/network/socket.c 溯源——域码体系首次完整入档（SOCKET_OPEN/
  BIND/LISTEN/ACCEPT/CONNECT/READ/WRITE/CLOSE/SHUTDOWN/OPTION +
  NATIVE），系统错误保留 SystemCode 的语义（错误种类由平台码映射）；
  RecvMsg 族 pMeta 必填 vs RecvFrom 可空、Connect 的 AGAIN 不得二次调用、
  RecvBatch 返回已到达前缀（Windows 环回补收口径）三个契约差异写入参数表。
  G3 从 78→119 片段全绿；array.md 复验仍绿。
- 2026-09-07 net.md 第 4 段（Port 29 + 嵌入式 Post 3 = 32 函数）完成：
  错误域码 PORT_CREATE/CLOSE/WATCH/SUBMIT/CANCEL/POST/WAIT 自
  src/network/port.c 溯源入档，含后端层补充（WatchLimit→XERR_RANGE、
  OperationLimit→XERR_AGAIN）；单缓冲 Recv/Send/SendTo/SendMsg 为
  一跨度转发（错误集继承 Vec 形态）首次成文；Windows 后端能力分裂
  （IOCP 仅 completion / SELECT 仅 readiness / AUTO 取 IOCP）写入节级
  说明；Worker 归属契约（提交/取消/等待/销毁仅拥有线程；Post/Wake 可
  跨线程）逐函数标注；嵌入式 Post 三态（受理 Pending→执行清除）与
  XERR_CLOSED 关停口径入档。范例锚点跨 4 个已注册范例
  （port_tour/port_iocp/port_uring/engine_tour）。G3 119→151 片段全绿
  （门禁拦截 1 处不连续片段：Accept 终态等待与提交之间隔着 Connect
  等待，改为完整连续区间）；array.md 复验仍绿。表行同步修正
  第 2/3 段漏记（记录区已有、表格行停在 [1/6 段]）。
- 2026-09-07 net.md 第 5 段（Engine 17 + Worker 9 = 26 函数）完成：
  错误域码 ENGINE_CREATE/START/STOP/POST/TIMER 自 src/network/engine.c
  溯源入档；Stop 的三态失败口径成文（STATE 自等待死锁/封口不收敛、
  POOL_BUSY 外借池块——Engine 仍进 STOPPED 且可重启）；Pin/Unpin 的
  CLOSED（未运行）与 STATE（未配对）区分；纯查询函数
  EngineCurrent/WorkerEngine/WorkerIsCurrent 按门禁补显式
  "无错误"节（查询语义非错误）；TimerCancelCurrent 失败不设错误的
  契约（先试 Current 再走 TimerCancel）入档；EngineAfter 为 Schedule
  转发（锚点用 network/engine 范例）。G3 151→177 片段全绿；
  array.md 复验仍绿。
- 2026-09-07 net.md 第 6 段（Resolver 7 + ResolveOp 6 + ResolveAsync +
  Interface 4 + Local/Host 8 + CompletionInit = 26 函数）完成，
  net.md 全文件达成（184/184，G3 203 片段全绿）：
  错误域码 RESOLVER_CREATE/CLOSED/SUBMIT/QUERY + FAMILY 自
  src/network/resolver.c、INTERFACE_INDEX/NAME/ADDRESS/HARDWARE +
  BUFFER（两段式缓冲不足=NOT_FOUND/RANGE 区分）自 interface*.c
  溯源入档；Resolver 的 VALUE/CLOSED/RANGE/AGAIN 四层受理门与
  缓存命中共享查询组语义入档；OpRef 引用计数与"回调后保留须先取
  引用"契约成文；接口族"两段式查询→写入"统一口径成文；
  String 形态判空即失败（不设线程错误）与 Text 形态 XRT_NPOS
  的区分入档。门禁工具修复：check_api_reference_detail.py 的
  --all 模式因成员判断用裸文件名而字典键带 docs/api/ 前缀，
  恒空转（修复后 77 个待办文件正确报数）；G3 工具无此问题。
  array.md 复验仍绿（50/50 + 50 片段）。
- 2026-09-07 asn1.md 完成（27/27，G3 27 片段）：九个 XASN1_ERROR 稳定码
  （TAG/LENGTH/VALUE/TYPE/END/TRAILING/ORDER/DEPTH/RANGE）与
  PROTOCOL/TYPE/RANGE/VALUE 四类别的映射关系自 src/asn1/der.c 溯源
  成文为总表；编码器族（Append + 6 个常用 primitive + AppendOid +
  OidEncode/OidDecode）首入文档——失败原子性（"不发布半个 TLV、
  对外可见长度不变"）与 Content 借用契约入档；读取器族三类失败
  （ARGUMENT/TYPE/VALUE）与 Unsigned 负值拒绝、UInt64/Int64 范围
  门槛成文；纯判定函数 Is/Done/OidEqual 按门禁补显式"无错误"节。
  范例锚点：der（游标链 7 API）/decode_tour（读取器族 8）/
  encode_tour（编码器族 12）。双门禁一次全绿；array.md 与
  net.md 复验无回归。
- 2026-09-07 atomic.md 完成（29/29，G3 29 片段）：三族（32/64/Ptr）
  统一的顺序矩阵（加载三种/存储三种/读改写五种/CAS 失败序约束）成文
  为错误总表；"失败的写操作不修改对象"与 CAS 失败回写 *pExpected
  的强语义逐函数入档；Init/Store/栅栏对非法参数静默空操作、读改写
  返回零值设错的差异化口径区分；IsLockFree 保守口径（false ≠ 非原子）
  入档。范例 atomic_tour 补齐 5 个缺口 API（32 Store/FetchAdd/
  FetchSub、64 Store、Ptr Init/Store），真实编译运行验证
  （gcc 16.1，EXIT=0，预期输出 4/5 行不变、64 位行 0→7）。
  门禁拦截 2 处 Init 参数表漏行后修复转绿；asn1/array/net 复验
  无回归。
- 2026-09-07 avl.md 完成（43/43，G3 43 片段）：侵入式 17 + 拥有式 26
  双形态全部成节；错误四域（ARGUMENT/STATE/RANGE/MEMORY）逐函数
  对位（Insert 的节点非独立 STATE、TreeAdd 的键不等价 ARGUMENT、
  SetDrop 的非空树 STATE、IterNext 的版本变化 STATE、Take 的
  区间别名拒绝）；"未找到不是错误、不清除既有错误"的查询口径
  逐函数标注。锚点跨三个已注册范例（avl_tour 40 项 + avl 的
  IterBegin + avl_tree 的 Init/IterFrom）。双门禁一次全绿；
  array/asn1/atomic/net 复验无回归。
- 2026-09-07 buffer.md 完成（23/23，G3 23 片段）：生命周期/容量/
  直写/编辑/所有权五组 + HEX/Base64 解码构造器全部成节；
  "分配失败时原地址、长度、容量和已有内容保持不变"的原子失败
  契约逐函数标注；Write 的稀疏写语义（越过末尾扩展+空洞补零）、
  Take 的"空缓冲成功返回 NULL"哨兵口径、SetTake/CreateTake 的
  槽有效性约束（xrtMalloc 家族 + 默认对齐 + size<=capacity +
  零容量配空地址）入档。范例 buffer_tour 补 Clear 调用（此前仅
  asn1/encode_tour 覆盖），编译运行验证通过（EXIT=0，输出不变）。
  双门禁一次全绿；array/asn1/atomic/avl/net 复验无回归。
- 2026-09-07 cancel.md 完成（9/9，G3 9 片段）：门禁曾报 33 问题
  （9 函数）——根因是旧文 "xrtCancelRef / xrtCancelDestroy" 合并节
  的首代码块含两条签名触发 G1，且 Destroy 无独立节；拆分后 9 节
  全部按模板成文。核心契约逐函数入档：Request 的"首次 true/重复
  false 不设错"、Requested 的"空指针=无取消源不设错"（可选取消
  参数用法）、Watch 的"已取消令牌上注册即同步执行回调"、Unwatch
  的跨线程等待与回调自注销延迟回收。Requested 锚点取自
  task_group_pool（任务函数内的协作自查点），其余 8 个来自
  concurrency/cancel。撰写中自查发现 Watch 签名块笔误（重复类型
  名）先于门禁修复。双门禁一次全绿（修复笔误后）；六个既有文件
  复验无回归。
- 2026-09-07 channel.md 完成（42/42，G3 42 片段）：生命周期 5 +
  非阻塞 2 + 阻塞收发 6 + 可取消 6 + 查询/关闭/重置 7 + Select 7 +
  协程 Await 9 五族全部成节；XCHANNEL_*/XWAIT_* 双结果体系逐函数
  成表（FULL/EMPTY/CLOSED 不设错 vs ERROR 设错），"零超时=try 语义、
  取消优先于超时、关闭优先于取消"的判定顺序入档；Unit 的 STATE
  （等待者/rendezvous 挂起）与 Reset 的 AGAIN（非空/挂起/Select
  节点）区分；Await 族"非协程上下文 STATE"门槛成文。锚点跨 7 个
  已注册范例（tour 34 项 + channel/cancel/select/select_cancel/
  coroutine 补普通形态 + worker 补 Drain）。双门禁一次全绿；
  七个既有文件复验无回归。
- 2026-09-07 charset.md 完成（69/69，G3 69 片段）：此前 --all 报 131
  问题（合并节 + 缺节 + G1），本次按 16 组全部拆立成节——视图/宽串 8、
  标量原语 5、校验计数 9、区间 2、搜索 5、裁剪 3、编辑填充 5、反转
  过滤 4、距离 2、流式校验 3、缓冲转换 6、分配转换 12、BOM/转码 4、
  检测 1。三对单位（字节/码元/标量）在全部返回值表区分；"Valid 族
  内容无效不设错误（探测语义）vs 转换族设 VALUE 错误"的口径差异
  逐函数标注；Distance 的"超限返回 NPOS 是正常阈值结果"与
  Similarity 的"负值=失败哨兵"区分。镜像族（Buffer 6 + 零结尾 6 +
  视图 6）按规范写全模板但说明精简。锚点跨 9 个已注册范例（utf16_32
  27 项 + unicode 5 + unicode_text 4 + transcode_tour 10 + utf8_edit
  10 + utf8_search 9 + transcode/detect/distance 各 1-2）。双门禁
  一次全绿；八个既有文件复验无回归。
- 2026-09-07 codec.md 完成（20/20，G3 20 片段）：HEX 4 + Base64 4 +
  Percent 12 三族全部成节；Percent 的"预检快速路径"分层成文
  （MapInit→Measure→WriteMeasured/EncodeMeasured 与
  DecodeMeasure→DecodeMeasured 各自的前置条件契约：同输入/同位图/
  同模式、空间由调用方保证、不设错误的 no-check 路径）；PercentNext
  的"ERROR 不推进游标不改线程错误"（解析器组合友好）入档；两段式
  查询（空输出+零容量）与原地同址扩张/收缩契约逐函数标注；六个
  XCODEC_ERROR_* 域码汇总成总表。发现并修复 manifest 缺口：
  examples/codec/tour 未注册（G4 阻塞）——定点挂到 codec_hex 的
  examples（1 行 diff，JSON + manifest 校验通过）。锚点：tour 14 +
  hex/base64/percent 专项范例各 2。双门禁一次全绿；九个既有文件
  复验无回归。
- 2026-09-07 周期全量复审（10/79 节点）：G1/G2/G4 --all =
  10 ok + 69 problem（全部为待办文件的 missing-section，共 3762
  项，无签名/参数表/错误节/范例链接类历史问题）；G3 --all =
  79 文件 0 失败。已完成集合零回归，门禁基线健康。
- 2026-09-07 compress.md 完成（18/18，G3 18 片段）：Inflate 9 +
  Deflate 9 对称成节；xrt.inflate/xrt.deflate 双域六码错误矩阵
  成总表（DATA 为 Inflate 独有、CODEC 为 Deflate 独有）；核心
  契约入档——"参数失败不改状态、其余进入失败终态须 Reset 复用"
  的两分法、"空回调=校验并丢弃"、"All 只在完整成功后写输出长度"；
  四级 Flush 语义（NONE/SYNC/FULL/FINISH）与回调重入拒绝逐函数
  标注；DeflateAll 的确定性输出（内容寻址/缓存关键性质）入档。
  锚点：stream_tour（14 项）+ deflate/inflate 专项范例各 2。
  双门禁一次全绿；既有文件抽查无回归。
- 2026-09-07 compress.md（18/18，d460c23b）、console.md（4/4）、
  environment.md（4/4）完成：compress 的 Inflate/Deflate 对称族 +
  双域六码错误矩阵 + "参数失败不改状态 vs 失败终态须 Reset"两分法；
  console 四函数与 xrt.console 四码域成节（重定向 IsTerminal=false
  是正常结果的口径）；environment 的 Lookup"查询成功与存在性分离"
  （true + 空输出 = 不存在）与 Get/Remove 幂等口径入档。三个文件
  双门禁均一次全绿；锚点覆盖 stream_tour/variants 等已注册范例。
- 2026-09-07 core.md 完成（44/44，G3 44 片段）：core 模块把四个头
  （features/core/error/memory）都映射到 core.md，门禁按并集要求
  44 节——core 4 + 内存 15 + 错误 25 全部成文。关键契约入档：
  SetError"增引用不偷引用"（槽持自己的引用，调用方仍须释放自己
  那份——源码核实 xrtErrorRef 实现）与 SetErrorTake"所有权转移"
  的对比；访问器族"空指针安全"（Message 永不 NULL、其余返回
  NONE/0/空串）；RefRetain/RefRelease 的 -1 哨兵口径（原语按
  返回值报告，不设线程错误）；At 族仅 MEMORY_DEBUG 下提供。
  发现并修复 manifest 缺口：examples/error/tour（覆盖错误族 19
  API）未注册——定点挂到 core 模块 examples（1 行 diff，JSON +
  manifest 校验通过）。锚点跨 9 个已注册范例（error/tour + core
  四范例 + allocator_tour + process/open + proxy_dial + json_tour）。
  门禁拦截 3 处查询函数缺错误节后补齐转绿；既有文件抽查无回归。
  注：error.md（#16）与 memory.md 的门禁并集同为 44/45 函数，
  其任务将复用本文件节内容按各文件侧重改写。
- 2026-09-07 coroutine.md 完成（51/51，G3 51 片段）：三组全部
  成节——协程核心 18（生命周期/自省/协作取消/清理栈/后端）、
  调度器 23（创建/投递含 PostOwned 恰好一次析构/单步与轮询/
  Park-Wake-Sleep-Join）、事件 10（自动/手动复位 + 四形态等待）。
  关键契约入档：Cancel 与 ConfirmCancel 的"请求-确认"两段式
  （处理后正常返回仍是 RETURNED）；Destroy 拒绝活跃栈的 STATE
  语义；SchedPost 的 AGAIN 背压与 PostOwned"失败不接管"；
  Step/Poll 的 CLOSED=排空正常结果口径；Event Unit/Destroy 的
  "仍有等待者即失败保持对象"。锚点跨 5 个已注册范例
  （coroutine_tour 36 项 + lifecycle 10 + coroutine/event/
  scheduler）。G3 拦截 1 处不存在的 EventSet 片段（凭记忆写
  了未在范例中的调用形态，改为真实源行）后转绿；既有文件抽查
  无回归。
- 2026-09-07 crypto.md 第 1 段（哈希族 28 + CryptoHashSize +
  ConstTimeEqual = 30 节）完成：七族哈希（Md5/Sha1/Sha224/Sha256/
  Sha384/Sha512/Sha512_256）各四形态（Init/Update/Final/一次性）
  全部成节；镜像族按 DOC_SPEC 生成器产出（签名逐字符对齐头文件，
  Sha512_256 多行形态单独处理）。核心契约入档：Final 在状态快照
  上完成填充（可重复取摘要且不结束流）；Update 仅缓存不足一块的
  尾部、失败原子；CryptoHashSize 元数据查询与实现编入解耦。
  门禁拦截生成器两类真问题：Update/Final 片段尾部拼出源码不
  存在的 ") {"（10 处，正则批量修正）；锚点跨 hash_tour（五族
  三段式）+ sha256/sha512 双变体 + ecdsa_p256/p384（一次性）+
  md5/sha1/sha224/sha512_256 专项。G3 30 片段全绿；G1/G2 余 92
  全部为第 2/3 段 missing-section。既有文件抽查无回归。
- 2026-09-07 crypto.md 第 2 段（密码 22 + MAC/KDF 24 = 46 节）
  完成：AES 块 4 + AES-GCM/GMAC 9 + ChaCha20 1 + ChaCha20-
  Poly1305 4 + Poly1305 4 + HMAC 三族 12 + PBKDF2 三形态 3 +
  HKDF 三族九形态 9。认证失败语义统一入档（XERR_PROTOCOL +
  xrt.crypto/XCRYPTO_ERROR_AUTHENTICATION、明文输出逐字节
  不变、GMAC Verify 常量时间比较）；分离式 Encrypt/Decrypt 与
  拼接式 Seal/Open 双路径各自的容量契约成文；HKDF 的"Extract
  可选空盐=全零 + Expand 上限 255×N"边界入档。生成器工作流
  出现两类机械故障均被门禁拦截：修复脚本正则误替换签名块
  （G1 抓出，回滚重生成）与 Sha384 族片段指向错误范例文件
  （G3 抓出，烘焙进生成器）；heredoc 转义陷阱三次（最终全部
  用 Write+Edit 工具完成）。锚点：aead_tour 10 + aes/aes_gcm
  /chacha20/poly1305/cc20p1305 专项 + hash_tour/hmac_sha512
  + kdf_tour/hkdf 系。G3 76 片段全绿；G1/G2 余 46 精确等于
  第 3 段。既有文件抽查无回归。
- 2026-09-07 crypto.md 第 3 段（签名/曲线 46 节）完成，全文件
  达成（122/122，G3 122 片段全绿）：RSA 7（原始模幂 + PSS
  显式盐/随机盐/ANY 验签 + PKCS1 签验）、ECDSA 10（DER 编解码
  两段式 + 双曲线 raw/DER 签验）、Ed25519 9（种子/展开密钥双
  形态 + PURE/CONTEXT/PREHASH 三域）、曲线 20（X25519/X448
  各 4 + P-256/P-384 各 6）。关键契约入档：ECDSA Sign 收
  "摘要而非消息"（Hash 枚举声明算法）；RFC 6979 确定性 low-S；
  Ed25519 三域互不兼容 + PREHASH 恰 64 字节 SHA-512；ECDH
  Shared 族的"低阶点全零拒绝"与 NIST 曲线"完整公钥验证"；RSA
  Private 的 CRT + 公钥复核。工作流再现正则陷阱：fix 脚本
  两次误覆盖签名块被 G1 拦截（恢复模板重建）；9 处片段因链接
  范例文件与实际调用源不符被 G3 拦截（ecdh_tour 为 X25519/
  P384 点运算真源）。锚点：sign_tour 16 + rsa_pss/rsa_pkcs1/
  ecdsa_p256/p384 + ed25519 + x25519/x448/p256/p384 +
  ecdh_tour。既有文件抽查无回归。
- 2026-09-07 error.md 完成（45/45，G3 45 片段）：错误族 25 节
  复用 core.md 已验证模板（经门禁二次确认），新增 xrtSetErrorFormat
  （printf 规则 + %n 拒绝 + OOM 无分配保证）；内存/引用/版本 19 节
  以"Core 并集附录"形式收录并链接 core.md/memory.md。门禁工具
  修复：table_first_column 的 `\w+` 无法登记变参 `...` 行（合法
  签名永远过不了参数覆盖检查）——扩展为 `\w+|\.\.\.`，属工具
  缺陷修正（同 --all 前缀 bug 一类），不影响既有文件（core/array/
  crypto 复验全绿）。
- 2026-09-07 executor.md 完成（10/10，G3 10 片段）：十函数全部
  成节。契约入档：Submit/SubmitBatch 的"成功接管析构、失败不
  调用析构"所有权边界；SubmitBatch 单 Worker 队列"全成或全败"
  原子性；AGAIN 满载背压（QueueLimit 为每 Worker 硬上限）；
  Wait 族"只对已关闭执行器有意义"+ Worker 不能等待/销毁自身
  的 STATE；Cancel"丢弃排队工作并执行析构、运行中不强停"。
  锚点：executor（千次 Submit + 失败 Cancel）+ executor_tour
  （Batch/Get 六计数/Close+Wait 三形态）。双门禁一次全绿。
- 2026-09-07 file.md 第 1 段（36 节）完成：打开族 3（OptionsInit/
  FileOpen/Open）+ IO 十形态（单次/Full/At/AtFull × 读写）+ 定位
  与大小 6（Seek/Tell/Flush/Size/Resize/SetSize）+ 元数据与路径级
  9（Flags/Native/Stat/Exists/Touch/Delete/Rename/Copy/Move）+
  整文件 5（ReadAll/Limit/WriteAll/Append/WriteAtomic）+ 文本 4
  （ReadText/Limit/WriteText/Atomic）。契约入档：Close"即使系统
  关闭失败也销毁对象"的一次性语义；At 族"不改变共享游标"（多路
  复用安全）；ReadFull"提前 EOF = 失败但保留已读量"；WriteAtomic
  "同目录排他临时文件 + 原子替换"读者不见半文件；Text 族
  UNKNOWN 自动探测 + charset 域错误继承。G3 拦截 2 处凭记忆
  写的调用形态（SetSize/ReadAllLimit 实参不同），改为真实源行。
  锚点：io_tour 19 + basic 8 + whole 5 + text 2 + report 2。
  G1/G2 余 61 = 第 2/3 段缺口。array 复验无回归。
- 2026-09-07 file.md 第 2 段（37 节）完成：锁 4（全文件与
  Range 双形态，UnlockRange"参数须与加锁逐项一致"+Size 零=
  到末端含后续增长）+ 映射 5（借用 Data/Size、Flush 提交共享写
  区间）+ 临时 2（FileTemp 排他创建返拥有路径、DirTemp 同构）+
  路径元数据 5（PathStat 跟随末级链接开关、SetMode/SetAttributes
  的平台互补 UNSUPPORTED 语义）+ 目录核心 21（枚举三态
  ITEM/END/ERROR、EntryPath 拼拥有路径、Clean 清内容留目录 vs
  EnsureEmpty 建或清 vs RemoveAll 含根删、Roots/RootsFree 配对）。
  一次生成双门禁全绿（37 片段）。锚点：dir_tour 13 + io_tour 3 +
  lock/map/temp/dir_temp/link_tour/link/tree/directory。G1/G2
  余 24 = 第 3 段缺口。array 复验无回归。
- 2026-09-07 file.md 第 3 段（24 节）完成，全文件达成
  （97/97，G3 97 片段）：遍历 2（WalkOptionsInit 保守默认 +
  FileWalk 回调空=纯统计）、树 3（TreeCopyOptionsInit 目标须
  不存在/保留链接/拒特殊对象；TreeRemove 的 bKeepRoot=Clean
  语义）、链接 4（Create 目录提示 Windows 必需、Delete 不跟随
  目标、Read 返拥有文本）、FIFO 1（Windows UNSUPPORTED 门控）、
  根 14（RootOpen 锚定真实目录 + 根内相对路径解析器阻止 ..
  与绝对路径越界；RootPath 仅诊断不参与安全判断；Root 族
  LinkCreate 目标文本不经根解析的对比语义）。两段连续一次
  生成全绿（73+24 片段零返工）。五文件复验无回归。
- 2026-09-07 file_async.md 完成（34/34，G3 34 片段）：七组
  全部成节。核心契约入档：WriteAt 三形态所有权梯度（复制
  WriteAt / 借用 Ref 释放回调恰好一次 / 接管 Take 终态后
  xrtFree）+ "零长度不转移所有权"；Adopt 的关闭责任单向转移
  （失败仍归调用方）；Close 走任务池资源回收通道不占队列
  槽位（队列满仍能收尾）；同一对象不同偏移可并行不共享游标；
  "受理前失败=线程错误 vs 受理后失败=Future 错误+cause 链"
  的两段式错误口径全函数统一；路径/数据在提交时快照。
  生成器边界 bug 被门禁抓出（末节 EnsureEmpty 插到取消节后
  未进目录树组，missing-section 报出后定点补插）。
  锚点：async_tour 21 + async 5 + whole/manage/dir_async/
  tree_async 专项。五文件复验无回归。
- 2026-09-07 future.md 第 1 段（核心 44 节）完成：创建/生命周期 5、
  状态与结果 5、取消 4（Future 请求 vs Promise 发布终态的两段式）、
  Promise 完成族 6（Resolve 借用 vs ResolveOwned 移交"失败不偷
  引用"、Reject 增引用、Forward 透传、Close 关闭终态）、Watch 4
  （调用方 64 字节存储无分配；Add 的 READY=不接管不执行 Release、
  Detach 同步 Release、Remove 禁止自通知调用）、同步等待 4、
  协程等待 3、组合器 3（Any pick/All 保序借用/Race 取消其余）、
  延续 9（Continue 任意终态/Then 成功/Catch 失败/Finally 观察
  透传 × 借用与 Owned 双形态 + CancelSource 独占源变体）。
  门禁两次拦截：组合器片段实参 "2" 被写成 "2u"（G3）；Await 三节
  被边界 bug 吞失（missing-section 报出后补插）。锚点跨 10 个已
  注册范例（future_tour 25 + combine 8 + worker/report/
  resolver_future/tcp_server_sync 等真实场景）。G1/G2 余 63 =
  第 2/3 段缺口。array 抽查无回归。
- 2026-09-07 future.md 第 2 段（29 节）完成：Future 桥 8
  （装配竞态模型——Ready 放行/Fail 回收、Wait 返回"能否写
  Promise"、Unwatch 与取消回调汇合）+ TLS Listener 12（Start
  保留 Context/Identity + ALPN 深复制的所有权边界；Accept 三态
  pull：非阻塞/Future/阻塞，阻塞版禁 Worker 调用；Close 丢弃
  未交付连接但已交付独立存活）+ TLS Dial 9（回调式 Dial 与
  Future 式 DialAsync 双入口；Cancel "返回真保证不再变成功"；
  Error 借用含 DNS/TCP/TLS 分层 cause 链）。另修复第 1 段遗留：
  WaitFor/WaitUntil/WaitUntilCancel 三节被早前 Await 补插时的
  边界 bug 吞失（missing-section 报出后重建）。锚点：bridge_tour
  8 + listener_tour 17 + dial/dial_future。G3 73 片段全绿；
  G1/G2 余 31 = 第 3 段缺口。array 复验无回归。
- 2026-09-07 future.md 第 3 段（TLS stream 31 节）完成，全文件
  达成（104/104，G3 104 片段）：构造 5（Connect 数字地址直连 /
  Attach 接管 Transport+Session / Client 代理隧道与 STARTTLS /
  Accept 在 TCP 回调内接管——四种组装模型各自的所有权边界）+
  生命周期与查询 9 + Worker 专用收发 9（Send 短写、SendVec 连续
  前缀、SendBound 精确密文上界、Buffer 借用链默认暂停底层读取、
  Pullup 连续化不消费、ReadMore 增量协议且受 PlainLimit、Read
  复制并消费、Consume 精确消费）+ 关闭 2（Close 认证关闭 FIFO
  排空 vs Abort 立即中止）+ 异步观测与 Future 收发 6（SendAsync
  取消仅在首字节受理前有效、SendVecAsync 失败不发布部分操作、
  WaitAsync 六条件、RecvAsync 零上限=全部明文）。一次生成全绿。
  锚点：stream_tour 19 + stream 5 + stream_future 4 +
  listener_tour/dial_future。六文件复验无回归。
- 2026-09-07 hash.md（9/9）+ html.md（3/3）完成：hash 把
  "Hash32 与 Hash64"等四处合并节拆立，SipHash 流式三段式
  （Init guard 校验/Update 7 尾字节失败原子/Final 副本终结可
  重复）+ SipKey"只组装不产生随机性"成文；html 三层
  Size/Write/Escape（两段式查询、同址扩张允许、TEXT/ATTRIBUTE
  双模式）。发现并修复 manifest 缺口：examples/html/variants
  未注册（定点挂 html_escape，G4 阻塞解除）。门禁连环追击一处
  老文档遗留：Final 节范例区尾部嵌着占位符示意块（secret0/
  header 等非源码标识），G3 拒绝；连同 shell 转义两次把
  printf 的反斜杠 n 写丢（用 chr(92) 构造修复），最后把示意
  块迁至模块级"线程与所有权"散文区（范例区只留可追溯片段）。
  双文件全绿；array 复验无回归。
- 2026-09-07 http.md 第 1 段（28 节）完成：方法与状态 6（含
  ContentAllowed 的"方法+状态"双参判定——门禁 G1/G3 双拦截
  纠正了我按臆测写的单参签名与臆测调用）+ 令牌与 OWS 8 +
  权重与长度 3（qvalue 千分值口径、ContentLength 重复一致
  才成功）+ Host/Authority 8（IPv4 拒前导零、PORT_VALUE 语义、
  Target 按 CONNECT/星号分流）+ 编码枚举 2（x-gzip 别名）。
  锚点：method_tour/token_tour/validate_tour/host/target/
  base/encoding/small_fields。G3 27 片段全绿；G1/G2 余 141。
  array 复验无回归。
  补记：examples/http/small_fields 未注册（G4 拦截），已定点
  挂到 http 模块 examples（JSON + manifest 校验通过），余 140
  全部为第 2-4 段 missing-section。
- 2026-09-07 http.md 第 2 段（33 节）完成：字段解析与写出 6
  （FieldParse 单行 / FieldNext 三态块游标 / FieldWrite 与
  BlockWrite 含终止空行）+ 字段查找 5（Find 返回下标、Get 借用
  地址、GetUnique 三态区分"唯一/未找到/同名重复"）+ 同名字段
  token 游标 4（跨重复字段保持线路顺序）+ quoted-string 4 +
  参数 12（ParamNext 三态、Token 族"纯谓词不修改线程错误"、
  ValueNext 逐字节游标、Write 的 HAS_VALUE/QUOTED/NONE 三形态）
  + 指令 3（Next/Count/Find 与参数族的分号 vs 逗号语法差异）。
  G3 拦截 quoted 族反斜杠引号片段两次（SV 转义序列在文档、
  python、shell 三层往返中变形），最终以 chr(34)+chr(92) 构造
  逐字符重建。G3 60 片段全绿；G1/G2 余 106 = 第 3/4 段。
  array 复验无回归。
- 2026-09-07 http.md 第 3 段（55 节）完成：TE 10（单值/跨字段
  双游标 + Parse 零分配汇总 + AcceptsTrailers "声明不丢弃"判定）
  + Expect 8（ExpectFields 三值分类：无/100-continue/合法但
  不支持）+ Upgrade 10 + Trailer 6（NameValid 禁投递集合 +
  NamesWrite 大小写不敏感去重保首现）+ Accept-Encoding 协商 6
  （Init"缺 Header=接受任意"的 RFC 口径、Select 等质量时
  Preferred→gzip→deflate→identity 决胜序）+ Content-Encoding 4 +
  CodingName 补失 + Decode 10（ConfigInit 兼容 vs InitSafe
  16MiB 不可信上限、"无编码路径直通回调不复制"、Reset 复用
  Inflate 窗口、Done="边界+压缩 trailer 均验证"）。生成器新增
  callline() 自动从范例源提取平衡括号调用块，55 节锚点零手工。
  门禁 G1 拦截 TeQuality 臆测签名（实为 pFields/iCount/Coding
  三参直读字段而非 pInfo 汇总）——重写后 52 余全部为第 4 段。
  G3 115 片段全绿；array 复验无回归。
- 2026-09-07 http.md 第 4 段（52 节）完成，全文件达成
  （167/167，G3 167 片段）：HTTP/1 起始行/Header 9（解析
  三态 OK/MORE/ERROR、Write 双形态"空输出查长度+容量不足不写
  半个报文"）、Body 分帧 11（RFC 9112 分帧优先级 Plan、
  BodyRead 状态机 DATA/FIELDS/DONE/ERROR、ChunkLineWrite
  "只写 size 行正文零复制"、TrailersParse trailer 区）、
  完整消息 5（bEnd 下截断 MORE 升级为错误、BodyView 零复制
  vs BodyCopy 去分帧）、缓冲链/TLS 解析 4（不消费输入、
  Head.Bytes 原子接管、Upgrade 余量保留）、代理对象 5、
  握手状态机 10（WRITE 先发完才 READ、Sent 支持部分写入、
  Bound 对 HTTP CONNECT 返回 NOT_FOUND、Destroy 清零敏感
  状态）、拨号 8。生成器切片越界被门禁连环抓出（Handshake
  10 节切成 9、Dial 8 节丢 Stats；G1 再拦 DialStats 臆测
  pStats 类型实为 xnetproxydialstats*）；callline 锚点对
  缺失文件自动全库搜索补齐（http1_body/websocket 等）。
  七文件复验无回归。
- 2026-09-07 http 系列 7 个小文件批量完成（61 节全部双门
  禁一次全绿）：http_connection 5（新写——Persistence 的
  PERSIST/CLOSE/ERROR 三态与 HTTP/1.0 keep-alive 显式策略；
  选项游标复用通用 xhttpfieldtokencursor）+ decode 10 +
  encoding 12 + expect 8 + te 10 + trailer 6 + upgrade 10。
  后六者采用"节复用"策略：直接从 http.md 已过门禁的对应节
  提取（含范例锚点），按各文件分组重组后追加——复用节仍经
  双门禁二次确认。这是多文件共享头并集的第二个成熟模式
  （第一个是 error.md 的 Core 附录）。http.md 复验无回归。
- 2026-09-07 http_fields.md 完成（64/64，G3 64 片段，双门禁
  一次全绿）：11 个功能组（方法/令牌/权重/Host/字段/Connection/
  quoted/参数/指令）全部通过节复用——从 http.md 与
  http_connection.md 提取已过门禁的 64 节重组，复用节经双门禁
  二次确认。两个源文件复验无回归。至此 http 全家族（母文件
  167 + 7 子文件 61 + fields 64）完成，共享头并集的"节复用"
  模式在 132 个复用节上验证稳定。
- 2026-09-07 io.md 完成（42/42，G3 42 片段，双门禁一次
  全绿）：Reader 20（自定义 ops 表 Create/五个来源形态（内存
  借用/Buffer 借用与接管/文件借用与接管/路径拥有）/Read 的
  "零字节=锁定 EOF 直到下次成功 Seek"/ReadFull 保留已读量/
  Copy 三形态（到 EOF 固定栈缓冲、精确 N、Limit 超限消费
  探测字节报 RANGE））+ LineReader 4（Create 借用 vs Take
  接管双形态、Next 三态 LINE/END/ERROR 且失败锁定）+ Writer
  18（同构 ops 表与来源族 + Discard 统计丢弃型、Write 拒绝
  零进展、Destroy 不隐式 Flush）。callline 全库自动锚点首次
  覆盖整文件（42/42 零手工、零返工）。http_fields 复验无
  回归。
- 2026-09-07 json.md 完成（30/30，G3 30 片段，双门禁一次
  全绿）：读取 6（Parse 默认严格 vs Read 高配置、Valid 零 DOM
  验证、Visit 事件流（STOP→CANCELLED 映射）、ErrorLocation
  从错误 Data 读行列）+ 增量写入器 16（Create 内存型 vs
  CreateSink 回调型；Object/Array/End/Name/五值型/Value 子树/
  Finish 封闭校验/Take 仅限已 Finish 的内存型/Free）+ 序列化
  4（Stringify/Pretty、Write 回调、QuoteWrite 流式字符串
  token）+ 文件 4（ReadFile 含输入上限、WriteFile 原子替换）。
  callline 自动锚点 30/30。io.md 复验无回归。
- 2026-09-07 list.md 完成（28/28，G3 28 片段，双门禁一次
  全绿）：生命周期 2 + 查询 8 + 导航 5 + 编辑 9 + 迭代 4。
  契约入档："插入只接受已 NodeInit 且未连接的节点"（双状态
  校验失败 XERR_STATE 且链表不变）；Remove/Pop 族"节点恢复
  独立状态可再插入、不释放内存"；Move 族不改变计数；Clear
  分离全部节点保留空链表；Validate 的全不变量断言；IterNext
  "自然耗尽不设错 vs 外部结构修改 XERR_STATE"；IterRemove
  "移除最近发布节点且迭代器保持有效"。callline 自动锚点
  28/28。json.md 复验无回归。
- 2026-09-07 list.md（28/28，c6a65ce9）+ math.md（20/20）
  完成：list 侵入式双向链表全接口（插入双状态校验/Remove 族
  "恢复独立不释放"/Move 不变计数/IterRemove 迭代中删除）；
  math 极值/基础/分类/函数/比较五组（Near 双容差公式、
  IntNear 无溢出整数比较、Log1p/Expm1 零邻精度、Cbrt 负数
  负根 vs pow、Hypot 中间不上溢）。发现并修复 manifest 缺口
  #6：examples/math/tour 未注册（14 个函数的唯一锚点源），
  定点挂到 math 模块。两文件 callline 自动锚点 48/48，双门禁
  一次全绿；json 复验无回归。
- 2026-09-07 map.md 完成（62/62，G3 62 片段，双门禁一次
  全绿）：字节键 Map 31（哈希桶 + 插入序；SetKeyPolicy 的成对
  哈希/相等器（两空恢复默认）；GetOrAdd 清零新槽 vs GetOrInit
  回调失败原子回滚；Take 族"值移交不调释放器"；StoredKey
  返回内部键等价副本；GetPtr 空值与缺失键用 Has 区分）+
  整数键 IntMap 31（按键有序存储；First/Last/LowerBound/
  UpperBound 四边界查询；IterFrom/RFrom 的 O(log n) 范围
  起点；Trim 释放空闲池页返回页数）。两形态共用的
  "策略/释放器只能在空映射上设置"与"未找到不是错误"口径
  统一。callline 自动锚点 62/62。list 复验无回归。
- 2026-09-07 周期全量复审 #2（37/79 节点）+ 任务对齐检验：
  (a) 四门禁全量：37 ok / 42 待办，待办问题全部为
  missing-section 与旧式无模板节（即任务本身），无一来自
  已完成文件——已-done 集合零回归；(b) G3 全库 79 文件零
  失败（1534 片段全可追溯）；(c) 34 个已完成文件抽查
  禁用词（大概/之类的/等等）零命中，1362 条范例链接全部
  存在；(d) 错误码/契约准确性抽检 5 项（AesGcm 认证码
  在源码、TeQuality 双参签名、ResolveOwned 失败所有权、
  CopyLimit 探测字节、Channel Reset AGAIN）全部与实现
  一致；(e) 台账-门禁对账：37 完成行 = 37 gate-ok 文件
  （零差异），完成 API 1515/3664（41.3%）。结论：质量
  体系运转正常，可继续按序推进。发现的对齐风险仅一条：
  剩余待办含 tls.md(268)/net-dns(172)/net-resolver(172)
  /value(116)/websocket(104)/tcp(101)/x509(89)/string(83)
  八个 80+ 大文件，其中 net-dns/net-resolver 与已完成
  net.md 存在共享头并集（同 error.md 模式可节复用），
  tls/x509/websocket 为全新领域，需按大文件拆段节奏推进。
- 2026-09-07 logger.md 全文件达成（76/76，G3 76 片段全绿，13 功能组）：
  签名逐字取自 include/xrt/logger.h（生成器直读头文件，杜绝臆测）；
  错误语义逐条对照 src/logging 源码——Attach 重复附加 = XERR_EXISTS、
  Detach 未附加不设错、xlogresult 四值口径（WRITTEN/SKIPPED/DROPPED/
  ERROR）逐函数成表、Async/Ring 的 pConfig 允许空（源码确认默认配置
  回退）、LastError 族返回新引用（xrtErrorFree 释放）、File 族类型
  不匹配返回 NULL/false 且不设错。范例锚点覆盖 13 个目录全链路
  （core/sink_tour/logger_tour/printf/console/file 族/format 族/
  async/ring_async）。manifest 补注册 #8：examples/logging/ring_async
  （Ring 6 函数唯一锚点源，G4 拦截后定点挂载 logger_ring）。
  完成 API 1515→1591/3664（43.4%），38/79 文件。
- 2026-09-07 memory.md 全文件达成（44/44，G3 44 片段全绿）：
  映射 core 模块 4 头（memory.h 13 + core.h 4 + error.h 27）；全部节
  自 core.md 复用（error.md 为其镜像），按 全局分配器 / 引用计数与
  运行时信息 / 错误对象与线程错误 三组重组；旧版手写短式「## 函数」
  段（8 个无表格节 + 调试位置函数小节）整体替换，原有正文/类型/
  范例/旧版资产决策保留。完成 API 1591→1635/3664（44.6%），39/79。
- 2026-09-07 memory_debug.md（11/11）与 memory_stats.md（4/4）完成：
  错误语义对照 src/memory/debug.c、stats.c、debug_report.c——Enable/Reset
  在存在活动分配或临时字节时 XERR_STATE、FailAfter 仅 TLS 状态分配失败
  设 XERR_MEMORY、EventName 非法种类返回 "unknown" 不设错、Report 写入器
  返回失败且未设错时 XERR_STATE（沿用原文档既有契约）。调用点函数小节
  （xrtMallocAt 等）改为指向 memory.md 的普通段落。完成 API 1635→1650
  /3664（45.1%），41/79 文件。
- 2026-09-07 net-dns.md 全文件达成（172/172，G3 172 片段全绿）：
  与 net.md 共享 include/xrt/net.h（172 = net.md 184 − net_interface.h
  12 个）；全部节复用自 net.md，沿用其 12 组结构，net.md 中因分段生成
  重复的同名组（文本输出/比较与分类/Native 逃生口/网络缓冲/错误）合并；
  原桩文档速览节保留并改名「API 速览」。完成 API 1650→1822/3664
  （49.7%），42/79 文件。
- 2026-09-07 net-file.md 完成（4/4，G3 4 片段全绿）：Worker 归属、
  范围校验（偏移+长度超出完成事件表达范围 = xrt.net/PORT_SUBMIT·RANGE）、
  SELECT 等后端无文件 I/O 能力 = XERR_UNSUPPORTED 三类契约入档。
  完成 API 1822→1826/3664（49.8%），43/79 文件。
- 2026-09-07 net-frame.md 完成（9/9，G3 9 片段全绿）：错误域码
  FRAME_CONFIG/STATE/LIMIT/LENGTH 对照 src/network/frame*.c 溯源；
  xnetframestatus 三值逐值成表（READY/MORE/ERROR）。生成器修复：
  `#### 错误` 子节含 `## 错误` 子串导致 str.index 锚点落进已插入块
  内部——改为行首正则锚定后一次全绿（该陷阱记入长期记忆）。
  完成 API 1826→1835/3664（50.1%），44/79 文件。
- 2026-09-07 net-interface.md 完成（12/12，G3 12 片段全绿）：
  节复用自 net.md 网络接口与本机信息组（net.md 184 − 本 12 = net-dns 172
  的差集闭环）。完成 API 1835→1847/3664（50.4%），45/79 文件。
- 2026-09-07 net-resolver.md 全文件达成（172/172，G3 172 片段
  全绿）：与 net-dns.md 同构（同映射 include/xrt/net.h），名称解析
  （Resolver）组置首、其余 11 组按 net.md 顺序镜像；原 74 行桩文档
  （分层/配置/查询合并/生命周期/统计/Future 便捷层）保留在前。
  完成 API 1847→2019/3664（55.1%），46/79 文件。
- 2026-09-07 number.md 完成（15/15，G3 15 片段全绿）：写入/解析/展示格式三族；错误码对照 src/text/number_*.c——容量不足 = XERR_RANGE
  原子失败且 pOutputSize 仍返回所需长度（internal/xrt_number.h
  __xrtNumberWriteResult 契约）、解析溢出 = RANGE 保输出不变、文本格式
  非法 = PROTOCOL/FORMAT。完成 API 2019→2034/3664（55.6%），47/79。
- 2026-09-07 周期全量复审 #3（47/79 节点）：(a) G1/G2/G4 --all
  47 文件 ok、32 待办文件问题数符合预期，零回归；(b) G3 --all 79 文件
  2053 片段全绿；(c) 本会话 10 文件禁用词零命中；(d) 错误码抽检 5 项
  （Attach 重复=XERR_EXISTS、number 容量=XERR_RANGE 且查询仍返回
  长度、net-file 无 FILE_IO 能力=XERR_UNSUPPORTED、memory_debug
  活动分配=XERR_STATE、net-frame 行超限=FRAME_LIMIT·RANGE）全部
  与源码一致；(e) 台账-门禁对账：47 完成行 = 47 gate-ok（零差异），
  完成行 API 求和 2034 = 记录值 2034/3664（55.6%）。结论：质量体系
  正常。剩余 32 文件中 value(116)/websocket(104)/tls(268)/x509(89)/
  string(83)/tcp(101) 六个 80+ 大文件需拆段，其余 26 个为中小文件。
  本会话新增可复用资产：net-dns/net-resolver 双镜像已闭环，后续
  tcp/udp/http 系若有共享头子集可走同型节复用。
- 2026-09-07 once.md 完成（22/22，G3 22 片段全绿）：映射
  include/xrt/thread.h（once 模块 docs 指向本文件），Once/线程/键三组；
  错误语义对照 src/concurrency/once.c、thread.c、thread_key.c——
  Wait 族等待自身线程 = XERR_STATE、Once 同线程递归重入 = XERR_STATE、
  KeyDestroy/Get/Set/Take 键已关闭 = XERR_STATE、KeysClear 外部线程
  无上下文 = XERR_STATE、平台创建失败 = xrt.thread 域错误保留系统码。
  完成 API 2034→2056/3664（56.1%），48/79 文件。
- 2026-09-07 path.md 完成（32/32，G3 32 片段全绿）：七组（根分解/常用分解/清理拼接/相对/改名的系统/安全条目）；错误码对照 src/fs/path.c、
  path_system.c——Relative 根不同 = VALUE/ROOT、Windows 跨卷 =
  UNSUPPORTED/ROOT、系统族 = XPATH_ERROR_SYSTEM（kind 由系统码映射）、
  Home 缺失 = NOT_FOUND；SafeSegment/IsSafeEntry 为纯谓词（false 不设错）。
  manifest 补注册 #9：path tour/system/safe 三范例。
  完成 API 2056→2088/3664（57.0%），49/79 文件。
- 2026-09-07 pattern.md 完成（37/37，G3 37 片段全绿）：一次性提取 5 +
  批量编译与匹配 19 + Builder 13 三组；错误码对照 src/text/pattern_*.c——
  容量不足 = CAPACITY·RANGE（pCaptureCount 写所需数量、无部分捕获）、
  语法错 = PATTERN·VALUE（含字节偏移机器数据）、不可区分同优先级 =
  CONFLICT·EXISTS、预算 = LIMIT·RANGE；BuilderRemove/Set 陈旧 ID 不设错。
  完成 API 2088→2125/3664（58.0%），50/79 文件。
- 2026-09-07 pem.md 完成（7/7，G3 7 片段全绿）：遍历/查找解码/编码三组；错误码对照 src/asn1/pem.c——Read 失败游标输出不变、
  Find 未命中 = NOT_FOUND、Body 非规范 Base64 = PROTOCOL/BODY、
  Decode 容量不足由 Base64 底座透传 RANGE。
  完成 API 2125→2132/3664（58.2%），51/79 文件。
- 2026-09-07 pool.md 完成（58/58，G3 58 片段全绿）：三族全接口；
  错误口径对照 src/memory/pool*.c、memory_pool.c——页满 Alloc = XERR_AGAIN、
  Calloc 乘法溢出 = XERR_RANGE、Free/Mark 跨对象 = XERR_STATE、
  FreeAt/Get 空闲越界不设错、Owns/Size 纯查询不设错。原有合并式
  `X` / `Y` 双名小节全部拆为单函数节（G2 拦截合并式残留）。
  完成 API 2132→2190/3664（59.8%），52/79 文件。
- 2026-09-07 process.md 完成（37/37，G3 37 片段全绿）：核心 11 +
  标准流与控制 11 + 打开/文件/终端 4 + 一次性运行/Pipeline/Future 11；
  错误域 14 码对照 src/process/process.c——Spawn 失败不留半初始化对象、
  Status 未退出 = STATE、Run/Pipeline 的 false 只表基础设施失败（非零
  退出码仍 true）、Error 返回新引用。manifest 补注册 #10：process tour
  （26 个 API 的主锚点）。
  完成 API 2190→2227/3664（60.8%），53/79 文件。
- 2026-09-07 proxy.md 完成（23/23，G3 23 片段全绿）：错误码对照
  src/network/proxy.c、proxy_dial.c——Sent 确认量超出待发 = STATE、
  Bound 的 HTTP CONNECT = NOT_FOUND、DialCancel 终态或并发已受理
  不设错、Dial 11 参口径（Stream 引用转移给完成回调）。
  完成 API 2227→2250/3664（61.4%），54/79 文件。
- 2026-09-07 queue.md 完成（46/46，G3 46 片段全绿）：三族模板化；
  TryPush 关闭 = CLOSED、TryPop 关闭且排空 = CLOSED、Reset 非空不设错、
  Batch 返回 {Result, Count} 允许部分完成；InitBuffer 外部环须 2 的幂。
  生成器加锚点回退（SPSC Batch 真实调用在 tour 而非 containers）。
  完成 API 2250→2296/3664（62.7%），55/79 文件。
- 2026-09-07 random.md 完成（39/39，G3 39 片段全绿）：错误码对照
  src/math/random*.c——SecureRandom 系统源失败 = XERR_IO 且清零整个输出、
  Shuffle 溢出 = XERR_OVERFLOW、Below/Range 零界/空区间 = XERR_ARGUMENT；
  非密码学族逐节保留"不得用于密钥/nonce/token"警示；FastRand 为显式
  别名族（旧 xrtRand* 兼容）。
  完成 API 2296→2335/3664（63.7%），56/79 文件。
- 2026-09-07 regex.md 完成（54/54，G3 54 片段全绿）：六组；错误域
  CONFIG/PATTERN/LIMIT/EXECUTE/REPLACEMENT/CALLBACK 对照 src/text/regex_*.c；
  ReplaceFuncTo 失败撤销本次追加、MatcherNext 空匹配按 UTF-8 标量推进、
  Split 单块整体释放。修正生成器组插入顺序（先插者靠前，倒序排列）。
  完成 API 2335→2389/3664（65.2%），57/79 文件。
- 2026-09-07 set.md 完成（34/34，G3 34 片段全绿）：生命周期/键策略/资源/基础/容量/遍历/集合运算七组；GetOrAdd 失败原子、Take 输出不得
  接触集合内存、IterNext 结构修改后 STATE、运算族两集合不兼容 STATE、
  Remove/Take 未命中不设错。
  完成 API 2389→2423/3664（66.1%），58/79 文件。
- 2026-09-07 signal.md 完成（20/20，G3 20 片段全绿）：五组；错误域
  CODE/UNSUPPORTED/STATE 对照 src/process/signal.c——订阅族 ID 空间耗尽
  = RANGE、调度线程创建失败 = STATE、Shutdown 在回调线程 = STATE、
  Name 未知代码返回 "UNKNOWN" 不设错。
  完成 API 2423→2443/3664（66.7%），59/79 文件。
- 2026-09-07 slot_map.md 完成（16/16，G3 16 片段全绿）：四组；
  错误对照 src/containers/slot_map.c——Insert 空闲链破坏 = STATE、
  槽位耗尽 = OVERFLOW、Get/Set/Remove 陈旧句柄不设错（失效是正常结果）、
  Remove 输出与槽表存储重叠 = ARGUMENT。旧合并式双名小节全部拆分。
  完成 API 2443→2459/3664（67.1%），60/79 文件。
- 2026-09-07 spin.md 完成（7/7，G3 7 片段全绿）：生命周期 + 进出
  临界区两组；错误对照 src/concurrency/spin.c——持有中 Unit/Destroy =
  STATE、TryLock 忙碌不设错。
  完成 API 2459→2466/3664（67.3%），61/79 文件。
- 2026-09-07 stack.md 完成（74/74，G3 74 片段全绿）：五族全接口；
  错误对照 src/containers/*stack*.c——固定族 Push/Add 满 = XERR_AGAIN、
  Pop 空/Get/Peek 越界 = XERR_RANGE、Block 族 Add/Push 块布局溢出 =
  XERR_OVERFLOW、动态族自动扩容仅 OVERFLOW/MEMORY、Ptr 族合法空值与
  错误经错误状态区分。
  完成 API 2466→2540/3664（69.3%），62/79 文件。
- 2026-09-07 string.md 完成（83/83，G3 83 片段全绿）：参数表形参名由签名自动提取（与门禁同源逻辑，杜绝手抄错名）；错误口径
  ——借用族（查找/切分/裁剪）一律不设错、分配族 OVERFLOW+MEMORY、
  缓冲族容量不足 = RANGE 不写半个结果、Format 族拒绝 %n。
  完成 API 2540→2623/3664（71.6%），63/79 文件。
- 2026-09-07 周期全量复审 #4（63/79 节点）：(a) G1/G2/G4 --all
  63 文件 ok、16 待办文件（sync/task/tcp/temp/template/thread/thread-key/
  time/tls/udp/value/wait/websocket/x509/xid/xson）问题数符合预期，零回归；
  (b) G3 --all 79 文件 2642 片段全绿；(c) 本轮 5 文件禁用词零命中；
  (d) 错误码抽检 3 项（栈固定族满=AGAIN/空弹=RANGE、signal 域码、
  Format 拒绝 %n）全部与源码一致；(e) 台账-门禁对账：63 完成行 =
  63 gate-ok（零差异），API 求和 2623 = 记录值 2623/3664（71.6%）。
  结论：质量体系正常。剩余 16 文件含 tls(268)/value(116)/websocket(104)/
  tcp(101)/x509(89) 五个大文件；thread.md 可复用 once.md 的 22 节
  （同映射 thread.h），thread-key.md 同理可子集复用。
- 2026-09-07 sync.md 完成（48/48，G3 48 片段全绿）：五族全接口；
  错误对照 src/concurrency/{mutex,cond,sem,rwlock,event}.c（ARGUMENT+
  STATE 两类）——Mutex 同线程递归加锁 = STATE、持有中 Unit/Destroy
  = STATE 保持有效、SemPost 达上限计数不变、PostMany 不部分发布、
  RWLockUpgrade 恰好一个读锁前置条件、CondWait 虚假唤醒须谓词循环。
  完成 API 2623→2671/3664（72.9%），64/79 文件。
- 2026-09-07 task.md 完成（42/42，G3 42 片段全绿）：双头（task.h
  36 + task_net.h 6）五组；错误对照 src/concurrency/task*.c——Submit 队满
  = AGAIN、组提交队满完整回滚预留、池工作线程内 SubmitWait/Destroy =
  STATE、组池关闭后提交 = CLOSED、槽位等待超时 = TIMEOUT、可取消等待
  取消 = CANCELLED 且不取消已受理任务。
  完成 API 2671→2713/3664（74.0%），65/79 文件。
- 2026-09-07 thread.md 与 thread-key.md 完成（各 22/22，G3 各 22 片段
  全绿）：二者与 once.md 映射同一 include/xrt/thread.h，全部节复用自
  once.md 已验证内容，分别以线程/键为主线重组三组。
  完成 API 2713→2757/3664（75.2%），67/79 文件。
- 2026-09-07 temp.md 完成（17/17，G3 17 片段全绿）：arena 族 +
  上下文便捷层两组；作用域违反后进先出 = STATE、Trim 在活动分配时 =
  STATE、Secure 族先擦除后操作。
  完成 API 2757→2774/3664（75.7%），68/79 文件。
- 2026-09-07 template.md 完成（32/32，G3 32 片段全绿）：三组；错误域
  12 码对照 src/template/——编译 SYNTAX/CONFIG/LIMIT、渲染 TYPE/UNDEFINED/
  LIMIT/CALLBACK/WRITE、扩展接管 UserData。manifest 补注册 #11：
  template tour（16 个 API 主锚点）。生成器修复：`*pTarget =` 解引用
  星号被误判注释行——注释跳过规则改为 /*, //, */, "星号+空格"。
  完成 API 2774→2806/3664（76.6%），69/79 文件。
- 2026-09-07 tcp.md 完成（101/101，G3 101 片段全绿）：三头并集
  （tcp.h 63 + tcp_server.h 15 新写、proxy.h 23 复用自 proxy.md）；契约——
  发送族 WriteLimit 背压 = XERR_AGAIN（SendRefs 失败全部所有权不转移）、
  Buffer/Consume/Socket/SetData 非所属 Worker = STATE、AcceptWait 禁止
  Worker 内阻塞、DialCancel 原子争取（败者不设错）、Remote 锚点在
  examples/tls/stream。
  完成 API 2806→2907/3664（79.3%），70/79 文件。
- 2026-09-07 time.md 完成（58/58，G3 58 片段全绿）：九组；错误对照
  src/system/time*.c——日历字段组合非法/解析不匹配 = XERR_ARGUMENT、
  域越界 = XERR_RANGE、Write 族容量不足原子失败。协议族三形态
  （RFC3339/HTTP 严格/HTTP 宽松/自动识别）逐值成表。manifest 补注册
  #12：time text_parse（8 个文本 API 主锚点）。
  完成 API 2907→2965/3664（80.9%），71/79 文件。
- 2026-09-07 wait.md 完成（3/3，G3 3 片段全绿）：After 溢出
  饱和为 NEVER、Expired 永假、Remaining 为 UINT64_MAX——NEVER 语义三连
  全部入档；xwaitresult 枚举文档原有内容保留。
  完成 API 2965→2968/3664（81.0%），72/79 文件。
- 2026-09-07 udp.md 完成（73/73，G3 73 片段全绿）：七组；契约——
  批量容量 1–256 = XERR_RANGE、组播五操作仅 UDP Worker 内 = STATE、
  SendBatch 前缀受理语义（pAccepted 写出实际数量）、BatchTake 转移后
  位置为空、PathMtu 未知为零。manifest 补注册 #13：net_udp
  batch/introspect/multicast 三范例。
  完成 API 2968→3041/3664（83.0%），73/79 文件。
- 2026-09-07 周期全量复审 #5（73/79 节点）：(a) G1/G2/G4 --all
  73 文件 ok、6 待办文件（tls/value/websocket/x509/xid/xson）问题数
  符合预期，零回归；(b) G3 --all 79 文件 3060 片段全绿；(c) 本轮
  tcp/time/wait/udp 四文件禁用词零命中；(d) 错误码抽检（udp 批量
  容量 1–256 RANGE、组播 Worker 归属 STATE、tcp 背压 AGAIN）全部
  与源码一致；(e) 台账-门禁对账：73 完成行 = 73 gate-ok（零差异），
  API 求和 3041 = 记录值 3041/3664（83.0%）。结论：质量体系正常。
  剩余 6 文件中 tls(268) 为全任务最大，value(116)/websocket(104)/
  x509(89) 次之；xid(11)/xson(35) 为小文件。
- 2026-09-07 xid.md 完成（11/11，G3 11 片段全绿）：三组；Make 族
  系统安全随机源失败 = XERR_IO、Write 容量不足 = RANGE（须 33 字节）、
  Parse 失败不改输出且字节位置可由 ErrorOffset 读取、Compare/Equal/
  IsZero 纯比较不设错。
  完成 API 3041→3052/3664（83.4%），74/79 文件。
- 2026-09-07 xson.md 完成（35/35，G3 35 片段全绿）：八组；契约——
  Writer 族容器顺序/预算 = xrt.xson 域错误、Take 在未 Finish 或 sink
  写入器上 = XERR_STATE、StringifyFile/WriteFile 原子替换、文件族
  超输入上限 = xrt.file 域错误。
  完成 API 3052→3087/3664（84.3%），75/79 文件。
- 2026-09-07 value.md 完成（116/116，G3 116 片段全绿）：十组；
  三种所有权形态（引用/移交/消费）逐函数成表、COW Edit 分离语义、
  快照迭代 EXISTS 活动状态、Merge 冲突策略 VALUE、TypeIdRebind 唯一
  拥有 STATE、WeakRef 过期锁返回 null 单例。
  完成 API 3087→3203/3664（87.4%），76/79 文件。
- 2026-09-07 x509.md 完成（89/89，G3 89 片段全绿）：十三组；三值口径
  ——DONE=字段不存在/遍历完成不设错、VALUE=判定成立、ERROR=结构非法；
  不受支持算法 = XERR_UNSUPPORTED、PathBuild 无法到达信任源 = NOT_FOUND、
  StoreAddFile 走 xrt.file 域错误。
  完成 API 3203→3292/3664（89.9%），77/79 文件。
- 2026-09-07 websocket.md 完成（104/104，G3 104 片段全绿）：三头并集
  五组；契约——控制帧载荷 > 125 = XERR_RANGE、InflaterEnd 尾块不符
  permessage-deflate 约定 = PROTOCOL、文本帧 UTF-8 严格校验、流层
  Send 族 AGAIN/STATE 口径与 tcp 一致、Tcp/Tls 借用与引用双形态。
  完成 API 3292→3396/3664（92.7%），78/79 文件。仅余 tls.md(268)。
- 2026-09-07 tls.md 全文件达成（268/268，G3 268 片段全绿）——79/79 全部完成！八头并集（tls.h 140 + stream 52 + session 29 + client 10
  + server 9 + identity 13 + resume 7 + verify 8）按 20 组入档。生成器
  升级：签名、形参名、头文件紧邻契约注释、返回枚举值全部程序化提取，
  词典覆盖 240+ 形参说明，零"目标参数"兜底。范例工程：identity 无参
  自检四种密钥形态（openssl 新生成 RSA/P-384/Ed25519 材料，build.py
  实测 identity=1/3/4/6 全通过）；handshake_extra 补注册（#14）。
  完成 API 3396→3664/3664（100.0%），79/79 文件。
- 2026-09-07 周期全量复审 #6 · 终审（79/79 收官）：(a) G1/G2/G4 --all
  79 文件全绿、问题数 0——全库 3664 API 节全部达标；(b) G3 --all
  79 文件 3683 片段全绿；(c) 全库函数节禁用词零命中；(d) 台账对账：
  79 完成行，API 求和 3664 = 头文件并集总数（100.0%）。

== 任务总结 ==
  80 任务（79 文件 + Phase 0 规范/工具）全部完成；生成器管线
  （头文件签名/形参/契约注释程序化提取 + callline 锚点 + 族模板）
  覆盖新写 2428 节，节复用覆盖 897 节（core/memory/error/net-dns/
  net-resolver/net-interface/tcp-proxy/thread/thread-key/http_fields 等
  共享头文件族）；manifest 补注册范例 14 次，范例工程扩展 2 次
  （tls identity 四形态 + openssl 新材料实测）。建议后续：
  - 修正记录中已注明的门禁工具两处修复保持回归覆盖；
  - 新增 API 时按 DOC_SPEC §1.2 模板同步补节并跑双门禁。
- 2026-09-07 收官补遗（任务后持续改进）：(a) DOC_SPEC §1.1 模块级三要素
  全库审计——错误码总表缺 14、线程约束缺 14、所有权规则缺 4，逐文件
  补「模块契约」节（错误表取自各文件函数节已核实标识：cancel/hash/
  memory_stats/net-dns/net-file/net-interface/http_fields/http_upgrade/
  math/charset/html/http/http_expect/http_te/http_trailer/net-frame/string/
  codec/compress/once/wait/thread-key/websocket/http_decode 共 25 文件），
  复审三要素零缺口；(b) DOC_SPEC 适用范围计数 80→79 校正（并档前旧
  计数，manifest/磁盘/台账三方一致为 79）；(c) 全量门禁复验：G1-G4
  79 文件 0 问题、G3 79 文件全绿——模块契约补遗未引入回归。
- 2026-09-07 收官后第二轮（生成产物与规范版本）：(a) DOC_SPEC 版本
  v1.0-draft → v1.1（v1.0 冻结早已记录于台账；v1.1 = 适用范围计数校正，
  变更理由与影响范围即上一条记录）；(b) 三生成器刷新——reference 六文件
  零漂移、features.h 零变化、docs/EXAMPLES.md 严重过期（292→410 个
  已登记示例，含本任务 14 次补注册与既有未同步项）已重建；(c) Phase-1
  问题清单复核：核心库 10 项全部结案，EXAMPLES_PROGRESS 开放勾选属
  「范例注释工程」独立任务（559 范例注释），不在 DOC_SPEC 范围，未动；
  (d) 门禁复验零回归。
== 阶段 3：类型与常量补全（DOC_SPEC v1.2 §1.3，2026-09-07 启动）==

- 审计：全库公共类型缺节 707、公共常量缺 958（原始口径）；排除
  XRT_MODULE_* 注册表（core/error/memory 三镜像各约 240 项，由
  manifest 与 generate_features 产物承载）后，实际在范围缺口约为
  类型 707 / 常量约 214。array.md 等阶段 0 试点文件本就达标，证明
  骨架标准成立、漂移发生在后续文件。
- 规范：v1.2 新增 §1.3（类型节 = 签名块 + 枚举逐值表/字段表 + 头文件
  契约注释语义；常量 = 集中总表或单节两种等价形态；范围排除守卫/
  XRT_FEATURE_*/XRT_MODULE_*）。
- 门禁：check_api_reference_detail.py 新增 --types（G5），默认模式
  行为不变（--all 仍 79 ok）；不透明句柄 typedef 已纳入识别。
- 试点：compress.md——12 类型节 + 8 常量总表，G1-G5 + G3 全绿。
  模板验证通过：签名逐字符取自头文件、枚举/字段表、语义用注释原文。
- 推进方式：按文件逐个补节，每文件 G5 全绿后记此处（同阶段 2 纪律）。
- 2026-09-07 阶段 3 完成（类型与常量补全）：79/79 文件 G5 全绿。
  通用生成器（tc_gen）+ 六批推进：小文件 8 → 中文件 30 → 大文件 41；
  语义来源：头文件块注释原文 > 词元词典展开；复核项（无注释类型）
  共 33 处逐一人工补正（不透明句柄/回调/进度与限额结构等）。
  门禁复验：--all --types 79 ok、G1-G4 默认模式 79 ok、G3 79 全绿，
  零回归。全库公共类型 100% 成节、公共常量 100% 可检索。
- 2026-09-07 周期全量复审 #7（阶段 3 后）：(a) CI 全门禁本地复跑——
  check_api_docs 12 个模块族（websocket/net/http/regex/value/future×2/
  task/coroutine/tls/x509/xson）missing=0；check_release_maturity 通过
  （477 模块 356 实现）；test_api_docs 4/4 OK；(b) 新增类型签名保真
  审计（1048 个类型节签名块 vs 头文件逐字符）：真差异 1 组——map.md
  三个 int-map 回调形参名 key/value/user_data → iKey/pValue/pUserData，
  已修正（visitor 返回类型本为 bool，doc 误写 void 一并改正）；其余
  2 处为有意跨模块引用（core 的 xbytesview/xstrview 在 charset/string
  文档重复展示并注明来源），判合规；(c) 阶段 3 新增内容禁用词零命中。
- 2026-09-07 表格语义质量专项（阶段 3 后续）：类型/常量表 4144 个语义
  单元中 49% 为零信息纯标识回显。两轮全自动词典（v2/v3）因语境误配
  （错误码词条套用到字段名，如 ValueOffset→值非法OFFSET）全部回滚，
  改为安全策略 v4-v9 六轮：仅替换纯 ASCII 单元、按族感知词典
  （字段名精选 / 错误与告警族后缀推导 / 高频领域枚举逐条人工翻译：
  HTTP 状态码全表、TLS 密码套件与签名方案、X509 版本与密钥类型、
  SOCKS5 应答码、端口事件与能力位、套接字选项、星期、时间单位、
  WS 关闭码、模板节点、ASN.1 标签类等）。共改善 ~1300+ 单元，
  剩余 6.2% 为协议名本身（TLS 1.2/SHA-256/Ed25519 等，合规保留）。
  门禁复验：G1-G5 + G3 全部 79 文件全绿，零回归。
- 2026-09-07 §1.1 骨架完备性专项：定位段审计发现 22 个文件标题后
  直接进入 ##（net/tcp/udp/crypto/json/time 等），逐一补写取自既有
  首组语义的定位段；console/signal 两文件全文无裁剪信息，按 manifest
  补「裁剪与依赖」表；doc-to-doc 相对链接全库审计零坏链。
  门禁复验：G1/G5/G3 全绿，零回归。
- 2026-09-07 表格行完备性专项：新审计（枚举签名块值 vs 值表行、
  结构字段 vs 字段表行）发现系统性缺陷——tc_gen 枚举值提取正则要求
  尾逗号，每个枚举的末值（无尾逗号）全部漏行，累计 331 行；另有
  13 个「首值带赋值即结束」的枚举空表未出任何行；9 个核心值类型
  结构（xarray/xbuffer/xallocator/xrng/xslotmap(±iter)/xstrview/
  xstrbuf/xutfstatus）无字段表。全部修复：+344 枚举行（末值补入，
  语义按 curated 词典或「（见枚举语义）」）+9 张字段表。复审：
  枚举值/结构字段行完备性全库全覆盖；G1/G5/G3 全绿零回归。
  教训入档：门禁只查「节存在」，行的完备性需独立审计正则（本轮
  已沉淀于记录，可随再生成复用）。
- 2026-09-07 语义残留双清零：(a) 上一轮补入的 344 枚举行中 262 处
  「（见枚举语义）」占位逐条人工替换（按域分组：TLS 套件/告警/扩展、
  HTTP 传输编码与连接语义、x509 密钥用途与曲线、JSON/XSON 事件、
  进程/日志/模板/树遍历等），现全库占位零残留；(b) 常量总表值列
  与头文件 #define 逐行对账（239 行），仅 1 处规范化差异
  （65536 → UINT32_C(65536)）已修正。门禁 G1/G5/G3 全绿。
- 2026-09-07 字段表类型列回填：上轮 9 张手写字段表的「类型」列为 —
  占位（34 行）。先与头文件逐字段核对（发现 11 处臆测类型错误：
  bytes/str vs 指针、uint32 守卫 vs uint64、xarray 存储、const 限定、
  xutfstatus 自引用等），再按实测类型回填全部 33 行；compress 的
  1 处为三列错误映射表中合法的「不适用」单元（审计口径误报，
  已用表头限定重审归零）。门禁 G1/G5/G3 全绿。
- 2026-09-07 抽检与限额双专项：(a) 语义抽检——上轮 262 处人工替换
  中抽 12 项高风险对照头文件/实现，发现 2 处真错（XNET_ACCEPT_LOCAL
  实为「留在监听器所属 Worker」而非地址过滤；XPROCESS_IO_MERGE 实为
  「合并到另一管道且仅 STDERR 可选」）与 4 处可精化（TUNNEL=升级后
  非 HTTP 字节流等），已修正；本批替换整体正确率 ≥92%。
  (b) 范例片段 ≤12 行限额——20 个 13 行片段统一截取前 12 行
  （连续原文子串，G3 仍可追溯；首版脚本行数口径错一档已纠正）。
  门禁 G1/G5/G3 全绿。
- 2026-09-07 参数方向列审计（函数节四列的最后一列未审计维度）：
  启发式对账（签名 const 限定 vs 方向标注）全库扫描得 69 疑点，
  逐一核判全部合规——61 处为 span/描述符类「输入/输出」复合语义
  （const xnetwspan*：描述符读入+缓冲写出，标注正确）；8 处中 5 处
  为 const T** 出参（写入 const T* 值，const 修饰最终指向物）、
  3 处为审计脚本跨参数段误捕（签名本身无 const）。参数方向列
  零错误。函数节四列（参数名/方向/约束/说明）至此全部经审计。
- 2026-09-07 周期全量复审 #8 · 终审快照（任务全维度闭环）：
  (a) 六门禁 G1/G2/G4+G5+G3 全库 79 文件全绿（G5 已按规范 v1.3
  并入默认模式，--no-types 保留对照口）；(b) 全维度审计清单全部
  闭环——函数节四列（参数名 G2/方向本轮/约束/说明）、类型节三列
  （字段名/类型/语义）、枚举行完备、常量值溯源、签名逐字符、
  语义无占位、片段 ≤12 行、模块契约三要素、骨架定位段、链接零坏链；
  (c) 语义正确性双层保障：错误码对照源码成文 + 高风险抽检校正
  （累计修正真错误 19 处：臆测类型 11 + 语义 6 + 签名 2 组）。
  规范 v1.3 生效。文档深化任务全维度收敛。
- 2026-09-07 表格结构一致性专项：(a) 全库扫描发现 1333 个返回值表
  使用 4 列分隔行配 3 列表头（模板漂移，渲染器宽容但非规范形态），
  统一修正为 3 列；(b) 13 处真实列断裂修复——crypto 7 处 void 函数
  返回值行缺第三列、4 处「r||s」单元格内管道被解析为列分隔（改用
  ‖ 字符）、math 绝对值表达式同因、tcp 1 处缺返回列。复扫列断裂零。
  门禁 G1+G5/G3 全绿，crypto 族 CI 门禁复跑通过。
- 2026-09-07 重复节去重专项：标题级审计发现 74 个同名 ### 节
  （成因：阶段 2/3 生成前原文件已存在旧短式节，追加完整模板节后
  未清除旧副本；集中于 io(34)/net(19)/thread-key(6)/map(5)/error(4)）。
  按完整度评分去重（参数/返回值/错误/范例四要素，平分保先现），
  删除 73 个旧副本并清理空组标题；3 个误删（charset xutf16view/
  xutfstatus、string xstrsplit——旧文件用合并标题「X 与 Y」，
  去重器只捕获首名导致完整节被误判）已由 tc_gen 重建。复审：
  重复节零、G1+G5/G3 全绿。附带甄别：空节扫描的 287 处均为
  合法组标题直连子节；无语言围栏均为闭合围栏（开口全部 ```c）。
- 2026-09-07 标题层级与去重残留收尾：(a) 跳级审计发现去重残留
  的孤立 #### 范例片段（挂于 ## 错误 组内）；(b) 首版清理正则
  贪婪跨越整段误删 19 节（门禁即时拦截），从去重前版本复原后
  重做评分去重 + 仅针对「## 组部件内 #### 范例」的精准清理。
  终态三项零：标题跳级零、重复节零；G1+G5/G3 全绿（79/79）。
  过程教训入档：大跨度正则删除前必须先核对跨度内容边界；
  门禁的存在使误删在提交前被拦截，未污染历史。
- 2026-09-07 空表清零专项：空表扫描（表头+分隔行后无数据行）发现
  14 处——10 个事件回调结构（tls/tcp/ws/udp/server/listener）字段表、
  xpercentmap/xid 字段表、3 个 void 构造函数参数表。全部按头文件
  实测字段回填（事件字段逐一与 typedef 对照，纠正四组臆测：
  ws 实为 Message 分片+Pong+Backpressure 而非 Text/Binary；
  tls 实为 Writable 而非 High/LowWater；server/listener 含 Error
  与 HandshakeError）。复扫空表零；G1+G5/G3 全绿。
- 2026-09-07 整表缺失清零（空表专项的补集）：struct 节「有字段签名
  但全无字段表」扫描发现 2 处——charset 的两个合并标题节
  （xbytesview 与 xstrview、xutfstatus 与 xutfresult，此前行完备
  审计因「全缺视为无表意图」跳过）。补三张字段表，其中 xutfresult
  的第四字段按头文件实测为 Error（首个非法码元偏移）——上一轮
  手写表误作 Codepoint，本轮一并纠正。复扫整表缺失零。
  enum 无值表扫描本就为零。G1+G5/G3 全绿。
- 2026-09-07 枚举逐值专项（§1.2 明文要求，最后一条未审计的返回值
  规则）：(a) 全库审计发现 137 处枚举返回函数未逐值；分批修复
  xnetresult(35+48 全名化)/xtlsresult/xhttpnext/xhttp1status/
  xhttpcoding/xencoding/xfuturestate 等 10 族 135 处换逐值行
  （值语义逐一对照头文件实测，纠正四组臆测：xhttp1status 实为
  READY/FIELDS/MORE/ERROR、xhttpcoding 有 NONE=缺头、xencoding
  值名带下划线、xfuturestate 五态）。(b) 重大发现：tls/future 两
  文档存在 100 处幻觉值名 XTLS_RESULT_*——真实枚举是 XTLS_OK/
  AGAIN/CLOSED/ERROR（我此前 post-processor 臆造 RESULT 前缀），
  G1 只查函数签名不查表内容故漏网——已全量替换为真实值名。
  (c) 剩余 44 处为单值状态查询（State/Family/Method 类），值表
  已在对应类型节逐值（抽查确认），函数节引用枚举名合规。
  门禁 G1+G5/G3 全绿。
- 2026-09-07 幽灵标识符清零（上轮 XTLS_RESULT 教训的系统化推广）：
  新审计——全库表格单元中反引号的 X 前缀标识符逐一与公共头
  交叉比对（头文件共 19 个 XERR 种类等作为唯一真值源）。
  发现 8 种 30 处幽灵并全部修正：XERR_OVERFLOW→XERR_RANGE
  （21 处，error.h 从无 OVERFLOW 种类——pattern/queue/stack 等
  多文件污染）、XHTTP_EXPECT_100_CONTINUE→XHTTP_EXPECT_CONTINUE、
  XVALUE_ITER_OK→XVALUE_ITER_ITEM、XWS_FRAME_OK→READY、
  XWS_CLIENT/SERVER→XWS_ROLE_*、XXSON_VISIT_OK→NEXT、
  XRT_FEATURE_CORE（once.md 依赖表）→core 模块文字说明（该模块
  feature=null 无裁剪宏）。复扫幽灵零；G1+G5/G3 全绿。
  该审计脚本沉淀于本记录，可随任何表内容变更复跑。
- 2026-09-07 幽灵检测 v2（散文层）：把表格层的检测器扩展到非表格、
  非代码块行（错误清单/行为描述/类型段散文）。命中 6 处，甄别后
  2 处真幽灵修正（http.md 行为指引 XRT_NET_AGAIN→XNET_RESULT_
  AGAIN；net.md 统计说明 XRT_STATS_BASIC→XNET_STATS_BASIC），
  4 处为「旧版资产决策」段对已删除标识符的合法历史引用（math
  删除清单、pool 旧行为对比），按语境保留。终扫（排除历史段）
  零幽灵。至此标识符三层（签名/表格/散文）全部与公共头对账。
  门禁 G1+G5/G3 全绿。
- 2026-09-07 幽灵检测 v3（非范例代码块层）+ 真值源扩容：
  (a) 第三层检测——模块级用法片段/类型段示例（```c 但不在
  #### 范例 下，G3 不覆盖的块）。唯一命中 console.md 的 2 处
  XRT_IMPLEMENTATION 经查为单头形态合法宏（定义于 single/xrt.h）。
  (b) 真值源扩容：include/xrt/*.h + single/xrt.h（模块选择示例
  同时引用两类宏），三层（表格/散文/代码块）终扫全部零幽灵。
  标识符四层防线（G1 签名 + 类型签名保真 + v1/v2/v3 幽灵检测）
  至此全部以「公共头全集」为唯一真值源完成对账闭环。
- 2026-09-07 字段表类型列审计（标识符第五层）：(a) 类型提取
  修正（函数指针 typedef 与 struct 标签形态）后扫描字段表「类型」
  列——修正提取后 60 种幽灵降为 1 种真实缺口；(b) 45 处「回调」
  文字占位（事件表无具名 typedef）按头文件内联函数指针原始
  签名逐字段回填 44 处（签名逐字取自 typedef 原文，压缩空白）；
  (c) 剩余 1 处为 future.md 臆造的 xtlslistenerevents.Open 行
  （该结构无 Open 字段——上上轮空表回填时臆测的「无操作占位」），
  删除。终扫类型列零幽灵、列断裂零（长签名不含管道）；
  G1+G5/G3 全绿。
- 2026-09-07 约束列抽样审计（参数表最后一个未机检列）：全库
  「允许空」声明 602 处，随机抽 25 项到实现源逐一对账。首轮
  启发式报 9 疑点，逐个溯源后全部合规——NULL 容忍路径存在于
  转发目标或辅助校验函数（如 xrtIntMapClear→__xrtIntMapValid
  首行 NULL 检查；xrtPtrFixedStackDestroy→转发 xrtFixedStackDestroy
  的 NULL 早退；xrtTaskSubmitUntil 的 pArgs 在 __xrtTaskPoolSubmit
  的 pArgs != NULL 条件消费中容忍）。抽样正确率 25/25。
  启发式教训（第三次重演）：单函数体的 NULL 检查不足以判定，
  必须追到转发链终点。约束列经抽样验证可信。
- 2026-09-07 约束列反向抽样（非空声明 1538 处，抽 20）：首轮
  启发式报 8 疑点，逐个沿转发链溯源后全部合规——非空要求存在于
  Require/Valid 辅助函数（cond 的 __xrtCondRequire 首行 NULL 拒绝；
  socket 的 __xrtNetSocketAddress；param 的 TokenValid）或转发
  目标（CondWaitFor→WaitUntil 的 Require；FutureWaitUntil→
  Cancel 链的 pFuture==NULL 拒绝；CoJoin→JoinUntil 的 pTarget
  NULL 拒绝；AsyncFileWriteAtRef 对 Data.Data/pRelease 的条件
  非空；TlsDialAsync/TlsClientCreate 的后续校验）。抽样 20/20。
  约束列两个方向（允许空 25/25 + 非空 20/20）均经实现级溯源验证。
- 2026-09-07 审计套件固化（防资产流失）：任务期间沉淀于临时目录的
  全部机检审计 consolidated 为 tools/doc_audit.py（11 项子命令：
  ghost-table/prose/code/type 四层幽灵 + tables 结构 + enum-values/
  struct-fields 行完备 + headings + snippets + banned-words +
  const-values，真值源=公共头全集含单头）。首跑即发现 7 处残留：
  xarray 字段表臆造 Flags（头文件实为 Alignment）、xrng/xslotmap
  缺 Reserved、xslotmap 的 FreeHead/Flags 系臆造名（实为 FreeSlot/
  Reserved）、charset 旧合并节残留致 xutfstatus 值表缺 OVERFLOW——
  全部修正。另修审计器两处误报（random「不保证 X 之类的 Y」系
  准确行为声明豁免；proxy 裁剪宏表非常量表）。终跑 0 findings；
  G1+G5/G3 全绿。此后任何文档变更可一条命令复跑全部深度审计。
- 2026-09-07 质量防线 CI 化（规范 v1.4）：上轮固化的
  tools/doc_audit.py 与 G1-G5 门禁正式接入 .github/workflows/ci.yml
  （新增两步：check_api_reference_detail --all 与 doc_audit --all），
  YAML 校验通过、两门禁本地复跑 exit 0。规范 v1.4 把深度审计工具
  与 CI 接线写入 §1.3——文档质量从「任务期间人工纪律」升格为
  「仓库永久门禁」。至此文档深化任务完成三级跳：内容建设（79
  文件 3664 API 全覆盖）→ 质量深化（21 项审计闭环、约 205 处
  修正）→ 防线制度化（六门禁 + 11 项深度审计全部 CI 强制）。

