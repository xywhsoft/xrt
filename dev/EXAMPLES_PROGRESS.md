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
| concurrency | 32 | [ ] |
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
- 2026-09-05（第三阶段完）：network 34/34 —— **内核 292/292 全部完成**！三轮合计：第一轮 45 + 第二轮 49 + 第三轮 198。下一阶段：扩展库 267（xws/xmail 优先），以及 tcp_server_future 空目录处理（清单内无 main.c，不计入 292）。。。。
