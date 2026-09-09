# xwork FAQ

> 状态：中文初稿，待审阅。

## xwork 和 xllm 的边界是什么？

`xllm` 负责模型侧：provider、request/response、stream、session、memory 和 tool call 协议。

`xwork` 负责工作流侧：workspace、run、tool execution、approval、artifact、checkpoint、persistence、multi-agent、remote worker 和 replay。

## xwork 是完整 Agent 产品吗？

不是。xwork 是 Agent 开发通用库和 runtime 基础设施。UI、CLI、云服务、用户账号、部署、产品策略和品牌化体验属于上层产品，例如 AI IDE 或 claw。

## 为什么 remote worker 不内置网络服务？

生产 remote worker 需要处理 socket 生命周期、认证、签名、mTLS、tenant/project 隔离、重试、限流、blob streaming 和部署拓扑。这些策略强依赖产品。xwork 只定义 control-plane 对象和 decoded-message transport 边界，避免把库绑定到某个云架构。

## 为什么恢复不恢复 live process/terminal？

OS process handle、terminal session、线程栈和 callback 栈不能可靠序列化。xwork 持久化的是 snapshot、event、artifact 和 replay cassette。恢复后，产品应重新发现或重启 live 资源。

## xwork 是否可以直接替代产品内的工具系统？

可以逐步替换共享语义部分：工具定义、审批、host service contract、artifact 和 persistence。产品特定 UI、权限、远程网络和编辑器 bridge 仍应保留在产品层，通过 xwork host services 接入。

## xwork 是否要求使用内置 file persistence？

不要求。内置 file backend 适合本地 durable run、示例和 smoke。产品可以实现 `xwork_persistence_backend` 对接自己的数据库或对象存储。

## xwork 是否内置 planner？

不内置完整 autonomous planner。xwork 提供 planner boundary、plan report artifact、tool choice 和 task graph 导入能力。真正规划策略由产品或模型层实现。

## 相关文档

- [架构说明](ARCHITECTURE.md)
- [最佳实践](BEST_PRACTICES.md)
- [迁移指南](MIGRATION.md)
