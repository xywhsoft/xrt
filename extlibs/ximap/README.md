# ximap

ximap 是构建在 xmail 邮件基座之上的 IMAP 客户端扩展库：协议解析、命令层、FETCH/BODYSTRUCTURE 数据视图、流式 APPEND 与 RFC 4978 COMPRESS=DEFLATE。通过 `XIMAP_MODULE_*` 宏裁剪，单头形态为 `single/ximap.h`。

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
