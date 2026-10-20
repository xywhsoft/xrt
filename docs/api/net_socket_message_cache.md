# Windows Socket 消息入口缓存

`WSARecvMsg`／`WSASendMsg` 的 Provider 入口按实际 Socket 缓存，首次查询可能来自
同步 I/O、能力查询或 IOCP。缓存的每一次读写都使用同一指针表示的 Interlocked
操作，不能混用普通读写与原子写入。首次成功结果通过 compare/exchange 发布；
竞争失败者返回已经发布的入口，不覆盖它。

Provider 查询不持锁，失败不写入失败哨兵，保留后续重试和原有诊断合同。
不增加锁、分配、公开 API 或模块依赖；不改变 Socket 的公开 ABI。
这不授权并发关闭、元数据配置或其它原本要求调用方串行化的操作。

专项入口：`tools/check_net_socket_message_cache.py`。指定全新输出目录和编译器，
可用 `--root` 选择干净的 HEAD 加本补丁快照，`--header` 追加固定消费端单头。
测试根依赖为 net_socket／thread／atomic／time；不是 net_socket 的产品依赖增量。

验证为 IPv4／IPv6 各创建一个未绑定 UDP 对象，8 个线程同时首次查询，检查返回
入口与能力一致并正常回收。没有 bind／connect／send／recv、名称解析或网络流量；
不属于网络安全或压力专项。Linux 验证同一源改动和既有非 Windows 分支。
此测试不能代替整个传输层或所有并发状态的验收。
