# API 文档深化台账（DOC_PROGRESS）

> 配套规范：`dev/DOC_SPEC.md`（每会话开头必读）。状态：待办 / 进行中 [x/y 段] / **完成**。
> 完成定义（DoD）：G1–G4 全绿 + 本台账已记 + commit `docs(api): <模块> 按 DOC_SPEC v1 细化`。

| # | 文件 | API 数 | 状态 | 备注 |
|---|---|---|---|---|
| 1 | array.md | 50 | **完成** | 试点 1；50 函数 G1-G4 全绿（2026-09-07） |
| 2 | asn1.md | 27 | 待办 |  |
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
| 46 | net.md | 184 | 进行中 [1/6 段] | 试点 2；地址族 20 函数全绿，余 buffer/socket/port/engine/resolver 164 待 |
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
