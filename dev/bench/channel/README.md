# Channel Bench

该基准覆盖 XRT 2 Channel 的三类基础成本：

- 容量为 `1` 的同线程 `TrySend` / `TryRecv` 配对；
- 容量为 `4096` 的单生产者、单消费者跨线程阻塞传输；
- 容量为 `0` 的单生产者、单消费者 rendezvous 传输。
- 两个始终就绪 case 的 `SelectTry`；
- 一个生产者在两个 Channel 间分发时的持续 Select。
- 同一调度器内两个协程的有缓冲与 rendezvous 传输；
- 一个生产协程与双 Channel `SelectAwait` 消费协程的持续传输。

它同时输出 `sizeof(xchannel)`、有缓冲消息环字节数和 rendezvous 消息区字节数。
Channel 是通用阻塞 MPMC 通信原语，不替代 `queue.h` 的无锁高吞吐通道。

Windows GCC x64 构建与运行：

```powershell
$env:PATH = 'E:\software\w64devkit\bin;' + $env:PATH
gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel.c `
	-o out/bench_channel.exe
out/bench_channel.exe 1000000 500000 100000

gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel_select.c `
	-o out/bench_channel_select.exe
out/bench_channel_select.exe 1000000 500000

gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/channel/bench_channel_coroutine.c `
	-o out/bench_channel_coroutine.exe
out/bench_channel_coroutine.exe 500000 200000 500000
```

发布基线至少连续串行运行三次，记录中位数、系统、CPU、编译器、优化等级和是否
固定 CPU。不同机器或调度策略的数字不能直接比较。
