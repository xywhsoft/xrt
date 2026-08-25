# xrt 邮件协议官方扩展 Spec

状态标记：

- `[ ]` 未开始
- `[x]` 已完成
- `[~]` 进行中
- `[!]` 需确认

## 1. 目标

[x] 在 `D:\git\xrt\extlibs` 下形成官方邮件协议扩展族，先完成 xrt 原生能力，再接入 xlang。当前 `xmail_mime`、`xsmtp`、`xpop3`、`ximap` 与聚合入口 `xmail` 均已落地并通过本地回归。

[x] 提供可复用的邮件 MIME 层，供 SMTP、POP3、IMAP 共用。当前 SMTP DATA 构建、POP3 RETR/TOP MIME 解析和 IMAP UID FETCH MIME 解析均已复用 `xmail_mime`。

[x] 将现有 `xsmtp` 从单一发信库升级为依赖共享 MIME 层的官方 SMTP 扩展。当前 DATA MIME 构建与附件 API 已复用 `xmail_mime`，并已补 AUTH XOAUTH2、8BITMIME、SMTPUTF8、SIZE 与 DSN envelope 参数；TLS handshake/feed/read/write 错误已补充 `tls=` 诊断码；SMTP/POP3/IMAP config 已补 `sCaFile` 显式 CA bundle 输入，默认 peer verify 已通过 QQ SMTP 587 STARTTLS、IMAPS 993 与 POP3S 995 真实服务商验证。

[x] 新增 `xpop3`，支持 POP3/POP3S/STARTTLS 收信。当前已支持明文同步客户端、POP3S 隐式 TLS 连接路径、本地 mock server 测试、dot-stuffing 解码和 RETR 基础 MIME 解析接入；已修复 TLS 握手后内部密文缓冲未先解密导致的 greeting 超时问题，并已补阻塞 TLS 握手内部 pending 数据继续 drive 逻辑；QQ POP3S 995 在限制 TLS 1.2 的真实服务商测试中已完成 LIST/RETR/MIME 解析闭环，本机 WSL2 Dovecot 已完成 POP3 110 STLS 收信闭环。

[x] 新增 `ximap`，支持 IMAP/IMAPS/STARTTLS 邮箱同步。当前已建立官方扩展骨架，支持明文同步客户端、IMAPS 隐式 TLS、IMAP STARTTLS、本地 mock server 测试、tag 命令、untagged response、literal 网络读取、CAPABILITY、LOGIN、AUTHENTICATE XOAUTH2、常用邮箱命令 raw API 与 UID FETCH MIME 基础解析接入；已修复 TLS 握手后内部密文缓冲未先解密导致的 greeting 超时问题，并已补阻塞 TLS 握手内部 pending 数据继续 drive 逻辑；QQ IMAPS 993 在限制 TLS 1.2 的真实服务商测试中已完成 SELECT/UID SEARCH/UID FETCH MIME 闭环，本机 WSL2 Dovecot 已完成 IMAP 143 STARTTLS 收信闭环和真实 IDLE `EXISTS` 事件样本；pipelined command 已提供 begin/finish API 和 pending response 缓冲，mock 测试覆盖乱序 tagged response。

[x] 在 xrt 扩展稳定后，再添加 xlang 包：`x.mail.mime`、`x.mail.smtp`、`x.mail.pop3`、`x.mail.imap`。当前已在 `D:\git\x-lang\demo5\release\libs` 下创建四个 source package 骨架，采用 dict API 合同形态；已新增 `xmail_xlang` native JSON ABI 并让 xlang 包调用 native JSON 入口，四个 package manifest 的 exports 已与源码 `export` 列表核对一致。MIME build/parse 已接入真实 `xmail_mime` 能力；SMTP capability/send 已完成 dict/JSON 到 `xsmtp` 的 native 映射并覆盖 EHLO 能力查询、无网络受控失败与 JSON ABI mock server 成功发送闭环；POP3 capability/status/list/fetch/delete_message 已完成 dict/JSON 到 `xpop3` 的 native 映射并通过 JSON ABI mock server 能力查询、邮箱状态、收信和显式删除闭环；IMAP capability/status/list/search/fetch/bodystructure/fetch_flags/store_flags/expunge/idle_probe/idle/watch 已完成 dict/JSON 到 `ximap` 的 native 映射并通过 JSON ABI mock server 能力查询、邮箱状态、邮箱目录、搜索、MIME fetch、header-only fetch、BODYSTRUCTURE、flags 读取/修改、EXPUNGE/UID EXPUNGE、短 IDLE 探针、有限 IDLE 事件收集和 watch loop 闭环；网络配置 dict 已补 `tls_max_version` 与 `ca_file`。第一阶段按已确认的 dict API 主形态返回批量事件，不跨 native JSON ABI 传脚本 callback。

## 2. 非目标

[x] 第一阶段不直接在 xlang 中实现协议细节。当前已在 `xmail-xlang-api-contract.md` 和设计文档中明确：xlang 只做 dict API 包装，SMTP/POP3/IMAP/MIME/TLS/AUTH 协议行为保留在 xrt extlibs。

[x] 第一阶段不绑定 xserver/xadmin 的配置系统或任务队列。当前设计文档已将 xserver/xadmin 的配置、队列、重试、业务模板和日志持久化划为上层职责，不进入邮件 extlibs 或 xlang 第一阶段包。

[x] 第一阶段不实现完整邮件服务端，只做客户端协议。当前 extlibs 和 xlang API 合同均只覆盖 MIME 构建/解析、SMTP 发信、POP3 收信、IMAP 同步/收信客户端能力。

[x] 第一阶段不强制支持 DKIM/DMARC/SPF 生成与校验，可作为后续扩展。当前已在设计和 xlang API 合同中明确 DKIM/DMARC/SPF 不作为第一阶段发布门槛，后续可作为独立安全扩展补充。

## 3. 目录结构

[x] 最终目录命名已确认。当前已创建并认可 `xmail`、`xmail_mime`、`xsmtp`、`xpop3`、`ximap` 目录，并按以下结构落地，命名冻结为正式发布名：

```text
extlibs/
  xmail/
    README.md
    xmail-mail-protocols-spec.md
    xmail-mail-protocols-design.md
    xmail-xlang-api-contract.md
    xmail.h
    xmail_xlang.h
    xmail_xlang.c
    xmail_xlang_test.c
    xmail_aggregate_test.c
    xmail_live_test.c
    build_test.bat
  xmail_mime/
    xmail_mime.h
    README.md
    build_test.bat
    xmail_mime_test.c
  xsmtp/
    xsmtp.h
    README.md
    build_test.bat
    xsmtp_test.c
  xpop3/
    xpop3.h
    README.md
    build_test.bat
    xpop3_test.c
  ximap/
    ximap.h
    README.md
    build_test.bat
    ximap_test.c
```

[x] 当前 extlibs README 已修正为使用实际单头文件命名：`xsmtp.h`、`xmail_mime.h`、`xpop3.h`、`ximap.h`。

## 4. xmail_mime

### 4.1 基础编码

[x] 实现 RFC 2047 encoded-word 编码：`=?UTF-8?B?...?=`。

[x] 实现 RFC 2047 encoded-word 解码。

[x] 实现 quoted-printable 编码。

[x] 实现 quoted-printable 解码。

[x] 实现 base64 76 字符换行包装，底层复用 `xrtBase64Encode`。

[x] 实现 CRLF 规范化。

[x] 实现 header 注入防护，拒绝不允许的 CR/LF。

### 4.2 Header

[x] 实现邮件 header folding。

[x] 实现邮件 header unfolding。

[x] 支持同名 header 多值。`xmailmessage.arrHeaders` 支持追加自定义 header，`xmailparsedmessage.arrHeaders` 保留解析出的同名 header，并提供 `xmailMimeParsedGetHeaderCount`、`xmailMimeParsedGetHeaderAt`、`xmailMimeParsedJoinHeaders` 查询 API。

[x] 支持 `Date`、`Message-ID` 生成。

[x] 支持地址显示名编码与邮箱地址格式化。

[x] 支持地址列表格式化和解析：`From`、`To`、`Cc`、`Reply-To`。当前已支持 `xmailMimeFormatAddress/List` 与 `xmailMimeParseAddress/List`，覆盖 `Name <mail>`、quoted display name、encoded-word display name、裸邮箱和逗号分隔列表。

[x] 支持 `Bcc` 只参与 SMTP envelope，不写入 MIME header。

### 4.3 MIME Part 模型

[x] C API 同时提供常用扁平 builder 和高级 part tree builder。`xmailmessage` 保持为发信常用 API；`xmailmimepart` 可用于解析树，也可通过 `xmailMimeBuildPart` 构建递归 MIME entity。

[x] 定义 `xmailmimepart`。当前用于解析侧 part tree，包含 header、Content-Type 摘要、boundary、Content-Disposition、filename/filename*、Content-ID、body 和 children。

[x] 定义 `xmailmessage`。

[x] 支持 `text/plain`。

[x] 支持 `text/html`。

[x] 支持 `multipart/alternative`。

[x] 支持 `multipart/mixed`。

[x] 支持 `multipart/related`。

[x] 支持附件 `Content-Disposition: attachment`。

[x] 支持 inline 资源 `Content-ID`。

[x] 支持递归 multipart 构建。当前 `xmailmessage` 已支持发信常用的 mixed + alternative + related + attachment 组合；高级场景可用 `xmailmimepart` 树和 `xmailMimeBuildPart` 构建任意嵌套 multipart entity，本地测试已覆盖 mixed 嵌套 alternative 的构建与回读解析。

[x] 支持递归 MIME message 解析。当前已支持 raw message 的 header/body 基础解析、folded header 展开、multipart part tree 递归解析、part 附件结构化字段解析、叶子 part 的 quoted-printable/base64 transfer 解码，以及 `message/rfc822` 基础递归解析。

[x] 支持 `message/rfc822` 作为后续兼容点。当前解析为 part 下的 `pMessage` 子邮件树。

### 4.4 与 xrt 复用

[x] 复用 `xrtHttpHeader*` 做 header 行基础解析与构建。当前 MIME header 行解析优先使用 `xrtHttpHeaderSplitLine`，自定义 header 输出使用 `xrtHttpHeaderBuildLineTo` 做基础构建与控制字符校验；邮件专用 folding/encoded-word 逻辑保留在 `xmail_mime`。

[x] 复用 `xrtHttpMediaType*` 做 `Content-Type` 参数解析。当前 MIME part 解析优先使用 `xrtHttpMediaTypeParse` 和 `xrtHttpMediaTypeFindParam` 解析 media type、subtype、boundary 与 name 参数，保留邮件兼容 fallback。

[x] 复用 `xrtHttpContentDisposition*` 做文件名参数解析。当前 MIME part 元数据解析优先使用 `xrtHttpContentDispositionParse` 和 `xrtHttpContentDispositionDecodeFileNameTo` 解析 disposition 与 filename/filename*，保留邮件兼容 fallback。

[x] 复用 `xrtMultipart*` 或抽取其 boundary 扫描能力。当前 multipart part tree 解析优先使用 `xrtMultipartNextN` 复用边界扫描与 header/body 视图切分；当遇到邮件合法但 HTTP form-data 专用解析不接受的场景时，自动回退到 MIME 专用边界扫描器。

[x] 复用 `xrtBase64Encode/Decode`。

[x] 复用 `xrtIsUTF8` 与 charset 转换工具。当前 MIME 构建侧对主题、地址显示名、文本正文、自定义 header 值和附件文件名执行 UTF-8 校验，非法 UTF-8 会拒绝构建；charset 转换仍作为后续非 UTF-8 输入扩展点。

## 5. xsmtp

[x] 已有 SMTP/SMTPS/STARTTLS 基础发送。

[x] 已有 `AUTH PLAIN`。

[x] 已有 `AUTH LOGIN`。

[x] 已有同步发送 API。

[x] 已有 future 异步 API。

[x] 已有协程等待封装。

[x] 已有 To/Cc/Bcc envelope 投递。

[x] `xsmtp` 的 DATA MIME 构建已迁移到 `xmail_mime`。

[x] 支持附件发送。`xsmtpmessage` 已暴露 `xsmtpattachment`，并映射到 `xmail_mime` 的附件/inline MIME builder。

[x] 支持 `AUTH XOAUTH2`。

[x] 支持 `SMTPUTF8` 能力识别。

[x] 支持 `8BITMIME` 能力识别。

[x] 支持 `SIZE` 能力识别与消息大小检查。

[x] 支持 `DSN` 能力识别与 envelope 参数。当前 `xsmtpmessage` 支持 `sDsnReturn` / `sDsnEnvid` 映射到 `MAIL FROM RET=` / `ENVID=`，`xsmtpaddr` 支持 `sDsnNotify` / `sDsnOrcpt` 映射到 `RCPT TO NOTIFY=` / `ORCPT=`；同步和 future 发送路径复用同一命令构建逻辑，mock server 已覆盖。

[x] 支持更细错误分类：connect、TLS、EHLO、STARTTLS、AUTH、MAIL FROM、RCPT、DATA。`xsmtpresult.iStage` 使用 `XSMTP_STAGE_*` 表达失败阶段；TLS handshake/feed/read/write 失败会返回 `tls=` 诊断码，socket/stream 失败保留既有 sys/reason 信息。

[x] 提供 `xsmtp` 与 `xmail_mime` 的集成测试。`xsmtp_mime_test.c` 覆盖 DATA MIME 输出、Bcc header 隐藏、multipart mixed/alternative/related、附件、inline CID、header 注入拒绝，以及本地 mock server 的明文发送、DSN envelope 参数、`AUTH PLAIN`、`AUTH LOGIN`、`AUTH XOAUTH2` 和 `AUTH_AUTO` 的 XOAUTH2/PLAIN/LOGIN 选择流程。

## 6. xpop3

[x] 支持 POP3 明文连接。当前明文同步客户端使用阻塞 socket 路径，本地 mock server 已覆盖。

[x] 支持 POP3S 995 隐式 TLS。当前已接入阻塞 socket + `xtlssession` 的隐式 TLS 连接路径；POP3/IMAP/SMTP 配置均新增 `iTlsMaxVersion`，QQ POP3S 995 使用 `tls1.2` 已完成真实 LIST/RETR/MIME 解析闭环；失败结果会区分 TLS 状态机错误码与 socket 系统错误码。

[x] 支持 POP3 STARTTLS 110。当前已在 greeting 后、认证前发送 `STLS` 并用 `xtlssession` 升级阻塞 socket 后续读写；QQ POP3 110 实测不支持 `STLS`，本机 WSL2 Dovecot 已完成 POP3 110 STLS 登录、LIST、RETR 与 MIME 解析闭环。

[x] 支持 `CAPA`。已由本地 mock server 自动测试覆盖。

[x] 支持 `USER/PASS`。已由本地 mock server 自动测试覆盖。

[x] 支持 `AUTH PLAIN`。当前 `xpop3config.iAuthMode` 可显式选择 `XPOP3_AUTH_PLAIN`，发送 POP3 SASL `AUTH PLAIN` 初始响应；默认 `XPOP3_AUTH_AUTO` 保持 USER/PASS 兼容路径，mock server 已覆盖。

[x] 支持 `STAT`。已由本地 mock server 自动测试覆盖。

[x] 支持 `LIST`。已由本地 mock server 自动测试覆盖。

[x] 支持 `UIDL`。已由本地 mock server 自动测试覆盖。

[x] 支持 `RETR`。raw 多行读取、dot-stuffing 解码和基础 MIME 解析接入已由 mock server 覆盖。

[x] 支持 `TOP`。`xrtPop3TopRaw` 和 `xrtPop3TopMime` 已支持按邮件编号和正文行数抓取头部/预览正文，mock server 已覆盖 raw、MIME 解析与 dot-stuffing。

[x] 支持邮件摘要列表。`xrtPop3ListSummaries` 已组合 `LIST` / `UIDL` / `TOP 0`，返回编号、大小、UID、Subject、From、Date 摘要，mock server 已覆盖。

[x] 支持 `DELE`。已由本地 mock server 自动测试覆盖。

[x] 支持 `NOOP`。已由本地 mock server 自动测试覆盖。

[x] 支持 `RSET`。已由本地 mock server 自动测试覆盖。

[x] 支持 `QUIT`。已由本地 mock server 自动测试覆盖。

[x] 支持 `RETR` 原始邮件交给 `xmail_mime` 解析。`xrtPop3RetrMime` 调用 `xmailMimeParseMessage` 返回 `xmailparsedmessage`。

[x] 支持同步 API。当前已提供并测试明文 open/auth-plain/stat/capa/list/uidl/list-summaries/retr/top/delete/noop/rset/quit API，POP3S 995 真实服务商闭环已通过，本机 WSL2 Dovecot 已完成 POP3 110 STLS 收信闭环；TLS handshake/feed/read/recv 失败会返回更具体的 `tls=` / `sys=` 诊断；POP3 `RETR`/`TOP` 默认不删除邮件，只有显式 `DELE` 或 future `bDeleteAfterRetr` 才触发删除。

[x] 支持 future 异步 API。当前提供 `xrtPop3FetchRawFuture`、`xrtPop3FetchMimeFuture`、`xrtPop3TopRawFuture`、`xrtPop3TopMimeFuture`、`xrtPop3ListSummariesFuture` 与对应 `AsyncWait` 封装，线程 future 内完成 open / RETR、TOP 或摘要列表 / optional DELE / QUIT；mock server 已覆盖 fetch raw、fetch MIME、TOP raw、TOP MIME 与摘要列表路径，且 `bDeleteAfterRetr` 不作用于 TOP 预览。

## 7. ximap

[x] 支持 IMAP 明文连接。当前明文同步客户端使用阻塞 socket 路径，本地 mock server 已覆盖。

[x] 支持 IMAPS 993 隐式 TLS。当前已接入 `xtlssession` 的隐式 TLS 握手和 TLS 包装读写路径，复用既有命令/literal 解析；POP3/IMAP/SMTP 配置均新增 `iTlsMaxVersion`，QQ IMAPS 993 使用 `tls1.2` 已完成真实 SELECT/UID SEARCH/UID FETCH MIME 闭环；失败结果会区分 TLS 状态机错误码与 socket 系统错误码。

[x] 支持 IMAP STARTTLS 143。当前已在 greeting 后、认证前发送 tagged `STARTTLS` 并用 `xtlssession` 升级后续命令/literal 读写；QQ IMAP 143 实测未完成 greeting/STARTTLS 流程，本机 WSL2 Dovecot 已完成 IMAP 143 STARTTLS 登录、SELECT、UID SEARCH、UID FETCH MIME 闭环。

[x] 支持 tag 命令状态机。当前已支持 tag 生成、tagged command 构建、tagged status line 解析与按 tag 读取命令响应；新增 `xrtImapCommandBegin` / `xrtImapCommandFinish` 支持 pipelined command，client 内部会暂存先完成的非目标 tag 响应，mock server 已覆盖两个命令乱序完成时按 tag 取回响应。

[x] 支持 untagged response 解析。当前已支持基础 atom/rest、FETCH FLAGS 与 BODYSTRUCTURE 递归 part tree 解析；叶子 part 已提取 body parameter、id、description、encoding、octets、TEXT lines、md5、disposition、language 与 location；multipart part 已提取 parameter、disposition、language 与 location；`message/rfc822` part 已提取 envelope 原文、嵌套 bodystructure 与 message lines；APPLICATION attachment leaf 已覆盖 mock 样本。

[x] 支持 literal 数据读取。当前已支持 literal marker `{n}` 解析，并在命令响应读取中按长度读取 literal 数据，本地 mock server 已覆盖。

[x] 支持 `CAPABILITY`。已由本地 mock server 自动测试覆盖。

[x] 支持 `LOGIN`。已由本地 mock server 自动测试覆盖。

[x] 支持 `AUTHENTICATE XOAUTH2`。当前使用 continuation 两步 SASL 流程，已由本地 mock server 自动测试覆盖。

[x] 支持 `STATUS`。当前提供 `xrtImapStatusRaw` 与 `xrtImapStatus`，固定查询 MESSAGES、RECENT、UIDNEXT、UIDVALIDITY、UNSEEN，并复用 `ximapmailboxstatus` 返回结构化状态。

[x] 支持 `SELECT` / `EXAMINE` raw 响应。已由本地 mock server 自动测试覆盖；`xrtImapSelectStatus` / `xrtImapExamineStatus` 已提供 `ximapmailboxstatus` 结构化解析，提取 EXISTS、RECENT、UIDVALIDITY、UIDNEXT 与 read-only/read-write 状态。

[x] 支持 `LIST` raw 响应。已由本地 mock server 自动测试覆盖；`xrtImapList` 已提供 `ximaplist` 结构化 mailbox 列表解析，提取 flags、delimiter 与 mailbox。

[x] 支持 `SEARCH` raw 响应。已由本地 mock server 自动测试覆盖；`xrtImapSearch` 已提供 `ximapsearchids` 结构化 ID 列表解析。

[x] 支持 `UID SEARCH` raw 响应。已由本地 mock server 自动测试覆盖；`xrtImapUidSearch` 已提供 `ximapsearchids` 结构化 UID 列表解析。

[x] 支持 `FETCH FLAGS`。当前已覆盖 raw 响应与 `ximapflags` 结构化解析，本地 mock server 已覆盖。

[x] 支持 `FETCH BODY[]` raw 响应。当前已覆盖基础 UID FETCH BODY[] raw 响应、literal 网络读取、`xrtImapUidFetchBodyLiteral` 纯 RFC822 literal 抽取、`xrtImapUidFetchMime` MIME 解析接入并复用 `xmail_mime` 输出递归 multipart tree、`xrtImapUidFetchBodySectionRaw` 安全 section token 抓取（如 BODY[HEADER]、BODY[1.MIME]、BODY[1.HEADER.FIELDS (...)]），以及 `xrtImapUidFetchHeaderFieldsRaw` / `xrtImapUidFetchHeaderFields` 指定 header 字段抓取；同时已补 `BODY.PEEK[]` / `BODY.PEEK[...]` 不隐式置已读路径，提供 `xrtImapUidPeekBodyRaw`、`xrtImapUidPeekBodyLiteral`、`xrtImapUidPeekBodySectionRaw`、`xrtImapUidPeekHeaderFieldsRaw`、`xrtImapUidPeekHeaderFields` 与 `xrtImapUidPeekMime`，mock server 已覆盖 raw、literal、MIME、multipart tree、PEEK、section、复杂 section 字段列表与 header-fields 路径。

[x] 支持 `FETCH BODYSTRUCTURE`。当前已支持 raw 响应、`ximapbodystructure` 递归 part tree 解析、multipart subtype、multipart parameter/disposition/language/location、子 part type/subtype、body parameter、id、description、encoding、octets、TEXT lines、md5、disposition、language 与 location，以及 `message/rfc822` envelope 原文、嵌套 bodystructure 与 message lines 提取；mock server 已覆盖 multipart alternative、message/rfc822 和 application/pdf attachment leaf。

[x] 支持 `STORE +FLAGS/-FLAGS`。当前已覆盖 sequence `STORE` 与 `UID STORE` 的 raw 响应和 `ximapflags` 结构化解析，`+FLAGS`/`-FLAGS` mock 流程均已覆盖。

[x] 支持 `EXPUNGE`。当前提供 `xrtImapExpungeRaw`、`xrtImapExpunge`、`xrtImapUidExpungeRaw` 与 `xrtImapUidExpunge`，本地 mock server 已覆盖 sequence/UID expunge 的 `* EXPUNGE` raw 响应、tagged OK 与 `ximapexpungeresult` 结构化序号列表解析。

[x] 支持 `LOGOUT`。已由本地 mock server 自动测试覆盖。

[x] 支持 `IDLE` 作为第二阶段能力。当前已支持短探针 `xrtImapIdleProbe`，同步 one-shot `xrtImapIdleOnce`，`xrtImapIdleCollect` 在一次 IDLE 会话中收集指定数量 untagged 事件后发送 `DONE` 退出，以及 `xrtImapIdleWatch` 通过 callback 逐条处理事件并可由 callback 返回 `false` 取消当前 IDLE 会话；已补 `xrtImapIdleCollectFuture` / `xrtImapIdleCollectAsyncWait` 有限异步收集，`xrtImapIdleWatchFuture` / `xrtImapIdleWatchAsyncWait` 单连接后台 watch 封装，以及 `xrtImapIdleLoop` 基础重连/backoff loop、全局停止 callback、trace callback 与周期业务心跳 callback 并覆盖 mock 测试；QQ IMAPS 993 在 peer verify + TLS 1.2 下已通过 `IDLE` 探针验证，本机 WSL2 Dovecot 已通过 `ximap_dovecot_idle_live_test.c` 覆盖真实 Maildir 写入后服务端推送 `EXISTS` 事件。

[x] 支持同步 API。当前已提供并测试明文 open/login/xoauth2/capability/status/select/select-status/examine/examine-status/list/list-items/search/uid-search/search-ids/uid-search-ids/fetch/uid-fetch-body/uid-peek-body/uid-fetch-body-section/uid-peek-body-section/uid-fetch-header-fields/uid-peek-header-fields/fetch-flags/fetch-bodystructure/store/store-flags/uid-store/uid-store-flags/expunge/expunge-result/uid-expunge/uid-expunge-result/idle-once/idle-collect/idle-watch/idle-loop/logout API，IMAPS 993 真实服务商闭环已通过，本机 WSL2 Dovecot 已完成 IMAP 143 STARTTLS 收信闭环；TLS handshake/feed/read/recv 失败会返回更具体的 `tls=` / `sys=` 诊断；IMAP 已读 `\Seen` 不影响后续 `UID FETCH`，只有显式删除标记和 expunge 才删除。

[x] 支持 future 异步 API。当前提供 `xrtImapStatusFuture`、`xrtImapListFuture`、`xrtImapSearchFuture`、`xrtImapUidSearchFuture`、`xrtImapFetchFlagsFuture`、`xrtImapStoreFlagsFuture`、`xrtImapUidStoreFlagsFuture`、`xrtImapExpungeFuture`、`xrtImapUidExpungeFuture`、`xrtImapIdleCollectFuture`、`xrtImapIdleWatchFuture`、`xrtImapFetchBodyStructureFuture`、`xrtImapUidFetchBodyRawFuture`、`xrtImapUidFetchBodyLiteralFuture`、`xrtImapUidFetchMimeFuture`、`xrtImapUidPeekBodyRawFuture`、`xrtImapUidPeekBodyLiteralFuture`、`xrtImapUidPeekMimeFuture`、`xrtImapUidFetchBodySectionRawFuture`、`xrtImapUidPeekBodySectionRawFuture`、`xrtImapUidFetchHeaderFieldsRawFuture`、`xrtImapUidFetchHeaderFieldsFuture`、`xrtImapUidPeekHeaderFieldsRawFuture`、`xrtImapUidPeekHeaderFieldsFuture` 与对应 `AsyncWait` 封装，线程 future 内完成 open / STATUS、LIST、SEARCH、FETCH FLAGS、STORE/UID STORE FLAGS、EXPUNGE/UID EXPUNGE、IDLE collect/watch、BODYSTRUCTURE、optional SELECT / UID FETCH 或 UID FETCH BODY.PEEK / LOGOUT，mock server 已覆盖 status、list、uid-search、fetch-flags、store-flags、uid-expunge、idle-collect、idle-watch、bodystructure、fetch raw、fetch literal、fetch MIME、peek raw、body-section raw 与 header-fields literal 路径。

## 8. xlang 接入

[x] 等 xrt extlibs API 稳定后，新增 `x.mail.mime` 包。当前已创建 source package，导出 `address`、`attachment`、`message`、`build`、`parse`、`build_json`、`parse_json`；`build/parse` 已通过 `xmail_mime_build_json_native` / `xmail_mime_parse_json_native` 接入真实 `xmail_mime` 构建与解析能力，并通过本地 native JSON ABI 测试。

[x] 新增 `x.mail.smtp` 包。当前已创建 source package，导出 `capability`、`send`、`capability_json` 与 `send_json`，package manifest 已同步导出列表，返回统一 dict 结果；`xmail_smtp_capability_json_native` 已解析 SMTP config 并调用真实 `xrtSmtpCapability` 返回 EHLO capability 字符串数组、mask 和 SIZE limit，`xmail_smtp_send_json_native` 已解析 `config/message/dsn` dict 并调用真实 `xrtSmtpSendMail`，返回 `xsmtpresult` 映射后的稳定 result dict。当前已覆盖 EHLO 能力查询、无网络受控失败路径与 JSON ABI mock server 成功发送闭环。

[x] 新增 `x.mail.pop3` 包。当前已创建 source package，导出 `capability`、`status`、`list`、`fetch`、`delete_message`、`capability_json`、`status_json`、`list_json`、`fetch_json`、`delete_message_json`，package manifest 已同步导出列表，返回统一 dict 结果；`xmail_pop3_capability_json_native` 已解析 POP3 config 并调用真实 `xrtPop3Capa` 返回 capability 字符串数组和 mask，`xmail_pop3_status_json_native` 已解析 POP3 config 并调用真实 `xrtPop3Stat` 返回邮箱消息数与总大小，`xmail_pop3_list_json_native` 已解析 POP3 config 并调用真实 `xrtPop3ListSummaries`，`xmail_pop3_fetch_json_native` 已解析 `config/request` dict 并调用真实 `xrtPop3RetrRaw` / `xrtPop3RetrMime` / `xrtPop3TopRaw` / `xrtPop3TopMime`，`xmail_pop3_delete_json_native` 已解析 `config/request` dict 并调用真实 `xrtPop3Delete` 后 `QUIT` 提交删除，返回稳定 result dict；JSON ABI mock server 已覆盖 CAPA、STAT、list、MIME fetch 收信与 DELE 显式删除闭环。

[x] 新增 `x.mail.imap` 包。当前已创建 source package，导出 `capability`、`status`、`list`、`search`、`fetch`、`bodystructure`、`fetch_flags`、`store_flags`、`expunge`、`idle_probe`、`idle`、`watch` 与 JSON 边界函数，返回统一 dict 结果；`xmail_imap_capability_json_native` 已解析 config dict 并调用真实 `xrtImapCapability` 返回 capability 字符串数组和 mask，`xmail_imap_status_json_native` 已解析 config/request dict 并调用真实 `xrtImapStatus` 返回邮箱摘要，`xmail_imap_list_json_native` 已解析 config/request dict 并调用真实 `xrtImapList` 返回邮箱目录，`xmail_imap_search_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapSearch` / `xrtImapUidSearch`，`xmail_imap_fetch_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapUidFetch/PeekMime`、BODY literal、section raw 与 `HEADER.FIELDS` 指定头字段路径，`xmail_imap_bodystructure_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapFetchBodyStructure` 返回递归 BODYSTRUCTURE dict，`xmail_imap_fetch_flags_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapFetchFlags`，`xmail_imap_store_flags_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapStoreFlagsParsed/UidStoreFlagsParsed`，`xmail_imap_expunge_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapExpunge/UidExpunge` 返回 expunged sequence 列表，`xmail_imap_idle_probe_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapIdleProbe` 做短探针验证，`xmail_imap_idle_json_native` 已解析 `config/request` dict 并调用真实 `xrtImapIdleCollect` 做有限事件收集，`xmail_imap_watch_json_native` 已解析 watch loop dict 并调用真实 `xrtImapIdleLoop` 返回事件与 summary；JSON ABI mock server 已覆盖 CAPABILITY、STATUS、LIST、UID search、MIME fetch、header-only fetch、BODYSTRUCTURE、flags 读取/修改、EXPUNGE/UID EXPUNGE、IDLE probe、IDLE collect 与 watch loop 闭环。第一阶段不跨 native JSON ABI 传脚本 callback，按 dict API 返回批量事件和 summary。

[x] xlang API 主形态确认采用 dict API。配置、请求参数和返回结果优先使用 dict/array/string；JSON 只作为边界兼容或高级批量输入输出，不作为第一主形态。

[x] xlang 层只做脚本友好包装，不实现协议细节。协议、MIME、网络、认证和错误模型保留在 xrt extlibs。

[x] xlang 层不暴露 xrt 内部结构体所有字段。当前已新增 `xmail-xlang-api-contract.md`，明确 xlang 只暴露 dict/array/string/bytes/bool/number 形态，禁止直接暴露 C 指针、socket、TLS session、future 句柄、parser 临时状态、AUTH payload 和结构体完整字段；协议能力、阶段和错误统一映射为稳定字符串与 dict 字段。

[x] xlang 返回值优先使用 dict 形式表达状态和错误，包含 `ok/error/stage/server_code/last_reply` 等字段；JSON 作为可选序列化边界。

## 9. 测试要求

[x] `xmail_mime` 添加纯本地测试，不依赖外部邮件服务。

[x] MIME 测试覆盖 encoded-word。

[x] MIME 测试覆盖 quoted-printable。

[x] MIME 测试覆盖 multipart mixed/alternative/related 嵌套。

[x] MIME 测试覆盖附件文件名 UTF-8 编码。

[x] MIME 测试覆盖 header 注入防护。

[x] `xmail_xlang` 添加本地 native JSON ABI 测试，覆盖 MIME JSON 构建、MIME raw 解析、SMTP 进入真实 `xsmtp` 的 capability/send 无网络受控失败路径和 mock server 成功发送闭环、POP3 进入真实 `xpop3` 的 capability/status/list/fetch/delete mock server 收信与显式删除闭环，以及 IMAP 进入真实 `ximap` 的 capability/status/list/search/fetch/header-only fetch/bodystructure/fetch_flags/store_flags/expunge/idle_probe/idle collect/watch loop mock server 同步闭环；GCC/TCC 均已通过，GCC 组合编译输出已收敛到无本地接入层警告。`xmail_xlang_test.c` 与 `xmail_live_test.c` 已通过 `xmail.h` 聚合头获取声明；`xmail/build_test.bat` 还会运行 `xmail_aggregate_test.c` 验证聚合头，并编译 `xmail_live_test.c` 但不运行，确保真实服务商测试入口不依赖凭据也能保持编译健康。2026-05-15 本轮复测：Windows GCC `xmail/build_test.bat` 通过；Windows TCC `xmail_xlang_test`、`xmail_aggregate_test` 运行通过，`xmail_live_test.c` 编译通过但未运行。

[x] SMTP 测试区分本地 mock server 和真实服务商/本机 Postfix 手工验证。当前已有本地 DATA MIME 集成测试与 SMTP command mock server，覆盖 EHLO、DSN、AUTH PLAIN、AUTH LOGIN、AUTH XOAUTH2、AUTH_AUTO 的 XOAUTH2/PLAIN/LOGIN 选择分支、MAIL FROM、RCPT TO、DATA、QUIT 明文发送流程；QQ SMTP 587 STARTTLS 已在默认 peer verify 下完成真实发信到自身并收到邮件，TLS 错误结果已补 `tls=` 诊断；显式 CA bundle 输入已接到 config；本机 WSL2 Postfix 已完成无认证明文 SMTP 投递到 Maildir，并由 Dovecot IMAP/POP3 收信闭环验证。

[x] POP3 测试区分本地 mock server 和真实服务商/本机 Dovecot 手工验证。当前已具备本地 mock server 自动测试；QQ POP3S 995 使用 `tls1.2` 已完成 LIST/RETR/MIME 解析闭环；QQ POP3 110 不支持 `STLS`，本机 WSL2 Dovecot 已完成 POP3 110 STLS 登录、LIST、RETR 与 MIME 解析闭环。

[x] IMAP 测试区分本地 mock server 和真实服务商/本机 Dovecot 手工验证。当前已具备本地 mock server 自动测试；QQ IMAPS 993 使用 `tls1.2` 已完成 SELECT/UID SEARCH/UID FETCH MIME 闭环，并已通过 `XMAIL_LIVE_CHECK_IDLE=1` 验证 `IDLE` 探针；QQ IMAP 143 未完成 STARTTLS 流程，本机 WSL2 Dovecot 已完成 IMAP 143 STARTTLS 登录、SELECT、UID SEARCH、UID FETCH MIME 闭环，`ximap_dovecot_idle_live_test.c` 已完成真实 IDLE `EXISTS` 事件样本。

[x] 每个扩展库保留 `build_test.bat`。当前 `xmail` 聚合测试、`xmail_mime`、`xsmtp`、`xpop3` 与 `ximap` 均已具备。

[x] 每个扩展库 README 记录能力矩阵。当前 `xmail_mime`、`xsmtp`、`xpop3`、`ximap` 均已补能力矩阵。

## 10. 发布门槛

[x] Windows GCC 编译通过。当前 `xmail`、`xmail_mime`、`xsmtp`、`xpop3` 与 `ximap` 的 `build_test.bat` 已通过，`xmail_xlang` JSON ABI GCC/TCC 组合测试已通过，`xmail_live_test.c` 已纳入编译检查但不自动运行；`xmail_xlang` 已局部抑制 header-only 扩展未使用函数噪音并清理 const/signedness 警告；QQ SMTP/IMAPS/POP3S 真实 peer verify 闭环已通过，本机 WSL2 Postfix + Dovecot 已覆盖 SMTP 投递、IMAP 143 STARTTLS、POP3 110 STLS 与 IMAP IDLE 真实事件样本。Windows 侧访问 WSL SMTP 25 时以当前 WSL IP 直连更稳定，避免依赖 `127.0.0.1` 端口代理。2026-05-15 本轮复测：`xmail_mime`、`xsmtp`、`xpop3`、`ximap`、`xmail` 全部 `build_test.bat` 通过。

[x] Windows TCC 兼容性评估通过。当前 `xmail_mime`、`xsmtp_mime`、`xpop3`、`ximap` 本地测试均已使用 `tcc.exe` 编译并运行通过；`xmail_xlang_test` 与 `xmail_aggregate_test` 已使用 TCC 编译并运行通过，`xmail_live_test.c` 已使用 TCC 编译通过但不运行；POP3/IMAP 已移除对 Windows TCC 缺失的 `strtoull` 符号依赖。2026-05-15 本轮复测同样通过。

[x] Linux GCC 编译通过。此前已在 `/opt/projects/xrt` 使用 Linux GCC 编译并运行 `xmail_mime`、`xsmtp_mime`、`xpop3`、`ximap` 本地测试通过；仍有来自 `singlehead/xrt.h` 的既有 warning，未阻断邮件扩展测试。2026-05-15 本轮检查当前 WSL Debian 可用但 `/opt/projects/xrt` 不存在，因此未重跑 Linux GCC。

[x] 无真实凭据进入仓库。当前 README 与测试只使用 `example.com`、`app-password`、`replace-with-app-password`、mock token 等占位符；真实服务商验证需使用未跟踪本地配置或环境变量。

[x] README 包含安全建议。当前 `xmail` 总览、`xmail_mime`、`xsmtp`、`xpop3`、`ximap` 均已记录凭据、TLS、日志、附件文件名等安全注意事项。

[x] README 包含最小示例。当前 `xmail_mime`、`xsmtp`、`xpop3`、`ximap` 均已提供最小同步或异步调用示例。

[x] API 命名稳定。当前 extlibs 入口统一采用目录名对应单头文件，C API 采用 `xrtSmtp*`、`xmailMime*`、`xrtPop3*`、`xrtImap*` 前缀，结果/配置/异步选项命名已与 README 和测试对齐。

[x] xrt singlehead 集成策略确认。邮件扩展默认不打入核心 `xrt.h`，以 `extlibs` 单头文件方式按需包含；当前已新增独立 `xmail/xmail.h` 聚合头，用于一次性包含 MIME、SMTP、POP3、IMAP 与 xlang native ABI 声明，并已在 `xmail_xlang_test.c`、`xmail_live_test.c` 和 `xmail_aggregate_test.c` 中覆盖编译。

[x] 邮件扩展默认不打入 singlehead，以 extlibs 单头文件方式分发。
