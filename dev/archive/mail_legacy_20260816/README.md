# 旧版邮件扩展归档

本目录保留重构前的 `xmail_mime`、`xsmtp`、`xpop3` 和 `ximap` 单文件实现，仅用于协议行为、
测试样本和历史设计追溯。

这些实现不进入产品清单，也不再作为公共 API：它们自行实现阻塞 socket/TLS，异步接口按
操作创建线程，并在多个协议中重复缓冲、错误和 MIME 逻辑。现代实现已经按层拆入
`extlibs/xmail`，复用 XRT 网络、TLS、压缩、取消、截止时间和结构化错误体系。

能力与测试映射见 `extlibs/xmail/docs/design/legacy-parity.md`。迁移时保留了有价值的协议
行为和边界用例，没有保留旧名称、兼容包装和隐式线程模型。

`xmail_binding` 保存旧版聚合头、xlang 绑定、聚合/在线测试和早期设计文档。这些文件依赖
已经归档的独立 `xsmtp`、`xpop3`、`ximap`、`xmail_mime` 以及旧单头文件路径，仅供历史
追溯，不属于现代 `extlibs/xmail` 的构建输入。新版 xlang 绑定应直接基于现代 xmail API
重新实现。
