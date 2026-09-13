# XRT 扩展库

`extlibs` 放置建立在 XRT 公共 API 之上的完整能力体系。扩展库不进入核心 `xrt.h`，每个库
拥有独立清单、裁剪宏、单头输出、测试、文档和体积门槛；正式实现不得依赖 `src/internal`。

## 当前扩展

- `xruntime`：类型化运行时、动态值与语言运行时支撑。
- `xhttp`：HTTP 高层语义、客户端和服务端抽象。
- `xws`：WebSocket 高层客户端与服务端抽象。
- `xmail`：邮件底层库——MIME 内容层（解析/构建/组合）与传输基座（线路读写、TCP/TLS 会话、SASL）。
- `xsmtp`：构建在 xmail 之上的 SMTP 客户端（协议、STARTTLS/隐式 TLS、认证、流式提交）。
- `xpop3`：构建在 xmail 之上的 POP3 客户端（协议、STLS、认证、RETR 到 MIME 树桥接）。
- `ximap`：构建在 xmail 之上的 IMAP 客户端（协议、命令层、数据视图、流式 APPEND、COMPRESS）。
- `xssh`：分层 SSH 协议扩展，wire 底层已经完成迁移。

每个现代扩展以 `config/modules.json` 声明依赖闭包，并通过仓库统一工具生成 features、单头和
API 参考。扩展之间允许真实的体系依赖，但不通过私有函数表伪造解耦，也不复制 XRT 已有的
网络、TLS、压缩、任务或错误实现。

## 历史实现

早期的单文件 `xsmtp`、`xpop3`、`ximap` 和 `xmail_mime` 先整合为现代 `xmail`，后又于
2026-09 拆分为 `xmail` 底层库加三个独立协议扩展（`xsmtp`/`xpop3`/`ximap`）。原始单文件
实现保存在 `dev/archive/mail_legacy_20260816`，不再属于产品构建。现代能力映射见
`xmail/docs/design/legacy-parity.md`。

旧 `xssh` 单头已经归档到 `dev/archive/ssh_legacy_20260816`。现代 `xssh` 按协议层级迁移，
不保留旧 xnet2 回调、固定容量运行时或兼容 API。
