# XTP3 性能与体积记录

2026-11-16，Windows x64、GCC -O2，CPU 标识 `AMD64 Family 25 Model 33 Stepping 2, AuthenticAMD`。这些是本机容量与回环记录，机器没有固定亲和性或电源策略，不作为跨机器基线或 HTTP 倍数承诺。源码/编译器信息、摘要及原始报告 SHA256 见 [测量收据](measurements/2026-11-16-windows.json)。原始报告保存在可重建的 out/xtp3-bench/，不把测试输出作为生产源码。

## 重建

从 XRT 根目录运行，PATH 中提供 GCC/Python；Linux 可用相同命令，但不能直接比较不同主机结果：

```text
python extlibs/xtp/bench/measure_matrix.py --compiler gcc --count 5000 --repeats 3 --report out/xtp3-bench/windows-matrix.json
python extlibs/xtp/bench/measure_native.py --compiler gcc --count 2000 --repeats 3 --report out/xtp3-bench/windows-native.json
python tools/measure_performance.py --compiler gcc --config extlibs/xtp/config/performance_profiles.json --report out/xtp3-bench/performance-windows.json
python tools/measure_size.py --compiler gcc --config extlibs/xtp/config/size_profiles.json --profiles xtp_codec,xtp_session,xtp_delivery --kind static --report out/xtp3-bench/size-windows.json
```

每个场景先独立进程预热，再记录三次样本。每次实际样本仍新建实例。JSON 保留每次样本，不只保留最快值；--quick 用于工具冒烟，不替代完整矩阵。Windows CPU 使用 GetProcessTimes；Linux 使用进程 clock。短样本 CPU 计数受分辨率限制，零值不表示零成本。本地 CPU 只计测量循环，原生 CPU 包含连接、TLS 握手与关闭生命周期。

## 覆盖与结果

本地 296 种配置、888 个样本：Codec 编码+解码覆盖四种形态、六种 Body 和批量 1/8/32；Session 覆盖同样形态/Body/批量以及 RPC/PUSH/NOTIFY；另列八种可恢复 RPC/PUSH 场景。Body 为 16/64/256/1024/16384/65536 B。普通场景全部提交并完成，无拒绝或超时。

64 B Body、一次提交八条普通 RPC、两项参数、本地同步往返，中位数如下。Latency 从 API 提交起包含排队；不是一次 Decode 的成绩：

| 组合 | RPC/s | P99 μs |
| --- | ---: | ---: |
| ID + array | 239,809 | 46.9 |
| name + array | 238,595 | 40.0 |
| ID + table | 209,726 | 66.3 |
| name + table | 233,322 | 42.7 |

完整连续帧 Decode 不分配、不复制 Body；上述本地路径每个 Output 额外复制一次到测试承载缓冲，再 Advance/Input。Submit 与 Reply 的 Body 编码复制仍存在，普通 RPC 每次两个缓冲分配请求；ACK 不单独分配缓冲。JSON 的 host_copy_bytes 仅计测试承载复制，不能冒充全部复制。Allocator 次数是库的分配请求次数，非 OS malloc 次数。峰值为 XTP 自有预留账本，非进程 RSS，分配器自身元数据与内核网络缓冲另计。

原生回环 60 种配置、180 个样本：54 种闭环配置（TCP/TLS/WS × 六种 Body × 在途 1/8/32）及六种固定到达率配置。每个样本同时测试双向 RPC，各方向 2000 个提交目标，一个网络 worker。Native 场景使用数字命令/空参数；TLS 固定测试身份 pin，WS 从已完成 Upgrade 的 stream 接入。64 B、每端在途 32、闭环结果如下：

| 承载 | 双向合计 RPC/s | 两端较慢 P99 μs |
| --- | ---: | ---: |
| TCP | 142,400 | 768.5 |
| TLS1.3 ChaCha20-Poly1305 | 82,390 | 1508.1 |
| WS（已升级 TCP） | 133,806 | 855.4 |

表格每项先取每次运行两端合计吞吐或较慢端 P99，再取三次中位数；原始报告另有 P50/P95/P99/P99.9/最大值、字节、分配、峰值、CPU、读写事件和定时器计数。网络吞吐包含业务回调、编码、分帧、真实 socket 和承载处理；RPC 时间从首条提交起至双方业务完成，排除握手。TLS、WS、复制读写和 Reader 跨输入组装的额外复制均仍存在，不声称端到端零复制。

固定到达率为每方向 10000 与 200000/s。1ms worker 定时器按独立理想到达时钟计算新增请求；实际调度可能合并到达，max generator_lag_us 单列。客户端在途窗口满时拒绝所有超额到达，仍计入 offered/rejected，不能只观察成功请求。10000/s 场景三种承载全部 4000/4000 完成；200000/s 出现显著客户端窗口拒绝，具体三次计数留在收据。高负载结果同时受生成器分辨率和在途窗口影响，不能据此推算协议的理想均匀到达极限。

native_sent_bytes/native_received_bytes 是开始关闭前的 TCP 并发快照，含 TLS 握手或 WS Mask/帧开销；最后写完成事件可能尚未退休。plaintext_bytes 是 XTP 线帧字节。两者均不包含 IP/TCP 包头、重传或链路层。事件/定时器计数不能替代内核唤醒和系统调用采样。

可恢复 RPC 默认 1024 个结果回执槽保留 60s，长期只有约 17 次/秒的容量；测试在 5000 个提交目标中明确记录 1024 完成、3976 拒绝。没有提前淘汰回执或延长截止时间来获得漂亮数据。普通 RPC、可恢复 PUSH 与可恢复 RPC 必须分开解释，见 [Delivery](delivery.md)。

## 体积

自有 include/src 的 C/h 按 UTF-8/LF 计 116790 B（114.05 KiB），包括可选模块与必要公共/内部头；源码 40–80 KiB 的初始预算没有满足。分项为公共头 27748 B、Codec 15203 B、Reader 13168 B、Session（含可选 Delivery）50654 B、内部头 3237 B、承载适配 6780 B。生成单头不重复算；依赖、测试、示例、文档不混入此口径。

GNU size 对自有 libxtp.a 的 text 指标：Codec 8088 B，基础 Codec+Reader+Session 36024 B，包含 Delivery 为 42912 B。data/bss 均为零；实例状态在堆内。它们不包括独立的 XRT/TLS/xws/xrudp 依赖或整个可执行文件，也不是原始源码字节。静态包分别构建依赖，不能把依赖从运行成本中消失。

## 资格边界

xrudp 目前有真实 UDP、Noise、60Hz 快照与独立资源流、丢失/乱序/重复、RTT 和 MTU 故障验收，尚未取得同口径饱和吞吐矩阵。完整 WSS/HTTP Upgrade、内核唤醒与所有复制的动态采样、多连接跨核扩展性、32位/大端、移动网络/NAT 和长稳仍需发布验收。当前结果不证明互联网时延、唯一游戏网络底座的全部生产资格或相对 HTTP 的固定性能倍数。
