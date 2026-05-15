# xsmtp

`xsmtp` 是一个基于 `xrt` 的 SMTP 发信扩展库。

它面向已经使用 `xrt` 的原生项目，目标是在不引入外部 SMTP 依赖的前提下，提供轻量、可集成的发信能力。

## 功能概览

- 纯 SMTP
- SMTPS / 隐式 TLS（`465`）
- STARTTLS（`587`）
- `AUTH PLAIN`
- `AUTH LOGIN`
- `AUTH XOAUTH2`
- `8BITMIME` / `SMTPUTF8` / `SIZE` / `DSN` 能力识别
- `SIZE` 消息大小检查
- DSN envelope 参数：`RET` / `ENVID` / `NOTIFY` / `ORCPT`
- UTF-8 邮件主题与邮箱显示名编码
- `text/plain`
- `text/html`
- `multipart/alternative`
- 附件与 inline 资源
- DATA MIME 构建复用 `../xmail_mime/xmail_mime.h`
- 同步发送 API
- 基于 future 的异步发送 API
- 适合协程环境的异步等待 API

## 能力矩阵

| 能力 | 状态 |
| --- | --- |
| 纯 SMTP | 已支持 |
| SMTPS 465 | 已支持 |
| STARTTLS 587 | 已支持 |
| AUTH PLAIN | 已支持 |
| AUTH LOGIN | 已支持 |
| AUTH XOAUTH2 | 已支持 |
| EHLO 能力解析 | 已支持 |
| 8BITMIME | 已支持 |
| SMTPUTF8 | 已支持 |
| SIZE 识别与检查 | 已支持 |
| DSN envelope 参数 | 已支持 |
| MAIL FROM / RCPT TO / DATA | 已支持 |
| To/Cc/Bcc envelope | 已支持 |
| Bcc 隐藏 MIME header | 已支持 |
| text/plain | 已支持 |
| text/html | 已支持 |
| multipart/alternative | 已支持 |
| attachment | 已支持 |
| inline Content-ID | 已支持 |
| DATA MIME 复用 xmail_mime | 已支持 |
| 同步 API | 已支持 |
| future 异步 API | 已支持 |
| 协程等待封装 | 已支持 |
| 本地 MIME 集成测试 | 已支持 |
| SMTP command mock server | 已支持基础明文命令流、AUTH PLAIN/LOGIN/XOAUTH2 与 AUTH_AUTO 三分支 |
| 真实服务商手工验证 | 部分已验证 |

## 目录结构

- [xsmtp.h](D:/Git/xrt/extlibs/xsmtp/xsmtp.h)
  实现文件与对外 API 定义。
- [xsmtp_test.c](D:/Git/xrt/extlibs/xsmtp/xsmtp_test.c)
  正式示例程序，所有 SMTP 配置都集中声明在文件顶部。
- [xsmtp_mime_test.c](D:/Git/xrt/extlibs/xsmtp/xsmtp_mime_test.c)
  本地 DATA MIME 集成测试与 SMTP command mock server 测试，覆盖基础明文命令流、DSN envelope 参数、`AUTH PLAIN`、`AUTH LOGIN`、`AUTH XOAUTH2` 和 `AUTH_AUTO` 的 XOAUTH2/PLAIN/LOGIN 选择分支，不连接真实 SMTP 服务。
- [build_test.bat](D:/Git/xrt/extlibs/xsmtp/build_test.bat)
  本地编译示例程序，并运行 MIME 集成测试的脚本。

## 快速开始

1. 打开 [xsmtp_test.c](D:/Git/xrt/extlibs/xsmtp/xsmtp_test.c)。
2. 修改文件顶部的 `XSMTP_EXAMPLE_*` 常量。
3. 编译示例：

```bat
build_test.bat
```

4. 运行示例：

```bat
.\bin\xsmtp_test.exe
```

这个示例是刻意做成自包含的。它不会从 `xadmin` 或任何外部 JSON 文件读取配置。

## 示例配置

示例程序开头有一段可直接编辑的配置块：

```c
#define XSMTP_EXAMPLE_HOST            "smtp.example.com"
#define XSMTP_EXAMPLE_PORT            587
#define XSMTP_EXAMPLE_SECURE_MODE     XSMTP_SECURE_STARTTLS
#define XSMTP_EXAMPLE_AUTH            1
#define XSMTP_EXAMPLE_AUTH_MODE       XSMTP_AUTH_AUTO
#define XSMTP_EXAMPLE_VERIFY_PEER     1
#define XSMTP_EXAMPLE_TIMEOUT_MS      15000
#define XSMTP_EXAMPLE_HELO_NAME       "localhost"

#define XSMTP_EXAMPLE_USERNAME        "user@example.com"
#define XSMTP_EXAMPLE_PASSWORD        "replace-with-app-password"

#define XSMTP_EXAMPLE_FROM_EMAIL      "user@example.com"
#define XSMTP_EXAMPLE_FROM_NAME       "xsmtp example"
#define XSMTP_EXAMPLE_REPLY_TO_EMAIL  "user@example.com"
#define XSMTP_EXAMPLE_REPLY_TO_NAME   "xsmtp example"

#define XSMTP_EXAMPLE_TO_EMAIL        "receiver@example.com"
#define XSMTP_EXAMPLE_TO_NAME         "Receiver"

#define XSMTP_EXAMPLE_USE_ASYNC       1
#define XSMTP_EXAMPLE_USE_COROUTINE   0
```

推荐默认值：

- 使用端口 `465` 搭配 `XSMTP_SECURE_SSL`，或者
- 使用端口 `587` 搭配 `XSMTP_SECURE_STARTTLS`

不要把真实的 SMTP 密码或邮箱服务商的应用专用密码提交到仓库。

## 编译

使用 [build_test.bat](D:/Git/xrt/extlibs/xsmtp/build_test.bat)：

```bat
build_test.bat
```

当前编译命令：

```bat
gcc -m64 xsmtp_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g -DXSMTP_DEBUG -Wall -Wextra -o .\bin\xsmtp_test.exe
gcc -m64 xsmtp_mime_test.c -I . -I ..\..\singlehead -lWs2_32 -lIPHLPAPI -lShell32 -O0 -g -DXSMTP_DEBUG -Wall -Wextra -o .\bin\xsmtp_mime_test.exe
.\bin\xsmtp_mime_test.exe
```

输出文件：

```text
.\bin\xsmtp_test.exe
```

## 支持的安全模式

- `XSMTP_SECURE_NONE`
  不使用 TLS 的纯 SMTP。
- `XSMTP_SECURE_SSL`
  隐式 TLS，通常对应端口 `465`。
- `XSMTP_SECURE_STARTTLS`
  先明文连接，再通过 `STARTTLS` 升级到 TLS，通常对应端口 `587`。
- `XSMTP_SECURE_AUTO`
  根据端口自动判断：
  - `465` -> 隐式 TLS
  - `587` -> STARTTLS
  - 其他端口 -> 纯 SMTP

## 支持的认证模式

- `XSMTP_AUTH_NONE`
  不使用 SMTP 认证。
- `XSMTP_AUTH_PLAIN`
  强制使用 `AUTH PLAIN`。
- `XSMTP_AUTH_LOGIN`
  强制使用 `AUTH LOGIN`。
- `XSMTP_AUTH_XOAUTH2`
  强制使用 `AUTH XOAUTH2`，需要设置 `sUser` 与 `sOAuth2Token`。
- `XSMTP_AUTH_AUTO`
  根据服务端能力自动选择。

推荐默认值：

- `XSMTP_AUTH_AUTO`

## 核心类型

### `xsmtpconfig`

连接与发件人配置。

重要字段：

- `sHost`
- `iPort`
- `iTimeoutMs`
- `iSecureMode`
- `bAuth`
- `iAuthMode`
- `bVerifyPeer`
- `sUser`
- `sPass`
- `sOAuth2Token`
- `sHeloName`
- `tFrom`
- `tReplyTo`

### `xsmtpmessage`

邮件内容与收件人数据。

重要字段：

- `tFrom`
- `tReplyTo`
- `arrTo`, `iToCount`
- `arrCc`, `iCcCount`
- `arrBcc`, `iBccCount`
- `sSubject`
- `sTextBody`
- `sHtmlBody`
- `arrAttachments`, `iAttachmentCount`
- `arrHeaderNames`
- `arrHeaderValues`
- `iHeaderCount`

### `xsmtpattachment`

附件与 inline 资源数据。

重要字段：

- `sFileName`
- `sContentType`
- `sContentId`
- `pData`, `iDataLen`
- `bInline`

### `xsmtpresult`

发送结果与诊断信息。

重要字段：

- `bSuccess`
- `bUsedTLS`
- `bUsedStartTLS`
- `iServerCode`
- `iAuthMode`
- `iStage`
- `iCapabilities`
- `iSizeLimit`
- `sError`
- `sLastReply`

`iStage` 使用 `XSMTP_STAGE_*` 常量表达失败阶段，例如 `XSMTP_STAGE_CONNECT`、`XSMTP_STAGE_TLS`、`XSMTP_STAGE_EHLO`、`XSMTP_STAGE_AUTH`、`XSMTP_STAGE_MAIL_FROM`、`XSMTP_STAGE_RCPT`、`XSMTP_STAGE_DATA`。

TLS handshake/feed/read/write 失败时，`sError` 会附带 `tls=<xnet_result>` 诊断码；stream 连接、发送、接收失败继续保留既有的 sys/reason 文本。

### `xsmtpasyncopts`

异步发送选项。

重要字段：

- `iTimeoutMs`
- `pEngine`
- `sDebugName`

## 对外 API

初始化辅助函数：

- `xrtSmtpConfigInit(...)`
- `xrtSmtpMessageInit(...)`
- `xrtSmtpResultInit(...)`
- `xrtSmtpAsyncOptsInit(...)`
- `xrtSmtpResultFree(...)`

发送相关 API：

- `xrtSmtpSendMail(...)`
- `xrtSmtpSendMailFuture(...)`
- `xrtSmtpSendMailAsyncWait(...)`
- `xrtSmtpSendMailCo(...)`

## 最小同步示例

```c
xsmtpconfig cfg;
xsmtpmessage msg;
xsmtpresult ret;
xsmtpaddr to;

xrtSmtpConfigInit(&cfg);
cfg.sHost = "smtp.example.com";
cfg.iPort = 587;
cfg.iSecureMode = XSMTP_SECURE_STARTTLS;
cfg.bAuth = TRUE;
cfg.iAuthMode = XSMTP_AUTH_AUTO;
cfg.bVerifyPeer = TRUE;
cfg.sUser = "user@example.com";
cfg.sPass = "app-password";
cfg.tFrom.sEmail = "user@example.com";
cfg.tFrom.sName = "Example Sender";

memset(&to, 0, sizeof(to));
to.sEmail = "receiver@example.com";
to.sName = "Receiver";

xrtSmtpMessageInit(&msg);
msg.arrTo = &to;
msg.iToCount = 1;
msg.sSubject = "Hello from xsmtp";
msg.sTextBody = "Plain text body";
msg.sHtmlBody = "<p>HTML body</p>";

xrtSmtpResultInit(&ret);
if ( !xrtSmtpSendMail(&cfg, &msg, &ret) ) {
	printf("send failed: %s\n", ret.sError);
}
```

## 最小异步示例

```c
xsmtpasyncopts opts;
xsmtpresult ret;

xrtSmtpAsyncOptsInit(&opts);
opts.sDebugName = "mail_send_job";

xrtSmtpResultInit(&ret);
if ( !xrtSmtpSendMailAsyncWait(&cfg, &msg, &opts, &ret) ) {
	printf("async send failed: %s\n", ret.sError);
}
```

## Future API 用法

`xrtSmtpSendMailFuture(...)` 返回一个 `xfuture*`。

说明：

- 如果 `xsmtpasyncopts.pEngine == NULL`，`xsmtp` 会创建私有网络引擎
- 如果 `xsmtpasyncopts.pEngine != NULL`，`xsmtp` 会复用调用方传入的引擎
- future 解析结果为 `xsmtpresult*`
- 这个结果对象要通过 `xrtSmtpResultFree(...)` 释放

典型流程：

```c
xfuture* fut = xrtSmtpSendMailFuture(&cfg, &msg, &opts);
xsmtpresult* ret = (xsmtpresult*)xFutureWaitValue(fut);
if ( ret ) {
	/* use result */
	xrtSmtpResultFree(ret);
}
xFutureRelease(fut);
```

## 协程 API 用法

`xrtSmtpSendMailCo(...)` 是异步发送路径上的一个便捷封装，使用协程风格等待结果。

当外层系统已经使用 `xrt` 协程调度时，可以优先使用这个接口。

## 协议行为

典型发信流程：

1. 建立连接
2. 读取服务端 banner
3. 发送 `EHLO`
4. 视情况执行 `STARTTLS`
5. TLS 升级后再次发送 `EHLO`
6. 认证
7. `MAIL FROM`
8. `RCPT TO`
9. `DATA`
10. 发送邮件内容
11. `QUIT`

## TLS 说明

### 隐式 TLS（`465`）

TLS 会在连接建立时直接附加。

### STARTTLS（`587`）

库内部流程是：

1. 先以明文方式建立连接
2. 读取服务端能力
3. 发送 `STARTTLS`
4. 执行 TLS 握手
5. 重新发送 `EHLO`
6. 在 TLS 通道内继续认证与投递

当前同步路径和原生异步路径都支持这一流程。

## 邮件内容规则

`xsmtp` 会自动处理：

- UTF-8 主题编码
- UTF-8 邮箱显示名编码
- MIME 头
- 纯文本正文编码
- HTML 正文编码
- `multipart/alternative` 组装
- `DATA` 阶段的 SMTP dot-stuffing

## 错误处理

始终建议检查：

- `ret.sError`
- `ret.sLastReply`
- `ret.iServerCode`

常见错误包括：

- 连接失败
- TLS 校验失败
- TLS 状态机失败，错误文本中会包含 `tls=<xnet_result>`
- 服务端不支持 `STARTTLS`
- 认证失败
- 收件人被拒收
- 收到非预期的 SMTP 响应码

## 安全建议

- 尽量使用邮箱服务商的应用专用密码，而不是账号主密码
- 生产环境保持 `bVerifyPeer = TRUE`
- 只有在受控的本地调试环境下才关闭证书校验
- 不要把真实密钥硬编码进提交到仓库的源码
- 日志中要避免输出密码和认证令牌

## 调试日志

使用 `-DXSMTP_DEBUG` 编译即可启用 trace 日志。

当前保留的日志重点是：

- 连接提交
- 引擎回退
- stream 错误 / 关闭原因
- 同步协议关键节点

不会再输出早期联调阶段那种非常细碎的 bring-up 级日志。

## 验证状态

本地示例已经验证过以下组合可真实发信：

- `465` + 隐式 TLS
- `587` + STARTTLS
- 同步发送
- 异步等待发送

## 建议的下一步集成方向

对于 `xserver` 或 `xadmin` 这样的上层系统，推荐的下一步是：

1. 把 `xsmtp` API 导入宿主运行时
2. 在队列 worker 中调用 `xrtSmtpSendMailFuture(...)`
3. 批量处理邮件任务时复用共享的 `xnetengine`

## 集成到 xserver

如果 `xsmtp` 由 `xserver` 托管，常见做法是：

1. 将 `xsmtp.h` 放到宿主侧的库目录
2. 在脚本宿主中增加导入桥接
3. 只暴露脚本层实际会用到的符号

典型导入文件：

- [import_xsmtp.h](D:/Git/xserver/src/script/import_c/import_xsmtp.h)

示例导入方式：

```c
static inline void ImportXSMTP(TCCState* s)
{
	if ( s == NULL ) {
		return;
	}

	tcc_add_symbol(s, "xrtSmtpConfigInit", xrtSmtpConfigInit);
	tcc_add_symbol(s, "xrtSmtpMessageInit", xrtSmtpMessageInit);
	tcc_add_symbol(s, "xrtSmtpResultInit", xrtSmtpResultInit);
	tcc_add_symbol(s, "xrtSmtpAsyncOptsInit", xrtSmtpAsyncOptsInit);
	tcc_add_symbol(s, "xrtSmtpSendMail", xrtSmtpSendMail);
	tcc_add_symbol(s, "xrtSmtpSendMailFuture", xrtSmtpSendMailFuture);
	tcc_add_symbol(s, "xrtSmtpSendMailAsyncWait", xrtSmtpSendMailAsyncWait);
	tcc_add_symbol(s, "xrtSmtpSendMailCo", xrtSmtpSendMailCo);
	tcc_add_symbol(s, "xrtSmtpResultFree", xrtSmtpResultFree);
}
```

推荐导出策略：

- 始终导出三个 init 辅助函数
- 导出 `xrtSmtpSendMail(...)` 作为最简单的脚本调用入口
- 如果脚本任务需要“阻塞式异步封装”，导出 `xrtSmtpSendMailAsyncWait(...)`
- 只有在脚本层本身管理 future 时，才导出 `xrtSmtpSendMailFuture(...)`
- 只要 future 结果对象会传到脚本层，就要导出 `xrtSmtpResultFree(...)`

## 集成到 xadmin

对于 `xadmin`，更适合把 `xsmtp` 用作邮件队列执行器，而不是直接放在页面请求里同步发送。

推荐执行流程：

1. 后台页面创建 `mail_task`
2. 队列扫描器加载 `pending` 任务
3. 每条任务转换为 `xsmtpconfig + xsmtpmessage`
4. worker 执行发信
5. 将任务状态更新为 `success` 或 `fail`
6. 在 `mail_log` 中写入服务端回复和错误信息

这样可以缩短页面请求耗时，也更方便做重试与排障。

建议字段映射：

- SMTP 主机、端口、安全模式、认证模式、超时：
  来自全局配置
- `cfg.tFrom` / `cfg.tReplyTo`：
  来自配置中的发件人身份
- `msg.arrTo`：
  来自解析后的收件人列表
- `msg.sSubject`、`msg.sTextBody`、`msg.sHtmlBody`：
  来自任务渲染后的正文或模板输出

建议队列策略：

- 如果队列执行器已经持有 `xnetengine`，默认优先使用异步发送
- 同步发送只保留给简单诊断页或一次性后台操作
- 每轮扫描限制批量处理数量
- 在任务表中保存重试次数和下次重试时间
- 持久化最后一次 SMTP 回复，便于后续排障

## 队列 Worker 示例

一个最小 worker 风格示例：

```c
xsmtpconfig cfg;
xsmtpmessage msg;
xsmtpresult ret;
xsmtpasyncopts opts;
xsmtpaddr to;

xrtSmtpConfigInit(&cfg);
xrtSmtpMessageInit(&msg);
xrtSmtpResultInit(&ret);
xrtSmtpAsyncOptsInit(&opts);

cfg.sHost = smtp_host;
cfg.iPort = (uint16)smtp_port;
cfg.iSecureMode = smtp_secure_mode;
cfg.bAuth = TRUE;
cfg.iAuthMode = XSMTP_AUTH_AUTO;
cfg.bVerifyPeer = TRUE;
cfg.sCaFile = NULL; /* optional PEM CA bundle path */
cfg.sUser = smtp_user;
cfg.sPass = smtp_pass;
cfg.tFrom.sEmail = from_email;
cfg.tFrom.sName = from_name;

memset(&to, 0, sizeof(to));
to.sEmail = task_to_email;
to.sName = task_to_name;

msg.arrTo = &to;
msg.iToCount = 1;
msg.sSubject = task_subject;
msg.sTextBody = task_text_body;
msg.sHtmlBody = task_html_body;

opts.pEngine = shared_engine;
opts.sDebugName = "mail_queue";

if ( !xrtSmtpSendMailAsyncWait(&cfg, &msg, &opts, &ret) ) {
	/* mark task failed and persist ret.sError / ret.sLastReply */
} else {
	/* mark task success and persist ret.sLastReply */
}
```

## 常见集成决策

### 同步还是异步

以下场景适合同步发送：

- 编写很小的独立工具
- 本地验证库功能
- 实现一次性的 SMTP 诊断页面

以下场景适合异步发送：

- 处理邮件任务队列
- 批量发送
- 复用已有的 `xnetengine`
- 运行在已经具备异步基础设施的服务进程中

### 共享引擎还是私有引擎

以下场景适合共享引擎：

- 同一个 worker 循环里会发送多封邮件
- 宿主已经维护了长期存在的网络引擎

以下场景适合私有引擎：

- 只发送单封邮件
- 编写测试工具
- 比起复用，更希望把 SMTP 和其他运行时能力隔离开

## 故障排查

### `SMTP socket connect failed`

通常表示在开始 SMTP 认证前，TCP 连接就失败了。

检查项：

- 主机和端口是否正确
- 当前网络是否能访问邮件服务商
- 安全模式和端口是否匹配：
  - `465` -> `XSMTP_SECURE_SSL`
  - `587` -> `XSMTP_SECURE_STARTTLS`
- 本地防火墙或出口策略是否拦截了该端口

### TLS verify failed

通常表示服务端证书与配置的主机名不匹配，或者证书链校验失败。

检查项：

- `cfg.sHost` 是否与 SMTP 服务证书名称一致
- 本地信任存储是否可用
- 生产环境下是否意外关闭了 `bVerifyPeer`

### Auth failed

通常表示用户名、密码或认证模式不正确。

检查项：

- 使用的是应用专用密码还是账号主密码
- 账号是否已开启 SMTP 认证
- `XSMTP_AUTH_AUTO` 是否协商到了期望的认证方式

### `STARTTLS` not supported

通常表示服务端在 `EHLO` 响应里没有声明 `STARTTLS`，或者当前连接打到了错误端口。

检查项：

- 是否使用了端口 `587`
- 安全模式是否为 `XSMTP_SECURE_STARTTLS`
- 上游代理是否篡改了 SMTP 会话

## 生产环境说明

- 示例中的凭据不要纳入版本控制。
- 比起在请求链路里直接发送，更推荐队列化投递。
- 建议把 SMTP 回复文本保存到日志中，方便支持和事后排障。
- 先从单服务商、单发件身份开始，再扩展到多服务商路由。
- 在启用自动队列投递前，建议先在后台提供一个 `test SMTP config` 诊断工具。
