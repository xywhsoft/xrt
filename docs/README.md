# XRT 文档

本目录只保留当前产品的使用、设计和参考资料。历史审计记录由版本控制保存，不作为发布文档的一部分。

## 从这里开始

- [构建与发布](BUILD.md)：工具链、构建方式、测试和发布产物。
- [特性选择](FEATURE_SELECTION.md)：模块化与单头模式的选择方法。
- [示例](EXAMPLES.md)：按场景挑选可运行的示例。
- [范围说明](SCOPE.md)：核心库、扩展库和不在支持范围内的能力。
- [仓库资产策略](ARTIFACT_POLICY.md)：正式生成资产、临时产物和历史归档的 CI 边界。

## 使用指南

- [并发](guide/concurrency.md)：任务、Future、取消和关闭顺序。
- [任务组](guide/task-group.md)：管理一组并发任务的生命周期。
- [加密](guide/crypto.md)：哈希、编码和随机数的边界。
- [时间、路径与文件](guide/time-path-file.md)：跨平台文件系统操作。
- [XID](guide/xid.md)：生成、解析和传递标识符。

## 设计说明

设计说明描述公开行为、资源所有权、并发边界和适用范围，而不是开发过程或内部迁移记录。

- [设计总览](DESIGN.md)：选择模块、组织资源生命周期，并确定核心与扩展的边界。
- [运行时架构](ARCHITECTURE.md)
- [HTTP 运行时](design/http-runtime.md)
- [JSON 与 XSON](design/json-xson.md)
- [日志](design/logger.md)
- [进程](design/process.md)
- [模板](design/template.md)
- [TLS 会话](design/tls-session.md)
- [XID](design/xid.md)

## API 与参考资料

`api/` 按模块提供 API 参考；公开头文件是函数签名与编译条件的最终依据。集成第三方代码前，请同时阅读[第三方组件](THIRD_PARTY.md)。发布前的兼容平台、验证方式和 io_uring 特殊限制见[发布支持](RELEASE_STATUS.md)。
