# TaskPool Bench

该基准测量任务从提交、工作线程调度、进入终态，到调用方等待并释放 `Future` 的
完整成本，不把线程池创建和销毁计入吞吐时间。

包含两条路径：

- `windowed`：队列上限和调用方 Future 窗口均为指定窗口，模拟持续有界吞吐。
- `queue_one`：队列上限固定为 `1`，持续使用 `xrtTaskSubmitWait`，覆盖容量等待和
  `Space` 条件变量唤醒路径。

每个任务执行一次原子计数。基准结束时会核对执行次数以及 `Submitted`、
`Completed`、`Succeeded`、`Rejected`、`Queued` 和 `Running` 统计，任一任务丢失、
重复执行、失败或拒绝都会使程序返回非零。

Windows GCC x64 构建与运行：

```powershell
$env:PATH = 'E:\software\w64devkit\bin;' + $env:PATH
gcc -std=c11 -O2 -Wall -Wextra -Werror `
	dev/bench/task/bench_task_pool.c `
	-o out/bench_task_pool.exe

for ( $run = 1; $run -le 3; $run++ ) {
	out/bench_task_pool.exe 200000 4 4096 50000
}
```

参数依次为常规任务数、工作线程数、Future 窗口和队列为一时的任务数。发布基线
至少连续串行运行三次并记录中位数，同时记录系统、CPU、编译器、优化级别和 CPU
固定策略。不同机器、线程数或电源策略下的绝对数字不能直接比较。

任务过程几乎没有业务负载，因此该结果主要反映任务对象、Future、调度锁、条件
变量和回收路径的固定成本。实际业务吞吐取决于任务粒度；极细粒度固定拓扑工作应
优先评估无锁队列或批处理，而不是为每个元素创建独立任务。
