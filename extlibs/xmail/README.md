# xmail extlibs plan

`xmail` 是 xrt 官方邮件协议扩展族的规划入口。

目标是在接入 xlang 之前，先在 xrt 的 `extlibs` 层完善邮件协议与 MIME 基础能力：

- `xmail_mime`: 邮件 MIME 构建与解析
- `xsmtp`: SMTP 发信
- `xpop3`: POP3 收信
- `ximap`: IMAP 收信与同步

当前进度：

- `xmail_mime` 已建立目录和本地测试，已支持基础编码、常用 MIME 构建、附件与 inline 资源
- `xsmtp` 已有独立 SMTP 发信能力，DATA MIME 构建已回改复用 `xmail_mime`
- `xpop3` 已建立官方扩展骨架、本地 mock server 测试和 MIME 解析接入
- `ximap` 已建立官方扩展骨架、本地 mock server 测试、UID FETCH MIME 与 BODYSTRUCTURE 基础解析
- xlang `x.mail.mime`、`x.mail.smtp`、`x.mail.pop3`、`x.mail.imap` source package 已创建，并已接到 `xmail_xlang` native JSON ABI；MIME build/parse 已接入真实 `xmail_mime`，SMTP capability/send 已接入真实 `xsmtp` 映射并通过本地 JSON ABI mock 能力查询和发送闭环，POP3 capability/status/list/fetch/delete_message 已接入真实 `xpop3` 映射并通过本地 JSON ABI mock 能力查询、邮箱状态、收信和显式删除闭环，IMAP capability/status/list/search/fetch/bodystructure/fetch_flags/store_flags/expunge/idle_probe/idle/watch 已接入真实 `ximap` 映射并通过本地 JSON ABI mock 能力查询、邮箱状态、邮箱目录、搜索、MIME fetch、header-only fetch、BODYSTRUCTURE、flags 读取/修改、EXPUNGE/UID EXPUNGE、短 IDLE 探针、有限 IDLE 事件收集和 watch loop 闭环
- 四个 xlang source package 的 `package.json` exports 已与源码 `export` 列表核对一致。
- `xmail.h` 是聚合头，可一次性包含 `xmail_mime`、`xsmtp`、`xpop3`、`ximap` 与 `xmail_xlang` 声明；各协议单头仍可按需单独包含。`xmail_xlang_test.c` 和 `xmail_live_test.c` 已改为通过该聚合头获取声明。
- `xmail/build_test.bat` 会构建并运行 `xmail_xlang_test`，用于聚合验证 native JSON ABI 到 MIME、SMTP、POP3、IMAP 的本地闭环；同时只编译不运行 `xmail_live_test`，避免无凭据环境触发真实网络。
- 真实 QQ 邮箱验证已开始：SMTP 587 STARTTLS、IMAPS 993、POP3S 995 均已在默认 peer verify 下完成真实闭环；本机 WSL2 Postfix + Dovecot 已完成 SMTP 投递到 Maildir、IMAP 143 STARTTLS、POP3 110 STLS 和 IMAP IDLE 真实事件样本；SMTP/POP3/IMAP TLS 失败结果已补 `tls=` 诊断码，POP3/IMAP socket 失败结果已补 `sys=` 诊断码

## 最小使用路径

1. 只需要构建或解析邮件内容时，直接包含 `../xmail_mime/xmail_mime.h`。
2. 需要发信时，使用 `xsmtp/xsmtp.h`，邮件内容构建会复用 `xmail_mime`。
3. 需要 POP3 收信时，使用 `xpop3/xpop3.h`，`RETR MIME` 会交给 `xmail_mime` 解析。
4. 需要 IMAP 收信或同步时，使用 `ximap/ximap.h`，`UID FETCH MIME` 会交给 `xmail_mime` 解析。
5. 需要一次性拿到全部邮件扩展声明时，包含 `xmail/xmail.h`。

## 本地验证

在 `D:\git\xrt\extlibs\xmail` 运行：

```bat
build_test.bat
```

该脚本会编译并运行 `xmail_xlang_test.c` 和 `xmail_aggregate_test.c`，同时编译 `xmail_live_test.c`；真实服务商测试程序只做编译检查。

## 实时验证

`xmail_live_test.c` 是可选真实服务商测试程序，只从环境变量读取凭据，不把真实配置写入仓库。

常用环境变量：

- `XMAIL_LIVE_EMAIL`
- `XMAIL_LIVE_PASSWORD`
- `XMAIL_LIVE_SUBJECT`
- `XMAIL_LIVE_SMTP_HOST` / `XMAIL_LIVE_SMTP_PORT` / `XMAIL_LIVE_SMTP_SECURE`
- `XMAIL_LIVE_SMTP_AUTH` / `XMAIL_LIVE_SMTP_EXPECT_TLS`，用于本机 Postfix 等无认证明文 SMTP 测试
- `XMAIL_LIVE_IMAP_HOST` / `XMAIL_LIVE_IMAP_PORT` / `XMAIL_LIVE_IMAP_SECURE`
- `XMAIL_LIVE_POP3_HOST` / `XMAIL_LIVE_POP3_PORT` / `XMAIL_LIVE_POP3_SECURE`
- `XMAIL_LIVE_SMTP_TLS_MAX_VERSION` / `XMAIL_LIVE_IMAP_TLS_MAX_VERSION` / `XMAIL_LIVE_POP3_TLS_MAX_VERSION`，可填 `tls1.2`、`tls1.3` 或数值版本
- `XMAIL_LIVE_SMTP_CA_FILE` / `XMAIL_LIVE_IMAP_CA_FILE` / `XMAIL_LIVE_POP3_CA_FILE`，可选 PEM CA bundle 路径
- `XMAIL_LIVE_VERIFY_PEER`
- `XMAIL_LIVE_SKIP_SMTP` / `XMAIL_LIVE_SKIP_IMAP` / `XMAIL_LIVE_SKIP_POP3`

已验证事实：

- QQ SMTP 587 STARTTLS 在默认 peer verify 下发信成功，收件端已收到 `xmail-live-*` 主题邮件。
- QQ IMAPS 993 和 POP3S 995 在默认 peer verify 下已完成真实收信闭环；本机 WSL2 Postfix + Dovecot 已完成 SMTP 明文无认证投递到 Maildir，再由 IMAP 143 STARTTLS 与 POP3 110 STLS 收信的闭环，Windows 侧 SMTP 建议使用当前 WSL IP 直连 25 端口；本机 Dovecot 还通过 `ximap_dovecot_idle_live_test.c` 验证了真实 IDLE `EXISTS` 事件推送；本次修复点包括 TLS 握手后先尝试解密内部密文缓冲、阻塞 TLS 握手内部 pending 数据继续 drive，以及 XRT TLS 叶子证书解析时即时匹配 DNS/IP SAN，避免多域名证书中 `imap.qq.com` / `pop.qq.com` 因固定 SAN 缓存截断被误判。
- QQ POP3 110 不支持 `STLS`；POP3 测试应优先修复 995 隐式 TLS。

## 收信语义

- POP3 `RETR` 不会自动删除邮件；只有 xlang `delete_message` / native 显式 `DELE` 或 future 选项 `bDeleteAfterRetr` 才会进入删除流程。
- IMAP `FETCH/UID FETCH` 不会删除邮件；打开或读取后即使被标记为 `\Seen`，仍可继续按 UID 或搜索结果读取。

## 安全建议

- 不要把真实邮箱密码、应用专用密码、OAuth2 token 或测试服务凭据提交到仓库。
- 真实网络验证应使用本地未跟踪配置、环境变量或临时测试程序。
- 生产环境默认启用 TLS 证书校验；只有受控本地调试才考虑关闭校验。
- 日志只记录协议阶段、服务端回复和错误摘要，避免输出认证负载。
- 收信侧不要默认信任 MIME 附件文件名；上层保存附件前应做路径和文件名规范化。

文档：

- [xmail-mail-protocols-spec.md](xmail-mail-protocols-spec.md)
- [xmail-mail-protocols-design.md](xmail-mail-protocols-design.md)
- [xmail-xlang-api-contract.md](xmail-xlang-api-contract.md)
