# XTP3 性能与体积记录

2026-11-17，Windows x64、GCC -O2，CPU `AMD64 Family 25 Model 33 Stepping 2, AuthenticAMD`。测量基于 `5e5cce76`，包含滚动结果确认与自动聚合。摘要、源码身份、原始报告及验证日志 SHA256 见 [最新收据](measurements/2026-11-17-windows.json)。[旧收据](measurements/2026-11-16-windows.json) 保留为历史，旧回执容量及体积不代表当前实现。

机器没有固定 CPU 亲和性和电源策略。各矩阵串行运行，每场景单独预热，保留三次重复；结果不代表互联网时延、跨机器基线或相对 HTTP 的固定倍数。原始文件在 out/xtp3-bench/，可按下列命令重建。

## 重建

```text
python extlibs/xtp/bench/measure_matrix.py --compiler gcc --count 5000 --repeats 3 --report out/xtp3-bench/final-matrix-windows.json
python extlibs/xtp/bench/measure_native.py --compiler gcc --count 2000 --repeats 3 --report out/xtp3-bench/final-native-windows.json
python extlibs/xtp/bench/measure_capacity.py --compiler gcc --count 500 --repeats 3 --report out/xtp3-bench/final-capacity-windows.json
python tools/measure_performance.py --compiler gcc --config extlibs/xtp/config/performance_profiles.json --report out/xtp3-bench/final-profiles-windows.json
python tools/measure_size.py --compiler gcc --config extlibs/xtp/config/size_profiles.json --profiles xtp_codec,xtp_session,xtp_delivery --kind static --report out/xtp3-bench/size-final-windows.json
```

Windows CPU 使用 GetProcessTimes，Linux 使用进程 clock。短样本有计时粒度限制，零不表示没有成本。本地只计循环 CPU；原生场景 CPU 包含连接和生命周期，xrudp NOTIFY 还包含接收排空等待。--quick 只用于冒烟。

## 本地与原生承载

本地296种配置、888个样本：四种 Command/Params 组合，16/64/256/1024/16384/65536 B Body，批量1/8/32，Codec、RPC/PUSH/NOTIFY及八种可恢复场景。全部提交目标完成，拒绝/超时均零。64 B、两项参数、每批八个RPC、每场景5000目标的三次中位数如下。延迟从提交开始包含排队；没有网络或加密。

| Command / Params | RPC/s | P99 μs |
| --- | ---: | ---: |
| ID / array | 294,017 | 45.4 |
| name / array | 308,926 | 37.7 |
| ID / table | 298,401 | 46.3 |
| name / table | 293,858 | 42.2 |

对应可恢复RPC为254314、258768、247281、262283 RPC/s，每次5000/5000完成、零拒绝。默认1024个完成记录使用滚动窗口，单元测试在同一保留时段连续完成12000次请求，原1024/60s约17次/秒的入场限制已移除。活跃请求不被淘汰；旧结果只明确退休，不能冒充成功消费。恢复仍增加32 B结果确认、缓存与预算，见 [Delivery](delivery.md)。

原生60种配置、180个样本：TCP/TLS1.3/WS × 六种Body × 在途1/8/32，另测每方向10000/200000/s固定到达率。每方向2000目标、一个worker；TLS使用测试pin/ChaCha20，WS从已升级TCP接入。64 B、每方向在途32的闭环中位数：

| 承载 | 双向合计RPC/s | 每次较慢端P99的中位数 μs |
| --- | ---: | ---: |
| TCP | 181,477 | 739.4 |
| TLS 1.3 / ChaCha20 | 154,295 | 602.0 |
| WS / 已升级TCP | 174,924 | 620.2 |

每方向10000/s时三种承载所有重复均4000/4000完成。200000/s时客户端有限窗口拒绝全部超额到达：TCP三次各拒绝3616，TLS为3558/3616/3616，WS为3616/3552/3616。生成器是1ms worker定时器，实际最大滞后约2ms，单独记录；不能据此推算理想均匀到达极限。

## xrudp 与多连接

xrudp有51种配置、153个样本，真实UDP + Noise IK/静态公钥pin/地址cookie，覆盖RPC/PUSH六种Body、NOTIFY的16/64/256 B、在途1/8/32，以及1%/20ms、10%/80ms损失/RTT。单执行者忙轮询，注入路径另有确定性抖动/重复。每方向500目标，64 B、在途32、无注入损失的中位数：

| 模型 | 双向合计消息/s | 完成含义 |
| --- | ---: | --- |
| RPC | 81,189 | 收到RESPONSE |
| 可靠PUSH | 169,866 | 收到接管ACK |
| NOTIFY | 97,069 | 本地DATAGRAM受理 |

以上无损代表场景每次合计1000目标均完成、实际接收1000。损失场景的NOTIFY接收数独立保留，不声称全部投递。500目标的P99.9只是有限次序统计量；大包受拥塞、分段和背压影响，全部原始结果保留。LATEST快照、独立资源流及MTU下降另见 [xrudp](xrudp.md)。

多连接27种配置、81个样本：同一引擎，1/8/32连接，1/2/4 worker，TCP/TLS/WS。每条连接配临时listener，两端同worker，独立连接跨worker并行，初始化后一起开始；每方向500个RPC。32连接、64 B的三次中位数：

| 承载 | 1 worker RPC/s | 2 worker RPC/s | 4 worker RPC/s |
| --- | ---: | ---: | ---: |
| TCP | 344,063 | 345,638 | 410,212 |
| TLS 1.3 | 145,389 | 292,590 | 293,146 |
| WS | 206,252 | 255,135 | 257,517 |

吞吐没有线性扩展；8连接WS的4 worker样本甚至低于2 worker。每条连接的完成数及尾延迟保留，不能用多个进程冒充共享引擎扩展。

## 复制、内存与持续压力

默认BatchBytes=16KiB，立即聚合同一可靠lane已排队帧，不等凑满；STREAM跨帧短写，RECORD整批受理，不可靠数据报不聚合。暂存缓冲惰性分配、计入配额，多一次复制；规范帧独立保存，OOM退回单帧。ID/array本地5000个普通RPC发送1250次、聚合复制1025000 B；可恢复RPC发送1875次、聚合复制1185000 B。宿主暂存和原生writer复制/封装、TLS加密另计。Batches/BatchedFrames/BatchCopiedBytes包含构建后未发送的快照；writer次数不等于系统调用或内核唤醒，未宣称全链路零复制。

Linux ASan/UBSan的8连接/4 worker持续TLS运行301秒、250轮、8000000次RPC，同一批连接/Session保持后正常退休，无sanitizer错误。Windows相同形态持续60秒、1371轮、43872000次RPC，31次OS内存采样和尾部区间保留在收据。OS内存包含整个进程，与XTP瞬时预留峰值不同。这些压力运行包含同时进行的验证/构建负载，不用作干净容量基线，不替代多日长稳。重建方法见 [部署资格](qualification.md)。

自有生产include/src C/h按UTF-8/LF共122016 B（119.16KiB），包括全部可选模块及公共/内部头。用户已取消源码硬上限，优先简洁、安全和性能；生成单头不重复算，依赖/测试/例程/文档另计。GNU size自有静态库text为：Codec8120 B、Codec+Reader+基础Session38360 B、包含Delivery44992 B；data/bss均零，不包含独立XRT/TLS/xws/xrudp或整个可执行文件。

## 质量门限和环境验收

标准profile（20000次RPC、三次重复）的P99触发质量门限：shape2的MAD为27.3%、中心范围104.6%。另做一次七次重复检查样本不足，shape0/1/2的P99中心范围仍为33.6%/31.5%/65.1%，超过30%门限。两个失败报告均保留，没有放宽阈值或反复重跑直至通过；当前P99不能作为稳定回归基线。

WSS真实TLS1.3/HTTP Upgrade、Windows x86、大端QEMU、包及宿主ABI已验证，特定优化编译告警记录于qualification.md。实体移动设备、运营商/NAT切换、休眠唤醒、多日长稳及全部复制/内核唤醒的动态归因仍需部署环境，当前没有这些资格证据。
