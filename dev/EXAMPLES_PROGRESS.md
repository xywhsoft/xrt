# 范例完善工程 · 进度台账

> 目标：为全部 559 个范例（内核 292 + 扩展 267）添加非常详细的注释，
> 补齐覆盖缺口，并保证每个范例可编译、可运行。
> 本台账是唯一进度事实源，每完成一个范例立即更新。

## 2026-09-06 问题复核与修正

原“核心库 10 项问题”已逐项核验，当前结论以
[问题复核与修复台账](EXAMPLES_LIBRARY_ISSUES.md) 顶部的结案表为准，
下文各批次的原始疑似问题描述保留作历史记录，不再表示仍待库侧修复。

- 确认 B-2 为 SendMsg/SendMsgVec 终态文档遗漏，已补齐空控制退化路径及 12 组终态回归；保留现有事件行为。
- 修正 8 个相关核心范例及 xws/connection_tour 的错误注释、断言、初始化、跨线程操作和资源生命周期问题；IO Writer 的错型 Close 回调越界写也已修正。
- A-1/A-2/A-3/A-6/B-1 的当前源码与复验不支持原判断；C-1 属初始化前提未满足，C-2 已有 Init；A-4/A-5 与 xws 附注未复现，不将其标成已修复的库缺陷。
- 保留 API 范例覆盖目标；本次不改动下文历史覆盖统计或其他批次的用户修改。详细验证范围与平台边界见问题台账。

## 注释标准（每个范例必须满足）

1. **文件头块**（`/* ... */`）包含五要素：
   - 范例名称与一句话目的
   - 演示 API 清单（逐个列出）
   - 所需模块宏（如 `XRT_MODULE_ERROR`）
   - 编译命令（单头形态，Windows 链接 `-lws2_32 -liphlpapi`）
   - 预期输出（与实际运行一致，逐行）
2. **分段注释**：初始化 / 主体演示 / 清理三段各有小节注释说明意图
3. **逐调用注释**：每个 XRT API 调用上方说明参数含义、返回值语义、
   所有权/借用关系（谁分配谁释放、视图是否可空）
4. **风格**：Tab 缩进、大括号换行 K&R、`if ( cond )` 空格风格，与仓库一致
5. **验证**：注释完成后必须通过编译并运行，预期输出与实际一致才算完成

## 编译验证方法（单头形态）

```bash
# impl.c: #define XRT_MODULE_ALL + XRT_IMPLEMENTATION + <xrt.h>
gcc -O1 -I single impl.c <范例main.c> -lws2_32 -liphlpapi   # Windows
```

网络类范例需要运行环境时，以「编译通过 + 运行退出码 0」为门槛。

## 状态图例

- `[ ]` 未处理
- `[x]` 已完成（详细注释 + 编译验证 + 输出核对）
- `[+]` 本次新增范例（覆盖缺口）

## 内核 examples/（292 个）

| 目录 | 数量 | 进度 |
|---|---:|---|
| asn1 | 2 | [x] der·pem |
| charset | 4 | [x] detect·transcode·unicode·unicode_text |
| codec | 3 | [x] base64·hex·percent |
| compress | 2 | [x] deflate·inflate |
| concurrency | 32 | [x] 全 32 完成（thread·sync·once·spin·rwlock·semaphore·condition·deadline·cancel·channel×6·coroutine×4·future×4·executor·task×6·thread_key·report·worker） |
| console | 1 | [x] output |
| containers | 18 | [x] 全 18 完成：array·stack·fixed_stack·block_stack·list·buffer·queue_spsc·queue_mpsc·queue_mpmc·slot_map·map·set·set/owned·avl·avl_tree·int_map·ptr_array·ptr_stack·ptr_fixed_stack |
| core | 5 | [x] atomic·error·error_format·memory·reference（全部编译运行通过） |
| crypto | 30 | [x] core·sha 族×6·hmac×2·hkdf×2·pbkdf2×2·aes·aes_gcm·chacha20·chacha20_poly1305·poly1305·ed25519×3·x25519·x448·ecdsa×2·p256·p384·rsa_pkcs1·rsa_pss·session |
| data | 4 | [x] buffer_base64·buffer_hex·json·xson |
| file | 19 | [x] basic·whole·text·directory·walk·map·lock·link·fifo·temp·root·tree·report·async·async_manage·async_whole·dir_async·dir_temp·tree_async |
| hash | 3 | [x] hash32·hash64·keyed |
| http | 14 | [x] base·connection·host·param·target·te·trailer·expect·upgrade·encoding·decode·http1·http1_body·http1_message |
| id | 2 | [x] xid·xid_batch |
| io | 4 | [x] buffer·file·line·memory |
| logging | 10 | [x] async·console·core·file·file_json·file_text·format_json_buffer·format_text_buffer·json·printf |
| math | 8 | [x] helpers·near·random·random_secure·random_secure_text·random_text·thread_random·thread_random_text |
| memory | 7 | [x] debug·debug_report·memory_pool·pool·pool_page·stats·temp |
| network | 34 | [x] address·dns·engine·tcp 族×7·udp 族×4·socket×2·buffer·frame×2·interface·local_info·port×5·proxy×4·resolver×2·task×2 |
| number | 3 | [x] format·integer·float |
| path | 3 | [x] basic·safe·system |
| process | 8 | [x] capture·file·future·open·pipeline·signal·stream·terminal |
| string | 6 | [x] basic·builder·distance·format·glob·split |
| system | 1 | [x] environment |
| template | 5 | [x] core·control·compose·file·extension |
| text | 6 | [x] html_escape·pattern·regex·regex_replace·regex_set·regex_split |
| time | 5 | [x] basic·clock·format·local·protocol |
| tls | 17 | [x] record·negotiate·policy·context·verify·key_exchange·cipher_backends·auth_messages·messages·resume·client_resume·identity·stream·dial·dial_future·server·stream_future |
| value | 6 | [x] basic·collections·containers·graph·handle·ownership |
| websocket | 10 | [x] close·deflate·deflater·extension·frame·handshake·inflater·message·stream_ref·upgrade |
| x509 | 16 | [x] inspect·name·identity·signature·crl·crl_policy·crl_profile·distribution·name_constraints·path·path_build·profile·store·store_file·store_system·verify |

## 扩展库（267 个）

| 库 | 数量 | 进度 |
|---|---:|---|
| xhttp | 133 | [ ] |
| xws | 15 | [ ]（覆盖缺口，优先补） |
| xruntime | 30 | [ ] |
| xmail | 24 | [ ]（覆盖缺口，优先补） |
| xssh | 65 | [ ] |

## 覆盖缺口清单（第二阶段）

- [ ] xws：组播/零拷贝发送等主题范例偏少
- [ ] xmail：IMAP/POP3 专题范例
- [ ] 内核无范例的模块盘点与补齐

## 会话记录

- 2026-09-04：工程启动，标准与台账建立。core 5/5 完成。
- 2026-09-04：asn1 2/2、charset 4/4、codec 3/3、compress 2/2、console 1/1、containers 18/18 完成（累计 33/292，全部编译运行验证，含 3 处按真实运行输出的注释修正）。
- 2026-09-04：hash 3/3、id 2/2、number 3/3、io 4/4 完成（累计 45/292；哈希值/XID 文本长度/批量比较方向/io buffer 覆写结果等 8 处按真实运行修正）。
- 2026-09-04（第二阶段）：data 4/4、system 1/1、template 5/5、text 6/6、time 5/5、math 8/8 完成（累计 66/292）
- 2026-09-04（第二阶段续）：memory 7/7 完成（累计 81/292；pool 范例 typedef 错位已当场修正）
- 2026-09-04（第二阶段续2）：logging 10/10、memory_pool FreeMarked 丢失段补回（累计 91/292）
- 2026-09-04（第二阶段续3）：path 3/3 完成。第二阶段合计 +49（data4/system1/template5/text6/time5/math8/memory7/logging10/path3），累计 94/292。下轮从 examples/string 继续。
- 2026-09-05（第三阶段）：string 6/6、value 6/6（头注释插入法）、process 8/8（累计 114/292；修正 handle 注释内 */ 提前终止、FilterTo 语义反向两处注释错误；验证脚本改为先删 exe 再编译防旧产物掩盖失败）
- 2026-09-05（第三阶段续）：http 14/14、websocket 10/10（累计 138/292；直连子头文件范例统一用 -include xrt.h 编译）。
- 2026-09-05（第三阶段续2）：x509 16/16（累计 154/292；fixtures 范例需 -I . 指向仓库根）。
- 2026-09-05（第三阶段续3）：tls 17/17（累计 171/292；大文件采用头注释插入法，代码零改动）。
- 2026-09-05（第三阶段续4）：file 19/19（累计 190/292）。剩 crypto30、network34 两个大目录。
- 2026-09-05（第三阶段续5）：crypto 30/30（累计 220/292；RFC/FIPS 标准向量范例注明出处）。
- 2026-09-05（质量审计）：**发现账目错误，纠正为 248/292**。真实缺口 44 个：concurrency 32（整目录漏算）、file 前 9 个（首批插入命令 heredoc 截断整体失败未察觉）、value 嵌套 3 个（collections/batch、containers/indexed、containers/lifo）。另：第三轮 154 个采用头块插入法，头块五要素齐但正文逐调用注释未按标准补齐，列为第二优先整改项。流程改进：每批收尾必须 grep -L "范例：" 实测而非心算。
- 2026-09-05（审计整改1）：file 9 个 + value 嵌套 3 个已补齐并验证（12/12 编译运行通过，lifo 预期输出按真实运行修正）。
- 2026-09-05（审计整改2）：concurrency 32/32 补齐并全部编译运行验证 —— **内核 292/292 真正完成**（grep -L "范例：" 实测为 0）。遗留整改项：第三轮头块插入法的 ~154 个文件正文逐调用注释待按一二轮标准补齐。
- 2026-09-05（补覆盖工程启动）：**string 模块达成 100%**（83/83）。新增 10 个家族范例（compare/find/case/edit/pad_trim/dup_join/iterators/list/builder_tour/format_tour），全部注册进 modules.json、编译运行验证、预期输出按真实运行校准。过程中修复 9 处签名/设计错误（Find 三参、UpperTo/ReverseBytesTo 返回 bool、Dup 收 cstr、va_list 包装的 %s 误读指针导致段错误等——全部由编译/运行环节暴露）。方法论验证有效：家族巡礼一个范例覆盖一族，10 个范例吃掉 68 缺口。
- 2026-09-05（全面复核轮）：全量重扫——(a)模板残留/重复插入/未闭合注释/头块位置：0 问题；(b)头块编译命令与实际 include 方式交叉核对：0 不一致；(c)274 个含静态预期输出的范例逐一重新编译+运行+输出对照：**0 编译失败**，15 处告警中 12 处为已标注的随机/时变输出（过滤器占位符未识别），3 处真实精度问题已修复（xid_batch 比较方向非确定性、client_resume 尺寸 1-2 字节浮动、link identity 因文件系统而异）。剩余优化项唯一：~211 个文件的正文逐调用注释。。。。

## 范例 API 覆盖度统计（2026-09-05，明细见 dev/EXAMPLES_API_GAPS.json）

| 范围 | API 总数 | 范例覆盖 | 覆盖率 |
|---|---:|---:|---:|
| 内核（91 头） | 2,958 | 1,130 | 38% |
| 扩展库（5 库） | 2,477 | 735 | 29% |
| **合计** | **5,435** | **1,865** | **34%** |

- 零缺口模块：error_format、wait、xid（features 无 API）
- 近全覆盖（缺口≤2）：cancel、console、core、environment、hash、html、http1_net、http1_tls、http_connection、memory_stats、net_frame、temp
- 内核最大缺口：tls(113)、net(104)、string(68)、crypto(60)、x509(58)、charset(56)
- 扩展缺口：xhttp 696/1036、xssh 442/586、xruntime 300/442、xmail 214/280、xws 90/133

补范例优先级建议（第三优先任务，先广后深）：
1. 高频地基模块的缺口（string/file/http/map/value 各 50±，多为同族变体，一个范例可覆盖一族）
2. tls/net 大缺口按主题归并（消息编码族、地址族、端口族各一个范例）
3. 扩展库随范例注释工程同步补

## 补覆盖工程进度（目标：内核 + 扩展 100%）

### 已 100% 清零的模块（86 个，截至 2026-09-06——内核 100%）

string(83)·hash(9)·number(15)·core(4)·console(4)·html(3)·environment(4)·memory_debug(11)·http_connection(5)·http1_net(2)·temp(17)·cancel(9)·memory_stats(4)·net_frame(9)·map(62)·value(116)·file(97)·http(59)·stack(74)·asn1(27)·pem(7)·charset(69)·logger(76)·udp(73)·time(58)·tcp(78)·tls(140)·tls_stream(52)·net(172)·crypto(122)·x509(89)·tls_session(29)·xson(35)·http1(25)·future_bridge(8)·websocket_upgrade(9)·memory(13)·http_decode(10)·pool(58)·tls_client(10)·tls_server(10)·tls_verify(6)·tls_identity(9)·tls_resume(11)·net_file(4)·task_net(7)·http1_tls(4)·queue(46)·array(50)·avl(43)·websocket_stream(38)·regex(54)·pattern(37)·io(42)·channel(42)·coroutine(51)·sync(48)·websocket(57)·random(39)·list(28)·future(44)·file_async(34)·set(34)·atomic(29)·path(32)·template(32)·json(30)·signal(20)·buffer(23)·codec(20)·math(20)·error(27)·thread(22)·process(37)·task(36)·compress(18)·slot_map(16)·spin(7)·net_interface(12)·http_te(10)·http_encoding(12)·http_upgrade(10)·http_expect(8)·http_trailer(6)·executor(10)·proxy(23) —— 合计 2958 API 全覆盖（= 内核全部）

★ 86 个模块 100% 清零：string·hash·number·core·console·html·environment·memory_debug·http_connection·http1_net·temp·cancel·memory_stats·net_frame·map·value·file·http·stack·asn1·pem·charset·logger·udp·time·tcp·tls·tls_stream·net·crypto·x509·tls_session·xson·http1·future_bridge·websocket_upgrade·memory·http_decode·pool·tls_client·tls_server·tls_verify·tls_identity·tls_resume·net_file·task_net·http1_tls·queue·array·avl·websocket_stream·regex·pattern·io·channel·coroutine·sync·websocket·random·list·future·file_async·set·atomic·path·template·json·signal·buffer·codec·math·error·thread·process·task·compress·slot_map·spin·net_interface·http_te·http_encoding·http_upgrade·http_expect·http_trailer·executor·proxy

本批新增/扩展范例：
- 新建 9 个：hash/variants、number/variants、core/version_limits、console/variants、html/variants、environment/variants、memory/fail_inject、http/connection_cursor、http1/parse_buffer
- 扩展 5 个：memory/temp(+SecureReset/SecureUnit/Reset/Trim)、concurrency/cancel(+Ref/Triggered)、memory/stats(+Enabled)、network/frame_line(+LineReset)
- 全部注册 modules.json、编译运行验证、预期输出与真实运行逐行校准
- 编译/运行环节抓出 14 处 API 理解错误（pOutputSize 必填、IntWrite 六参、NumWrite 无 Format 参数、UIntWrite 十六进制小写输出、resourcelimits 字段名 i 前缀、FailAfter(N)=第 N+1 次失败、FailClear 连触发标志一起清等）

### 遗留说明

- 2026-09-06：**全量质检**（426 个范例逐个编译+运行+预期输出比对，含通配符口径 NNNNN/[GUID]/±1）：抓出并修复 **8 类质量退化**——①注册丢失（前批会话的 63 个 tour 范例在 modules.json 中丢失——本会话早时的 git checkout 事故只恢复了当时已知部分；另质检过程中的两次 checkout 又反复冲掉未提交注册，最终以"结构性 JSON 重建+与 HEAD 语义 diff 校验（59 模块仅 examples 增项、63 条新增、无其它变更）"一次性补齐并验证）；②头注释漂移（51 个项目范例的预期输出与真实输出不符——前批"逐行校准"的承诺实际有失守，批量以真实输出重写预期块，动态值保留 NNNNN/[GUID]/±1 占位符约定）；③两个非确定性输出（map_tour 打印裸指针、stack/tour 打印指针十六进制——改为身份断言确定性输出）；④一个缓冲区误用（charset/utf16_32 在缓冲版覆盖 A16 后继续按零结尾转换，输出含垃圾字符——复原+补结尾零修复，顺带修正头注释的 16→32 计数 3→2）；⑤一个语义错误演示（http/method_tour 把大小写敏感的 MethodEqual 当不敏感演示——改为同形真/异形假双路）；⑥transcode_tour 打印 SIZE_MAX 原值——改为 none 文本；⑦两个环境残留（仓库根的空目录 dbg-e 与 NUL 文件——调试期重定向产物，曾致 file/walk 范例遍历根目录时元数据查询失败）；⑧sync_tour 条件变量丢失唤醒竞态（谓词循环+锁内改谓词修复，30/30 稳定）。另：executor_tour 审计中出现一次 rc=1 为高负载偶发（复测 20/20 稳定）；原有范例（prose 风格头注释、平台限定 port_epoll/kqueue/uring 等 18 个比对不匹配项）确认为设计口径而非缺陷，属"注释补齐"待办。复验：内核覆盖 2958/2958、注册无缺失、72 个触碰过的范例全绿、xws connection_tour 10/10。
> **问题清单汇总**：全部批次发现的核心库问题（A 疑似缺陷 6 / B 文档不符 2 / C 健壮性 2）与 xws 扩展 4 项，已整理为独立文档 [dev/EXAMPLES_LIBRARY_ISSUES.md](EXAMPLES_LIBRARY_ISSUES.md)。
> **易用性摩擦清单**：各批次记录的 API 使用不顺点（生命周期/线程契约/三态极性/参数形态等 8 类约 30 条，含潜在问题假设与优先级建议），已整理为 [dev/EXAMPLES_API_FRICTION.md](EXAMPLES_API_FRICTION.md)。


- 2026-09-06：扩展库第一刀 **xws 75/133（+22，由 90 缺口降至 58）**（新增 extlibs/xws/examples/websocket/connection_tour：离线握手构建族——RequestCreate 的 URL 映射/UserInfo 拒绝、ClientRequestCreate 的 Key+协议字段、ClientRequestClone、Server/Conn ConfigInit+ConfigValid 的零值与 Init 形态判定；活回环双端——xhttp 服务器+客户端经环回对接，ServerCheck→UpgradeAccept→ClientDone 内 ClientCheck 验真实 101 + 自省族 Role/Tcp/TcpRef/Tls/TlsRef/Worker/Deflate/Writable/Paused/Error；同步发送九形态 Text/Binary/Send/TextRef/BinaryRef/SendTake/TextTake/BinaryTake + Writer 的 WriteTake/FinishTake/IsFinished 流式收尾；异步族 TextAsync/BinaryAsync/SendAsync/TextRefAsync/PingAsync/WaitAsync 写屏障——异步接口任意线程可调但在 Worker 任务内等待自身 Future 会死锁，必须主线程调用；Pong 经自动 Pong 观察）。构建配方：impl TU 需 XRT+XHTTP+XWS 三层 MODULE_ALL+IMPLEMENTATION 一起 define 再 include <single/xws.h>，仅 XWS 实现会缺内核与 xhttp 符号。**抓出 4 处疑似库问题（记台账待库侧核对）**：①xrtWsConnPause 主线程调用不生效（消息照常到达服务端，实现在设置 ReadPaused 后对 TCP 施加传输暂停，但跨线程调用实测无效）；②消息回调内 Pause 会导致连接静默转 CLOSED（无 Error/Close 事件，state=2，两种回调类型复现）；③Paused() 快照在暂停确实生效（消息滞留已验证）时仍返回假；④服务端连接在 13 条消息的回显流量后无任何事件转 CLOSED（Close/Error 回调均未触发）。因 ②④，Pause/Resume/Close/CloseInfo/ServerReject 段从范例裁剪（异步 Ping 的自动 Pong 保留了 Pong 观察），待库侧修复后补齐。8/8 稳定性通过。**扩展库工作到此收尾**：xws 剩余 58 缺口（Group 广播族 30+、握手 Sync 族、压缩发送族、Pause/Resume/Close 补齐）与 xhttp(696)/xruntime(300)/xmail(214)/xssh(442) 未开工，见 dev/EXAMPLES_API_GAPS.json。
- 2026-09-06：最后批次 **内核 2958/2958 = 100% 全覆盖达成**（新增 tls/handshake_extra、tls/resume_tour、network/file_tour 并扩展 tls/stream_tour(+HTTP over TLS 段) 与 network/task(+After/Until/GroupNetUntil)：handshake_extra 覆盖身份五接口（外部签名器用真实 P-256 链签名驱动完整握手——签名器先以 NULL 输出探测长度须返回成功、TLS 线格式 ECDSA 签名是 DER 不是裸 r||s、xrtEcdsaP256 族收裸 32 字节标量而嵌入密钥是 SEC1 DER 须偏移 7 提取）+ 验证五接口（PeerVerify 只能在验证器回调内调用——对端指针仅回调期间有效，回调内走默认路径 + 真实两证书链入锚）+ 客户端三接口（Certificate/Count/KeyUpdate）+ 服务端四接口（Name/Cookie 未配置选择器时保持调用方初值/Resumed/KeyUpdate）；resume_tour 覆盖票据签发两形态（TicketNew 默认随机 + Ticket 自定义）+ 客户端接管三接口（ResumeCount/TakeResume/ResumeDropped）+ 票据对象三接口（Retain/ValidAt/TicketAge）+ 恢复闭环（服务端恢复必须配置 Resume 回调按票据字节在应用层查表——服务器无状态，TicketNew 返回的对象即查表数据源）；file_tour 覆盖 NetFile 四接口（默认选项是只读——新文件须显式 CREATE|WRITE；读可能先于取消完成——两态兼容 OK/CANCELLED）；stream_tour 增 HTTP over TLS 段（xrtHttp1Request/ResponseParseTls 在 Worker 的 Read 回调内从明文块链解析；此前回显段的残留字节会污染后续解析——逐字节推进到请求/响应前缀；请求应答文本一律 sizeof 定长，手数字节数两次差一）。抓出 9 处关键语义如括号所注。五程序各 8/8 稳定性通过。**内核全部 91 个头文件 2958 API 覆盖完成，86 个模块 100% 清零。剩余工作：~211 个原有范例的正文逐调用注释补齐 + 扩展库 267 个范例。**
- 2026-09-06：小模块批次 2 **future_bridge 8/8 + websocket_upgrade 9/9 + memory 13/13 + http_decode 10/10 + pool 58/58 五个模块 100%**（新增 concurrency/bridge_tour、websocket/upgrade_tour、core/allocator_tour、http/decode_tour 并扩展 memory/pool 与 pool_page：bridge_tour 覆盖 Create/Init 双形态 + Promise 借用 + Watch 取消转发 + Ready/Fail/Wait 装配三态 + Unwatch 汇合；upgrade_tour 覆盖 Client/Server 配置四接口 + 全链协商闭环——KeyGenerate → RequestFields 组请求 → 服务端 RequestCheck 出 Accept → ResponseFields 组 101 → 客户端 ResponseCheck 验 Key/协议绑定 → StreamConfig 落成；allocator_tour 覆盖 GetAllocator/SetAllocator + At 族五接口——SetAllocator 只许首次分配前调用、首次分配后永久锁定（第二次替换返回状态错误），At 族可能由内存池层服务、只有穿透 backing 的分配经过自定义 Alloc；decode_tour 覆盖 ConfigInit/ConfigInitSafe 双配置 + Mode/InputSize/OutputSize 计量 + Reset 复用窗口换 identity 透传；pool/pool_page 扩展默认对齐 Init + 显式布局 InitLayout + CreateAligned）。抓出 6 处语义（FutureBridgeWait 是给异步操作线程的装配窗口等待——须 Ready/Fail 之后再调用，先 Wait 后 Ready 会永久阻塞；FutureBridgeFail 返回真表示发布成功且 Promise 归调用方仍可写终态；清零的 ws 客户端升级配置是最小合法形态而非非法；服务端要选定子协议必须配置里声明协议列表否则 101 无 Sec-WebSocket-Protocol；gzip 解压 "identity-passthrough-check" 恰 26 字节；xpool 的 Alignment 在池级字段不在首页上）。另修复一次 modules.json 注册丢失事故——多次 git checkout 还原把历次会话的 28 个范例注册全部冲掉，按唯一标记逐模块重新补齐并全量审计。六程序各 8/8 稳定性通过。内核 98.9%（2924/2958）。剩余 8 小模块 34 API。
- 2026-09-06：小模块批次 1 **tls_session 29/29 + xson 35/35 + http1 25/25 三个模块 100%**（新增 tls/session_tour、data/xson_tour、http1/head_tour：session_tour 覆盖喂入五形态（Borrow 借用/Take 接管/Ref 自定义释放恰好一次/Buffer 零复制缓冲链/Size 计数）+ 发送 Span 聚集双接口 + 明文队列零复制四接口（Size/SpanCount/Front/Spans/Consume）+ 会话属性五接口（Role/Version/Cipher/Wait/Context——未配置 Context 时仍挂共享默认上下文）+ Close/Eof/PeerAlert 三接口；xson_tour 覆盖 Valid 正反/Read 带配置/ErrorLocation 行定位/文件四接口（ParseFile/ReadFile/WriteFile/StringifyFile 原子替换）/Write 回调同步写出（{"n":7} 恰 7 字节）/WriterCreateSink 增量写入器全值域（Array/IntMap 容器 + Null/Bool/UInt/Float/Bytes/Time/Tag/Value 八种值）；head_tour 覆盖 LimitsInit 三限额/TargetValid 正反/Field 按名查找/TransferCodingNext 严格迭代/RequestBodyPlan 固定与 chunked 双形态/ChunkLineWrite 裸 size 与带扩展/RequestMessageParse 整条扫描 + MessageBodyView 免解分帧借用正文/BodyTrailers 后绑重绑 + TrailersParse 独立解析）。抓出 6 处关键语义（FeedBorrow 借用数据必须存活到对端消费——顺序固定为喂入→驱动对端→消费源 Span，提前 SendConsume 即悬垂指针报 unknown content type；干净关闭的 close_notify 本身就是 warning 级 Alert——PeerAlert 返回真且代码恰为 close_notify 而非"无 Alert"；xhttp1message 的 Head 字段存储与 Trailers 槽由调用方预挂/清零——容量不足返回 FIELDS、未清零是野指针段错误；BodyTrailers 重绑只在终止块后的 trailer 态合法——chunked 正文须喂完 0 块才进该态，且首块 DATA 返回只消费到数据边界（"5
hello" 8 字节）余下 "
0
" 5 字节须补喂；xrtXsonWrite 的 DOM {"n":7} 恰 7 字节；xhttp1limits 字段名是 MaxHead/MaxStartLine/MaxFieldLine 非 MaxLine/MaxHeader）。三范例各 8/8 稳定性通过。内核 97.7%（2889/2958）。剩余 13 小模块 69 API。
- 2026-09-06：x509 大块 **89/89 达成 100%**（新增 x509/fixture.h 共享凭据 + cert_tour、crl_tour、store_tour 三个范例：fixture.h 由 openssl 预生成——EC P-256 CA（CA:TRUE+SKI+cRLSign）、叶证书（KU critical/EKU 双用途/SAN 四形态/SKI/AKI/CRLDP/freshestCRL，序列号 ...BB）、双条目 CRL（BA 仅 reason、BB reason+invalidityDate，AKI+CRLNumber 4098）+ CA PEM 行数组；cert_tour 覆盖扩展游标四接口/策略三扩展/标识四接口（SKI 独立 DER 与 AKI SEQUENCE 双形态、叶 AKI=CA SKI 签发关联）/SAN 四形态遍历/IAN 与 NameConstraints 缺失 DONE/分发点三接口（CRLDP+FreshestCRL+单点 TLV+最小合法 IDP——空 IDP 被拒须至少一字段且 [0] IMPLICIT 单层编码）/名称匹配五接口（CN 属性、NameWithin 自包含、GeneralNameWithin 复合字面量、IssuerMatch 双参、MatchHost 收 xstrview）/TimeParse GeneralizedTime/ValidAt 窗口内外/SignatureParse+SignatureVerify+CertificateVerifyKey 验签；crl_tour 覆盖档案六接口（CRLNumber 去符号零 0x1002、AKI、DeltaBase/IssuingPoint/Freshest 缺失 DONE）/条目六接口（CrlFind 按序列号、CrlRevokes 按证书、Reason=keyCompromise、InvalidityDate、EntryIssuer 缺失、独立 GeneralizedTime 解析）/验证四接口（ValidAt/VerifyKey/Verify/Validate 输出可复用 valid 视图）/状态两接口（CrlStatus 未吊销也是 VALUE+GOOD 而非 DONE——DONE 表示 CRL 不适用；CrlSetInit 要求 complete+delta 双输入，openssl 配置不产出 deltaCRLIndicator 故以 NULL-delta 负路径覆盖，Set 须先清零否则野指针段错误）；store_tour 覆盖 AddPem 事务导入+1/AddSystem 平台锚/双索引借用/PathConfigInit Time 非零/Revocation 三段式（Init→CrlCheck 产出→Update 归并→Result 终态）。抓出 8 处易错（CrlAuthorityKeyId 出参是 xx509authoritykeyid 不是视图、RevocationUpdate 收 CrlCheck 的结果而非 (valid,cert) 三参、NameConstraintsCheck 空约束对象是参数错误且手构 GeneralSubtrees 须按"隐式 SEQUENCE 内容+每树 SEQUENCE{base}"编码、openssl 的 keyUsage=critical 使 KU 扩展 Critical=1、CrlStatus 对未吊销证书返回 VALUE+GOOD、CrlSet 系列对未清零 Set 的 SetCheck 会解引用野指针、Git Bash 会把 /CN= 路径化须用 -config 文件、openssl PEM 输出含 
 须剥离）。三范例各 8/8 稳定性通过。x509 100% 清零，内核 96.0%（2839/2958）。剩余 16 小模块 119 API。
- 2026-09-06：crypto 大块 **122/122 达成 100%**（新增 crypto/hash_tour、kdf_tour、aead_tour、sign_tour、ecdh_tour 五个范例：hash_tour 覆盖 MD5/SHA1/SHA224/SHA512_256/SHA512 流式三段式与一次性交叉验证 + HMAC-SHA256/512 流式 vs 一次性 + HMAC-SHA384 一次性确定性；kdf_tour 覆盖 HKDF-SHA256/512 两段式 Extract+Expand 与组合式逐位一致 + HKDF-SHA384 组合式 + PBKDF2-SHA384 同参确定性；aead_tour 覆盖 AES-GCM 分离式加解密往返+篡改拒绝+认证失败不触碰明文+TagSize、GMAC 计算/常量时间校验/篡改拒绝、ChaCha20-Poly1305 往返；sign_tour 覆盖 Ed25519 纯消息种子签名 + CONTEXT/PREHASH 双域（KeyInit 展开密钥复用）+ ECDSA P256/P384 RFC 6979 签名→raw/DER 双验证 + DER 编解码往返；ecdh_tour 覆盖 X25519/X448 公钥导出与共享密钥对称性 + P256/P384 公钥导出/点验证（非法点拒绝）/Add(P,P)==Multiply(2,P) 自洽）。抓出 4 处易错（AesGcmInit 第四参是标签长不是密钥位宽、ChaCha20 密钥固定 32 字节与 AES-128 的 16 字节不同、ECDSA DER 编码约 102 字节比 96 字节 raw 长——缓冲须独立、P-256 段写入的标量字节必须清零再复用同缓冲做 P-384 段）。五范例各 8/8 稳定性通过。crypto_core 100% 清零，内核 94.0%（2781/2958）。剩余：x509_parse(58) 大块 + 16 小模块 119 API。
- 2026-09-06：net 大块 **172/172 达成 100%**（新增 network/addr_tour、buf_tour、socket_tour、engine_tour、port_tour、resolve_tour 六个范例 + 扩展 tcp_stream_tour(+BytesRef)：addr_tour 覆盖比较三件套/五类判定/IPv4 映射 Unmap/sockaddr 16 字节往返/列表换端口与共享引用/DNS 三接口 localhost 本机解析；buf_tour 覆盖池四接口+四类追加（复制/借用/接管/自定义释放回调恰好一次）拼出 "hello world" 11 字节≥4 段+Prepend/Pullup/Peek/Find/Consume+Reserve-Cancel+Move 转移源清空；socket_tour 覆盖同步 Socket 层全接口——自省五件套+Set/Get+元数据默认零、向量与消息形态七种收发、批量 2+2、多播环回自收、TCP 非阻塞 FinishConnect+Remote；engine_tour 覆盖 Engine 状态机/双 Worker 自省（Current/IsCurrent/Engine/Index/Port/BufPool/Alloc/Free/OperationId）/Pin/Unpin/PostPending 真→假/Schedule 到期+TimerCancel CANCELLED+TimerCancelCurrent 回调内取消另一个/聚合统计；port_tour 覆盖 20 接口——SELECT 管 Watch/Unwatch readiness、IOCP 管全部完成式提交（Connect/Accept/ReadProbe/Recv/RecvVec/Send/SendVec/RecvFromVec/RecvMsg/RecvMsgVec/SendToVec/SendMsgVec/Cancel→CANCELLED/Post→USER/Wake→WAKE/RecvError 平台门控）+Backend/Capabilities/GetConfig；resolve_tour 覆盖 ResolverStats 前后对比/OpState 状态机/OpRef 双销毁/Clear 清缓存归零）。抓出 9 处关键语义（通配绑定的 Local 是 0.0.0.0 不能当发送目标；RecvMsg 族 pMeta 必填——NULL 报参数错误；SendBatch 后 Windows 环回第二个数据报可能晚到——RecvBatch 返回已到达前缀须补收；非阻塞 Connect 的 AGAIN 分支不能二次调用——在途 socket 重连必败；Windows 无数据报错误队列——DgramRecvError 恒 ERROR 且 DGRAM_ERRORS 选项置不上；Windows 端口能力分属两个后端——IOCP 只有 completion（Watch 报 no readiness）、SELECT 只有 readiness（提交报 no completion）；PortRecvMsg 要求 Socket 先启用至少一位元数据；一次 Wait 同批可返回多操作终态——逐个等取须暂存其余事件否则丢失；零标志控制的 SendMsgVec 终态实测 SEND_TO 与头文件注释 SEND_MSG 不符——记为疑似文档问题）。六范例各 8/8 稳定性通过。net 100% 清零，内核 92.0%（2721/2958）。剩余：crypto_core(60)/x509_parse(58) 大块 + 16 个小模块 119 API。
- 2026-09-06：tls 大块第三刀 **140/140 达成 100%**（新增 tls/message_tour：通用框架 Handshake/Extension 的 Size/Encode/分片感知 Parse + RecordSize + Name 三件套；消息族 Alert/Finished/KeyUpdate 与 CertificateVerify/EncryptedExtensions/SessionTicket/CertificateStatus/CompressedCertificate 的 Size/Encode/Parse 全闭环；TLS12 CKE/CR 与 TLS13 CR/CVC 专属布局；扩展数据解析器 ServerVersion/ServerKeyShare/ServerPsk/ClientVersions/ClientVersionSelect；Authorities 游标 + Size/Encode 往返；HandshakeReader 全六接口跨 3 分片渐进重组）。抓出 7 处线路格式语义（Handshake/Extension Parse 的 AGAIN Required 是"下次调用前至少要凑够的字节数"——头(4 字节)未齐给 4、头已齐给整条大小；CertificateVerify 布局 scheme(2)+签名向量(2)+签名而非裸签名、CertificateStatus 响应是 24 位向量、CompressedCertificate 压缩流无向量前缀且 algorithm 是 16 位、12 CKE 公钥用 8 位向量；EE 与 13 CR 的扩展入参是裸记录列表（无外层向量长）且 13 CR 必须含 signature_algorithms 记录、Hello.Extensions 视图同样不含外框；supported_versions 数据的列表长是字节数非条目数；Authorities 条目 16 位长 + 外框 16 位总长——Size=2+(2+3)+(2+4)=13；Name 族对未知值从不返回 NULL 返回 "unknown_xxx"）。8/8 稳定性通过。tls 100% 清零，内核 88.6%（2621/2958）。剩余大块：net(104)/crypto(60)/x509(58)。
- 2026-09-06：tls 大块第二刀 83/140（新增 tls/writer_tour：Writer 全 16 接口——Init/Reset/Data + Extension/HostName/Protocols/Ids/ClientVersions/ServerVersion/ClientKeyShares/ServerKeyShare/RetryGroup/RetryCookie/PskModes/ClientPsks/ServerPsk，加 ClientHello/ServerHello Size/Encode/Parse 写出→读回闭环）。抓出 3 处关键约束（Writer 拒绝重复扩展类型——supported_versions 的 Client/Server 双形态、key_share 的 Client/Server/RetryGroup 三形态、pre_shared_key 的 Client/Server 双形态都必须 Reset 隔离后单独验证；ServerHello 不得携带 SNI 扩展——"server-name acknowledgement must be empty"；WriterHostName 收 xbytesview 非 xstrview）。写出端与解析端同格式闭环（46/6 字节扩展向量被 ExtensionsValidate 逐次校验）。8/8 稳定性通过。内核 87%。剩余 57：握手 Encode-Size 36 / TLS12 6 / TLS13 5 / other 10。
- 2026-09-06：tls 大块第一刀 61/140（新增 tls/extension_tour：Ids 工具族 Count/Get/Contain/Select 按偏好交集选组、扩展向量 Validate/Find/Init/Read 双扩展 TLV 手构+遍历、SNI 游标两读 + HostName 快捷提取、ALPN 游标 + ProtocolSelected 单选（输入仍是完整 ProtocolNameList 外框）+ ProtocolFind + ProtocolSelect 交集选 h2、Groups/Signatures 解析（输入是含 16 位向量长的扩展数据形态）、SignatureInfo/Compatible/Select 版本在前、CipherCompatible 四路矩阵（1.3 套件对任意身份真、1.2 ECDHE_RSA 配 RSA 真/ECDSA 假/配错版本假——套件版本字段先于身份检查）、key_share 空列表三路径 + KeyShareSelect 空共享 DONE、PSK 模式向量 [1 字节长+模式] 与 ClientPsks 完整布局 [id 向量长][id 长+id+age][binder 向量长][binder 长+32 字节]、LimitsInit/Valid 预算阈值正反 + ContextRetain + RetryGroup/Cookie）。抓出 6 处线路格式（16 位标识是大端、SNI 列表长 6=1+2+3、ALPN 列表长 13=3+10、扩展数据自带 16 位向量长、PSK binders 向量最少 33 且条目有独立 1 字节长、ProtocolSelected 输入含外框）。内核 86%。剩余 79：Writer 16 / 握手 Encode-Size 36 / TLS12 6 / TLS13 5 / other 16。
- 2026-09-06：executor 10/10 + proxy 23/23 达成 100%（executor_tour：SubmitBatch 三项原子受理、Get 统计 Submitted/Completed/Threads 断言、Close→WaitFor/WaitUntil/Wait 三形态收口 + Closed/Queued 终态核对——抓出 Wait 族只对已关闭执行器有意义（未关闭调用返回 ERROR 非等待新工作），须先自旋等工作完成；proxy_tour：Retain/Info 六字段视图、Handshake 创建即 WRITE 态且首段输出非空（Sent 推进）、未完成握手 Bound/Code 双假 + Error 空、Dial 对不可解析代理的失败路径 Ref/State=FAILED/Error 非空/Stats 快照——抓出零值 EngineConfig 会被拒绝（须 ConfigInit + Workers>=1））。8/8 稳定性通过。内核总覆盖 84%。
- 2026-09-06：net_interface 12/12 + http_te 10/10 + http_encoding 12/12 + http_upgrade 10/10 + http_expect 8/8 + http_trailer 6/6 六模块达成 100%（interface_tour：InterfaceIndex/Name 名索引往返（Windows 回环规范名是 GUID）、LocalAddress/Text/Hardware/Text/HostName 全部两段式；http/small_fields 一个范例覆盖五族——TE（CodingParse 的 trailers 语法特例+q 值 800/1000、FieldNext 跨行 3 成员、AcceptsTrailers ITEM）、Encoding（AcceptEncodingParse 收字段数组、Quality gzip=900/identity 默认 1000、CodingParse x-gzip 别名、ContentEncoding 游标+Write 拼接）、Upgrade（游标 2 项、Build 14 字节、ElementWrite h2c/v2）、Expect（100-continue 名称核对、游标 2 项）、Trailer（Count/Find/NameValid、NamesWrite 收"实际 trailer 字段"而非 Trailer 头、SectionValid 正反））。抓出 7 处语义（HTTP 列表空成员全部按语法忽略——Valid 不拒绝、trailers 成员带 q 合法但普通编码 q 属 t-params、identity 未显式声明时 Quality 返回默认 1000、Upgrade Build "websocket, h2c" 是 14 字节、NamesWrite 的输入是实际字段非声明头、SectionValid/Find 等收字段数组、AcceptEncodingParse 也是字段数组而非裸值）。8/8 稳定性通过。内核总覆盖 84%。
- 2026-09-06：slot_map 16/16 + spin 7/7 达成 100%（slot_map_tour：堆形态 Create/Reserve/Destroy、Get/Set/Generation/Index 代际分解（索引 0/1、代际从 1 起）、删除后陈旧句柄失效（Get 空/Contains 假/Remove 假）与同索引新句柄代际递增、Clear 全清 + 迭代器两对槽值核对；spin 原范例扩展堆形态段——Create/TryLock 持有时失败与空闲时成功正反/Destroy）。slot_map 抓出 2 处实现语义（Set 受理成功但 Get 仍返回插入时的原值——值不随 Set 更新、Remove 出参写 NULL 不回传值——两处均按实测行为记录台账待核对是否为缺陷）。8/8 稳定性通过。内核总覆盖 82%。
- 2026-09-06：task 36/36 + compress 18/18 达成 100%（task_tour：提交族四种容量等待形态逐个收割值 7、池收口 Close→WaitFor+Get 统计断言、微型池满载下已触发令牌的 SubmitUntilCancel 立即取消+PoolCancel/WaitUntil/WaitUntilCancel/Wait 全套、组提交族 GroupSubmitWait/Until/UntilCancel+GroupStart 启动器+组等待 WaitUntil 双形态+Error/CancelToken；stream_tour：Deflate 两段写入 SYNC+FINISH 流式与 Done/OutputSize 对账、Inflate 回环 OutputSize 核对原文长度、双对象 Reset 复用第二条流、ConfigValid 越界级别与非法格式拒绝）。compress 抓出 3 处关键语义（Deflate 默认 GZIP 而 Inflate 默认裸 DEFLATE——回环必须把 Inflate 切到 XINFLATE_GZIP 配对、Flush 枚举带 FLUSH_ 前缀、InflateWrite 第三参 bFinal 显式标记最终块）。8/8 稳定性通过。内核总覆盖 82%。
（新增 concurrency/task_tour：提交族四种容量等待形态 SubmitWait/SubmitFor/SubmitUntil/SubmitUntilCancel 逐个收割值 7；池收口 Close→WaitFor+Get 统计（Completed/Succeeded>=5、Closed 置位）；微型池（1 线程 1 队列）满载下已触发令牌的 SubmitUntilCancel 立即取消 + PoolCancel/WaitUntil/WaitUntilCancel/Wait 全套；组提交族 GroupSubmitWait/Until/UntilCancel + GroupStart Future 启动器 + 组等待 WaitUntil/WaitUntilCancel + Error/CancelToken 非空）。8/8 稳定性通过。内核总覆盖 81%。
- 2026-09-06：process 37/37 达成 100%（新增 process/tour：Run 全流程（输入写出+有界捕获+结果六字段）、Capture 默认策略、生命周期 Id/Native/State/Status/Error/StreamNative、WaitFor 短窗超时与 WaitUntil 成功、WaitUntilCancel 已触发令牌、停止族 Interrupt（协作请求，重定向子进程可能不响应）→Terminate→Kill 逐级、KillTree 无子树等价 Kill、PipelineOptionsInit、Terminal+Resize 能力门控调用）。抓出 5 处（ShellConfigInit 是两参收完整命令串、结果字段是 Stdout/Stderr 小写、流枚举 XPROCESS_STDOUT 无 STREAM_ 前缀、退出种类是 XPROCESS_EXIT_CODE、Windows Terminate 对 cmd 管道进程可能不立即生效——Terminate 后补 Kill 强制收口）。6/6 稳定性通过。内核总覆盖 81%。
- 2026-09-06：error 27/27 + thread 22/22 达成 100%（error/tour：Build 七字段/BuildAt 三定位器/Cause 链 Find 命中与未命中/Ref 配对释放后原指针仍活/SetErrorTake 所有权转移/SetErrorInfo/SetErrorKind/SetErrorHandler 全局通知三次后注销；thread_tour：WaitFor 短窗超时→WaitUntil 成功、State/ExitCode/Id、Stop 协作（主线程 Stop 请求→工作线程 Stopping 自检自愿退出码 7）、TLS KeyTake 取出即清空 + KeysClear、ThreadCurrent 工作线程内非空主线程为空）。抓出 2 处语义（xrtSleep 收毫秒不是微秒——50000 成 50 秒致 WaitUntil 超时、xrtThreadStopRequested(NULL) 恒返回假——线程内自检必须用 Stopping()，实现 NULL 分支直接 return false）。10/10 稳定性通过。内核总覆盖 80%。
（新增 error/tour：Build 完整描述七字段核对、BuildAt 源码位置 File/Line/Column 三定位器、Cause 链 Find 命中与未命中双路径、Ref 共享引用配对释放后原指针仍可访问、线程绑定族 SetErrorTake 所有权转移（转移后原指针不得再用）/SetErrorInfo/SetErrorKind/SetErrorHandler 全局通知——每次设置错误各通知一次本例 3 次后注销）。8/8 稳定性通过。内核总覆盖 80%。
- 2026-09-06：codec 20/20 + math 20/20 达成 100%（codec/tour：HEX 两段式与原地同址解码、Base64 缓冲查询+写入、Percent 流式族——MapInit 位图 + Measure→WriteMeasured→EncodeMeasured 三段式 + DecodeMeasure→DecodeMeasured + PercentNext 游标 + Write 片段 + EncodeNew；math/tour：关系族 Min/Max/Sign/Trunc/Mod/Rad、分类三函数 NaN/Inf/Finite 用 0/0 与 1/0 与 1.5 三分样本、指对数 Log2/Exp2/Log1p/Expm1 已知常数断言、Cbrt 负数域 -27→-3）。抓出 2 处语义（PercentDecode 四参无 config、xrtMathMod 随被除数符号是 C 截断语义非 floor；xrtMathNear 是双容差四参绝对+相对）。8/8 稳定性通过。内核总覆盖 79%。
（新增 codec/tour：HEX 两段式编码查询+写入与原地同址解码（4142→AB 写回同一缓冲）、Base64 缓冲版 Decode 空输出查询+实际写入与 EncodeNew 分配版、Percent 流式族——MapInit 预编译字符位图（空安全集使每字符 3 字节）、Measure→WriteMeasured→EncodeMeasured 三段式编码流水线（9 字节 %41%20%2F 含终止零验证）、DecodeMeasure→DecodeMeasured 两段式解码、PercentNext 游标逐字节（BYTE→END 两态）、Write 无终止零片段与 EncodeNew 分配版）。抓出 1 处签名（PercentDecode 是四参无 config）。8/8 稳定性通过。内核总覆盖 79%。
- 2026-09-06：signal 20/20 + buffer 23/23 达成 100%（signal_tour：OnOwned+Ref 双引用析构时机（最后一个引用释放时恰好一次）、Once/OnceOwned 双形态（首次调度先注销再回调、句柄无需再 Off）、Count/Received/Clear、Ignore/Restore/RestoreAll——抓出 Windows 对 TERM 有仿真支持、未知代码 Name 返回 "UNKNOWN"、Once 计数跨段续算三处；buffer_tour：容量族 Reserve/Resize/Trim、编辑族 Add 返回直写指针/Insert/Remove/AppendByte/Assign、接管族 SetTake/CreateTake（成功后清空来源槽）/From 复制——抓出 Buffer 的 InsertSpace 同样是未初始化槽不零填充，与 xarray 一致）。10/10 与 8/8 稳定性通过。内核总覆盖 78%。
（新增 process/signal_tour：元数据 Supported 正反/Name/Healthy；OnOwned+Ref 双引用——析构器只在最后一个引用释放时执行恰好一次（先释放原引用 Ref 仍在→不析构、再释放 Ref→恰好一次）；Once/OnceOwned 一次性双形态（首次调度先自动注销再执行回调——句柄无需再 Off）；Count/Received/Clear 收计数三视角；Ignore/Restore/RestoreAll 忽略恢复链）。抓出 3 处语义（Windows 对 TERM 有仿真支持非"不支持"、未知代码 Name 返回 "UNKNOWN" 而非 NULL、Once 计数器跨段续算——重置后 spin 门槛应回到 1）。10/10 稳定性通过。内核总覆盖 78%。
- 2026-09-06：json 30/30 达成 100%（新增 data/json_tour：Read 带配置 + Valid 正反（零 DOM）、ParseFile/ReadFile 双形态、WriteFile/StringifyFile 含 pretty 后读回验证、Write 输出回调 23 字节逐位核对、QuoteWrite 引号转义 9 字节、CreateSink 增量写入器拼出数组五元素 [1,true,null,2.5,"v"] 共 21 字节（WriterUInt/Bool/Null/Float/Value 全用）、ErrorLocation 行定位）。抓出 2 处计数（QuoteWrite 输出含首尾引号共 9 字节、Sink 写入器紧凑数组 21 字节——手数错两次）。8/8 稳定性通过。内核总覆盖 78%。
- 2026-09-06：template 32/32 达成 100%（新增 template/tour：CompileFileConfig 文件+高级配置 + Source 视图、NodeCount/Node 节点自省（TEXT 首节点与 OUTPUT 表达式 name）、Write 流式回调（明文 10 字节/HTML 转义 16 字节双档）+ RenderHtmlConfigInit、RegistryRef 共享引用、shout 块扩展在回调内消费全部调用域访问器——CallName/Data/ArgumentCount/Argument/Find 未命中路径/Eval/Raw/Current/Root/Global/Render/RenderCurrent（临时替换当前值渲染主体）、ErrorLocation 行定位）。抓出 3 处语法语义（输出标签是 {$path} 而非 {path}、函数扩展调用是 {@name:args}、块扩展是 {#name:args}...{#end} 缺 end 编译失败；Global 未配置时 CallGlobal 返回空合法）。8/8 稳定性通过。内核总覆盖 77%。
- 2026-09-06：path 32/32 达成 100%（新增 path/tour：Parse 零分配五视图（Root 含分隔符三字节、Ext 含前导点）、IsAbs/IsRoot/IsRooted 三判定、Build 多段数组（驱动器前缀段后不补分隔符）、Clean 折叠 .. 与重复分隔符、Parent/WithExt、Relative 风格化与 Rel 系统化输出一致、Abs 以 Cwd 为基准、Sep/ListSep 平台实测（\ 与 ;）、Executable 进程映像、SetCwd 切换后还原、SafeSegment 流式逐字节 Feed/Finish 含 COM1 设备保留名拒绝）。抓出 2 处语义（Parse 的 Root 视图含分隔符共三字节、Build 的驱动器段后不加 \——C: 与 dir 拼出 C:dir 而非 C:\dir）。8/8 稳定性通过。内核总覆盖 77%。
- 2026-09-06：set 34/34 + atomic 29/29 达成 100%（set_tour：构造族含对齐变体与 Trim、GetOrAdd 双路径、代数四运算按 {1,2,3}×{3,4} 教科书断言、谓词族含 bProper 严格性、逆序迭代实测插入逆序——谓词族是三参易错；atomic_tour：RMW 补集 Exchange/And/Or/Xor 全部断言"返回旧值"、32 位与指针 CAS 演示强语义失败回写 Expected、64 位 Init+五运算链式核对、IsLockFree(4/8)/ThreadFence/SignalFence/Pause——x86-64 上自然对齐 4/8 字节均无锁）。8/8 稳定性通过。内核总覆盖 76%。
（新增 containers/set_tour：构造族 Create/CreateAligned/InitAligned/Reserve/Trim/Clear；查询族 GetOrAdd 双路径（存在不新增/不存在插入报告新）/Has/Remove 正反/Visit 第二项停止；代数族 Merge/Union/Difference/SymmetricDifference 按 {1,2,3}×{3,4} 教科书结果断言；谓词族 IsSubset/IsSuperset（第三参严格性）/IsDisjoint/Equal 用克隆对验证；逆序迭代 IterRBegin 实测按插入逆序 3,2,1）。抓出 1 处易错（谓词族是三参——bProper 区分子集与真子集）。8/8 稳定性通过。内核总覆盖 75%。
- 2026-09-06：file_async 34/34 达成 100%（新增 file/async_tour：Adopt 接管已开文件+Flags 快照、WriteAtRef 零复制释放回调恰好一次、WriteAtTake 接管分配、Flush/Size/Resize；整文件族 ReadAllLimit/WriteAll/Append/FileMove；目录族 CreateMode/CreateAll/CreateAllMode/Empty/Stats/Size/EnsureEmpty/Clean/DirMove；树族 TreeCopy/TreeRemove。抓出 1 个 ABI 不一致（DirStatsAsync 头文件标注值为 xdirquery{Empty}，实现实际发布 xwalkstats 计数器——按 Empty 读出的是 Items 首字节的垃圾值，范例按 walkstats 判读并记台账）与 2 处语义（xwalkstats.Items 含根目录本身、清空判定看 Files==0；Move/TreeRemove/Stats/Size 均有第四参 bReplace/bKeepRoot/bRecursive）。8/8 稳定性通过（每轮前清残留目录——CreateMode 对已存在目录报错）。内核总覆盖 75%。
- 2026-09-06：list 28/28 + future 44/44 达成 100%（list_tour：侵入式双向链表全接口——五种插入形态拼 1..5、编辑族用顺序字符串全程核对（23451→345→354→213）、迭代器正逆序+遍历中删除清空、Linked=已挂入链表需独立离链节点测假；future_tour：等待族 WaitUntil/WaitUntilCancel/AwaitUntil(协程)/Done/Ref、Promise 补集 Reject/Forward/Done/Ref、延续族四分支 Then/Catch/Continue/Finally 的 Plain+Owned+CancelSource 全变体（Then+1 链、Catch 失败转 99、Finally 透传源值）、Watch 三态 Add/Detach/Remove——触发恰好一次且摘除后不再触发）。future 抓出 2 处语义（Promise 只能完成一次——Resolve 后再 Reject 必失败须新开 Promise、Owned 析构回调是双参 pData+pDestroyData）。10/10 稳定性通过。内核总覆盖 74%。
（新增 containers/list_tour：生命周期 Init/NodeInit/Ready/Validate 含空表 Pop 双向返回空；五种插入形态拼出 1..5 顺序断言；查询族 Count/First/Last/Prev/Next/Owner/Contains/Linked；编辑族 MoveBack/MoveFront/PopFront/PopBack/Remove/Clear 全程用顺序字符串核对（23451→345→354→213）；迭代器正序/逆序/遍历中 IterRemove 清空）。抓出 1 处语义（Linked = 已挂入任意链表，在链为真——与直觉的"是否空闲"相反，离链节点测假需要独立 Detached 节点）。10/10 稳定性通过。内核总覆盖 74%。
- 2026-09-06：random 39/39 达成 100%（新增 math/random_tour：全局线程层 Rand64/Below/Range/Bytes/Shuffle/RandText/RandStringFrom/RandString 含洗牌多重集保持断言；Fast 层同种子可复现——32/64/字节序列回放逐字节一致；RNG 实例层双实例同步对比 Rng64/Below32/Below64/Range/RngText/RngStringFrom + Ready 门；Secure 层 SecureStringFrom/SecureText 按 16 进制字母表校验）。抓出 2 处易错点（Text 族是字母表+缓冲+容量+长度四参、同一 RNG 的每次额外抽取都会推进状态——双实例对比必须逐调用同步消费，短路条件的第二次调用即失步）。10/10 稳定性通过。内核总覆盖 73%。
- 2026-09-06：websocket 57/57 达成 100%（新增 websocket/extension_tour：握手纯函数 Key/Accept/CloseCode 校验含 RFC 6455 样例值；子协议族 ProtocolNext 游标迭代 + ProtocolsValid/Has；扩展族 ExtensionCount/Write；压缩协商闭环——offer 构造（窗口参数必须伴随存在标志）→ OfferWrite 两段式 → ResponseParse 真实解析 → ResponseCheck → Direction 拆双方向（响应未确认的 client 方向回落 15）→ 应用到收发配置；变换层 DeflaterReset/Flush 同步边界/Abort 丢弃历史/Bound/Size + InflaterReset/Size；消息配置 ConfigInit/InitSafe/Reset）。抓出 4 处语义（xwsdeflate 的 Flags 表达参数是否存在、无标志的窗口字段非法、Response 显式 ClientMaxWindowBits=0 非法须保留 Init 默认、DeflaterSize 实测为压缩后线路字节数与文档"语义字节数"表述不符——记为疑似文档问题）。10/10 稳定性通过。内核总覆盖 72%。
- 2026-09-06：coroutine 51/51 + sync 48/48 达成 100%（coroutine_tour：单步调度族 Step/PollFor/PollUntil/Alive、生命周期协程内自省 + 主线程终态、Join/Park/Wake 三协程同场四断言、事件族三等待点、PostOwned 恰好一次析构；sync_tour：五原语堆形态补齐——Mutex TryLock 持有/空闲正反、Cond 工作线程两相（For 到期→无限 Wait 被 Signal）、Sem TryWait/PostMany 三枚消耗、RWLock TryWrite 与读持有互斥、Event 自动复位消费语义 + 手动复位 Reset 后阻塞）。协程范例一次编译通过；sync 范例抓出 1 处设计错误（主线程直接 CondWait 无 Signal 者即死锁——等待必须放在工作线程）。15/15 稳定性通过。内核总覆盖 70%。
（新增 concurrency/coroutine_tour：单步调度族 CreateLimit/Step/PollFor/PollUntil/Alive；生命周期族协程内 Current/State/Stopping/CancelToken/SchedCurrent + 主线程终态 Term/Error/Backend(fiber-win)/SleepUntil；Join/Park 族三条协程同场——JoinFor 短窗 TIMEOUT、JoinUntil 长 OK、ParkFor 被唤醒协程 Wake、ParkUntil 过期 TIMEOUT；事件族 Create/预置位 TryAwait 消费/AwaitFor 与 AwaitUntil 双 Set 唤醒/Reset/Destroy；PostOwned 受理后析构恰好一次）。一次编译通过，15/15 稳定性通过。内核总覆盖 69%。
- 2026-09-06：channel 42/42 达成 100%（新增 concurrency/channel_tour：内嵌缓冲形态 InitBuffer 收槽位数组；阻塞族 Send/SendFor/SendUntil/RecvFor/RecvUntil 含满发空收双向超时；取消族五个变体经共享堆令牌由主线程 Request 中断挂起线程（收发等待者须分用满/空两条通道否则互相解锁）；Select 族 Try/For/Until 逐步控制就绪侧含选择超时；协程族七条协程——RecvAwaitFor 到期、SendAwaitUntil 被唤醒、SelectAwaitFor 预填命中、满发 AwaitFor 超时、过期 AwaitUntil、无限期 SelectAwait、过期 SelectAwaitUntil）。编译运行验证抓出 4 处理解错误（InitBuffer 第二参是槽位数组而非字节存储、容量 2 连发 3 个必满、"已等待的接收者优先于超时"使到期接收在有就绪项时仍成功、rendezvous 通道上挂起的接收会让另一协程的发送 case 就绪——到期路径的通道必须无人触碰）。20/20 稳定性通过。内核总覆盖 69%。
- 2026-09-06：io 42/42 达成 100%（新增 io/stream_tour：内存/文件/缓冲/丢弃/自定义回调五种后端同构巡礼——Reader 读/定位/EOF/ReadAll；复制族 CopyN 精确与 CopyLimit 上限含超限错误路径；自定义回调表 Reader+Writer 含 Seek 四参原点形式与 Close 生命周期；缓冲族 TakeBuffer/FromBuffer/WriteBuffer 配 Discard 计数；文件族 FromFile 双向/TakeFile 双向/OpenAppend 追加；LineReaderCreate 非接管形态）。编译运行验证抓出 5 处理解错误（Seek 是 offset+Origin+出参四参、WriterWrite 收散参而非视图、行枚举是 XLINE_NEXT_LINE、ReadAll 后 EOF 标志粘滞 seek 不清除复用 Reader 会静默零拷贝、CopyN/CopyLimit 的计数出参只在失败时写入）。10/10 稳定性通过。内核总覆盖 68%。
- 2026-09-06：pattern 37/37 达成 100%（新增 text/pattern_tour：一次性 Extract/ExtractConfig 含容量不足报所需数；单条 Compile/CompileConfig/Ref；批量 CompileMany/CompileManyConfig 携带 Value 与优先级；自省族 Count/CompiledBytes/Separators/Source/Id/Value/CaptureCount/MaxCaptureCount/CaptureName/CaptureIndex；匹配三态 Match/Lookup/Test 按优先级择优；错误机器数据 ErrorOffset/ErrorPattern；Builder 全族 Create·Config/Add/AddMany/Set/Remove/Reserve/Count/Version/Dirty/Clear/Compile 缓存复用）。编译运行验证抓出 2 处语法理解错误（默认分隔符集合只有 '/'，'{user}@{host}' 在默认配置下是单字段双捕获直接报错、{*name} 必须独占整个分隔段——前缀文本要放在分隔符之前如 'mail/{*rest}'）。10/10 稳定性通过。内核总覆盖 67%。
- 2026-09-06：regex 54/54 达成 100%（新增 text/regex_tour：编译自省族含 ConfigInit/CompileConfig 忽略大小写/Valid 正反例/Ref/Pattern/Flags/CaptureCount·Name·Index 命名组双访问/ErrorOffset 从编译错误读字节位置；转义两段式 EscapeSize+EscapeWrite；Matcher 补集 At 锚定/Full 全覆盖/Matched/Text/Capture 下标版；一次性 Match/FullMatch；替换族 ReplaceTo 限额+计数/ReplaceFirst/ReplaceFuncTo 回调大写替换；集合补集 SetCreate 聚合编译对象/SetCompileConfig/SetRef/SetCount/SetRegex/SetMatcherFirst·Matched/SetTest/SetErrorIndex 批量编译失败定位；补 Test 与 Split 整段拆分）。编译运行验证抓出 5 处理解错误（CaptureCount 含 0 号整组共 3、命名捕获从下标 1 起、EscapeSize("a.b*c")=7 两段式、EscapeWrite 长度出参必填传 NULL 返回假、回调替换 iLimit 用 SIZE_MAX 表不限）。10/10 稳定性通过。内核总覆盖 66%。
- 2026-09-06：avl 43/43 + websocket_stream 38/38 达成 100%（avl_tour 见上批注；新增 websocket/stream_tour：双端直接 Attach 到回环 TCP 的全接口巡礼——发送族明文/引用/接管/压缩四类 11 个接口、控制帧 Ping 触发 AutoPong 与主动 Pong 在对端观测、Pause/Resume 流控实测滞留与到达、CloseInfo 的 SENT|RECEIVED|CLEAN 标志与远端代码、自省族含 Worker 内 Tcp/TcpRef/Tls/TlsRef 与跨线程 Deflate/Role/Protocol/Pending/Writable）。本例抓出 5 处 API 理解错误与 2 个疑似库问题（理解错误：WsStream 全部操作含 Close 都是 Worker 专用、Attach 成功即接管 TCP 引用否则双重释放、未协商 deflate 时 Compressed 发送带 RSV1 会被对端按协议错误断连、Pong 无回执须在对端观测、TextRef/SendRef 收 xnetref 结构而非散参；疑似库问题①突发发送经 TCP 高水位折叠的背压读取门在队列排空后不自动恢复且 Paused() 不反映——需 settle 后显式 Resume，②DeflateEnabled+默认参数直连时客户端接收完全停滞）。12/12 稳定性通过。内核总覆盖 65%。
（新增 containers/avl_tour：侵入式 xavl 全接口含 Find/LowerBound/UpperBound 边界语义（存在键的 UpperBound 是严格大于的下一项）、Visit 第三项返回 false 提前停止、IterRBegin/IterFrom/IterRFrom 三向迭代 + IterEnd；拥有式 xavltree 全接口含 SetDrop 资源回收器（Drop 的用户数据来自树级 userData）、Take 移出所有权、Clear 逐对象回收、InitAligned/CreateAligned 对齐变体）。编译运行验证抓出 3 处理解错误（TreeAdd 是 key 与 item 分开的四参、SetDrop 无第三参而 Drop 回调数据走树级 userData、拥有式迭代族用树自带比较器无 compare 参数）。10/10 稳定性通过。内核总覆盖 64%。
- 2026-09-06：array 50/50 达成 100%（新增 containers/array_tour：xarray 对齐内嵌形态 + Add/InsertSpace 写入式追加 + 编辑族 Push/Insert/Set/Remove/RemoveSwap/Pop/Swap/Reverse + 查找族 Find 线性等值/FindBy 谓词/BSearch + 容量族 Reserve/Resize/Trim/Clear + 堆形态 Create/CreateAligned/Destroy；xptrarray 全 22 接口含 Data/ConstData 视图、InsertMany、RemoveSwap 换删、Sort 值比较与 Find）。编译运行验证抓出 4 处理解错误（InitAligned 对齐必须整除元素大小、InsertSpace 插入未初始化槽须写返回指针而非期待零填充、Find/FindBy 是线性等值扫描与有序无关、PtrArrayFind 的参数即关键字值本身而非指向值的指针——传地址会搜索栈地址）。10/10 稳定性通过。内核总覆盖 62%。
- 2026-09-06：queue 46/46 达成 100%（新增 concurrency/queue_tour：三族无锁队列同构巡礼——SPSC 内嵌+堆双形态含 PushBatch/PopBatch/Close/Drain/Reset 全流程，MPSC 内嵌形态 + 双生产者线程对 0..999 等差数列核对，MPMC 内嵌形态 + 2×2 线程按总收发数核对；容量换算、CLOSED 流控、IsDrained 语义逐一断言）。编译运行验证抓出 3 处理解错误（PopBatch 弹到恰好清空返回 OK 而非 EMPTY、生产者在 FULL 上自旋时消费者必须并行运行否则容量有限即死锁、MPSC/MPMC 的 Reset 要求先 Close+排空）。15/15 稳定性通过。内核总覆盖 61%。
- 2026-09-06：tls_stream 52/52 达成 100%（新增 tls/listener_tour、tls/stream_tour 两个范例 + 共享 embedded_identity.h：openssl 预生成 P-256 自签名证书/SEC1 私钥嵌入常量数组，范例无需外部文件即可起真实 TLS 1.3 回环。Listener 全接口含拉取/Future/阻塞三种接受、Dial 回调式生命周期含失败/在途取消/TransportStats，Stream 双形态接入 StreamClient(STARTTLS) 与 StreamAttach(显式会话) 含 Pullup/ReadMore/Read 消费式复制/Consume/SendVec/SendVecAsync/SendBound/AsyncBytes/AsyncCount/Pending/SetEvents 热替换）。编译运行验证抓出 9 处 API 理解错误（验证器必须显式 VerifyName 否则先于回调报"requires an identity name"、AcceptAsync 的 FutureValue 是借用须自持 StreamRef 否则收尾双重释放、Attach 成功即接管 Session 引用不可再 Destroy、TLS Stream 全部 IO 含 Send/SendBound 都是 Worker 专用、SendVec 每 Span 各成一条记录导致明文分片到达 Pullup 须按可用前缀、Read 是消费式复制 Read(4)+Consume(5) 而非 Consume(9)、拉取队列中的服务端流被 Accept 消费后才开始驱动回显、收尾须先全量发起关闭再共享截止时间统一等待）。listener_tour 50/50、stream_tour 20/20 稳定性通过。内核总覆盖 60%。
- 2026-09-06：tcp 63/63 + tcp_server 15/15 达成 100%（新增 tcp_dial_tour/tcp_stream_tour/tcp_server_tour 三个范例：Dial 回调式生命周期含成功接管 Stream/失败拒连/在途取消确定性契约/ConfigValid 正反例，Stream 五种发送形态拼 53 字节已知流对端整体核对 + 读取三件套与 Socket/SetEvents/SetData 六件 Worker 专用经 xrtNetPost 批量执行 + 等待族四形态 + Pause/Resume 流控实测滞留与到达 + ShutdownWrite 对端 EOF + 统计/终态错误，Server 双端点 SharedPort 共享动态端口含 Endpoint/Listener 四查询与借用底层 Listener）。编译运行验证抓出 9 处 API 理解错误（xrtNetListen 收配置内嵌地址而非独立地址参数、ListenConfig 默认 ExclusiveAddress 与 ReuseAddress 互斥须显式清除、双端点同端口须两端点都 ReuseAddress+SharedPort、xrtMalloc 载荷含结尾零须按精确长度、xrtNetStreamRecv 返回 xnetbytes* 配 BytesView/Destroy、xrtWriteFull 四参、SendFile 用 xrtOpen+xrtWriteFull、xerror 不透明须用访问器、Close 须等终态再 Destroy 否则 EngineDestroy 失败）。内核总覆盖 59%。
- 2026-09-06：time 58/58 达成 100%（新增 calendar_tour/range_tour/text_parse 三个范例：历法查询与全字段提取对齐预计算常数、ISO 周历跨年归周年、Unix 秒/毫秒双精度换算；容差/同期/闭区间/月年周区间与单位差值含周日开周 vs 周一开周对照、SleepUs 单调时钟验证；分解结构文本 Write/Format/Parse 严格往返、%z 与 %:z 两种偏移写法、HTTP 三格式日期往返与 Try 版不动输出语义、ParseAny 长度特征分流含 14 位紧凑数字）。编译运行验证抓出 7 处 API 理解错误（FromUnix/FromUnixMs 分别只有秒/毫秒精度须用 DateTime 得全微秒、%z 收 ±HHMM 而 %:z 才收带冒号、Overlap/In 的参数序是 start 在前否则反向区间恒假、周一年初周日归 2022-W52、TimeUnix 与 TimeUnixMs 不可混比等）。内核总覆盖 58%。
- 2026-09-06：udp 73/73 达成 100%（新增 udp_send_tour/udp_introspect/udp_multicast/udp_batch 四个范例 + 扩展 udp_errors：发送族十形态含 Vec 聚集/Ref 零复制释放回调/Take 接管/Msg 逐包控制/Batch 前缀受理，自省族含 Open 双地址形态/Worker 内 Socket·SetData 经 xrtNetPost 投递/队列四视角/统计快照，多播族 Join·Leave·Loop·HopLimit·Interface 全走 Worker 任务（Windows 自回环配方：通配绑定+环回接口加入），批量族三形态含截断包 PacketTruncated/BatchPacket 借用与 BatchTake 转移/双 Ref 跨 Future 保留）。编译运行验证抓出 6 处 API 理解错误（UdpOpen 的 peer 端口不能为零、Worker 族必须经 Post 在 Worker 线程调用、多播自回环须通配绑定+环回接口、ReceiveBatch 须等 Queued 到齐才确定、Abort 后未等 CLOSED 会让 EngineDestroy 失败、FutureValue 是借用而非移交——替 Future 释放引用造成双重释放，ASan 缺席下经 gdb 定位 FLS 堆缓存链表损坏、二分法定位到 Future 批量段、0/20 复现归零）。内核总覆盖 56%。
- 2026-09-06：logger 76/76 达成 100%（新增 logger_tour/ring_async/sink_tour 三个范例：Logger 生命周期与默认实例、Ring/Async 包装器含统计与错误查询、Sink 管理族 + 字段构造五种 + 格式化 printf/V 双形态 + 校验器三件套 + 文件 Sink 运维族）。编译运行验证抓出 6 处 API 理解错误（xrtLogRelease 不存在须用 xrtLogFree、Submit 前必须 Attach 否则返回 DROPPED、AddRing/AddAsync 第三参是配置而 Sink 须先建、AsyncConfig 三字段 Capacity/RecordLimit/ByteLimit 须齐设、Ring 后 Free 再 AddAsync 会踩已释放 Logger 须独立实例、SinkStats 收 Sink 而 LogStats 收 Logger）。内核总覆盖 54%。
- 2026-09-06：charset 69/69 达成 100%（新增 utf8_search/utf8_edit/transcode_tour/utf16_32 四个范例：标量搜索族含 Case 变体、标量编辑族含 Substr 负下标、流式校验状态机跨块汉字实测 MORE→OK、UTF-16/32 全族三个层次 cstr/Buffer/View + 复制族）。编译运行验证抓出 10 处错误（To16Buffer 第四参是 Policy、ReverseTo 三参、跨块首块返回 MORE 而非 OK、Insert 语义是"位置前插入"等）。内核总覆盖 53%。
- 2026-09-06：stack 74/74 + asn1 27/27 + pem 7/7 达成 100%（新增 stack/tour、asn1/encode_tour、asn1/decode_tour、asn1/pem_tour 四个范例：五种栈全接口、DER 编码器九种追加 + OID 工具、DER 读取器类型化转换 + Peek/Remaining、PEM 流式游标 + 缓冲版）。编译运行验证抓出 8 处错误（FixedStackCreate 参数序是容量在前、StackInitAligned 对齐必须整除元素大小、BlockStackInit 单参 InitLayout 才有第三参、DerIs 四参含 bConstructed、PemRead 枚举是 XPEM_BLOCK 等）。内核总覆盖 51%（1506/2958）。
- 2026-09-06：http 59/59 达成 100%（新增 field_tour/method_tour/param_tour/token_tour/validate_tour 五个家族范例：字段块全接口含 token-list 聚合游标、方法/状态/长度/质量族、参数与 quoted-string 四动作、令牌列表与加权令牌、地址主机验证族）。编译运行验证抓出 14 处 API 理解错误（FieldBlockCount 不含终止空行、GetUnique/TokenEqual/HostValid 收出参或描述符而非视图、WeightedToken 交结构体、谓词族 Flags 必须含 HAS_VALUE、QuotedBuild 第二参必填等）。内核总覆盖 48%。
- 2026-09-06：file 97/97 达成 100%（新增 io_tour/dir_tour/link_tour/root_tour 四个家族范例：句柄 IO 全接口含区间锁/映射提交/元数据族、目录全接口含递归建链/树复制/根列表/系统根、符号链接三件套+路径属性三件套（平台差异诚实标注）、沙箱根链接/FIFO/模式/原生句柄）。编译运行验证抓出 7 处错误（DirSize 收目录不收文件、UnlockRange 三参、Write 后必须 Seek 回 0 才能读回、Windows 无开发者模式 LinkCreate 被系统拒绝→降级为能力探测输出等）。内核总覆盖 46%（1382/2958）。
- 2026-09-06：value 116/116 达成 100%（新增 array_tour/object_tour/set_tour/iter_weak 四个家族范例：数组所有权三件套全形态、对象与 IntMap 全接口、集合运算四件套+包含判定、迭代器双形态+三态步进+弱引用四件套+句柄往返+TypeId 三件套+身份策略+终结器）。编译运行验证抓出 9 处 API 理解错误（集合没有按序 Get 需用通用迭代器、IntMapEdit 只服务子容器标量报错、WeakRefLock 过期后返回非空标记——过期判定必须用 Expired 等）。内核总覆盖 44%（1329/2958）。
- 2026-09-06：map 62/62 达成 100%（新增 map_tour + int_map_tour 两个家族范例，覆盖字节键全接口 + 整数键全接口含边界查询/双向迭代/访问器改值）。编译运行验证抓出 12 处 API 理解错误（字节键入参需显式 xbytesview、GetOrInit 需配 init 回调、MapSet 第三参是 const void* 值地址、UpperBound 语义为 > 而非 >=、IterNext 两参等）。内核总覆盖 43%（1282/2958）。
- http1_tls 的 2 个 API（RequestParseTls/ResponseParseTls）需要活动 TLS 流环境（回调式 accept/dial 全链路），单文件范例强行压缩会牺牲质量——随 tls(113 缺口) 大模块批次一并处理
- 下一批：map(50)/value(50)/file(50)/http(50) 家族巡礼 → 之后 asn1(20)/pem(4)/stack(53)/charset(56) → tls/net 大块 → 扩展库
