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
| 49 | path.md | 32 | 待办 |  |
| 50 | pattern.md | 37 | 待办 |  |
| 51 | pem.md | 7 | 待办 |  |
| 52 | pool.md | 58 | 待办 |  |
| 53 | process.md | 37 | 待办 |  |
| 54 | proxy.md | 23 | 待办 |  |
| 55 | queue.md | 46 | 待办 |  |
| 56 | random.md | 39 | 待办 |  |
| 57 | regex.md | 54 | 待办 |  |
| 58 | set.md | 34 | 待办 |  |
| 59 | signal.md | 20 | 待办 |  |
| 60 | slot_map.md | 16 | 待办 |  |
| 61 | spin.md | 7 | 待办 |  |
| 62 | stack.md | 74 | 待办 |  |
| 63 | string.md | 83 | 待办 |  |
| 64 | sync.md | 48 | 待办 |  |
| 65 | task.md | 42 | 待办 |  |
| 66 | tcp.md | 101 | 待办 |  |
| 67 | temp.md | 17 | 待办 |  |
| 68 | template.md | 32 | 待办 |  |
| 69 | thread-key.md | 22 | 待办 |  |
| 70 | thread.md | 22 | 待办 |  |
| 71 | time.md | 58 | 待办 |  |
| 72 | tls.md | 268 | 待办 |  |
| 73 | udp.md | 73 | 待办 |  |
| 74 | value.md | 116 | 待办 |  |
| 75 | wait.md | 3 | 待办 |  |
| 76 | websocket.md | 104 | 待办 |  |
| 77 | x509.md | 89 | 待办 |  |
| 78 | xid.md | 11 | 待办 |  |
| 79 | xson.md | 35 | 待办 |  |

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

