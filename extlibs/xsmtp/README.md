# xsmtp

真实提交范例在网络清理未完成时返回失败，保留拥有型句柄和原始错误供重试；
诊断只输出阶段、错误类别和错误码。共享实现见 `../xmail/examples/mail_client_setup.h`。

xsmtp 是构建在 xmail 邮件基座（MIME 内容层与传输层）之上的 SMTP 客户端扩展库：协议解析、同步客户端、STARTTLS/隐式 TLS、SASL 认证与从 xmailmessage 派生的流式提交。通过 `XSMTP_MODULE_*` 宏裁剪，单头形态为 `single/extlibs/xsmtp.h`。

单头实现与声明分别为仓库根目录的 `single/extlibs/xsmtp.h` 与
`single/extlibs/xsmtp_decl.h`，均只包含 xsmtp 自身代码。使用前须按顺序提供
XRT → xmail 的所需模块，再包含 xsmtp；实现宏为 `XSMTP_IMPLEMENTATION`。
依赖选择与实现组合见 [构建说明](../../docs/BUILD.md#扩展单头与依赖顺序)。

## 提交范例

`examples/offline/main.c` 在进程内回环 SMTP 服务上执行完整的 `xrtSmtpSubmit`，
服务端核对发件人、收件人、邮件主题与正文，再确认 `QUIT`。无需账号或外部网络：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xsmtp/config/modules.json --suite smtp_offline_example --no-single
```

`examples/submit/main.c` 使用真实网络连接、TLS、SMTP AUTH 和 `xrtSmtpSubmit` 提交文本邮件。构建命令：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xsmtp/config/modules.json --suite smtp_submit_example --no-single --exclude-test '*'
```

设置 `XSMTP_USER`、`XSMTP_PASSWORD` 后运行生成的 `examples_submit_main`，参数为 `host port ca.pem from to subject body [tls|starttls]`。`ca.pem` 是可信 PEM CA；默认使用 STARTTLS。范例要求服务端支持 PLAIN 认证，密码仅从环境变量读取。无参数运行只打印用法，供离线 CI 验证。

最终 DATA 的正向回复表示提交已完成。范例将随后 QUIT/TLS 关闭失败单独报告为
`submission completed; shutdown failed`，成功的提交不会因此变成“未发送”；应用应
记录关闭异常，避免自动重发同一封邮件。`xrtSmtpClientQuit` 仍严格检查协议与 TLS
关闭，未收到 DATA 最终回复仍返回失败，发送结果可能未知。QQ 邮箱可使用
`smtp.qq.com 465` 和 `tls`；认证码从上述环境变量读取，CA 文件使用系统可信根，
不要将账号认证码放入命令行或提交到仓库。

从仓库根目录运行 `python tools/test_mail_tls_interop.py`，可用独立 Python TLS
服务端、临时 CA 和该真实提交范例完成隐式 TLS/STARTTLS、AUTH PLAIN 与
邮件提交；DNS、IPv4 和 IPv6 端点均核对证书身份与 SNI，证书身份不匹配时
拒绝连接。脚本还验证服务端接收完整 DATA 后未返回最终结果时客户端报告失败，
以及提交完成后缺失 TLS `close_notify` 时单独报告关闭警告，
同时检查 POP3 和 IMAP 范例；已接入 Linux CI，IPv6 回环不可用时跳过对应场景。

## TLS 上传故障回归

在仓库根目录执行下列离线回环测试，可验证 DATA 与 BDAT 两条发送路径。
每条路径分别在同一个 TLS 写入 Future 已部分提交、尚未完成时注入取消、
超时和对端断流；测试核对原始错误、最近完整回复、失败会话拒绝复用，以及
客户端销毁前的连接中止。DATA 使用带 CRLF 的短行，BDAT 精确声明块长度。
Worker 上的发送快照与 IMAP 共用测试夹具，无需账号或外部服务：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xsmtp/config/modules.json --suite smtp_upload_tls_fault_runtime_tests --no-single --no-examples
```

Linux CI 另执行 ASan/UBSan；`--suite xsmtp_tests` 包含完整模块化与单头测试。
这项回归本身使用模块化构建读取私有传输状态。

## 覆盖率

从仓库根目录执行：

```text
python tools/test_mail_coverage_inputs.py
python tools/measure_mail_coverage.py --product xsmtp
python tools/measure_mail_coverage.py --product xsmtp --report-only
```

工具重建模块套件，以 gcov JSON 原始计数统计本库全部自有 `.c` 文件，
保存精确行数、分支结果数和未触达位置。默认口径为
`module_and_independent_tls_object_profiles`，合并模块套件与独立 TLS 范例的原始
客户端对象计数，仅统计本库自有 `.c` 文件。Core 和 Python 服务端代码不计入；
内部头中的 static 函数另存于 `header-functions.json`，不改变主报告分母。
`--module-only` 可选择仅模块套件的独立口径；同一报告不能混用两种口径。
默认防倒退门槛与 CI 一致：行 75% / 分支结果 56%；门槛通过不代表生产验收完成。

报告位于 `out/mail/coverage/{win32,linux}/xsmtp/coverage.json`。
Windows 与 Linux 的编译计数分别保存在 `out/gcc/native-windows` 与
`out/gcc/native-linux`，避免交叉覆盖。报告绑定源码、Core、测试、夹具、清单、
构建工具和原始 `.gcno`/`.gcda` 的 SHA256；测量期间输入变化会使测量失败。
`--report-only` 要求保留原报告和原始计数，重新核对输入和精确计数，不重建测试。
旧计数未绑定报告、输入已经变化、计数丢失或报告与计数不符时都会拒绝读取，
不会给旧计数重新附上当前源码的哈希。CI 在两个平台执行溯源回归并保留报告及自有对象计数。
