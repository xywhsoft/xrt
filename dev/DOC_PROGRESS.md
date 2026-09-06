# API 文档深化台账（DOC_PROGRESS）

> 配套规范：`dev/DOC_SPEC.md`（每会话开头必读）。状态：待办 / 进行中 [x/y 段] / **完成**。
> 完成定义（DoD）：G1–G4 全绿 + 本台账已记 + commit `docs(api): <模块> 按 DOC_SPEC v1 细化`。

| # | 文件 | API 数 | 状态 | 备注 |
|---|---|---|---|---|
| 1 | array.md | 50 | **完成** | 试点 1；50 函数 G1-G4 全绿（2026-09-07） |
| 2 | asn1.md | 27 | **完成** | 27/27 全绿（G3 27 片段，2026-09-07）；错误码 TAG/LENGTH/VALUE/TYPE/END/TRAILING/ORDER/DEPTH/RANGE 九码全表成文；编码器族（Append*8 + Oid 工具 3）首入文档 |
| 3 | atomic.md | 29 | 待办 |  |
| 4 | avl.md | 43 | 待办 |  |
| 5 | buffer.md | 23 | 待办 |  |
| 6 | cancel.md | 9 | 待办 |  |
| 7 | channel.md | 42 | 待办 |  |
| 8 | charset.md | 69 | 待办 |  |
| 9 | codec.md | 20 | 待办 |  |
| 10 | compress.md | 18 | 待办 |  |
| 11 | console.md | 4 | 待办 |  |
| 12 | core.md | 44 | 待办 |  |
| 13 | coroutine.md | 51 | 待办 |  |
| 14 | crypto.md | 122 | 待办 |  |
| 15 | environment.md | 4 | 待办 |  |
| 16 | error.md | 45 | 待办 |  |
| 17 | executor.md | 10 | 待办 |  |
| 18 | file.md | 97 | 待办 |  |
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
