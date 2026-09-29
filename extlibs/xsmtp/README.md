# xsmtp

xsmtp 是构建在 xmail 邮件基座（MIME 内容层与传输层）之上的 SMTP 客户端扩展库：协议解析、同步客户端、STARTTLS/隐式 TLS、SASL 认证与从 xmailmessage 派生的流式提交。通过 `XSMTP_MODULE_*` 宏裁剪，单头形态为 `single/xsmtp.h`。

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
