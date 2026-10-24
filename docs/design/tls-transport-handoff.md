# TCP 与 TLS 的拥有型交接

TLS 组合对象必须复用 TCP 的事件拥有者机制，不能直接改写 Events／Data。
旧上下文可能仍被当前 Open／Accept／Read 调用栈借用，必须在 Worker 分派栈
退出后退役；保留旧 EventOwner 却改写事件也不是合法拥有图。

`xrtNetStreamConnectOwnedV1` 在投递任何启动工作前安装一个永久事件策略和
恰好一个真实 Data 引用。成功消费该引用，失败消费零个；失败发布启动任务
时先解除未发布的拥有记录，再释放构造资源。旧借用式 Connect 不改变合同。

TLS Attach／Accept 使用 TCP 的事件替换准入，包括已被接受但尚未 Open 的
TCP Stream。Attach 遇到待读 Future 时拒绝交接；失败保留原事件上下文、
两份调用方引用和 Session 四个缓冲的原分配策略。成功后才发布 Transport
和输出引用，之后调用方不能继续直接操作已转移的 TCP／Session。

这一批是 TLS 拥有图的交接前置，不是完整原生 TLS 拥有权 API。Stream 的
运行时根、会话／配置拥有边、Promise／CancelWatch 节点、Listener／Dial、
静止检查和 checked 退役仍需补齐；不能把它们标成叶节点或伪造引用计数。

专项入口 `tools/check_tls_transport_handoff.py` 仅运行明确列出的新生命周期
用例和两份既有正常固定密钥 TLS 回环用例，不执行模块的广泛测试列表。
没有外部地址、DNS、畸形流量、模糊测试、探测或安全性质验收。

## 本批常规验证

冻结 2331 份原生输入。模块化和单头均通过 GCC O0／O2、TCC，Linux 另
通过 ASan／UBSan。Windows select／IOCP 与 Linux select／epoll 执行新交接
用例；两份既有正常固定密钥 TLS 握手和 Attach 用例分别回归。原生共 945
个编译／运行步骤、42 次 fixture 执行，包含 84 轮交接生命周期；这是重复
构建和运行的计数，不代表 945 个独立测试。覆盖创建失败不消耗上下文引用、
真实事件拥有边、待读 Future 拒绝交接、分配策略回滚、延迟 Drop 和内存平衡。

本批三个单头命名段与生成器一致，反向移除本批改动可精确恢复原内容。
未选择段不导入；完整原生单头仍有非本批生成差异，未因此宣称完整工作区
或发布产品通过。证据根为
`F:/xlang-validation/five-groups-20261024/p6-tls-transport-handoff`。
