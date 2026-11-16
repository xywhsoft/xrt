# xtp

纯 C XTP 3.0 扩展。已实现 Codec、Reader、Session、可选 Delivery 及已有 TCP/TLS/WS/xrudp 适配：32 字节固定头、两类命令、保留顺序的 table/array、批量编码、STREAM/RECORD 增量输入、请求匹配、可靠推送回执和不可靠推送。不兼容 1.0/2.0。

Codec 不分配；Reader 对完整连续帧直接借用，仅为跨输入帧分配组装缓冲。无网络、线程和全局状态。字节流损坏的帧边界不可推测恢复，RECORD 可在可信记录边界恢复；可辨认长度的坏帧直接跳过 Body。配置始终限制工作量、缓冲及增长峰值、坏帧数和接收期限。

`xtpParamAt` 对 array/table 均按发送顺序 O(1) 读取；`xtpParamFind` 仅对 table 按 key 顺序查找。去重校验只排序临时索引，不改变字段顺序。Body 和值不做转换。

```sh
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp_tests --cflag=-O2
python tools/build.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --cflag=-O2
python tools/package.py --compiler gcc --manifest extlibs/xtp/config/modules.json --suite xtp --kind static --verify
```

选择 `XTP_MODULE_XTP_CODEC`、`XTP_MODULE_XTP_READER` 或 `XTP_MODULE_XTP_SESSION`，声明入口 `<xtp.h>`。扩展单头只包含本库，先提供同一份 XRT；单个编译单元定义 `XTP_IMPLEMENTATION`。

Reader 回调返回 PAUSE 表示尚未消费该消息；完整借用帧仍在调用者的未消费后缀，已组装帧由 Reader 保留。调用 Resume 后重交未消费后缀；AGAIN 按 Used 继续，空 Input 不代表 EOF。已有网络后端只需传入字节和边界，见 [增量后端示例](examples/backend/main.c)。

Session 使用同一 `xtpSubmit` 与 `OnMessage` 表达四种 Command/Params 组合。`xtpReply` 接受请求令牌，结果和 ACK 通过 `OnComplete` 完成原操作。网络层按 `xtpOutput` 的 Ticket 获取数据，再以 `xtpAdvance` 报告实际受理前缀；`xtpDrive` 驱动期限和有界维护，无后台任务。见 [双端会话示例](examples/session/main.c) 与 [会话接入契约](docs/session.md)。

Session 的 Bind 接受宿主已验证的安全连接声明，自身不执行 TLS/Noise 认证。P4 已提供通用 `xtpDrain` 和已有 TCP/TLS/WS 写入适配，见 [承载接入](docs/transport.md)。选择相应模块才引入网络依赖，纯消息后端可继续直接使用 Input/Output/Advance。

P5 已提供 xrudp BYTES/DATAGRAM 适配及 [游戏例程](docs/xrudp.md)。P6 已提供可选 [Delivery](docs/delivery.md)：经认证的状态续接、原帧重放、BUSY 退避、结果缓存及 RESULT_ACK；默认关闭，按消息启用。XS 已替换原 XTP2 并通过外层/嵌套 TCC 验证。模块/单头与静态/动态包已验收；[性能与体积记录](docs/performance.md) 分开列出已测结果和生产资格缺口。

完整协议、交付语义和阶段验收见 [设计](../../docs/design/xtp3.md) 与 [API 方案](../../docs/design/xtp3-api.md)。[Codec 示例](examples/codec/main.c) 可直接运行；[覆盖与进度](docs/coverage.md) 明确尚未完成的阶段。

可恢复 RPC 的默认 1024 槽/60s 保留约束会限制持续速率，默认不启用。自有源码/头共 114.05 KiB，超出初始源码预算；GCC 自有 Session 含 Delivery 的 text 约 41.9 KiB，依赖另计。生产部署资格不由本地功能/回环测试自动取得，详见验收记录。

许可证沿用 XRT 仓库的 MIT 声明，独立快照包含 [LICENSE](LICENSE)。
