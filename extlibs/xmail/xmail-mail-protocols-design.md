# xrt 邮件协议官方扩展落地设计

## 1. 总体路线

邮件协议扩展先在 xrt 的 `extlibs` 层完成，再接入 xlang。

分层边界：

- xrt extlibs: 协议、TLS、认证、MIME、错误模型、异步模型、测试。
- xlang libs: 包元信息、脚本友好 dict API、可选 JSON 边界转换、示例。
- xserver/xadmin: 配置、队列、重试、业务模板、日志持久化。

这样可以避免把协议复杂度提前带入 xlang，也能让 C/C++ 宿主直接使用邮件扩展。

第一阶段边界：

- 协议细节只在 xrt extlibs 中实现，xlang 不重新实现协议状态机、MIME parser 或 TLS/AUTH 细节。
- xserver/xadmin 的配置系统、任务队列、重试策略、业务模板和日志持久化不进入邮件 extlibs 或 xlang 第一阶段包。
- 本项目只实现邮件客户端能力，不实现完整邮件服务端。
- DKIM/DMARC/SPF 生成与校验不作为第一阶段发布门槛，后续可在独立安全扩展中补充。

## 2. 模块划分

### 2.1 `xmail_mime`

职责：

- 构建邮件 MIME message。
- 解析邮件 MIME message。
- 处理邮件 header 编码与折行。
- 处理 multipart tree。
- 为 SMTP 发信与 POP3/IMAP 收信提供共享 MIME 数据模型。

不负责：

- 不建立网络连接。
- 不处理 SMTP/POP3/IMAP 命令。
- 不读取业务配置。

建议主文件：

```text
extlibs/xmail_mime/xmail_mime.h
```

建议核心类型：

```c
typedef struct xmailaddr xmailaddr;
typedef struct xmailheader xmailheader;
typedef struct xmailmimepart xmailmimepart;
typedef struct xmailmessage xmailmessage;
typedef struct xmailmimeresult xmailmimeresult;
```

建议 API 分组：

- init/free
- header encode/decode
- body transfer encode/decode
- message builder
- message parser
- diagnostics

### 2.2 `xsmtp`

职责：

- SMTP/SMTPS/STARTTLS 连接。
- EHLO 能力解析。
- SMTP AUTH。
- envelope 投递。
- DATA 发送。
- 同步、future、协程等待 API。

与 MIME 的关系：

- `xsmtp` 不再手写复杂 MIME。
- `xsmtpmessage` 可以保留简单字段。
- 内部将 `xsmtpmessage` 转换为 `xmailmessage`，再调用 `xmail_mime` 构建 DATA 内容。
- 当前实现已经完成该适配：`xsmtp_build_message_data` 只负责地址、附件、自定义 header 等字段映射，实际 MIME 输出由 `xmailMimeBuildMessage` 生成。

兼容策略：

- 保留现有 `xrtSmtpSendMail` 系列 API。
- `xsmtpattachment` 暴露附件和 inline 资源；更通用的 MIME part tree API 后续再补。
- 原有 text/html 发送路径继续可用。

### 2.3 `xpop3`

职责：

- POP3/POP3S/STARTTLS。
- 列出消息、按 UIDL 去重、拉取原始邮件。
- 将 `RETR` 返回的原始邮件交给 `xmail_mime` 解析。

建议第一阶段 API：

- connect/login/quit
- stat/list/uidl
- retr/delete
- fetch message as raw
- fetch message parsed

### 2.4 `ximap`

职责：

- IMAP/IMAPS/STARTTLS。
- mailbox 选择与查询。
- UID 增量同步。
- FETCH 原始邮件或 MIME 结构。
- 将 BODY 内容交给 `xmail_mime` 解析。

第一阶段优先：

- `CAPABILITY`
- `LOGIN`
- `SELECT`
- `UID SEARCH`
- `UID FETCH`
- `LOGOUT`

第二阶段：

- `AUTHENTICATE XOAUTH2`
- `BODYSTRUCTURE`
- `STORE FLAGS`
- `IDLE`

## 3. 公共基础层

可以新增 `xmail_common.h`，也可以先把公共 helper 放在 `xmail_mime.h` 内部，待 POP3/IMAP 开始复用后再拆分。

公共能力：

- 字符串 buffer。
- CRLF 规范化。
- 大小写无关比较。
- line reader。
- 错误对象。
- 账号密码和 token 日志脱敏。
- 统一超时字段。

建议不要过早抽象网络 session。SMTP、POP3、IMAP 的状态机不同，初期共享 xrt 网络原语即可。

## 4. MIME 数据模型

建议使用树模型表达 MIME：

```text
message
  headers
  root_part
    headers
    content_type
    transfer_encoding
    body
    children[]
```

常见发信结构：

纯文本：

```text
text/plain
```

纯 HTML：

```text
text/html
```

文本 + HTML：

```text
multipart/alternative
  text/plain
  text/html
```

带附件：

```text
multipart/mixed
  multipart/alternative
    text/plain
    text/html
  attachment
  attachment
```

HTML 内联图片：

```text
multipart/related
  text/html
  inline image with Content-ID
```

HTML + 纯文本 + 内联图片 + 附件：

```text
multipart/mixed
  multipart/alternative
    text/plain
    multipart/related
      text/html
      inline image
  attachment
```

## 5. 编码策略

Header：

- ASCII 安全文本原样输出。
- 非 ASCII 使用 RFC 2047 encoded-word。
- 长 header 需要 folding。
- 输入 header value 禁止裸 CR/LF，除非明确处于 parser unfolding 阶段。

Body：

- 7bit 安全文本可用 `7bit`。
- 普通 UTF-8 文本优先 quoted-printable。
- 二进制附件使用 base64。
- base64 每行 76 字符。
- 所有 MIME 输出统一 CRLF。

文件名：

- 优先同时输出 ASCII fallback `filename=...` 和 RFC 5987 `filename*=UTF-8''...`。
- 邮件客户端兼容性需要实测。

## 6. 错误模型

每个协议 result 都应包含：

- `bSuccess`
- `iStage`
- `iServerCode` 或协议状态码
- `iCapabilities`
- `sError`
- `sLastReply`
- `iSysError`

当前 SMTP/POP3/IMAP 的 TLS 读写路径已把 handshake/feed/read/write 的 `xnet_result` 映射到错误文本中的 `tls=`；POP3/IMAP 的 socket `recv` 失败会映射到 `sys=`，SMTP stream 路径保留既有 sys/reason 信息。这保持 result 结构稳定，同时给真实服务商兼容性调试留下可追踪信号；后续如果 result 结构冻结，可再把这些值提升为独立字段。SMTP/POP3/IMAP config 均提供 `iTlsMaxVersion` 和 `sCaFile`，xlang dict 侧对应 `tls_max_version` 和 `ca_file`，用于服务商需要固定 TLS 版本或显式 CA bundle 时限制协商上限和指定信任源。

TLS 握手后读取应用层数据时，协议扩展必须先调用 `xrtNetTlsSessionReadPlain` 尝试解密 TLS session 内部已缓存的密文记录；只有返回 `XRT_NET_AGAIN` 才继续从 socket/stream 读取新密文。QQ IMAPS/POP3S 的 greeting 可能与 TLS server Finished 同批到达，如果只看已解密明文 pending 数量会误判为无数据并阻塞到 socket 超时。

X.509 主机名校验不应依赖固定数量的 `subjectAltName` dNSName 缓存。QQ 邮箱证书包含多个业务域名，`imap.qq.com` / `pop.qq.com` 可能排在较靠后的位置；XRT TLS 已改为叶子证书解析时对 DNS/IP SAN 即时匹配目标 hostname，只保存匹配状态，避免合法多域名证书因 SAN 列表截断而误判主机名不匹配。

建议阶段枚举：

SMTP：

- connect
- tls
- banner
- ehlo
- starttls
- auth
- mail_from
- rcpt
- data
- quit

POP3：

- connect
- tls
- greeting
- capa
- auth
- transaction
- update
- quit

IMAP：

- connect
- tls
- greeting
- capability
- auth
- select
- command
- fetch
- logout

## 7. 异步模型

建议每个协议都提供三层 API：

- 同步 API：适合测试和小工具。
- future API：适合服务端 worker。
- async wait/co API：适合 xrt 协程环境。

`xsmtp` 现有模型可以作为模板。

对 POP3/IMAP，第一阶段可以先实现同步 API，再补 future 包装；IMAP 的原生异步状态机复杂度较高，不建议第一阶段硬做完整 native async。

## 8. 测试策略

MIME 测试必须本地可跑，不依赖网络。

SMTP/POP3/IMAP 测试分两类：

- mock server 自动测试：验证协议状态机。
- 真实服务商手工测试：验证兼容性，凭据不入库。

建议测试文件：

```text
xmail_mime_test.c
xsmtp_test.c
xpop3_test.c
ximap_test.c
```

建议 fixture：

```text
tests/fixtures/
  simple_text.eml
  alternative.eml
  mixed_attachment.eml
  related_inline.eml
  encoded_headers.eml
```

## 9. xlang 接入设计

xrt API 稳定后，在 xlang 中新增扩展包。

建议包：

```text
demo5/release/libs/x.mail.mime/
demo5/release/libs/x.mail.smtp/
demo5/release/libs/x.mail.pop3/
demo5/release/libs/x.mail.imap/
```

当前四个包的 source package 已创建在 `D:\git\x-lang\demo5\release\libs`：

- `x.mail.mime`: dict helper 与 `build/parse` native JSON 入口。
- `x.mail.smtp`: `send` / `send_json` native JSON 入口。
- `x.mail.pop3`: `list` / `fetch` / `delete_message` / JSON 边界 native JSON 入口。
- `x.mail.imap`: `search` / `fetch` / `bodystructure` / `fetch_flags` / `store_flags` / `expunge` / `idle` / `watch` / JSON 边界 native JSON 入口。

这些入口已经改为调用 `xmail_xlang` 的 native JSON ABI：`xmail_mime_build_json_native`、`xmail_mime_parse_json_native`、`xmail_smtp_capability_json_native`、`xmail_smtp_send_json_native`、`xmail_pop3_capability_json_native`、`xmail_pop3_status_json_native`、`xmail_pop3_list_json_native`、`xmail_pop3_fetch_json_native`、`xmail_pop3_delete_json_native`、`xmail_imap_capability_json_native`、`xmail_imap_status_json_native`、`xmail_imap_list_json_native`、`xmail_imap_search_json_native`、`xmail_imap_fetch_json_native`、`xmail_imap_bodystructure_json_native`、`xmail_imap_fetch_flags_json_native`、`xmail_imap_store_flags_json_native`、`xmail_imap_expunge_json_native`、`xmail_imap_idle_probe_json_native`、`xmail_imap_idle_json_native`、`xmail_imap_watch_json_native`。其中 MIME build/parse 已接入真实 `xmail_mime` 能力；SMTP capability/send 已完成 dict/JSON 到 `xsmtpconfig`、`xrtSmtpCapability`、`xsmtpmessage`、DSN 和 result dict 的映射，并调用真实 `xrtSmtpSendMail`，本地 JSON ABI mock server 已覆盖 EHLO 能力查询与成功发送闭环；POP3 capability/status/list/fetch/delete_message 已完成 dict/JSON 到 `xpop3config`、`xrtPop3Capa`、`xrtPop3Stat`、`xrtPop3ListSummaries`、`xrtPop3RetrRaw/Mime`、`xrtPop3TopRaw/Mime`、`xrtPop3Delete` 和 result dict 的映射，本地 JSON ABI mock server 已覆盖 CAPA、STAT、摘要列表、MIME fetch 与 DELE 显式删除闭环；IMAP capability/status/list/search/fetch/bodystructure/fetch_flags/store_flags/expunge/idle_probe/idle/watch 已完成 dict/JSON 到 `ximapconfig`、`xrtImapCapability`、`xrtImapStatus`、`xrtImapList`、`xrtImapSearch/UidSearch`、`xrtImapUidFetch/PeekMime`、BODY literal、section raw、`HEADER.FIELDS` 头字段抓取、`xrtImapFetchBodyStructure`、`xrtImapFetchFlags`、`xrtImapStoreFlagsParsed/UidStoreFlagsParsed`、`xrtImapExpunge/UidExpunge`、`xrtImapIdleProbe`、`xrtImapIdleCollect` 和 `xrtImapIdleLoop` 路径的映射，本地 JSON ABI mock server 已覆盖 CAPABILITY、STATUS、LIST、UID search、MIME fetch、header-only fetch、BODYSTRUCTURE、flags 读取/修改、EXPUNGE/UID EXPUNGE、短 IDLE 探针、有限 IDLE 事件收集与 watch loop 闭环。后续只在不改变 dict API 主形态的前提下补真正的脚本 callback 形态。

`xmail_xlang.c` 作为多扩展组合翻译单元，会包含多个 header-only 扩展。为避免 GCC `-Wunused-function` 被未调用的公开 static API 刷屏，该翻译单元只在 include 区域局部抑制该警告；真实类型转换、const/signedness 和 native JSON ABI 逻辑仍保持普通 `-Wall -Wextra` 检查。

xlang API 原则：

- 不暴露复杂 C 指针生命周期。
- 主形态采用 dict API：配置、请求参数、返回结果优先使用 dict/array/string。
- JSON 仅作为边界兼容或高级批量输入输出，不作为第一主形态。
- 错误返回包含 `ok/error/stage/server_code/last_reply`。
- 详细字段白名单、返回 dict、callback 和禁止暴露的 C 内部字段见 `xmail-xlang-api-contract.md`。

SMTP 示例形态：

```xl
import x.mail.smtp as smtp

auto ret = smtp.send({
  "host": "smtp.example.com",
  "port": 587,
  "secure": "starttls",
  "user": "user@example.com",
  "password": "app-password",
  "from": {"email": "user@example.com", "name": "Sender"},
  "to": [{"email": "receiver@example.com"}],
  "subject": "hello",
  "text": "plain body",
  "html": "<p>html body</p>"
})
```

POP3 示例形态：

```xl
import x.mail.pop3 as pop3

auto client = pop3.open(config)
auto ids = client.uidl()
auto msg = client.retr(ids[0])
```

IMAP 示例形态：

```xl
import x.mail.imap as imap

auto client = imap.open(config)
client.select("INBOX")
auto uids = client.uid_search("UNSEEN")
auto msg = client.fetch_message(uids[0])
```

## 10. 推荐实施顺序

1. 建立 `xmail_mime` 目录、README、测试骨架。
2. 完成 MIME 基础编码：encoded-word、quoted-printable、base64 wrap、CRLF。
3. 完成 MIME builder：plain/html/alternative/mixed/attachment。
4. 回改 `xsmtp` 使用 `xmail_mime`。
5. 完成 SMTP 附件与 XOAUTH2。
6. 新增 `xpop3` 同步 API。
7. 新增 `ximap` 同步 API。
8. 补 future/async wait。
9. 补真实服务商兼容性记录。
10. 接入 xlang 包。

## 11. 关键确认点

- 已保留现有 `xsmtp.h` 文件名风格，并统一采用 `xmail_mime.h`、`xpop3.h`、`ximap.h` 作为 extlibs 单头文件入口。
- 邮件扩展默认不纳入核心 `xrt.h` 自动生成，以 extlibs 单头文件方式按需包含；当前已新增独立 `xmail/xmail.h` 聚合头，用于一次性包含 MIME、SMTP、POP3、IMAP 与 xlang native ABI 声明。
- xlang API 第一主形态已确认采用 dict API，JSON 仅作为可选边界。
- xlang 层已确认不暴露 xrt 邮件 extlibs 的 C 指针、socket、TLS session、future 句柄、parser 临时状态和结构体完整字段，只暴露脚本友好的 dict/array/string/bytes/bool/number。
- IMAP 第一阶段是否必须支持 IDLE。
- XOAUTH2 token 获取是否只接收外部 token，不在 xrt 内置 OAuth 浏览器流程。
