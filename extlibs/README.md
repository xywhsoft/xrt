# XRT 扩展库

`extlibs` 放置建立在 XRT 公共 API 之上的完整能力体系。扩展库不进入核心 `xrt.h`，每个库
拥有独立清单、裁剪宏、单头输出、测试、文档和体积门槛；正式实现不得依赖 `src/internal`。

## 当前扩展

- `xruntime`：类型化运行时、动态值与语言运行时支撑。
- `xhttp`：HTTP 高层语义、客户端和服务端抽象。
- `xws`：WebSocket 高层客户端与服务端抽象。
- `xregex`：正则表达式引擎。
- `xmail`：邮件内容、SMTP、POP3 和 IMAP 客户端体系。
- `xssh`：分层 SSH 协议扩展，wire 底层已经完成迁移。

每个现代扩展以 `config/modules.json` 声明依赖闭包，并通过仓库统一工具生成 features、单头和
API 参考。扩展之间允许真实的体系依赖，但不通过私有函数表伪造解耦，也不复制 XRT 已有的
网络、TLS、压缩、任务或错误实现。

## 历史实现

旧的 `xsmtp`、`xpop3`、`ximap` 和 `xmail_mime` 已整合为现代 `xmail`，原始单文件实现保存在
`dev/archive/mail_legacy_20260816`，不再属于产品构建。现代能力映射见
`xmail/docs/design/legacy-parity.md`。

旧 `xssh` 单头已经归档到 `dev/archive/ssh_legacy_20260816`。现代 `xssh` 按协议层级迁移，
不保留旧 xnet2 回调、固定容量运行时或兼容 API。
