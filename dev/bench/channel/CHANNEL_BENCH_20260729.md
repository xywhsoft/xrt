# XRT 2 Channel 基线（2026-07-29）

本文记录重构版 Channel 在 Windows 上的首个内存与吞吐量基线。


## 环境

- 系统：Windows 10 Pro 10.0.19045 x64
- CPU：AMD Ryzen 5 5600，6 核 / 12 逻辑处理器
- 编译器：GCC 16.1.0
- 编译参数：C11、`-O2`、`-Wall -Wextra -Werror`
- 调度策略：进程未固定 CPU，三次测试串行执行


## 测试矩阵

- 同线程：容量 `1`，`1,000,000` 次 `TrySend` / `TryRecv`
- 有缓冲传输：容量 `4096`，单生产者、单消费者，`500,000` 项
- rendezvous：容量 `0`，单生产者、单消费者，`100,000` 项
- 协程有缓冲传输：容量 `64`，同一调度器内两个协程，`500,000` 项
- 协程 rendezvous：容量 `0`，同一调度器内两个协程，`200,000` 项
- 协程 Select：两个容量 `64` 的 Channel，`500,000` 项


## 内存

| 项目 | 字节 |
|---|---:|
| Windows x64 `sizeof(xchannel)` | `192` |
| 容量 `4096` 的消息环 | `32768` |
| 容量 `64` 的消息环 | `512` |
| 容量 `0` 的消息区 | `0` |
| Windows x64 单个 Select 等待节点 | `56` |
| 8 case 栈内 Select 节点 | `448` |

Channel 没有每对象固定 8K 消息缓冲。有缓冲模式只按精确容量分配
`capacity * sizeof(ptr)`；rendezvous 不分配消息区。`xchannel` 的 192 字节是同步
状态和后续 select 适配预留，不随消息容量增长。


## 结果

| 路径 | 样本（项/秒） | 中位数（项/秒） |
|---|---:|---:|
| 同线程成对 try | `31,608,060.055`; `30,829,942.040`; `30,233,584.675` | `30,829,942.040` |
| 容量 4096 跨线程传输 | `19,903,349.336`; `18,752,648.812`; `19,620,846.757` | `19,620,846.757` |
| 容量 0 rendezvous | `1,865,549.824`; `1,803,208.629`; `1,951,482.248` | `1,865,549.824` |
| 两个就绪 case 的 `SelectTry` | `10,134,412.716`; `10,768,934.209`; `10,230,859.341` | `10,230,859.341` |
| 双 Channel 持续 Select 传输 | `4,979,296.087`; `4,783,274.611`; `4,891,913.178` | `4,891,913.178` |
| 容量 64 的协程传输 | `6,896,913.218`; `6,891,941.253`; `6,915,246.736` | `6,896,913.218` |
| 容量 0 的协程 rendezvous | `2,069,093.230`; `2,038,214.483`; `2,081,115.644` | `2,069,093.230` |
| 双 Channel 协程 `SelectAwait` | `6,598,726.182`; `6,518,462.894`; `6,537,716.742` | `6,537,716.742` |


## 解释

- 有缓冲路径在保留阻塞、deadline、关闭和 MPMC 契约的前提下，单生产者与单消费
  者传输达到约 `19.6M` 项/秒。
- rendezvous 每项都要求发送与接收线程交接，吞吐量明显低于有缓冲路径，符合其
  同步语义定位。
- 两个 case 的 `SelectTry` 包含交叉别名校验、轮转、公平扫描和一次原子选择。
  持续 Select 传输还包含生产线程、两个消息环和必要的阻塞注册。
- 最多 8 个 case 的阻塞 Select 使用栈内节点；测试矩阵中的常见双 case 路径不为
  等待节点分配堆内存。
- 协程路径直接复用 Channel 注册和原子提交核心，不创建 Future、Promise 或原生
  Event。资源通知令牌只在快速尝试失败后开启；单 case 路径不获取全局公平票据。
  同一调度器内达到约 `6.90M` 项/秒，双 Channel `SelectAwait` 达到约
  `6.54M` 项/秒。
- 对只需要极限吞吐、不需要阻塞契约的固定拓扑，应继续使用 `queue.h` 的无锁
  SPSC、MPSC 或 MPMC 队列。
- 当前数字是本机未固定 CPU 的回归基线，不是跨平台性能承诺。


## 复现

```powershell
$env:PATH = 'E:\software\w64devkit\bin;' + $env:PATH
gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel.c `
	-o out/bench_channel.exe
gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel_select.c `
	-o out/bench_channel_select.exe
gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel_coroutine.c `
	-o out/bench_channel_coroutine.exe

for ( $run = 1; $run -le 3; $run++ ) {
	out/bench_channel.exe 1000000 500000 100000
	out/bench_channel_select.exe 1000000 500000
	out/bench_channel_coroutine.exe 500000 200000 500000
}
```
