# xpop3

xpop3 是构建在 xmail 邮件基座之上的 POP3 客户端扩展库：协议解析、同步客户端、STLS、SASL 认证与 RETR/TOP 到 MIME 树的桥接。通过 `XPOP3_MODULE_*` 宏裁剪，单头形态为 `single/extlibs/xpop3.h`。

单头实现与声明分别为仓库根目录的 `single/extlibs/xpop3.h` 与
`single/extlibs/xpop3_decl.h`，均只包含 xpop3 自身代码。使用前须按顺序提供
XRT → xmail 的所需模块，再包含 xpop3；实现宏为 `XPOP3_IMPLEMENTATION`。
依赖选择与实现组合见 [构建说明](../../docs/BUILD.md#扩展单头与依赖顺序)。

## 客户端范例

网络清理等待异步退休，失败时保留拥有型句柄并返回失败；诊断只输出阶段、
错误类别和错误码。共享实现见 `../xmail/examples/mail_client_setup.h`。

只读客户端范例取得完整 RETR 结束标记后，随后 QUIT/TLS 关闭失败单独报告为
`retrieval completed; shutdown failed`，完整的取信结果仍可使用；未收全正文仍失败。
此策略限于没有 DELE 的只读取信，删除邮件仍必须核对 UPDATE/QUIT 的结果。
`xrtPop3ClientQuit` 保持严格关闭检查。QQ 邮箱使用 `pop.qq.com 995`、`tls`，
通过 `XPOP3_USER`、`XPOP3_PASSWORD` 提供运行时账号和认证码，CA 文件使用系统可信根。

取信范例按原始邮件字节写入标准输出；Windows 使用二进制模式，避免把 CRLF
改成 CRCRLF。正文输出和刷新失败会使范例返回失败。

CAPA 的扩展标签支持数字及除点号外的可打印 ASCII 标点，例如 QQ 服务广告的
`XOAUTH2`。未知标签可解析但不设置已知能力位，也不会被当作 SASL 认证机制。
控制字节、嵌入 NUL、点号和线路注入仍拒绝。标签规则参考
[RFC 2449 第 3 节](https://www.rfc-editor.org/rfc/rfc2449#section-3)；解析器保留
拒绝 DEL 的策略。独立 TLS/STLS 夹具在默认能力发现开启时核对未知标签兼容。

`examples/offline/main.c` 自建本机回环 POP3 服务，执行 `Open → USER/PASS →
RETR → QUIT` 并核对三行邮件内容，不需要外部账号或网络服务：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xpop3/config/modules.json --suite pop3_offline_example --no-single
```

其中的明文凭据只在进程内回环使用。连接真实服务请运行下述 TLS 范例。

`examples/client/main.c` 连接 POP3 服务、升级 STLS（或使用隐式 TLS）、认证并以 `RETR` 读取一封邮件。先构建范例：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xpop3/config/modules.json --suite pop3_client_example --no-single --exclude-test '*'
```

实际使用时设置 `XPOP3_USER`、`XPOP3_PASSWORD`，再运行生成的 `examples_client_main`，参数为 `host port ca.pem message [tls|stls]`。`ca.pem` 是你信任的 PEM CA 证书；默认使用 STLS。端口、主机、账号和邮件编号应与服务端一致。范例不会在命令行接收密码，也不会跳过证书验证。`main` 的无参数路径仅打印用法，适合离线 CI 检查。

从仓库根目录运行 `python tools/test_mail_tls_interop.py`，可用独立 Python TLS
服务端、临时 CA 和该真实客户端范例完成隐式 TLS/STLS 取信；DNS、IPv4 和
IPv6 端点均核对证书身份与 SNI，证书身份不匹配时拒绝连接。此脚本同时检查
SMTP 和 IMAP 范例，并验证 RETR 正文中途断开时客户端失败；已接入 Linux CI，
IPv6 回环不可用时跳过对应场景。

## TLS 读取故障回归

以下离线回环测试覆盖 STAT 状态行、逐行 RETR、流式 RETR 和 RETR 字节收集
四个入口，每个入口分别注入取消、超时和对端断流。服务器先发送缺少 CRLF 的
线路前缀；测试观察完整密文已接收、明文已取走且接收 Future 仍待定，并在失败
返回后核对协议层确实保存了该前缀。还核对原始错误、最近完整回复、拒绝复用，
以及客户端销毁前的连接中止。流式回调只能收到完整且已去除 dot transparency
的行；不完整状态和收集结果不得交付。无需账号或外部服务：

```sh
python tools/build.py --compiler gcc --manifest extlibs/xpop3/config/modules.json --suite pop3_read_tls_fault_runtime_tests --no-single --no-examples
```

已纳入 `xpop3_tests` 完整套件，Linux CI 另执行 ASan/UBSan；这项回归使用模块化
构建读取私有传输状态。

## 覆盖率

从仓库根目录执行：

```text
python tools/test_mail_coverage_inputs.py
python tools/measure_mail_coverage.py --product xpop3
python tools/measure_mail_coverage.py --product xpop3 --report-only
```

工具重建模块套件，以 gcov JSON 原始计数统计本库全部自有 `.c` 文件，
保存精确行数、分支结果数和未触达位置。默认口径为
`module_and_independent_tls_object_profiles`，合并模块套件与独立 TLS 范例的原始
客户端对象计数，仅统计本库自有 `.c` 文件。Core 和 Python 服务端代码不计入；
内部头中的 static 函数另存于 `header-functions.json`，不改变主报告分母。
`--module-only` 可选择仅模块套件的独立口径；同一报告不能混用两种口径。
默认防倒退门槛与 CI 一致：行 72% / 分支结果 55%；门槛通过不代表生产验收完成。

报告位于 `out/mail/coverage/{win32,linux}/xpop3/coverage.json`。
Windows 与 Linux 的编译计数分别保存在 `out/gcc/native-windows` 与
`out/gcc/native-linux`，避免交叉覆盖。报告绑定源码、Core、测试、夹具、清单、
构建工具和原始 `.gcno`/`.gcda` 的 SHA256；测量期间输入变化会使测量失败。
`--report-only` 要求保留原报告和原始计数，重新核对输入和精确计数，不重建测试。
旧计数未绑定报告、输入已经变化、计数丢失或报告与计数不符时都会拒绝读取，
不会给旧计数重新附上当前源码的哈希。CI 在两个平台执行溯源回归并保留报告及自有对象计数。
