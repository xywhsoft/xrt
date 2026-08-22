# XRT Windows GCC 16 x64 性能基线

日期：2026-08-09

本报告是 `PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json` 的人类可读摘要。JSON 保存全部
原始样本、环境身份、runner 指纹和指标契约，是自动比较的唯一权威输入。

## 环境

| 项目 | 值 |
| --- | --- |
| 报告 schema | 2 |
| 平台 | Windows 10 x64 |
| CPU | AMD Ryzen 5 5600，6 核 12 线程 |
| 进程亲和 | `0xfff/0xfff`（进程掩码/系统掩码） |
| 电源计划 | `381b4222-f694-41f0-9685-ff5bb260df2e` |
| 编译器 | GCC 16.1.0 |
| 编译参数 | C11、`-O2 -Wall -Wextra -Werror -m64` |
| 运行策略 | 串行、1 次预热、5 次计入样本、中位数 |
| 默认质量上限 | MAD 20%，中央范围 30% |
| 源码提交 | `51d3492b3868440acb46595bb9ef2682bd30bd54` |
| 工作树 | 有未提交修改 |
| 源码指纹 | `19c95fcee8d737f6b5d799df4ccb5e66f312d8ffee80aad12ae8be2faa0b2c20` |
| runner 指纹 | `a078a780ab851c9b742dfb1a4ad70b00d01572a6ee46086a5dc9ea41298334f4` |

当前仓库仍处于整体重构期，因此这是当前工作树的开发基线。正式发布候选必须从干净
工作树重新生成，不能把 `source_dirty=true` 的数据作为最终发布提交证明。

## 结果

`MAD` 是五个独立进程样本的中位绝对偏差除以中位数；`完整范围` 保留最大值与最小值
之差，`中央范围` 去掉一个最高值和一个最低值后计算。质量门禁使用 MAD 和中央范围，
完整范围保留单次系统抢占证据。吞吐指标检查下降，延迟指标检查上升。

| 体系 | 指标 | 中位数 | MAD | 完整范围 | 中央范围 | 回归上限 |
| --- | --- | ---: | ---: | ---: | ---: | ---: |
| Task Pool | windowed (`windowed_tasks_per_sec`) | 1175085 tasks/s | 0.4% | 2.9% | 1.1% | -10% |
| Task Pool | queue-one (`queue_one_tasks_per_sec`) | 569963 tasks/s | 1.5% | 19.0% | 2.1% | -10% |
| Channel | local pair (`local_items_per_sec`) | 31244239 items/s | 0.2% | 0.7% | 0.4% | -10% |
| Channel | buffered (`buffered_items_per_sec`) | 20286585 items/s | 0.2% | 1.5% | 0.4% | -10% |
| Channel | rendezvous (`rendezvous_items_per_sec`) | 1846788 items/s | 1.7% | 11.0% | 9.4% | -10% |
| Channel Select | ready (`select_ready_items_per_sec`) | 13185103 items/s | 0.0% | 0.2% | 0.1% | -10% |
| Channel Select | transfer (`select_transfer_items_per_sec`) | 5844790 items/s | 0.6% | 4.9% | 1.1% | -10% |
| Coroutine Channel | buffered (`buffered_co_items_per_sec`) | 6880998 items/s | 0.5% | 1.6% | 0.8% | -10% |
| Coroutine Channel | rendezvous (`rendezvous_co_items_per_sec`) | 2127925 items/s | 0.5% | 10.0% | 4.6% | -10% |
| Coroutine Channel | select (`select_co_items_per_sec`) | 6479309 items/s | 0.4% | 28.2% | 0.5% | -10% |
| Queue | MPSC (`mpsc_items_per_sec`) | 8984458 items/s | 2.0% | 5.0% | 2.4% | -10% |
| Queue | MPSC batch 32 (`mpsc_batch_items_per_sec`) | 110075484 items/s | 3.5% | 12.8% | 6.1% | -10% |
| Queue | MPMC (`mpmc_items_per_sec`) | 7034077 items/s | 0.1% | 0.8% | 0.7% | -10% |
| Queue | MPMC batch 32 (`mpmc_batch_items_per_sec`) | 120086871 items/s | 1.7% | 12.7% | 2.3% | -10% |
| TCP sync loopback | round trip (`tcp_round_trips_per_sec`) | 41799 round-trips/s | 0.1% | 0.9% | 0.2% | -15% |
| TCP sync loopback | p50 (`tcp_latency_p50_us`) | 23.8 us | 0.0% | 0.4% | 0.4% | +20% |
| TCP sync loopback | p99 (`tcp_latency_p99_us`) | 29.2 us | 5.5% | 12.7% | 10.6% | +25% |
| UDP batch loopback | packet (`udp_packets_per_sec`) | 230652 packets/s | 0.8% | 2.1% | 1.1% | -15% |
| HTTP reply keep-alive | throughput (`http_reply_requests_per_sec`) | 24536 requests/s | 0.3% | 1.4% | 0.4% | -15% |
| HTTP reply keep-alive | p50 (`http_reply_latency_p50_us`) | 40.9 us | 0.0% | 1.2% | 0.0% | +20% |
| HTTP reply keep-alive | p99 (`http_reply_latency_p99_us`) | 56.1 us | 5.2% | 15.9% | 8.4% | +25% |
| HTTP raw keep-alive | throughput (`http_raw_requests_per_sec`) | 30157 requests/s | 0.6% | 1.4% | 0.6% | -15% |
| HTTP raw keep-alive | p50 (`http_raw_latency_p50_us`) | 34.9 us | 0.3% | 5.7% | 0.9% | +20% |
| HTTP raw keep-alive | p99 (`http_raw_latency_p99_us`) | 42.2 us | 1.7% | 11.8% | 3.1% | +25% |
| WebSocket | upgrade (`ws_upgrade_latency_us`) | 661.6 us | 4.6% | 12.1% | 6.3% | +25% |
| WebSocket | message (`ws_messages_per_sec`) | 51143 messages/s | 0.2% | 1.7% | 0.2% | -15% |
| WebSocket | p50 (`ws_latency_p50_us`) | 19.4 us | 1.0% | 2.1% | 1.0% | +20% |
| WebSocket | p99 (`ws_latency_p99_us`) | 24.8 us | 2.4% | 12.9% | 8.1% | +25% |
| TLS 1.3 full | throughput (`tls_full_handshakes_per_sec`) | 141 handshakes/s | 0.1% | 0.9% | 0.2% | -15% |
| TLS 1.3 full | p50 (`tls_full_handshake_latency_p50_us`) | 7040.5 us | 0.1% | 0.6% | 0.2% | +20% |
| TLS 1.3 full | p99 (`tls_full_handshake_latency_p99_us`) | 7477.0 us | 0.4% | 6.7% | 4.1% | +25% |
| TLS 1.3 resume | throughput (`tls_resume_handshakes_per_sec`) | 936 handshakes/s | 0.4% | 1.6% | 0.8% | -15% |
| TLS 1.3 resume | p50 (`tls_resume_handshake_latency_p50_us`) | 1053.4 us | 2.0% | 5.0% | 2.6% | +20% |
| TLS 1.3 resume | p99 (`tls_resume_handshake_latency_p99_us`) | 1208.9 us | 0.5% | 2.3% | 1.3% | +25% |
| TLS stream | round trip (`tls_stream_round_trips_per_sec`) | 59446 round-trips/s | 0.5% | 1.3% | 0.9% | -15% |
| TLS stream | p50 (`tls_stream_latency_p50_us`) | 16.6 us | 0.0% | 1.2% | 0.6% | +20% |
| TLS stream | p99 (`tls_stream_latency_p99_us`) | 19.7 us | 2.0% | 8.1% | 3.0% | +25% |

正式 Task 场景分别提交 20 万个 windowed 与 queue-one 任务。普通 Channel 使用
500 万 local、500 万 buffered、50 万 rendezvous；Select 使用 300 万 ready 与 150 万
transfer；协程 Channel 使用 200 万 buffered、60 万 rendezvous 与 200 万 select。Queue
使用 4 个生产者各 500 万项，MPMC 同时使用 4 个消费者，batch 大小为 32。上述窗口让
受控路径持续运行约 0.2 秒以上，降低线程启动、短暂睿频和调度落点对中位数的影响。

SPSC 无锁队列吞吐仍由 `bench_queue_pointer.c` 输出，但当前 Windows 未固定线程拓扑时
呈现约 2500 万和 4200 万 items/s 的双峰分布，不能形成 10% 自动阈值，因此没有进入
上表。后续应建立拓扑感知的生产者与消费者固定策略，再把该指标提升为发布门禁。

## 网络口径

本机五个正式样本均记录实际后端事实 `network_backend=iocp`；比较时后端名称必须完全
一致，不能把 fallback 数据与 IOCP 基线混合。

TCP 指标使用当前公开同步 API，在一个双 Worker Engine 上完成 5000 次 64 字节完整
ping-pong。每次接收都处理 TCP 分片并校验内容，统计同时验证双端收发字节、拒绝数、
缓冲归零和 Engine 对象归零。

UDP 指标使用当前公开批量 API，在 64 包有界窗口内完成 50000 个 256 字节数据报。计时
包含发送受理、内核传输、批量拉取、所有权回收和发送队列 drain；样本要求包数与字节
完全一致，且无丢包、截断、IO 错误和接收队列残留。

HTTP 指标在同一 keep-alive 连接上分别完成 5000 次结构化 `Reply` 和预构建 raw 响应，
客户端逐次验证状态与 64 字节正文；统计要求只建立一个连接、精确复用 4999 次，并在
结束后清空连接池、服务端连接和 Engine 对象。

WebSocket 指标测量一次真实 HTTP Upgrade，并在升级后的连接上完成 5000 次 64 字节
二进制消息回环。每条消息都流式核对内容，最后验证双方正常关闭码、异步发送预算、
HTTP 客户端池、服务端升级统计和 Engine 对象全部归零。

TLS 指标使用真实 RSA 证书、X25519、AES-128-GCM 和 SHA-256 完成 100 次完整握手、
200 次连续票据恢复及 5000 次 64 字节加密流回环。每轮都验证客户端和服务端恢复状态、
恢复连接不重复暴露证书、票据轮换、负载内容、双向 `close_notify` 与引用清理。

这些 localhost 数据只证明当前 Windows IOCP 后端和本机协议栈开销，不能外推到
Linux io_uring/epoll、macOS kqueue 或真实网络。

## 复现

小规模 smoke：

```powershell
python tools\measure_performance.py --compiler gcc --arch x64 --smoke
```

生成完整报告：

```powershell
python tools\measure_performance.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --report out\performance\gnu\x64\performance-report.json
```

与固定基线比较：

```powershell
python tools\measure_performance.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --baseline dev\bench\performance\PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
```

比较要求平台、CPU、逻辑核、亲和范围、电源计划、编译器、目标架构、编译参数、运行
策略和 runner 指纹完全一致。不同环境必须建立独立基线。
