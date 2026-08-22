# XRT Coroutine Benchmarks

本目录保存当前协程契约的开发基准。四个入口均直接选择最小单头模块，并由
`config/performance_profiles.json` 的 `coroutine` 组合统一编译、采样和校验。

## Benchmarks

- `bench_context_switch.c`
  - measures main<->coroutine context-switch throughput on the current backend
- `bench_create_destroy.c`
  - measures `create/destroy` and `create/resume/destroy` lifecycle cost
- `bench_timer_churn.c`
  - measures scheduler timer insert/remove + immediate wake churn on the same
    monotonic clock base used by the coroutine runtime
- `bench_sched_post.c`
  - measures cross-thread `xrtCoSchedPost()` enqueue, wake and callback throughput

## 统一运行

```text
python tools/measure_performance.py --profiles coroutine --smoke
python tools/measure_performance.py --profiles coroutine --check --baseline <同机基线.json>
```

## Notes

- `bench_timer_churn.c` 使用 `xrtDeadlineAfter(0)` 覆盖即时定时器插入、摘除和恢复。
- `bench_sched_post.c` 测量 scheduler post，不代表 Channel 或 Future 的消息吞吐。
- Before treating any result as a baseline, rerun with larger iteration counts and
  pin down CPU/power-management noise on the target machine.

## 历史脚本

- `dev/bench/run_coroutine_bench_windows.ps1`
  - builds and runs the curated Windows baseline matrix
- `dev/bench/run_coroutine_bench_linux.sh`
  - builds and runs the curated Linux baseline matrix
- `dev/bench/COROUTINE_BENCH_20260314.md` 只保留旧版历史数据，不得用于当前 API 发布结论。
