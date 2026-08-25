# XRT 修复轮遗留问题清单（正式修复工单）

发布日期：2026-08-25
评估基线：`master aff1ac5a` + 当前工作区修复改动（182 文件）
来源：《XRT 代码库深度审计报告》（2026-08-24）、《XRT 扩展库深度审计报告》（2026-08-25）、
《修复效果评估报告》（2026-08-25）。本清单是评估报告第三节"必须处理的问题"的展开，
每项给出现象、重现方式、根因、代码位置与修复方案，作为下一轮修复的工单。

## 汇总

| 编号 | 标题 | 严重级别 | 状态要求 |
| --- | --- | --- | --- |
| R-01 | test_tls_negotiate 红灯：套件/身份兼容语义收紧未同步测试，且方向存疑 | 发布阻断 | 必须处理 |
| R-02 | SMTP STARTTLS 失败分支读取陈旧响应码（与 POP3 修复不对称） | 低 | 应修复 |
| R-03 | websocket.h 注释与 message.c 默认值矛盾未澄清 | 低（文档） | 应修复 |
| R-04 | Engine 停机第二轮 drain 无迭代上限 | 低 | 应修复 |
| R-05 | RFC 2047 编码词 `charset*language` 后缀被整体当 charset 名拒绝 | 低 | 应修复 |
| R-06 | MIME singleton 重复头导致整实体解析失败（严格契约） | — | 已关闭：文档化 |
| R-07 | IMAP literal 大小无可配置上限（低层流式契约） | — | 已关闭：文档化 |
| R-08 | SSH 端口转发：消息层缺少 0–65535 校验 | 低 | 应修复 |
| R-09 | known_hosts CHANGED 判定不跨算法 | 低 | 排期决策 |
| R-10 | 根清单与 xmail 清单的 xlang 集成状态不同（作用域不同） | — | 已关闭：说明 |
| R-11 | 三个新 fuzz harness 无持久化种子语料库 | 加固 | 已完成 |

---

## R-01【发布阻断】TLS 套件/身份兼容语义收紧未同步测试，且方向存疑

**现象**
全量测试套件在 `test_tls_negotiate` 失败：

```text
[FAIL] TLS 1.2 ECDHE_ECDSA rejected an EdDSA identity
```

`xrtTlsCipherCompatible(XTLS_VERSION_12, XTLS_ECDHE_ECDSA_AES_128_GCM_SHA256,
XTLS_IDENTITY_ED25519)` 现在返回 false，而既有测试仍断言其为 true。

**重现方式**

```sh
python tools/build.py --compiler gcc --arch x64 --suite all --test test_tls_negotiate
# 或直接运行：./out/gcc/x64/all/test_tls_negotiate.exe
```

**问题根因**
本轮修复落实审计建议"TLS 1.2 ServerKeyExchange 签名方案应与协商套件的认证族一致"时，
把收紧同时应用到了 `xrtTlsCipherCompatible` 的套件×身份兼容表：从 ECDSA 认证族中
移除了 `XTLS_IDENTITY_ED25519 / ED448`（negotiate.c，对照基线 diff），但
`tests/tls/test_tls_negotiate.c:108-112` 的既有断言仍要求 ECDHE_ECDSA 套件接受
Ed25519 身份，两者矛盾。修复者自验只执行了 7 个子集套件
（thread_key_context、net_buffer、task_net_oom_tests、tls_client_tests、
x509_path_rsa_tests、http1_message、websocket_deflater），未覆盖本测试，全量门禁红。

语义方向本身存疑：TLS 1.2 没有 Ed25519 专用套件，OpenSSL 等主流实现允许
Ed25519 证书搭配 `ECDHE_ECDSA` 套件完成 TLS 1.2 握手（RFC 8422 的 `ed25519`
签名方案通过 signature_algorithms 协商）。当前收紧会拒绝这类合法部署形态，
属于超出原审计建议范围（原建议针对服务端 SKE 签名与套件族不一致）的兼容性回退。

**代码位置**
- `src/tls/negotiate.c:462-467`（ECDSA 分支已移除 ED25519/ED448）
- `tests/tls/test_tls_negotiate.c:108-117`（断言旧行为）
- 关联：`src/tls/client12.c:233-241`（SKE 签名族绑定，本部分正确，应保留）

**修复方案**（二选一，推荐方案 A）
- 方案 A（推荐）：回退 `xrtTlsCipherCompatible` 对 ED25519/ED448 的排除，
  恢复"ECDSA 套件接受 EdDSA 身份"的兼容表；SKE 签名方案的强验证保留在
  `client12.c` 的签名族绑定处（对端签名仍必须与其证书密钥类型匹配）。
  测试无需改动即恢复绿。
- 方案 B：坚持收紧语义——更新 `test_tls_negotiate.c:108-112` 断言为
  `!xrtTlsCipherCompatible(...)`，并在 `include/xrt/tls.h` 与
  `docs/api/tls.md` 显式声明：本实现不允许 TLS 1.2 Ed25519 证书搭配
  ECDHE_ECDSA 套件（与 OpenSSL 行为的差异及迁移指引）。

**验证要点**
方案 A：全量套件恢复绿 + 新增用例"TLS 1.2 + ECDHE_ECDSA 套件 + Ed25519 证书 +
SKE 以 ed25519 方案签名"端到端通过（对照 OpenSSL s_server 互操作）。
方案 B：测试改绿 + 文档评审通过。

---

## R-02 SMTP STARTTLS 失败分支读取陈旧响应码

**现象**
`xrtSmtpClientOpen` 在 Security 要求 STARTTLS 时，若 STARTTLS 命令在发送/接收
阶段失败，失败分支读取的 `Reply.Code` 不是本次命令的结果，而是上一次 EHLO/问候
阶段的 250 码。由于 250 != 220，`(Reply.Code != 220)` 恒真，在
`xrtGetError() == NULL` 的路径上会把失败归类为"意外 SMTP 应答码"，掩盖真实原因；
同时也无法区分"未收到应答"与"收到非 220 应答"两种错误。注：Open 内
`smtp_client.c:402` 已有 `memset(&Reply, 0, ...)`，因此这不是未初始化读 UB，
而是陈旧值导致的错误分类污染。

**重现方式**
构造一个先正常问候、随后在 STARTTLS 写入阶段切断连接（或注入网络层失败）的
SMTP 服务器，调用 `xrtSmtpClientOpen(Security = STARTTLS)`；观察错误码为
`XERR_PROTOCOL "unexpected SMTP reply code"` 而非底层 IO 错误。
静态复现：对照 `xrtSmtpClientCommand`（`smtp_client.c:671-699`）的早期失败
路径（视图校验、命令模式、行构造、发送），这些路径均不写 `*pReply`。

**问题根因**
`xrtSmtpClientCommand` 在多条早期失败路径直接 `return false` 而不写
`*pReply`；调用方 `xrtSmtpClientOpen` 的 STARTTLS 分支使用
`!Command(...) || (Reply.Code != 220) || ...` 短路链，命令失败后仍读取
`Reply.Code`。同型的 POP3 路径本轮已修复（`pop3_client.c:276` 起：命令前
memset + 失败分支拆分），SMTP 侧漏修，行为不对称。

**代码位置**
- `extlibs/xmail/src/smtp/smtp_client.c:445-469`（STARTTLS 分支与失败读取，读取点约 :464）
- `extlibs/xmail/src/smtp/smtp_client.c:389`（Reply 声明）、`:402`（既有 memset）
- 参照已修复实现：`extlibs/xmail/src/pop3/pop3_client.c:276-312`

**修复方案**
照抄 POP3 修复模式：
1. 在 STARTTLS `xrtSmtpClientCommand` 调用前补 `memset(&Reply, 0, sizeof(Reply));`
   （清掉 EHLO 的 250，使"命令失败"路径读到 0）；
2. 拆分短路链：命令本身失败 → 直接 Destroy 返回；`Reply.Code != 220` →
   `__xrtSmtpClientUnexpected()` + Destroy 返回；STARTTLS 升级/重新 EHLO 失败 →
   Destroy 返回。与 `pop3_client.c:276-312` 结构对齐。

**验证要点**
新增用例：EHLO 成功后 STARTTLS 阶段注入发送失败/非 220 应答/连接重置三种场景，
断言错误码分别为底层 IO 错误与 `unexpected SMTP reply code`，且不出现类别串扰。

---

## R-03 websocket.h 注释与 message.c 默认值矛盾未澄清

**现象**
公共头 `include/xrt/websocket.h:610-612` 对 `xrtWsMessageConfigInit` 的契约注释
写"初始化默认消息上限……"，而实现 `src/websocket/message.c:26-28` 将
`MaxSize` 初始化为 `SIZE_MAX`（无上限）。直接组合帧层+消息层的使用者按注释
理解会误以为存在默认上限，在不可信输入下形成无界消息 DoS 面（stream 层
默认 1 MiB 覆盖了该值，故走 `xrtWsStream*` 的用户不受影响）。

**重现方式**
阅读对照两处代码即可；行为复现：以默认 config 直接驱动 `xrtWsMessage*`
状态机重组一个远超预期大小的分片消息，不会被拒绝。

**问题根因**
本轮修复采纳了"流式层不限长、拥有型层必须限长"的分层结论，但只停留在
验证文档中，未回写到公共头注释，注释与实现的矛盾原样保留。

**代码位置**
- `include/xrt/websocket.h:610-612`（注释）
- `src/websocket/message.c:26-28`（`pConfig->MaxSize = SIZE_MAX;`）
- 关联分层基线：`include/xrt/websocket_stream.h:42`
  （`XWS_STREAM_MESSAGE_LIMIT_DEFAULT = 1 MiB`）、
  `src/http/http_decode.c:337`（流式解码默认不限 + 拥有型 xhttp 层 64 MiB）

**修复方案**
重写 `xrtWsMessageConfigInit` 注释，明确三层契约：
1) 消息层是无聚合的流式状态机，默认不设消息上限，调用方必须自行设置
`MaxSize` 或由上层拥有型组件约束；
2) stream 层默认 1 MiB（`XWS_STREAM_MESSAGE_LIMIT_DEFAULT`）；
3) 同步检查 `include/xrt/http_decode.h` 中 `OutputLimit` 默认值的注释是否
同样明确了"流式不限、拥有型受限"语义，保持两族 API 文档口径一致。

**验证要点**
文档评审；`tools/check_api_docs.py` 通过；无行为改动。

---

## R-04 Engine 停机第二轮 drain 无迭代上限

**现象**
Worker 停止序列在 `TimersClose` 之后增加了不动点 drain 循环（本轮 H2 修复的
组成部分）。该循环无迭代上限，且 `__xrtNetEnginePostInternal`（engine.c:2042）
不经过 `SubmitEnter` 门控——公开 `xrtNetPost` 已被门控拦截，但内部状态机的
关闭任务仍可在停机期间互相转发 Post。若未来某个内部状态机形成转发环
（A 的 close 回调 post B，B 的 close 回调 post A），停机会从"泄漏 Post"退化为
"永久挂起"，worker 线程无法退出。

**重现方式**
当前无已知可达转发环（静态不可复现）。防御性复现：在任意两个内部对象的
close/finish 回调里互相调用 `__xrtNetEnginePostInternal`，调用
`xrtNetEngineStop` 观察 worker 线程永不退出。

**问题根因**
不动点循环 `while (InternalDrain(...) != 0 || CommandsDrain(...) != 0)` 假设
drain 必然收敛，但没有防御性上限；内部 Post 通道在停机期无配额。

**代码位置**
- `src/network/engine.c:951-953`（TimersClose 后的 drain 循环）
- `src/network/engine.c:947-949`（TimersClose 前的同型循环，一并加上限）
- `src/network/engine.c:2042`（`__xrtNetEnginePostInternal`，无门禁）

**修复方案**
1. 为两个停机 drain 循环加迭代上限（建议 1024 轮，正常关闭远用不到）；
超限时记 `Stats` 错误（如复用 `WakeErrors` 风格新增 `ShutdownStalls`），
Debug 构建断言、Release 构建跳出循环并保留首错——宁可暴露问题也不无限挂起；
2. 可选加固：停机期给 `PostInternal` 计数配额（每轮 drain 上限已由
`InternalDrain(pWorker, SIZE_MAX)` 的第二个参数天然支持，改为传入有限值即可）。

**验证要点**
新增测试：构造互相转发的两个内部任务 → Stop 在有限时间内返回且
`ShutdownStalls > 0`；现有 `test_net_engine` 停机/重启用例全部保持通过。

---

## R-05 RFC 2047 编码词 `charset*language` 后缀被整体当作 charset 名

**现象**
合法的带语言子标签编码词（RFC 2047 §2 明确允许并"鼓励实现忽略语言子标签继续
解码"），例如 `Subject: =?UTF-8*en?Q?hello?=`，在默认严格模式下被整体判为
"unsupported RFC 2047 charset"而解析失败；RELAXED 模式下保留原文。

**重现方式**

```c
xrtMailWordDecode("=?UTF-8*en?Q?hello?=", XMAIL_WORD_DEFAULT, ...)
/* 期望：解码为 "hello"；实际：错误（charset 不支持） */
```
或投递一封带 `=?ISO-8859-1*fr?Q?...?=` 主题的真实邮件调用 word 解码。

**问题根因**
`__xrtMailWordParseBody` 以第一个 `?` 结束 charset 记号，把
`UTF-8*en` 整体（含 `*en`）作为 charset 名传给 `__xrtMailCharsetSupported`
（mail_word.c:94 `Word.Charset = ...`），后缀未剥离，字符集表查不到。

**代码位置**
- `extlibs/xmail/src/mail/mail_word.c:94`（charset 记号切分，未处理 `*`）
- `extlibs/xmail/src/mail/mail_word.c:574-611`（`__xrtMailWordDecodeOne` 的
  supported 判定与 RELAXED 分流）
- 字符集表：`extlibs/xmail/src/mail/mail_charset.c:30-51`

**修复方案**
在切分 charset 记号时按第一个 `*` 拆分：前半作为 charset 名参与
supported 判定与转码，后半（语言子标签）忽略（RFC 2047 允许）；对语言部分只做
可打印字符合法性校验，不改变解码结果。同时在
`tests/mail/test_mail_word.c` 增加 `=?UTF-8*en?Q?...?=`、
`=?ISO-8859-1*fr?B?...?=` 正反用例。

**验证要点**
上述两个用例严格模式解码成功；无语言后缀与带后缀的同 charset 输出逐字节一致。

---

## R-06 MIME singleton 重复头导致整实体解析失败

> **2026-08-26 处置变更：采纳修复端"不修改解析行为"的建议，本项按方案 B 关闭，仅保留文档动作。**
> 裁定依据（经 RFC 原文核实）：RFC 2045 §3 的 entity-headers 语法为 `[ content ] [ encoding ] [ id ] ...`，
> 每个 entity header 至多出现一次——重复即语法违规，且全文没有"取第一个"的处理指引；
> RFC 5322 §4.5 原文 "Except for destination address fields (described in section 4.5.3),
> the interpretation of multiple occurrences of fields is unspecified"——多重 singleton
> 的解释是**未定义**，不是"忽略后续"（仅目的地地址字段有合并 SHOULD）。本工单原方案 A
> 所引"RFC 精神取第一个"不成立，系对客户端实现惯例的误引，予以更正。
> 严格拒绝是保守的安全取向，且低层 Header API（`xrtMailMessageHeader` 的 Index/Duplicate
> 出参）保留了完整枚举重复头的逃生通道（`mail_tree.c:161` 正是以此探测重复）。
> **关闭结果**：`include/xrt/mail_tree.h` 与 `docs/api/mail_tree.md` 已写明重复 singleton
> 导致整树解析失败是有意的严格契约，并保留低层 Header API 作为自定义取舍路径。
> 只有真实邮件语料证明必要时再评估显式宽松标志，默认维持严格。

**现象**
`xrtMailTreeParse` 对 Content-Type/Content-Transfer-Encoding 等 singleton 头的
重复出现直接报 `XMAIL_ERROR_HEADER "MIME entity has a duplicate singleton
field"` 并使整个实体（乃至整封邮件的树解析）失败，而不是按 RFC 2045 §5/§7
的精神取第一个。畸形或恶意的一行重复头即可让邮件"安全但不可展示"。

**重现方式**
构造头部含两行 `Content-Type: text/plain;` 的邮件调用 `xrtMailTreeParse`，
返回错误。

**问题根因**
`__xrtMailTreeHeader` 在取到第 0 个字段后主动探测第 1 个同名字段，命中即报错
（fail-closed 严格策略）。这是设计立场而非笔误，本轮修复未采纳"取第一个"的
建议；但与 RFC 5322 §3.6"重复 singleton 应忽略后续"的宽容指引存在差距，
属需要明确决策的互操作项。

**代码位置**
- `extlibs/xmail/src/mail/mail_tree.c:160-175`（singleton 探测与报错）
- 同型：`extlibs/xmail/src/mail/mail_param.c:677-688`（参数重复同样报错）
- 关联：`extlibs/xmail/src/mail/mail_message.c:288-300`

**修复方案**（按产品取向二选一）
- 方案 A（宽松，推荐与 M5 审计建议一致）：singleton 重复时取第一个、忽略
  后续，可通过 `XMAIL_TREE_RELAXED` 类既有风格旗标提供严格模式开关（默认宽松）；
- 方案 B（维持严格）：在 `include/xrt/mail_tree.h` 与 `docs/api/mail_tree.md`
  显式文档化"重复 singleton 头将导致整树解析失败"是有意行为及理由
  （不可信输入下的确定性优先），关闭该项。

**验证要点**
方案 A：重复头邮件解析为第一个头的语义 + 严格模式仍报错的用例；
方案 B：文档评审。

---

## R-07 IMAP literal 大小无可配置上限

> **2026-08-26 处置变更：采纳修复端"不修复"的建议，本项按文档+测试关闭，不加默认上限。**
> 裁定依据（经代码与 RFC 核实）：
> 1. 逃生通道真实存在——`xrtImapClientAbort`（imap_client.c）直接调用
>    `__xrtMailTransportAbort`，不受 `LiteralRemaining` 门控（该门只拦截新命令），
>    调用方在收到带 `HasLiteral` 的事件后可立即检查 `Event.Literal.Size` 并中止会话；
>    叠加 transport deadline/cancel，会话不存在永久锁死。
> 2. RFC 9051 §1.3/附录 D 确认协议支持 63 位正文/消息大小——低层流式客户端默认
>    64 MiB 上限会破坏合规大对象的流式传输。低层原语不发明拥有型策略，与本轮确立的
>    "流式层不限长、拥有型层限长"分层原则（见 R-03）一致；本工单原方案与该原则自相
>    矛盾，予以修正。便利层 64 MiB 预算（imap_message.c:411-425）保持不变。
> **关闭结果**：`include/xrt/imap_client.h` 与 `docs/api/imap_client.md` 已写明低层流式接口
> 不设置默认 literal 上限，调用方必须在 `HasLiteral` 后检查 `Event.Literal.Size`，超出
> 自身预算可立即 `xrtImapClientAbort`；拥有型便利层仍维持独立的 64 MiB 预算。本项按
> 产品契约说明关闭，不新增低层策略字段，也不改变协议状态机。

**现象**
`xrtImapClientReceive` 直接采用服务器声明的 literal 字节数
（`{N}`）进入流式接收状态；N 为任意 size_t（仅受 SIZE_MAX 约束）。恶意/被劫持
服务器声明 `{999999999999}` 即可让会话长期停留在 literal 接收状态（期间拒绝
一切新命令，`imap_client.c:681`），直到 deadline/cancel。便利层
（`xrtImapClientBodyWrite`）读取前有 64 MB 预算兜底，但直接使用底层
`Receive/ReadLiteral` 的调用方没有防线。

**重现方式**
本地起一个假 IMAP 服务器，对 FETCH 响应声明
`* 1 FETCH (BODY[] {1099511627776}`（1 TiB）但不发送数据，调用
`xrtImapClientReceive`：客户端进入 literal 等待，连接被独占直至超时。

**问题根因**
`xrtImapLiteralParse`（imap.c）仅做数值合法性校验；`imap_client.c:871`
`pClient->LiteralRemaining = pEvent->Literal.Size;` 无上限比较；配置结构
（`ximapclientconfig`）没有 literal 上限字段。

**代码位置**
- `extlibs/xmail/src/imap/imap_client.c:865-875`（LiteralRemaining 赋值）
- `extlibs/xmail/src/imap/imap_client.c:681`（literal 期间封锁命令）
- `extlibs/xmail/src/imap/imap.c`（`xrtImapLiteralParse`）
- 配置：`extlibs/xmail/include/xrt/imap.h`（ximapclientconfig）
- 兜底参照：`extlibs/xmail/src/imap/imap_message.c:411-425`（64 MB 预算）

**修复方案**
1. `ximapclientconfig` 新增 `LiteralLimit`（默认对齐便利层 64 MB，
   `SIZE_MAX` 显式表示不限制并文档化风险）；
2. `imap_client.c:871` 处比较声明值与上限，超限时以协议错误失败并关闭连接
   （RFC 9051 §4.3 允许客户端对过大 literal 断开）；
3. 便利层预算改为读取同一配置项，消除两套上限。

**验证要点**
假服务器声明超限 literal → 客户端立即返回结构化错误而非等待；
正常大小 literal 收发不受影响；现有 imap 测试全绿。

---

## R-08 SSH 端口转发：消息层缺少 0–65535 端口校验

**现象**
本轮修复在高级 API（`ssh_client_forward.c`，`xsshClientForwardPort` 校验
`iPort > UINT16_MAX` 拒绝，0 保留给动态监听）补齐了端口范围校验，但消息编解码层
`ssh_forward_message.c` 读写路径仍接受任意 uint32 端口原样上线。绕过高级 API
直接使用消息层的调用者可以构造端口字段非法（如 70000）的
direct-tcpip/tcpip-forward 报文，错误只能在服务器侧暴露。

**重现方式**
直接调用 `ssh_forward_message.c` 的写出接口构造 `Forward.Port = 70000` 的
消息并序列化：成功编码，无任何错误。

**问题根因**
校验只加在高级 API 入口，消息层作为公共可复用层未做语义范围校验
（线格式本身是 uint32，合法；TCP 端口语义上限 65535 未检查）。

**代码位置**
- `extlibs/xssh/src/connection/ssh_forward_message.c:86-94`（读路径
  `xsshForwardGlobalRead`，Port 无范围检查）、`:171-181`（`xsshForwardOpenRead`）、
  写出侧 `:52-53`、`:135-138`
- 已修复的高级层：`extlibs/xssh/src/session/ssh_client_forward.c:27-33`
  （`xsshClientForwardPort`）及 :145/:208/:227 调用点

**修复方案**
把范围校验下沉到消息层：在 `xsshForwardGlobalRead`/`xsshForwardOpenRead`
消费完 Port 字段后校验 `Port > 65535` 即返回 `XSSH_ERROR_PROTOCOL`
（读路径拒绝非法报文）；写出侧在构造前同样校验并返回
`XSSH_ERROR_ARGUMENT`。高级层校验保留（快速失败），两层一致。

**验证要点**
新增消息层用例：编码/解码含 70000 端口的 direct-tcpip 与 tcpip-forward
报文均被拒绝；0 端口（动态分配）仍被接受。

---

## R-09 known_hosts CHANGED 判定不跨算法

**现象**
known_hosts 中主机只有不同算法（如 RSA/ECDSA）的旧密钥时，服务器改用新的
ed25519 密钥连接，判定结果为 `TRUST_NEW`（未知主机）而非 `TRUST_CHANGED`
（密钥已变更）。在调用方采用 TOFU（自动接受新主机）策略时，MITM 场景下的
提示强度从"密钥已变更"（硬告警）降级为"新主机"（常被自动接受）。

**重现方式**
1. 写入 known_hosts：`example.com ssh-rsa AAAA...（旧 RSA 公钥）`；
2. 以持有全新 ed25519 密钥的服务器（或 MITM）发起连接，
   `xrtSshKnownHostDbCheck` 返回 `XSSH_KNOWN_HOST_NEW` 而非
   `XSSH_KNOWN_HOST_CHANGED`。
对照：写入 `example.com ssh-ed25519 AAAA...（旧 ed25519 密钥）` 时能正确返回
CHANGED。

**问题根因**
`ssh_known_host_db.c:350-356` 的 CHANGED 分支以
`xsshKeyTextEqual(Entry.KnownHost.Algorithm, PublicKey.Algorithm)` 为前置
条件——只有"算法相同且密钥不同"才记 CHANGED。OpenSSH `check_host_key` 的语义
是：主机名命中任一行且没有任何一行密钥匹配 → HOST_CHANGED（算法无关）。

**代码位置**
- `extlibs/xssh/src/key/ssh_known_host_db.c:344-356`（判定分支）
- 注意兼容面：一个主机合法持有多种算法密钥的场景应保持"任一密钥匹配即 OK"
  （当前 `bKeyMatch` 逻辑已如此），修复只影响 CHANGED 判定。

**修复方案**
去掉 CHANGED 分支的算法相等前置：主机名匹配的行只要密钥不匹配即记
CHANGED 候选（保留"首个匹配行的优先级语义"与 revoked/CA 的现有处理顺序）。
新增用例覆盖：单算法旧钥换算法、多算法混合、多算法全不匹配三种输入。

**验证要点**
上述三种输入分别得到 CHANGED / OK / CHANGED；revoked、@cert-authority、
哈希主机用例全部不回归。

---

## R-10 根 config/modules.json 的 xlang 集成状态与 xmail 侧漂移

> **2026-08-26 处置变更：采纳修复端"不修复"的建议，本项关闭，撤回跨清单一致性断言。**
> 裁定依据：作用域论证成立——根清单 `external_integrations.xlang` 描述的是 xlang
> 产品与 XRT **核心**的集成关系（README：XLang 标准库/运行时绑定作为独立产品任务
> 维护）；xmail 清单的同名条目描述的是 **xmail 自身**绑定归档后的状态（deferred）。
> 两处条目描述的是两对不同关系，本就不要求一致；强制同状态反而会让扩展库状态
> 决定根产品声明。`tools/test_scope.py` 按产品分别断言（8/8 通过）的语义与此一致。
> 本工单原"跨清单同名 integration 状态必须一致"的断言基于"同一关系声明两次"的
> 误读，予以撤回。
> **关闭结果**：`docs/SCOPE.md` 已写明根清单只描述 XRT 核心产品，扩展清单只描述各自
> 产品；同名 external integration 不要求状态相同，也不得跨作用域覆盖。

**现象**
本轮已将 `extlibs/xmail` 根目录的 xlang 绑定资产（xmail_xlang.c/h、xmail.h、
build_test.bat 及三个测试/设计稿）删除并归档至
`dev/archive/mail_legacy_20260816/xmail_binding/`，xmail 侧清单同步将
xlang 集成状态改为 `"deferred"`；但仓库根 `config/modules.json:7-11` 的
`external_integrations` 中 xlang 仍声明为 `"integrated"`，两处不一致。

**重现方式**
对照阅读 `config/modules.json:7-11` 与
`extlibs/xmail/config/modules.json` 的 `external_integrations`；或运行
`tools/check_release_maturity.py` / `tools/test_scope.py` 观察声明一致性
（当前工具未交叉校验两处状态，属声明性漂移）。

**问题根因**
绑定归档时只更新了扩展库侧清单，根产品清单漏改。

**代码位置**
- `config/modules.json:7-11`（根，xlang: "integrated"）
- 参照已改：`extlibs/xmail/config/modules.json`（xlang: "deferred"）
- 归档落点：`dev/archive/mail_legacy_20260816/xmail_binding/`

**修复方案**
将根 `config/modules.json` 中 xlang 状态改为 `"deferred"`（与 xmail 侧一致）；
同时在 `tools/test_scope.py` 增加一条断言：根与各扩展清单的同名
external_integration 状态必须一致，防止再次漂移。

**验证要点**
`python tools/test_build.py`、`tools/test_scope.py` 全绿。

---

## R-11 三个新 fuzz harness 无持久化种子语料库

> **2026-08-26 修复完成。** 已建立
> `fuzz/corpus/{tls_protocol,x509_asn1,net_address}`，包含 TLS 记录边界、真实 Ed25519 DER
> 证书、非规范 DER 与数字地址种子；`tools/test_protocol_fuzz.py` 会把持久种子复制到可写
> 工作 corpus，CI 对全部协议目标执行 20000 轮 libFuzzer + ASan/UBSan，并在失败时上传
> 崩溃工件。`fuzz/README.md` 固化了复现、最小化和回流流程。本项关闭。

**现象**
本轮按审计建议新增了 `fuzz/tls_protocol.c`、`fuzz/x509_asn1.c`、
`fuzz/net_address.c`（配套 tests 为 xorshift 固定种子随机 + 少量结构化种子，
质量良好：真喂解析器、断言视图入界/游标单调）。但缺少持久化种子 corpus 目录
与 CI 中的 libFuzzer 短跑目标，长期回归发现能力有限（crash 无法沉淀为语料）。

**重现方式**
观察 `fuzz/` 目录（无 corpus 子目录）与 `.github/workflows`（无
tls/x509/net_address 的 libFuzzer 目标）；`tools/test_protocol_fuzz.py` 新增了
三个目标定义但仅覆盖内置 PRNG 种子。

**问题根因**
harness 一步到位，语料工程（seed 收集、最小化、入库）未纳入本轮范围。

**代码位置**
- `fuzz/tls_protocol.c`、`fuzz/x509_asn1.c`、`fuzz/net_address.c`
- `fuzz/`（缺 `corpus/{tls,x509,addr}/` 目录与种子文件）
- `tools/test_protocol_fuzz.py:19-64`（目标注册）
- `.github/workflows/`（无对应 fuzz 短跑 job）

**修复方案**
1. 建立 `fuzz/corpus/{tls_protocol,x509_asn1,net_address}/` 种子目录，初版
   收入：现有 tests 的结构化种子、真实 Ed25519 证书/PEM、非规范 DER 长度、
   TLS 记录边界样本（0/1/2 字节头、最大长度、密文边界）；
2. CI 增加 libFuzzer + ASan/UBSan 短跑（如各 20000 轮，与现有
   http1/websocket 门禁同规格）；
3. harness 增加 crash 语料落盘（libFuzzer 原生 `-artifact_prefix` 指向
   corpus 目录）并在 README 说明回流流程。

**验证要点**
CI fuzz job 绿；注入一个已知崩溃样本能被种子复现并最小化。

---

## 附：处理顺序建议

> 2026-08-26 更新：R-06/R-07/R-10 已按最终产品裁定补充必要说明并关闭；R-11 的
> 持久语料、sanitizer 短跑、失败工件保留和回流流程已经落地。

1. **立即**：R-01（发布阻断，先做语义决策再动代码）；
2. **本迭代收尾**：R-02、R-08（小改动，消除不对称与补校验下沉）、R-04（防御上限）；
3. **下一迭代**：R-05、R-03（文档分层口径）；
4. **决策项**：R-09（产品取向决策：OpenSSH 对齐 vs 现状），决策后按工单落地；
5. **已关闭**：R-06（严格拒绝 + 文档化）、R-07（流式不限长 + 调用方预算）、
   R-10（作用域不同，无需一致）、R-11（持久 corpus + 发布门禁）。
