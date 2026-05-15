# xrt extlibs

`extlibs` 用于放置这类扩展库：

- 不是 `xrt` 最核心的通用能力
- 但在特定业务场景里很有用
- 适合做成 header-only，直接被 `xsbase.h` 集成

当前约定：

- 每个扩展库使用独立目录，例如 `xrt/extlibs/xsmtp/`
- 对外主入口使用扩展库名对应的单头文件，例如 `xsmtp.h`、`xmail_mime.h`、`xpop3.h`、`ximap.h`
- 头文件应尽量自洽，不依赖宿主项目私有模块
- 默认不打入核心 `xrt.h`；按需从 `extlibs` 目录显式包含
- 优先复用 `xrt` 现有基础设施，例如：
  - 内存分配
  - 编码工具
  - 加密/TLS
  - 时间与网络抽象

当前已接入：

- `xsmtp`
  - 文件：`xsmtp/xsmtp.h`
  - 状态：进行中，真实服务商手工验证待补
  - 能力：SMTP/SMTPS/STARTTLS、AUTH PLAIN/LOGIN/XOAUTH2、AUTH_AUTO、EHLO 能力解析、8BITMIME、SMTPUTF8、SIZE 检查、To/Cc/Bcc envelope、Bcc header 隐藏、plain/html/alternative、附件、inline Content-ID、DATA MIME 复用 `xmail_mime`、同步 API、future 异步 API、协程等待封装、本地 MIME 与 SMTP command mock server 测试
- `xmail_mime`
  - 文件：`xmail_mime/xmail_mime.h`
  - 状态：进行中
  - 能力：CRLF 规范化、RFC 2047 encoded-word 编解码、quoted-printable 编解码、base64 wrap、header 注入检测、header folding/unfolding、同名 header 多值、Date、Message-ID、地址格式化与解析、Reply-To、自定义 header、Bcc 排除、plain/html/alternative/mixed/related/attachment/inline message builder、通用递归 `xmailmimepart` builder、递归 MIME message/part 解析、`message/rfc822` 递归解析、附件结构化字段解析、叶子 part transfer 解码
- `xpop3`
  - 文件：`xpop3/xpop3.h`
  - 状态：进行中
  - 能力：配置/结果类型、阶段常量、基础响应解析、CAPA/STAT/LIST/UIDL 解析、多行 dot-stuffing 解码、明文 POP3 同步 API、POP3S 隐式 TLS 连接路径、STARTTLS/STLS 升级路径、RETR raw、RETR MIME 解析、DELE/NOOP/RSET/QUIT、future 异步 API、本地 mock server 测试
- `ximap`
  - 文件：`ximap/ximap.h`
  - 状态：进行中
  - 能力：配置/结果类型、阶段常量、tag 命令生成、tagged status 解析、untagged response 基础解析、literal marker 与 literal 网络读取、IMAPS 隐式 TLS 连接路径、STARTTLS 升级路径、CAPABILITY/LOGIN/AUTHENTICATE XOAUTH2、SELECT/EXAMINE/LIST/SEARCH/UID SEARCH、FETCH FLAGS、FETCH BODY[] raw、安全 BODY section raw 抓取、FETCH BODYSTRUCTURE 递归结构解析与 multipart/leaf 摘要字段、STORE +FLAGS/-FLAGS、IDLE one-shot 与批量事件收集、UID FETCH MIME、future 异步 API、本地 mock server 测试

规划中：

- `xmail`
  - 文档：`xmail/xmail-mail-protocols-spec.md`
  - 目标：先在 xrt extlibs 层完善 MIME、SMTP、POP3、IMAP，再接入 xlang
