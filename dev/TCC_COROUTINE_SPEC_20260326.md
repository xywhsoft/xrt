# XRT TCC Coroutine Recovery Spec

状态: Draft

日期: 2026-03-26

负责人: Codex


## 1. 目标

恢复 `TCC` 下的主线协程能力，并最终做到以下目标：

- Windows `x86_64` / `x86`
- Linux `x86_64` / `x86`
- 能编译并运行主线 coroutine runtime
- 能跑通当前 coroutine 相关测试
- 能跑通当前 foundation benchmark runner


## 2. 非目标

本阶段不做以下事情：

- 恢复旧 `fiber / ucontext / setjmp` archive fallback
- 改变当前主线 coroutine API
- 把 `TCC` runner 结果直接定义为正式性能基线


## 3. 当前现状

### 3.1 主线策略

当前主线只保留 production backend，不再自动退回 archive fallback。

现状入口：

- `lib/coroutine.h`
- `dev/archive/coroutine/README.md`

### 3.2 当前阻塞

当前 `TCC` 构建脚本全部通过 `-DXRT_NO_COROUTINE` 关闭协程。

直接原因有两类：

1. `lib/coroutine.h` 的 backend 选择逻辑排除了 `__TINYC__`
2. `TCC` 的 x86_64 GNU/AT&T inline asm 只兼容 `GCC` 的一部分子集

当前已确认的 `TCC x64` 限制：

- 支持 `__asm__ volatile`
- 支持局部数字标签 `1f`
- 支持 `leaq ...(%rip)`
- 支持 `movq`
- 不支持 `jmpq`
- 不支持 `movdqu`
- 不识别 `xmm8 ~ xmm15` 寄存器名
- 支持 `.byte`


## 4. 技术路线

### 4.1 总体原则

- 不恢复旧 fallback
- 继续使用当前 coroutine runtime
- 为 `TCC` 增加同级 production backend
- 先打通 `x86_64`，再扩到 `x86`

### 4.2 backend 策略

新增或恢复以下 target family：

- `asm-x64-win64-tcc`
- `asm-x64-sysv-tcc`
- `asm-x86-win32-tcc`
- `asm-x86-sysv-tcc`

其中：

- `x64 win64` 复用现有 Win64 保存集合语义
- `x64 sysv` 复用现有 SysV 保存集合语义
- `x86 win32` / `x86 sysv` 新增 32 位 context switch backend

### 4.3 Win64 TCC 特殊处理

Win64 backend 需要保存：

- `rbx`
- `rbp`
- `rdi`
- `rsi`
- `r12 ~ r15`
- `xmm6 ~ xmm15`

对 `TCC` 的处理策略：

- `xmm6 / xmm7` 使用 `movups`
- `xmm8 ~ xmm15` 使用 `.byte` 手工编码
- 间接跳转统一使用 `jmp`，不使用 `jmpq`

### 4.4 Linux TCC 特殊处理

Linux `x64` backend 不需要 Win64 那组高位 XMM 保存，优先直接复用当前 SysV backend。

Linux `TCC` 额外风险点不是协程本身，而是：

- `__sync_*` 原子族
- `xnet`
- `queue`
- `nettls`

因此必须补统一的 `TCC x86/x64` 原子兼容层。


## 5. 分阶段实施

### 阶段 A

目标:

- `Windows x64 + TCC`
- 打通 coroutine backend
- 移除 `x64` 构建脚本中的 `-DXRT_NO_COROUTINE`
- 跑通最小 smoke 和 coroutine bench

交付:

- `lib/coroutine.h` 支持 `asm-x64-win64-tcc`
- 恢复 `build_TCC_*_x64.bat` 协程编译
- 增加 `TCC` coroutine benchmark runner

### 阶段 B

目标:

- `Linux x64 + TCC`
- 打通 `asm-x64-sysv-tcc`
- 补 Linux `TCC` 原子兼容层
- 跑通 coroutine / queue / xnet compare runner

交付:

- `lib/coroutine.h`
- `xrt.h`
- `lib/xnet_base.h`
- `lib/nettls.h`
- Linux TCC runner

### 阶段 C

目标:

- `Windows x86 + TCC`
- 实现 `asm-x86-win32-tcc`
- 补 32 位栈初始化与 ABI 回归

### 阶段 D

目标:

- `Linux x86 + TCC`
- 实现 `asm-x86-sysv-tcc`
- 与 Linux 原子兼容层一起完成 benchmark runner 回归


## 6. 验收标准

### 6.1 功能验收

至少需要通过：

- `test_coroutine.h`
- `test_coroutine_min.h`
- `future_core`
- `xnet2_sync`

### 6.2 Benchmark 验收

至少需要跑通：

- `dev/bench/run_coroutine_bench_windows.ps1` 的 TCC 对应 runner
- `dev/bench/run_coroutine_bench_linux.sh` 的 TCC 对应 runner
- `dev/bench/run_queue_*`
- `dev/bench/run_xnet_compare_*`
- TLS handshake / resume bench 入口

### 6.3 ABI 验收

必须补专项验证：

- Win64 `xmm6 ~ xmm15`
- `yield / resume`
- 深栈
- scheduler
- sleep / deadline
- cross-thread `xrtCoSchedPost`


## 7. 风险

### 7.1 Win64 TCC inline asm 风险

高位 XMM 只能通过 `.byte` 编码，会增加维护成本与出错概率。

### 7.2 Linux TCC 原子风险

如果不先完成 `__sync_*` 替代层，即便协程 backend 正常，queue / xnet / TLS benchmark 仍可能失败。

### 7.3 x86_32 ABI 风险

32 位下更容易出现以下问题：

- 栈对齐错误
- 返回点布局错误
- `cdecl` / SysV 细节差异
- 64 位原子在 `i386` 路径上的实现复杂度偏高


## 8. 当前执行决策

当前先执行阶段 A：

- 先恢复 `Windows x64 + TCC`
- 优先把 coroutine backend 编译链和最小运行链打通
- 同时为后续 Linux `TCC` 原子兼容层保留设计空间
