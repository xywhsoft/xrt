# XRT Windows GCC 16 x64 性能基线

日期：2026-08-14

本文件是 `PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json` 的人类可读摘要。JSON 保存
16 个 profile、66 个受控指标的全部原始样本、统计量、方向和阈值，是自动比较的唯一
权威输入。


## 环境

| 项目 | 值 |
| --- | --- |
| 报告 schema | 2 |
| 平台 | Windows 10 x64 |
| CPU | AMD Ryzen 5 5600，6 核 12 线程 |
| 进程亲和 | `0xfff/0xfff` |
| 电源计划 | `381b4222-f694-41f0-9685-ff5bb260df2e` |
| 编译器 | GCC 16.1.0，C11，`-O2 -Wall -Wextra -Werror -m64` |
| 运行策略 | 串行，1 次预热，5 次独立进程样本，中位数 |
| 源码提交 | `51d3492b3868440acb46595bb9ef2682bd30bd54` |
| 工作树 | 有未提交修改 |
| 源码指纹 | `a05aa83096446dd67905944a87d0daae01877de8b3eac044d760935453e44610` |
| 基准指纹 | `dd07a541f7c6093719d537b43ca3571f780f947bb5d2eaf7820ba9cc45f8b6d5` |

这是重构工作树的开发基线，不是最终发布提交证明。正式发布候选必须在干净工作树上
重新采样。


## Web 协议核心

| 指标 | 中位数 | MAD | 中央范围 |
| --- | ---: | ---: | ---: |
| HTTP/1 完整消息解析 | 1,524,749 ops/s | 4.1% | 4.8% |
| HTTP/1 Header 写出 | 10,039,808 ops/s | 0.3% | 0.4% |
| HTTP gzip 流式解码 | 14.274 MiB/s | 0.3% | 0.4% |
| WebSocket 帧头解析 | 62,288,608 ops/s | 0.1% | 0.7% |
| WebSocket 帧头写出 | 57,873,050 ops/s | 0.3% | 0.8% |
| WebSocket 原地 mask | 17,381 MiB/s | 0.4% | 0.7% |

HTTP 基准不创建客户端、服务器、路由或拥有型报文；它只测借用式 HTTP/1 消息解析、
调用方缓冲写出和可复用内容解码器。WebSocket 基准只测帧协议核心。旧应用级回环基准及
原始基线保存在 `extlibs/xhttp/archive` 与 `extlibs/xws/archive`，不参与 XRT 核心门禁。


## 网络基线

| 指标 | 中位数 | MAD | 中央范围 |
| --- | ---: | ---: | ---: |
| TCP 同步回环 | 39,534 round-trips/s | 0.6% | 0.9% |
| TCP p50 | 24.6 us | 0.0% | 1.6% |
| TCP p99 | 42.4 us | 1.9% | 8.3% |
| UDP 批量回环 | 222,309 packets/s | 0.6% | 1.3% |
| TCP Ref 大流 | 2,249.7 MiB/s | 3.5% | 3.6% |
| TCP Ref 提交 | 143,980 sends/s | 3.5% | 3.6% |
| 空闲连接建立 | 9,062 connections/s | 0.6% | 0.7% |
| 空闲 Stream 固定正文缓冲 | 0 bytes | 0.0% | 0.0% |

TCP Ref 大流使用单 Worker 隔离发送队列、背压和引用所有权成本，正式样本传输 1 GiB。
多 Worker 扩展性由独立连接与压力场景衡量，不混入这个稳态吞吐口径。全部网络样本记录
实际后端 `iocp`；不能外推到 epoll、kqueue、io_uring 或 Select。


## 复现

```powershell
python tools\measure_performance.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --smoke
python tools\measure_performance.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --baseline dev\bench\performance\PERFORMANCE_BASELINE_WINDOWS_GCC16_X64.json --check
```

正式报告默认拒绝 MAD 超过 20% 或中央范围超过 30% 的指标。一次系统抢占会保留在完整
范围中，但不会污染中央范围；双峰或持续噪声必须修复基准口径，不能通过覆盖基线放行。


<!-- performance-machine-audit-start -->
## 机器审计明细

`network_backend=iocp`

以下中位数与 JSON 基线逐项一致，符号名保持稳定以供自动审计。

### `task`

- `windowed_tasks_per_sec`: 1182221 tasks/s
- `queue_one_tasks_per_sec`: 629933 tasks/s
- `executor_tasks_per_sec`: 5631366 tasks/s

### `coroutine`

- `coroutine_switches_per_sec`: 19382912 switches/s
- `coroutine_ns_per_switch`: 52 ns
- `coroutine_create_destroy_ops_per_sec`: 144458 coroutines/s
- `coroutine_run_destroy_ops_per_sec`: 121444 coroutines/s
- `coroutine_posts_per_sec`: 5093885 posts/s
- `coroutine_timers_per_sec`: 13263585 timers/s

### `channel`

- `local_items_per_sec`: 31588949 items/s
- `buffered_items_per_sec`: 20030270 items/s
- `rendezvous_items_per_sec`: 1848766 items/s
- `select_ready_items_per_sec`: 13249134 items/s
- `select_transfer_items_per_sec`: 5515831 items/s
- `buffered_co_items_per_sec`: 6337103 items/s
- `rendezvous_co_items_per_sec`: 1970627 items/s
- `select_co_items_per_sec`: 6362473 items/s

### `queue`

- `mpsc_items_per_sec`: 9664029 items/s
- `mpsc_batch_items_per_sec`: 108504354 items/s
- `mpmc_items_per_sec`: 7267685 items/s
- `mpmc_batch_items_per_sec`: 120017114 items/s

### `runtime`

- `runtime_type_ops_per_sec`: 222624926 operations/s
- `runtime_object_ref_pairs_per_sec`: 116458633 ref-pairs/s
- `runtime_calls_per_sec`: 9067334 calls/s

### `template`

- `template_compile_ops_per_sec`: 803749 templates/s
- `template_write_ops_per_sec`: 144942 renders/s
- `template_render_ops_per_sec`: 130307 renders/s

### `data`

- `parse_mib_per_sec`: 67 MiB/s
- `visit_mib_per_sec`: 185 MiB/s
- `write_ops_per_sec`: 337446 documents/s

### `identifier`

- `make_ops_per_sec`: 11140261 ids/s
- `batch_ops_per_sec`: 27046032 ids/s
- `write_ops_per_sec`: 8901272 ids/s
- `parse_ops_per_sec`: 3655358 ids/s

### `network`

- `tcp_round_trips_per_sec`: 39534 round-trips/s
- `tcp_latency_p50_us`: 24.6 us
- `tcp_latency_p99_us`: 42.4 us
- `udp_packets_per_sec`: 222309 packets/s
- `tcp_ref_mib_per_sec`: 2250 MiB/s
- `tcp_ref_sends_per_sec`: 143980 sends/s
- `tcp_idle_connects_per_sec`: 9062 connections/s
- `tcp_idle_buffer_bytes_per_stream`: 0 bytes

### `http`

- `http_parse_ops_per_sec`: 1524749 ops/s
- `http_write_ops_per_sec`: 10039808 ops/s
- `http_gzip_decode_mib_per_sec`: 14 MiB/s

### `websocket`

- `ws_frame_parse_ops_per_sec`: 62288608 ops/s
- `ws_frame_write_ops_per_sec`: 57873050 ops/s
- `ws_mask_mib_per_sec`: 17381 MiB/s

### `tls`

- `tls_full_handshakes_per_sec`: 136 handshakes/s
- `tls_full_handshake_latency_p50_us`: 7210.4 us
- `tls_full_handshake_latency_p99_us`: 8388.1 us
- `tls_resume_handshakes_per_sec`: 904 handshakes/s
- `tls_resume_handshake_latency_p50_us`: 1085.7 us
- `tls_resume_handshake_latency_p99_us`: 1492.7 us
- `tls_stream_round_trips_per_sec`: 56305 round-trips/s
- `tls_stream_latency_p50_us`: 16.8 us
- `tls_stream_latency_p99_us`: 32.5 us

### `logging`

- `logger_ring_records_per_sec`: 1525342 records/s

### `file_io`

- `file_async_write_mib_per_sec`: 2827 MiB/s
- `file_async_read_mib_per_sec`: 2043 MiB/s

### `crypto`

- `aes_gcm_encrypt_mib_per_sec`: 419 MiB/s
- `sha256_mib_per_sec`: 275 MiB/s
- `chacha20_poly1305_encrypt_mib_per_sec`: 369 MiB/s
- `x25519_shared_ops_per_sec`: 4448 agreements/s

### `x509`

- `x509_parse_ops_per_sec`: 441058 certificates/s
- `x509_rsa_verify_ops_per_sec`: 1599 verifications/s
<!-- performance-machine-audit-end -->
