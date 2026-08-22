# 旧版 SSH 扩展归档

本目录保留重构前的 `xssh` 单头、测试、设计稿和示例，仅用于协议行为、测试向量与历史设计
追溯。

旧实现将 wire、packet、密码、KEX、网络、认证、channel、known_hosts、private key 和运行时
集中在一个头中，并依赖已经移除的 xnet2 回调接口。其 runtime 还带有固定 channel、event、
接收和 pending 缓冲上限，因此不再作为新版公共 API 或产品实现。

现代实现位于 `extlibs/xssh`，按 wire、packet、crypto/KEX、transport、auth、channel 顺序迁移。
迁移会复用已验证算法、协议样本和边界测试，但不会保留旧名称、固定容量、隐式 Engine 或
兼容包装。
