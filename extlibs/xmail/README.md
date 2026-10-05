# xmail

POP3、SMTP、IMAP 的真实客户端范例共享 `examples/mail_client_setup.h`。
清理在五秒预算内推进 Resolver 和 Engine 的退休，只在成功时清空拥有型指针；
忙碌或失败时保留句柄供重试，并保留调用前的错误。清理未完成时范例返回失败。

`xmail` 是只依赖 XRT 公共 API 的邮件底层扩展库：MIME 内容层与跨协议传输基座。SMTP、POP3
与 IMAP 协议客户端分别由 `xsmtp`、`xpop3`、`ximap` 扩展库提供，它们以本库为依赖
（`dependency_manifests`）组合出完整邮件能力。正式实现不复刻 socket、TLS、压缩、取消或
截止时间能力。

单头实现与声明分别为仓库根目录的 `single/extlibs/xmail.h` 与
`single/extlibs/xmail_decl.h`，均只包含 xmail 自身代码。使用前须按顺序提供
XRT 的所需模块，再包含 xmail；实现宏为 `XMAIL_IMPLEMENTATION`。
依赖选择与实现组合见 [构建说明](../../docs/BUILD.md#扩展单头与依赖顺序)。

## 分层

- `mail_content`：CRLF、Quoted-Printable、MIME Base64、Header、编码词、地址、日期、
  Message-ID、RFC 2231 参数、multipart 游标、轻量消息视图、可选拥有型 MIME 树、流式
  Builder 和高层 Compose。
- `mail_transport`：增量线路读写、dot transparency、共享 TCP、TLS、SASL（PLAIN/LOGIN/
  XOAUTH2/OAUTHBEARER 编码）与 raw-DEFLATE 传输适配。
- `src/mail`：纯邮件内容实现。
- `src/transport`：跨协议共享的线路、网络、TLS、压缩和认证实现。

内容解析层使用借用视图与调用方缓冲，常用写出接口支持精确容量，不强制建立消息对象树。
传输层借用调用方 Engine、Resolver、TLS Context 和 Verifier，不隐藏共享对象生命周期；
所有阻塞等待统一接受 deadline 和 cancel。

邮件构建分为两层：`mail_build` 直接向 sink 写字段、原始正文和 multipart part，不持有
正文；`mail_compose` 才提供文本、HTML、内联资源和附件的常见结构。只需要高性能原始
报文路径时，不会被迫携带高层消息描述和自动生成逻辑。

## 协议扩展

- `xsmtp`：SMTP 协议、同步客户端、STARTTLS/隐式 TLS、认证与 Compose 流式提交。
- `xpop3`：POP3 协议、同步客户端、STLS、认证与 RETR/TOP 到 MIME 树桥接。
- `ximap`：IMAP 协议、命令层、数据视图、流式 APPEND 与 RFC 4978 COMPRESS=DEFLATE。

## 裁剪

只选择内容与传输原语：

```c
#define XMAIL_MODULE_XMAIL
#include <xmail.h>
```

也可以选择单个层，例如 `XMAIL_MODULE_MAIL_BUILD`、`XMAIL_MODULE_MAIL_COMPOSE`、
`XMAIL_MODULE_MAIL_MESSAGE`、`XMAIL_MODULE_MAIL_TREE`、`XMAIL_MODULE_MAIL_WIRE`、
`XMAIL_MODULE_MAIL_NET` 或 `XMAIL_MODULE_MAIL_NET_TLS`。每个模块宏只展开清单声明的
依赖闭包；内容、线路、网络与 TLS 互相独立。

## 验证

```text
python tools/amalgamate.py --manifest extlibs/xmail/config/modules.json
python tools/build.py --compiler gcc --manifest extlibs/xmail/config/modules.json --suite xmail_tests --cflag=-Werror
python tools/build.py --compiler gcc --manifest extlibs/xmail/config/modules.json --suite xmail --trim-only --cflag=-Werror
python tools/check_api_docs.py --manifest extlibs/xmail/config/modules.json
python tools/check_release_maturity.py --release --manifest extlibs/xmail/config/modules.json
python tools/measure_performance.py --config extlibs/xmail/config/performance_profiles.json --manifest extlibs/xmail/config/modules.json --profiles '*'
python tools/measure_size.py --config extlibs/xmail/config/size_profiles.json --manifest extlibs/xmail/config/modules.json --profiles '*'
```

`xmail_tests` 包含 MIME 对抗输入回归：重复单例字段、非法传输编码、未闭合
multipart、头内 NUL、伪装分隔线，以及逐字节截断与控制字节变异。
可用 `--test test_mail_tree_adversarial --no-single` 单独运行这组样本。
`test_mail_net` 与 `test_mail_net_tls_runtime` 还用停顿的本地 DNS 解析器验证
明文及隐式 TLS 拨号进行中的取消、超时，以及解析器恢复后明文无迟到连接、
TLS 无迟到会话；两项测试由
`xmail_tests` 自动收集。

`examples/mail/compose` 向 stdout 输出完整 MIME 报文；Windows 使用二进制模式保留
CRLF，写入或刷新输出失败时返回失败。`python tools/test_mail_example_output.py`
独立解析实际输出的主题和正文，并以只读输出句柄验证失败返回；两平台 CI 均执行。
`test_mail_net_tls_close_fault` 在 TLS 关闭等待 Future 分配时注入内存失败，
确认立即请求中止、保留内存错误，并在销毁传输前由对端观察到异常关闭。
可用 `--suite mail_net_tls_close_fault_tests --no-single --no-examples`
单独运行。

根目录中的旧协议设计稿和 `xmail_xlang` 文件是历史迁移资产，不进入当前模块清单、公共头、
单头或发布包。正式 API、测试和文档分别以 `include`、`tests` 与 `docs/api` 为准。

## 覆盖率

实际 Dovecot/Postfix 互操作的运行条件、十八个场景和 Windows/WSL 用法见
[独立邮件服务器验收](../mail-real-server-testing.md)。

从仓库根目录执行：

```text
python tools/test_mail_coverage_inputs.py
python tools/measure_mail_coverage.py --product xmail
python tools/measure_mail_coverage.py --product xmail --report-only
```

工具重建模块套件，以 gcov JSON 原始计数统计本库全部自有 `.c` 文件，
保存精确行数、分支结果数和未触达位置。默认口径为
`module_and_independent_tls_object_profiles`，合并模块套件与独立 TLS 范例的原始
客户端对象计数，仅统计本库自有 `.c` 文件。Core 和 Python 服务端代码不计入；
内部头中的 static 函数另存于 `header-functions.json`，不改变主报告分母。
`--module-only` 可选择仅模块套件的独立口径；同一报告不能混用两种口径。
默认防倒退门槛与 CI 一致：行 73% / 分支结果 59%；门槛通过不代表生产验收完成。

报告位于 `out/mail/coverage/{win32,linux}/xmail/coverage.json`。
Windows 与 Linux 的编译计数分别保存在 `out/gcc/native-windows` 与
`out/gcc/native-linux`，避免交叉覆盖。报告绑定源码、Core、测试、夹具、清单、
构建工具和原始 `.gcno`/`.gcda` 的 SHA256；测量期间输入变化会使测量失败。
`--report-only` 要求保留原报告和原始计数，重新核对输入和精确计数，不重建测试。
旧计数未绑定报告、输入已经变化、计数丢失或报告与计数不符时都会拒绝读取，
不会给旧计数重新附上当前源码的哈希。CI 在两个平台执行溯源回归并保留报告及自有对象计数。
