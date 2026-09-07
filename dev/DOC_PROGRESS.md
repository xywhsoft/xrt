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
| 18 | file.md | 97 | 进行中 [1/3 段] | 第 1 段 IO/打开/定位/整文件/文本 36/97 全绿（G3 36 片段）；余锁/映射/临时/目录 36 + 遍历/树/链接/FIFO/根 25 |
| 19 | file_async.md | 34 | 待办 |  |
| 20 | future.md | 104 | 待办 |  |
| 21 | hash.md | 9 | 待办 |  |
| 22 | html.md | 3 | 待办 |  |
| 23 | http.md | 167 | 待办 |  |
| 24 | http_connection.md | 5 | 待办 |  |
| 25 | http_decode.md | 10 | 待办 |  |
| 26 | http_encoding.md | 12 | 待办 |  |
| 27 | http_expect.md | 8 | 待办 |  |
| 28 | http_fields.md | 64 | 待办 |  |
| 29 | http_te.md | 10 | 待办 |  |
| 30 | http_trailer.md | 6 | 待办 |  |
| 31 | http_upgrade.md | 10 | 待办 |  |
| 32 | io.md | 42 | 待办 |  |
| 33 | json.md | 30 | 待办 |  |
| 34 | list.md | 28 | 待办 |  |
| 35 | logger.md | 76 | 待办 |  |
| 36 | map.md | 62 | 待办 |  |
| 37 | math.md | 20 | 待办 |  |
| 38 | memory.md | 44 | 待办 |  |
| 39 | memory_debug.md | 11 | 待办 |  |
| 40 | memory_stats.md | 4 | 待办 |  |
| 41 | net-dns.md | 172 | 待办 |  |
| 42 | net-file.md | 4 | 待办 |  |
| 43 | net-frame.md | 9 | 待办 |  |
| 44 | net-interface.md | 12 | 待办 |  |
| 45 | net-resolver.md | 172 | 待办 |  |
| 46 | net.md | 184 | **完成** | 试点 2；六段全绿（184/184，G3 203 片段）：地址族 20 + 缓冲/DNS/Bytes 39 + Socket 39 + Port 29 + Post 3 + Engine 17 + Worker 9 + 第 6 段 26 + CompletionInit 1 |
| 47 | number.md | 15 | 待办 |  |
| 48 | once.md | 22 | 待办 |  |
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
