# 范例完善工程 · 进度台账

> 目标：为全部 559 个范例（内核 292 + 扩展 267）添加非常详细的注释，
> 补齐覆盖缺口，并保证每个范例可编译、可运行。
> 本台账是唯一进度事实源，每完成一个范例立即更新。

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

### 已 100% 清零的模块（14 个，截至 2026-09-06）

string(83)·hash(9)·number(15)·core(4)·console(4)·html(3)·environment(4)·memory_debug(11)·http_connection(5)·http1_net(2)·temp(17)·cancel(9)·memory_stats(4)·net_frame(9)·map(62)·value(116)·file(97)·http(59)·stack(74)·asn1(27)·pem(7)·charset(69) —— 合计 690 API 全覆盖

★ 22 个模块 100% 清零：string·hash·number·core·console·html·environment·memory_debug·http_connection·http1_net·temp·cancel·memory_stats·net_frame·map·value·file·http·stack·asn1·pem·charset

本批新增/扩展范例：
- 新建 9 个：hash/variants、number/variants、core/version_limits、console/variants、html/variants、environment/variants、memory/fail_inject、http/connection_cursor、http1/parse_buffer
- 扩展 5 个：memory/temp(+SecureReset/SecureUnit/Reset/Trim)、concurrency/cancel(+Ref/Triggered)、memory/stats(+Enabled)、network/frame_line(+LineReset)
- 全部注册 modules.json、编译运行验证、预期输出与真实运行逐行校准
- 编译/运行环节抓出 14 处 API 理解错误（pOutputSize 必填、IntWrite 六参、NumWrite 无 Format 参数、UIntWrite 十六进制小写输出、resourcelimits 字段名 i 前缀、FailAfter(N)=第 N+1 次失败、FailClear 连触发标志一起清等）

### 遗留说明

- 2026-09-06：charset 69/69 达成 100%（新增 utf8_search/utf8_edit/transcode_tour/utf16_32 四个范例：标量搜索族含 Case 变体、标量编辑族含 Substr 负下标、流式校验状态机跨块汉字实测 MORE→OK、UTF-16/32 全族三个层次 cstr/Buffer/View + 复制族）。编译运行验证抓出 10 处错误（To16Buffer 第四参是 Policy、ReverseTo 三参、跨块首块返回 MORE 而非 OK、Insert 语义是"位置前插入"等）。内核总覆盖 53%。
- 2026-09-06：stack 74/74 + asn1 27/27 + pem 7/7 达成 100%（新增 stack/tour、asn1/encode_tour、asn1/decode_tour、asn1/pem_tour 四个范例：五种栈全接口、DER 编码器九种追加 + OID 工具、DER 读取器类型化转换 + Peek/Remaining、PEM 流式游标 + 缓冲版）。编译运行验证抓出 8 处错误（FixedStackCreate 参数序是容量在前、StackInitAligned 对齐必须整除元素大小、BlockStackInit 单参 InitLayout 才有第三参、DerIs 四参含 bConstructed、PemRead 枚举是 XPEM_BLOCK 等）。内核总覆盖 51%（1506/2958）。
- 2026-09-06：http 59/59 达成 100%（新增 field_tour/method_tour/param_tour/token_tour/validate_tour 五个家族范例：字段块全接口含 token-list 聚合游标、方法/状态/长度/质量族、参数与 quoted-string 四动作、令牌列表与加权令牌、地址主机验证族）。编译运行验证抓出 14 处 API 理解错误（FieldBlockCount 不含终止空行、GetUnique/TokenEqual/HostValid 收出参或描述符而非视图、WeightedToken 交结构体、谓词族 Flags 必须含 HAS_VALUE、QuotedBuild 第二参必填等）。内核总覆盖 48%。
- 2026-09-06：file 97/97 达成 100%（新增 io_tour/dir_tour/link_tour/root_tour 四个家族范例：句柄 IO 全接口含区间锁/映射提交/元数据族、目录全接口含递归建链/树复制/根列表/系统根、符号链接三件套+路径属性三件套（平台差异诚实标注）、沙箱根链接/FIFO/模式/原生句柄）。编译运行验证抓出 7 处错误（DirSize 收目录不收文件、UnlockRange 三参、Write 后必须 Seek 回 0 才能读回、Windows 无开发者模式 LinkCreate 被系统拒绝→降级为能力探测输出等）。内核总覆盖 46%（1382/2958）。
- 2026-09-06：value 116/116 达成 100%（新增 array_tour/object_tour/set_tour/iter_weak 四个家族范例：数组所有权三件套全形态、对象与 IntMap 全接口、集合运算四件套+包含判定、迭代器双形态+三态步进+弱引用四件套+句柄往返+TypeId 三件套+身份策略+终结器）。编译运行验证抓出 9 处 API 理解错误（集合没有按序 Get 需用通用迭代器、IntMapEdit 只服务子容器标量报错、WeakRefLock 过期后返回非空标记——过期判定必须用 Expired 等）。内核总覆盖 44%（1329/2958）。
- 2026-09-06：map 62/62 达成 100%（新增 map_tour + int_map_tour 两个家族范例，覆盖字节键全接口 + 整数键全接口含边界查询/双向迭代/访问器改值）。编译运行验证抓出 12 处 API 理解错误（字节键入参需显式 xbytesview、GetOrInit 需配 init 回调、MapSet 第三参是 const void* 值地址、UpperBound 语义为 > 而非 >=、IterNext 两参等）。内核总覆盖 43%（1282/2958）。
- http1_tls 的 2 个 API（RequestParseTls/ResponseParseTls）需要活动 TLS 流环境（回调式 accept/dial 全链路），单文件范例强行压缩会牺牲质量——随 tls(113 缺口) 大模块批次一并处理
- 下一批：map(50)/value(50)/file(50)/http(50) 家族巡礼 → 之后 asn1(20)/pem(4)/stack(53)/charset(56) → tls/net 大块 → 扩展库
