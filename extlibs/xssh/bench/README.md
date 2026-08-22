# SSH Wire 性能基准

`bench_ssh_wire.c` 分开测量三条无分配热路径：

- uint32、uint64 和 string 的 writer/reader 往返。
- 按客户端优先级扫描双方 name-list。
- 无符号 magnitude 的规范 mpint 写入与读取。

所有缓冲均在栈上复用，计时段不分配内存，也不混入网络或密码操作。校验和会消费每轮输出，
避免优化器删除协议工作。

```powershell
python tools/measure_performance.py `
	--config extlibs/xssh/config/performance_profiles.json `
	--manifest extlibs/xssh/config/modules.json `
	--profiles ssh_wire --smoke
```

绝对速率只应在相同机器、编译器、架构和参数下比较。修改基础类型布局、列表扫描或 mpint
规范化后，吞吐量中位数下降超过配置门槛时必须复测并分析。

`bench_ssh_packet.c` 使用可复现的会话 PRNG 填充 padding，测量 16 字节块下 packet 构建、
借用解析和序列递增的完整往返。该数字不包含系统随机和密码算法成本。

`bench_ssh_packet_aes_gcm.c` 测量 AES-128-GCM packet 的原位加密、认证和调用方缓冲解密。
计时路径无堆分配，也不包含系统随机调用，因此可以单独观察 packet 与 AES-GCM 实现的变化。

`bench_ssh_kex_curve25519.c` 使用 RFC 7748 固定密钥材料测量 X25519 共享秘密计算，不把系统随机、
报文构建或 exchange hash 混入标量乘法指标。

`bench_ssh_transport_state.c` 在计时前完成 identification、初始 strict KEX 和首包机会窗口处理，
分别测量稳定 OPEN 数据面与对端已发起 rekey 时，本端每包 `Check -> Commit` 状态转换成本。
该路径不分配内存，也不混入 packet codec、密码算法和网络入队时间。

`bench_ssh_transport_core.c` 在 AES-GCM OPEN 状态下循环执行最终线路包准备、可靠发送提交、认证
接收准备和上层接收提交。它包含 packet codec、协议分类和双向 rekey 计数，不包含网络等待、
系统随机或任何堆分配，可用于观察组合层相对裸 codec 的固定编排成本。
