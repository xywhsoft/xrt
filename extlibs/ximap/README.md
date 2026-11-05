# ximap

真实客户端范例在网络清理未完成时返回失败，保留拥有型句柄和原始错误供重试；
诊断只输出阶段、错误类别和错误码。共享实现见 `../xmail/examples/mail_client_setup.h`。

只读 EXAMINE 的 tagged OK 完成后，随后 LOGOUT/TLS 关闭失败单独报告为
`query completed; shutdown failed`，完整查询结果仍可使用；只有未标记的 EXISTS
回复不足以表示查询完成。`xrtImapClientLogout` 保持严格关闭检查。QQ 邮箱使用
`imap.qq.com 993`、`tls`，通过 `XIMAP_USER`、`XIMAP_PASSWORD` 提供运行时账号和认证码，
CA 文件使用系统可信根。

ximap 是构建在 xmail 邮件基座之上的 IMAP 客户端扩展库：协议解析、命令层、FETCH/BODYSTRUCTURE 数据视图、流式 APPEND 与 RFC 4978 COMPRESS=DEFLATE。通过 `XIMAP_MODULE_*` 宏裁剪，单头形态为 `single/extlibs/ximap.h`。

单头实现与声明分别为仓库根目录的 `single/extlibs/ximap.h` 与
`single/extlibs/ximap_decl.h`，均只包含 ximap 自身代码。使用前须按顺序提供
XRT → xmail 的所需模块，再包含 ximap；实现宏为 `XIMAP_IMPLEMENTATION`。
依赖选择与实现组合见 [构建说明](../../docs/BUILD.md#扩展单头与依赖顺序)。

## 客户端范例

`examples/offline/main.c` 在进程内回环 IMAP 服务上执行 `Open`、`LOGIN`、
`EXAMINE INBOX` 和 `LOGOUT`，并核对只读邮箱摘要。无需账号或外部网络：

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_offline_example --no-single
```

`examples/client/main.c` 连接 IMAP 服务、建立 TLS、使用 PLAIN 认证、在支持时启用 COMPRESS=DEFLATE，并以 `EXAMINE` 只读打开邮箱。构建命令：

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_client_example --no-single --exclude-test '*'
```

设置 `XIMAP_USER`、`XIMAP_PASSWORD` 后运行生成的 `examples_client_main`，参数为 `host port ca.pem mailbox [tls|starttls]`。`ca.pem` 是可信 PEM CA；默认使用 STARTTLS。服务器需支持 PLAIN 认证。无参数运行只打印用法，供离线 CI 验证。

从仓库根目录运行 `python tools/test_mail_tls_interop.py`，可用独立 Python TLS
服务端、临时 CA 和该真实客户端范例完成隐式 TLS/STARTTLS、AUTH PLAIN 与
只读 `EXAMINE`；DNS、IPv4 和 IPv6 端点均核对证书身份与 SNI，证书身份
不匹配时拒绝连接。脚本还验证 EXAMINE 的 tagged completion 缺失时客户端失败，
同时检查 POP3 和 SMTP 范例；已接入 Linux CI，IPv6 回环不可用时跳过对应场景。

## 生产与验证入口

`ximap` 聚合十个功能根并保留协议 smoke；`ximap_tests` 承载完整回归和夹具依赖，
独立示例通过各自的 example 根构建。生产闭包不携带 TLS 夹具的 RSA 私钥运算、
签名和 RSA 身份构造模块。Core `tls_stream` 现有的 client/server 依赖仍保留。

## 非法超时与会话恢复回归

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_deadline_runtime_tests --no-examples
```

十八个场景在模块化与单头入口中复用同一实现，以公开 Open 和真实 PREAUTH、SELECT、
APPEND、IDLE 转录建立会话。检查非法超时不会发送字节、误报空写成功、改变活跃 tag、
最近完整响应或 APPEND 剩余长度，并用同一事务的实际完成、NOOP 和 LOGOUT 证明可恢复。
EndIdle 另核对无效请求不会提前用掉 DONE 资格。已有正确委托路径、正常收发、IDLE、
APPEND、无限等待及关闭作为对照。对端全部退出后检查 Engine 退休和内存调试器。

## TLS 上传故障回归

以下离线回环测试在同一次 TLS APPEND 写入已经部分提交、Future 尚未完成时，
分别注入取消、超时和对端断流。它核对原始错误、失败会话拒绝复用，以及客户端
销毁前的连接中止；发送计数和待发密文在所属 Worker 上读取，不依赖固定睡眠
猜测发送进度。无需外部服务或账号：

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_append_tls_fault_runtime_tests --no-single --no-examples
```

Linux CI 对该测试另执行 ASan/UBSan。完整套件使用 `--suite ximap_tests`，
包含模块化和单头测试；该回归本身使用模块化构建读取私有传输状态。

## 覆盖率

从仓库根目录执行：

```text
python tools/test_mail_coverage_inputs.py
python tools/measure_mail_coverage.py --product ximap
python tools/measure_mail_coverage.py --product ximap --report-only
```

工具重建模块套件，以 gcov JSON 原始计数统计本库全部自有 `.c` 文件，
保存精确行数、分支结果数和未触达位置。默认口径为
`module_and_independent_tls_object_profiles`，合并模块套件与独立 TLS 范例的原始
客户端对象计数，仅统计本库自有 `.c` 文件。Core 和 Python 服务端代码不计入；
内部头中的 static 函数另存于 `header-functions.json`，不改变主报告分母。
`--module-only` 可选择仅模块套件的独立口径；同一报告不能混用两种口径。
默认防倒退门槛与 CI 一致：行 74% / 分支结果 55%；门槛通过不代表生产验收完成。

报告位于 `out/mail/coverage/{win32,linux}/ximap/coverage.json`。
Windows 与 Linux 的编译计数分别保存在 `out/gcc/native-windows` 与
`out/gcc/native-linux`，避免交叉覆盖。报告绑定源码、Core、测试、夹具、清单、
构建工具和原始 `.gcno`/`.gcda` 的 SHA256；测量期间输入变化会使测量失败。
`--report-only` 要求保留原报告和原始计数，重新核对输入和精确计数，不重建测试。
旧计数未绑定报告、输入已经变化、计数丢失或报告与计数不符时都会拒绝读取，
不会给旧计数重新附上当前源码的哈希。CI 在两个平台执行溯源回归并保留报告及自有对象计数。

## 压缩传输故障回归

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_compress_fault_runtime_tests --no-examples
```

模块与单头各执行 32 项本地 TCP/TLS 场景，涵盖取消、超时、非法/截断输入、
线路限制、编解码器安装 OOM、持续 APPEND，以及解码缓冲首次分配/扩容、
压缩发送分配和已经传输 literal 前缀后的最终刷新 OOM。
读取 OOM 先从真实连接收齐压缩响应，再注入解码缓冲分配，避免误命中接收
Future；配套无故障控制核对完整回复、后续命令与关闭。
发送刷新失败核对对端实际收到的 literal 前缀，允许此前成功操作已经传输
部分数据；失败必须关闭会话、保留最近完整响应和原错误，且禁止复用。
全部场景结束后检查逻辑分配已释放。CI 同时执行模块/单头及 Linux ASan/UBSan。

## 独立压缩互操作

从仓库根目录执行，无需外部服务器或账号：

```sh
python tools/test_imap_compress_interop.py
python tools/test_imap_compress_interop.py --compiler clang --sanitize
```

需要 C 编译器、Python 标准库 SSL/zlib 和 PATH 中的 OpenSSL。默认编译模块化与
单头公开 API 探针，在本地明文、隐式 TLS 和 STARTTLS 下验证持续 raw DEFLATE，
stored/fixed/dynamic 块、FULL_FLUSH、9 位窗口、确认与压缩前缀同次发送、逐字节
回复，以及协商 NO/BAD 后保持原有传输。成功路径逐字核对 40,000 字节 APPEND，
确认空写片段被拒绝后仍能完成上传，并继续执行命令和 LOGOUT。故障路径验证非法
块、zlib 包装格式、截断回复和解码后的线路超限，核对错误、最近完整回复与禁止复用。
TLS 1.3 P-256 重试场景通过独立 OpenSSL 消息回调确认两个 ClientHello 和一次
HelloRetryRequest；明文模式没有此握手场景。参考
[RFC 4978](https://www.rfc-editor.org/rfc/rfc4978.html) 和
[RFC 8446](https://www.rfc-editor.org/rfc/rfc8446.html#section-4.1.3)。

TLS/STARTTLS 还验证完整 LOGOUT 回复后的未经认证 EOF 和关闭超时必须失败，保留
最后的 tagged OK 并禁止后续命令。默认两种构建共执行 90 项组合；`--layout`、`--mode`、`--case` 可选择具体回归，
执行记录与二进制哈希保存到 `out/imap-compress-interop`。这个探针单独记录互操作
证据，尚未合入上述覆盖率主报告。Linux CI 已配置常规与 ASan/UBSan 构建。

筛选结果没有适用场景时（例如只选明文和 `hrr`），命令以参数错误退出，
不会编译或生成零用例成功记录。`--output-dir` 支持仓库外目录；记录内的
外部二进制使用绝对路径，仓库内产物仍使用相对路径，均绑定实际字节哈希。
输入与记录边界回归可用 `python tools/test_imap_compress_interop_inputs.py` 运行。


## 会话边界回归

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_boundary_runtime_tests --no-examples
```

模块化与单头共享十二个真实 TCP/PREAUTH 场景：APPEND 输出参数预检、greeting/状态/
continuation 说明文本的花括号尾部，以及 IDLE 仍有 literal/fragment 时的 DONE 资格。
有效控制读取两个 FETCH literal，第二个由后续 fragment 声明，并继续 NOOP/LOGOUT；
另有普通状态、读完 IDLE 和非法等待控制。每次拒绝后核对可恢复边界，peer 退出并 join，
最后验证 Engine 退休计数和逻辑内存释放。该回归同时包含在完整 ximap_tests 中。


## 认证边界回归

```sh
python tools/build.py --compiler gcc --manifest extlibs/ximap/config/modules.json --suite imap_auth_runtime_tests --no-examples
```

模块化与单头共享十四个真实 TCP、公开 Open 客户端场景：意外 literal 的会话终止、
LOGIN 取消的 EOF 错误保真及未完成取消、非 initial-response 凭据的命令前长度检查，
并覆盖成功的精确 PLAIN 载荷、完成的拒绝/取消、IR 超限、非法等待、明文安全门、
未声明能力及两个凭据分配点的 OOM。可恢复路径继续实际 NOOP/LOGOUT；终态路径在
显式 Abort 前观察 peer EOF 并 join，最后检查 Engine 退役和逻辑内存释放。
