# xpop3

xpop3 是构建在 xmail 邮件基座之上的 POP3 客户端扩展库：协议解析、同步客户端、STLS、SASL 认证与 RETR/TOP 到 MIME 树的桥接。通过 `XPOP3_MODULE_*` 宏裁剪，单头形态为 `single/xpop3.h`。

## 客户端范例

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
