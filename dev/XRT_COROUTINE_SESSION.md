# XRT Coroutine Session

## 1. 文档目的

本文件用于在关闭当前会话后，快速恢复协程重构任务的上下文。

它不是长期设计 spec 的替代品，而是面向“下一次继续开发”的交接文档。

主参考文档仍然是：

- [XRT_COROUTINE_REFACTOR_SPEC.md](/D:/git/xrt/dev/XRT_COROUTINE_REFACTOR_SPEC.md)
- [XNET_V2_SPEC.md](/D:/git/xrt/dev/XNET_V2_SPEC.md)


## 2. 当前总体状态

协程重构已经完成了内核级第一阶段，当前不再是原型状态，而是“核心 runtime 已成型，后续继续完善 backend、公共 API、xnet 集成和测试文档”的状态。

当前里程碑状态：

| 里程碑 | 状态 | 粗略进度 | 说明 |
|---|---|---:|---|
| M0 Spec freeze | done | 100% | 目标、边界、backend 策略已冻结 |
| M1 Runtime integration | done | 100% | 协程 runtime 已迁入线程态 |
| M2 Lifecycle hardening | done | 100% | destroy/cancel/join/thread affinity 已落地 |
| M3 Scheduler core | done | 100% | ready queue + timer heap + post queue 已落地 |
| M4 Backend rewrite/audit | in_progress | 80% | Windows x64 与 Linux x64 production backend 已跑通，非 x86 真机验证未完成 |
| M5 Public API compat layer | in_progress | 80% | 标准 runtime API 已成型，但尚未完全收口 |
| M6 xnet integration | done | 100% | future/stream/dgram/listener accept 等待链已接通，socket-level readiness 仍为可选后续项 |
| M7 Tests, docs, benchmarks | in_progress | 85% | 默认 modern 测试流、双端回归脚本、首份基线报告、公开 coroutine API 文档与 xnet wait bridge 说明已落地，剩余主要是更大规模基准 sweep |

一句话总结：

- 协程“能用”的部分已经很多
- 协程“可以宣称完成”的部分还不够
- 当前最缺的是：非 x86 真机 backend 验证，以及更大规模 benchmark sweep


## 3. 已完成任务

### 3.1 M0-M3：协程核心内核已经完成

已经完成的核心改造包括：

- 协程 runtime 状态已迁入 `xrtThreadData`，不再使用进程级静态主上下文和当前协程指针
- 协程创建、恢复、销毁、scheduler 入口都已接入线程归属约束
- destroy 非法状态、scheduler-owned destroy、cross-thread misuse 已明确拒绝
- `cancel/join/close` 第一版生命周期已经落地
- `join` 引入了 join pin / 延迟 reap，避免 scheduler 提前回收句柄
- scheduler 已不再依赖 legacy 的 `sleep(1ms)` 轮询
- `ready queue + timer heap + post queue` 已落地
- `xrtCoSchedCurrent()`、`xrtCoSchedPost()`、`xrtCoSchedPollOnce()` 已落地
- 非 Fiber backend 的栈已切到正式 stack allocator，并带 guard page

这一部分已经奠定了协程系统的主骨架，后续不会再回到旧的原型级模型。


### 3.2 M4：backend 重构和 ABI 审计已做了第一大段

已经完成的 backend 工作：

- backend 分层策略已冻结：production backend / compat backend
- 增加了 strict 模式相关约束：`XRT_CO_REQUIRE_PRODUCTION_BACKEND`
- 单头文件优先、inline asm 优先 的路线已冻结
- 当前 Windows x64 GCC/Clang 路径已提升为 production backend
- Win64 x64 backend 已补齐非易失 GPR / XMM / shadow space 要求
- Fiber compat 已支持 already-fiber / host fiber 语义
- ARM64 backend 已补齐 `q8-q15`
- RV64 backend 已补齐硬浮点 ABI 下的 `fs0-fs11`
- LA64 backend 已补齐硬浮点 ABI 下的 `fs0-fs7`
- 新增 backend introspection API，可查询 backend 的 `name/tier/style`
- 深栈跨 yield 保持回归已补

当前这部分的真实状态不是“全部完成”，而是“代码级审计已推进很远，但非 x86 的真机验证没有做完”。


### 3.3 M5：标准 runtime API 已经具备可用轮廓

已经新增并稳定下来的 API/能力：

- `xrtCoExit()`
- `xrtCoGetExitCode()`
- `xrtCoSleepUntil()`
- `xrtCoCreateEx()` 和轻量 `xco_create_args`
- `xrtCoWaitDeadline()`
- `xcoevent` 及其等待接口
- `xrtCoWaitEventTimeout()`
- `xrtCoWaitEventUntil()`
- `xrtCoPushCleanup()`
- `xrtCoPopCleanup()`
- `xrtCoSetResult()`
- `xrtCoGetResult()`

已经冻结的行为合同：

- `cancel` 仍是协作式，不做强制栈卸载
- `join` 当前只支持单等待者
- cleanup 回调不能 `yield/wait`
- `xcoevent` 第一版只支持单 waiter
- `result` 第一版先保持为“原始结果指针 + exit code”

这部分已经足够支撑后续更高层的 task/wait 语义，但公共 API 还没有到“对外文档完全收口”的阶段。


### 3.4 M6：xnet 集成已经接通了第一条正式主线

这一部分是本会话里推进最多的内容之一。

已经完成的 xnet 集成包括：

- `future -> coroutine wait`
- `future timeout / deadline -> coroutine wait`
- `task/post -> future -> coroutine wake`
- posted future 的 async-hold 生命周期保护
- `stream drain future`
- `stream close future`
- `stream readable future`
- `stream writable future`
- stream 直接协程等待 helper
- stream 直接协程 timed helper
- stream 的 generic wait-kind API
- sync-thread 与 coroutine 统一 wait-kind 语义
- `xnetwaitsrc` 统一等待源
- `xnetwaitsrc` 的带值等待
- `dgram recv` 进入统一等待源
- `dgram` 的 sync/coroutine/future/timeout/deadline 路径
- `dgram` 的误用合同回归：`OnRecv` 冲突、第二 waiter 冲突

当前 `xnetwaitsrc` 已覆盖的对象：

- `future`
- `stream wait-kind`
- `dgram recv`

当前已经冻结的重要合同：

- `stream readable` 第一版要求 paused-read
- `stream writable` 第一版表达的是 backpressure relief，而不是原始 socket-ready
- `dgram recv` 是 one-shot packet
- `dgram recv` 当前只支持单 waiter
- `dgram recv` 与 `OnRecv` 回调互斥
- `listener accept` 目前明确禁止做成 polling 伪 future / 伪 coroutine wait


## 4. 未完成任务

### 4.1 M4 未完成项：backend 仍缺真机验证

当前最重要的未完成项：

- ARM64 真机验证未完成
- RV64 真机验证未完成
- LA64 真机验证未完成
- Windows 以外主平台的 production backend 运行验证还未真正做完
- backend ABI 专项回归仍不完整
- `MSVC x64/ARM64` 原生 production backend 仍未进入正式支持范围

这部分已经记录到全局 TODO，设备到位后应优先做真机 bring-up 和 ABI smoke test。


### 4.2 M5 未完成项：公共 API 仍未完全收口

尚未完成的点：

- API 公开面还没有完整文档化
- 兼容层和标准层之间还没有做最终梳理
- structured concurrency 仍然停留在方向层，没有形成可交付 API
- `xco_entry` 是否升级签名仍是开放问题
- result/exit/user data 三者的最终模型仍未完全冻结

当前判断：

- M5 已经不是“缺功能”
- 而是“缺最终 API 收口和对外表达”


### 4.3 M6 未完成项：xnet 集成还差最后一段 runtime 主线

最核心的未完成项：

- `listener accept` 还没有进入统一等待源
- `listener` 目前仍然是 `accept()` + polling helper 模型
- `listener` 尚不具备：
  - owner-worker 归属
  - async-hold
  - accept waiter 槽位
  - accept 注册/撤销/完成的完整 port 生命周期
- 更低层的 `socket/port readiness` 还没有进入统一等待源
- `event loop` 正式接线还未完成

当前明确禁止的做法：

- 不能为了推进进度，把 `listener accept` 做成 polling 式伪 future
- 不能绕开 `xnet_port` 生命周期做一条质量较低的旁路

正确路线已经明确：

1. 先让 listener 拥有真实的 owner-worker / async-hold / accept waiter 模型  
2. 再把 accept 接成 future / coroutine wait / wait-source  
3. 最后再考虑更底层的 socket/port 级 readiness


### 4.4 M7 未完成项：测试、文档、benchmark 仍然明显不足

当前状态：

- 已经有很多专项回归
- modern 主测试流已经可复跑
- coroutine 公开 API 文档已经落地
- 基准测试已具备首份正式脚本与报告，但规模还不够大

未完成的具体事项：

- benchmark 还未完成更大规模 sweep：
  - context switch 成本的长时间趋势采样
  - coroutine create/destroy 成本的更大样本验证
  - timer 唤醒吞吐的更大规模压力结果
  - post queue 跨线程唤醒成本的优化前后对照
  - 与 thread-per-task 的正式对比


## 5. 当前关键合同

这些合同在恢复任务时不要打破：

- 协程 runtime 必须继续挂在 `xrtThreadData`
- 不能重新引入进程级静态协程状态
- `cancel` 继续保持协作式
- `join` 当前仍是单等待者模型
- cleanup 回调不得 `yield`
- `xcoevent` 当前仍是单 waiter
- `stream` timed wait 必须显式撤销 pending waiter 后才能返回 timeout/deadline
- `xnetwaitsrc` 当前是统一等待源的主线，不能重新分叉出多套分发逻辑
- `dgram recv` 当前是 one-shot packet、单 waiter、与 `OnRecv` 互斥
- `listener accept` 必须等待真实 port/worker 生命周期接入后再进入 wait-source


## 6. 当前验证基线

当前这条线最近一次稳定验证包括：

- `gcc -m64 -c xrt.c`
- `gcc -m64 -c test.c`
- `gcc -fsyntax-only dev/test_xnet2_stage.c`
- 协程专用 harness 已通过
- `xnet sync` 专用 harness 已通过

最近一次 `xnet sync` harness 已覆盖：

- future wait
- future timeout/deadline
- post future / delayed post future
- stream readable / writable / drain / close
- direct coroutine helper
- timed direct helper
- `xnetwaitsrc`
- value-returning wait-source
- `dgram recv`
- `dgram OnRecv` 冲突拒绝
- `dgram second waiter` 冲突拒绝
- `dgram sync/coroutine deadline`


## 7. 建议的恢复顺序

如果在新会话恢复任务，建议按这个顺序继续：

1. 先继续 M6  
   目标：把 `listener accept` 接入真实的 owner-worker / async-hold / accept waiter / port submit-harvest 生命周期  

2. 再继续 M4  
   目标：等设备到位后完成 ARM64 / RV64 / LA64 的真机 backend 验证  

3. 然后收 M7  
   目标：把专项回归纳入主测试流，补公开文档和 benchmark  

4. 最后再回头统一收 M5  
   目标：把标准 runtime API 和兼容层做最终文档化与对外收口


## 8. 建议下一会话先看的文件

恢复任务时建议优先打开这些文件：

- [coroutine.h](/D:/git/xrt/lib/coroutine.h)
- [xrt.h](/D:/git/xrt/xrt.h)
- [test_coroutine.h](/D:/git/xrt/test/test_coroutine.h)
- [xnet_sync.h](/D:/git/xrt/lib/xnet_sync.h)
- [xnet_stream.h](/D:/git/xrt/lib/xnet_stream.h)
- [xnet_dgram.h](/D:/git/xrt/lib/xnet_dgram.h)
- [test_xnet2_sync.h](/D:/git/xrt/test/test_xnet2_sync.h)
- [XRT_COROUTINE_REFACTOR_SPEC.md](/D:/git/xrt/dev/XRT_COROUTINE_REFACTOR_SPEC.md)
- [XNET_V2_SPEC.md](/D:/git/xrt/dev/XNET_V2_SPEC.md)
- [XRT_GLOBAL_TODO.md](/D:/git/xrt/XRT_GLOBAL_TODO.md)


## 9. 恢复任务时的第一条提示

如果下一会话直接继续做协程，请先检查两件事：

- 当前目标是不是继续 `listener accept` 的真实 wait-source 主线
- 当前是否已经具备 ARM64 / RV64 / LA64 的真机验证条件

如果没有新设备，优先继续 `listener accept` 的 runtime 主线，而不是继续扩更多未经收口的等待面。


## 10. Session Update (2026-03-14)

`listener accept` 这一条之前被明确 deferred 的 wait-source 主线，现在已经接通到了真实的 `owner-worker / async-hold / accept waiter / xnet_port submit-harvest` 生命周期上。

本次已完成：

- `lib/xnet_stream.h`
  - listener 现在具备 `pWorker`、`iAsyncHoldCount`、`tAcceptWait`、`iAcceptOpId`、`bAcceptArmed`
  - accept 的注册、撤销、完成分发现在走真实的 `xnet_port` watch 生命周期
- `lib/xnet_sync.h`
  - 新增 `XNET_WAITSRC_LISTENER`
  - 新增 `xrtNetListenerAcceptFuture()`
  - 新增 `xrtNetListenerAccept()/AcceptTimeout()/AcceptUntil()`
  - 新增 `xrtNetListenerAcceptCo()/AcceptCoTimeout()/AcceptCoUntil()`
  - 新增 `xrtNetWaitSourceListenerAccept()`
- `test/test_xnet2_sync.h`
  - listener future 回归
  - listener wait-source 回归
  - listener coroutine helper / deadline helper 回归
- `dev/test_xnet2_listener_accept_core.c`
  - 新增 focused harness，用来隔离验证 listener accept 的 future / wait-source / coroutine 三条路径

本次已验证：

- `gcc -fsyntax-only dev/test_xnet2_listener_accept_core.c -I .`
- `gcc dev/test_xnet2_listener_accept_core.c xrt.c -I . -o dev/test_xnet2_listener_accept_core_dbg2.exe -lws2_32 -liphlpapi`
- focused harness 运行通过
  - future path PASS
  - wait-source path PASS
  - coroutine path PASS
- `gcc -fsyntax-only dev/test_xnet2_stage.c -I .`
- `gcc dev/test_xnet2_stage.c xrt.c -I . -o dev/test_xnet2_stage_coroutine_listener.exe -lws2_32 -liphlpapi`
- `xnet2` stage harness 运行通过，说明 listener/coroutine 接线没有破坏现有网络栈主线

这意味着当前 coroutine backlog 的重点已经变化：

- `M6 xnet integration` 不再被 `listener accept` 阻塞
- coroutine 侧剩余重点主要变成：
  - `M4` 真机 backend 验证
  - `M7` 主测试流 / 文档 / benchmark 收口
  - 如确实需要，再考虑更低层的 `socket/port readiness` wait-source

推荐下一步：

1. 将 `listener accept` wait-source 视为已收口的可用基础设施。
2. 回到网络库剩余任务，继续推进性能 / benchmark / 其余协议层 backlog。
3. 后续任何修改 `xnet_sync.h` 或 listener 生命周期时，都优先重跑这个 focused harness。

## 11. Session Update (2026-03-14, Linux x64 backend)

这次在 Debian 13 真机上继续完成了 Linux x64 production backend 的 bring-up 与回归闭环，结论是：

- 之前在 Linux 上暴露出来的 `listener accept coroutine` 崩溃，根因不在 listener/xnet 集成，而在 `asm-x64-sysv` 协程后端本身
- 根因收敛到 `lib/coroutine.h` 里的 `__xrt_co_swap()`：旧实现使用自由寄存器约束，编译器可能把上下文指针分配到会被切换代码保存/覆盖的寄存器里，导致第一次切换就踩坏上下文
- 修复后改为固定使用 SysV caller-saved 的 `rdi/rsi` 传递 `pFrom/pTo`

这次新增/使用的验证入口：

- `dev/test_coroutine_min.c`
  - 最小协程调度器 smoke，用来确认崩溃是否与网络层无关
- `dev/test_coroutine_core.c`
  - 旧 `test/test_coroutine.h` 的独立可执行入口
- `dev/test_xnet2_listener_accept_core.c`
  - listener accept 的 focused harness
- `dev/test_xnet2_sync_core.c`
  - `xnet_sync` 的独立入口

这次在 Debian 13 上实际通过的验证：

- `dev/test_coroutine_min.c + xrt.c`
  - 修复前：ASan 下崩溃在 `__xrt_co_swap`
  - 修复后：退出码 `0`
- `dev/test_xnet2_listener_accept_core.c + xrt.c`
  - future / wait-source / coroutine 三条 listener accept 路径全部 PASS
- `dev/test_xnet2_sync_core.c + xrt.c`
  - listener accept、future、stream、dgram 的 sync/coroutine 等待链全部 PASS
- `dev/test_coroutine_core.c + xrt.c`
  - `test/test_coroutine.h` 现有 27 项协程回归在 Linux x64 `asm-x64-sysv` 下通过
- `dev/test_xnet2_stage.c + xrt.c`
  - Linux 原生 stage harness 再次完整通过

对路线图的影响：

- `M4` 现在可以把 “Windows / Linux 主流 x64 production backend 已验证” 视为已完成的阶段性结论
- `M6` 现在可以视为正式收口，不再被 listener accept 或 Linux 协程后端阻塞
- coroutine 剩余重点进一步收敛为：
  - `M4` 的 ARM64 / RV64 / LA64 真机验证
  - `M7` 的主测试流、公开文档、benchmark 收口

## 12. Session Update (2026-03-14, M7 benchmark scaffolding)

为了让协程库后续的优化和网络库性能工作有共同基线，这次补上了第一批 coroutine benchmark 入口：

- `dev/bench/coroutine/bench_context_switch.c`
  - 主线程与协程之间的 context switch 吞吐
- `dev/bench/coroutine/bench_create_destroy.c`
  - `create/destroy` 与 `create/resume/destroy` 生命周期成本
- `dev/bench/coroutine/bench_timer_churn.c`
  - scheduler timer insert/remove + immediate wake churn
- `dev/bench/coroutine/bench_sched_post.c`
  - 跨线程 `xrtCoSchedPost()` 唤醒 sleeping coroutine 的吞吐
- `dev/bench/coroutine/README.md`
  - build/run 说明与 smoke 命令
- `build_test_coroutine.sh`
  - 纯协程回归脚本
- `build_test_coroutine_stage.sh`
  - 协程 + xnet coroutine bridge 阶段回归脚本

这批 bench 的设计目标是“先有稳定入口，再扩正式基线”，所以当前结论只到 smoke：

- Windows x64 `asm-x64-win64`
  - `bench_context_switch.exe 10000`
  - `bench_create_destroy.exe 20000`
  - `bench_timer_churn.exe 5000`
  - `bench_sched_post.exe 5000`
- Debian 13 x64 `asm-x64-sysv`
  - `bench_context_switch_linux 10000`
  - `bench_create_destroy_linux 20000`
  - `bench_timer_churn_linux 5000`
  - `bench_sched_post_linux 5000`

已知边界：

- 这些是 smoke/trend 入口，还不是正式性能基线
- `bench_timer_churn` 已明确固定到与 coroutine runtime 相同的单调时钟基准，避免跨时钟错误
- `bench_sched_post` 现在衡量的是 scheduler-post wake 路径本身，而不是带粗粒度轮询延迟的测试夹具

## 13. Session Update (2026-03-14, coroutine test stage scripts)

为了让 `M7` 不再依赖零散的手工命令，这次把协程回归整理成了两条明确脚本：

- `build_test_coroutine.sh`
  - 构建并运行 `dev/test_coroutine_core.c`
- `build_test_coroutine_stage.sh`
  - 依次构建并运行：
    - `dev/test_coroutine_min.c`
    - `dev/test_coroutine_core.c`
    - `dev/test_xnet2_sync_core.c`
    - `dev/test_xnet2_listener_accept_core.c`

这意味着当前已经形成三层可复跑入口：

1. 最小 smoke
   - `dev/test_coroutine_min.c`
2. 纯协程主回归
   - `dev/test_coroutine_core.c`
3. 协程 + xnet bridge 阶段回归
   - `build_test_coroutine_stage.sh`

这次已验证：

- Windows 本地
  - `release/x64/xrt_test_coroutine.exe`
- Debian 13
  - `./build_test_coroutine.sh`
  - `./build_test_coroutine_stage.sh`

当前判断：

- 协程库已经不再缺“怎么复跑”的基础设施
- `M7` 接下来更像是“把这些入口进一步整理进默认文档和长期 benchmark 结果沉淀”，而不是继续补零散 harness

## 14. Session Update (2026-03-14, cross-platform modern test flow)

为了把协程验证真正纳入默认主测试流，这次继续把 modern 测试入口整理成跨平台可复跑形态：

- `build_test.sh`
  - 默认进入 modern flow
- `build_test_modern.sh`
  - 依次运行 coroutine stage 和 `xnet2` stage
- `build_test_coroutine.sh`
- `build_test_coroutine_stage.sh`
- `build_test_xnet2_stage.sh`
- `build_test_legacy.sh`
  - 保留 legacy `test.c` 入口，但不再作为 modern flow 的主路径

这批 shell 脚本现在会按平台自动选择基础链接参数：

- Windows Git Bash
  - `-lws2_32 -liphlpapi`
- Linux
  - `-pthread`

这次已验证：

- Windows 本地
  - `./build_test_modern.sh`
- Debian 13
  - `./build_test_modern.sh`

这意味着验收标准里“协程测试已纳入默认测试流”这一条，当前已经有了实际可运行的交付形态，而不再只是文档目标。

## 15. Session Update (2026-03-14, first scripted coroutine baseline)

为了让后续协程优化和网络库剩余 hot path 收尾有共同参照，这次把协程 benchmark 进一步整理成正式脚本与报告：

- `dev/bench/run_coroutine_bench_windows.ps1`
- `dev/bench/run_coroutine_bench_linux.sh`
- `dev/bench/COROUTINE_BENCH_20260314.md`

当前矩阵统一为：

- context switch: `100000`
- create/destroy: `50000`
- timer churn: `10000`
- scheduler post wake: `10000`

首轮结果的关键信号是：

- Windows x64 与 Linux x64 的 context switch / timer 成本已经进入同一数量级并可复跑
- Windows 当前在 `create/destroy` 和 `create/resume/destroy` 上领先
- Linux x64 当前最明显的 coroutine/runtime hot path 是 `xrtCoSchedPost()` 跨线程唤醒

这意味着 `M7` 已经不再停留在“bench scaffolding only”，而是有了第一份可复跑、可比较、可继续扩展的正式基线。

## 16. Session Update (2026-03-14, public coroutine API docs)

为了把 `M7` 从“只有内部 spec 和 harness”推进到“有公开交付面”，这次补上了正式的 coroutine API 文档：

- `docs/api-coroutine.md`
- `docs/api-coroutine.en.md`

并把它们接入了现有索引：

- `docs/README.md`
- `docs/README.en.md`
- `README.md`
- `README.en.md`

当前公开文档覆盖的重点是：

- coroutine 的产品定位与线程绑定合同
- `xcoro` / `xcosched` / `xcoevent` / `xco_create_args`
- create / resume / yield / cancel / close / join / exit
- scheduler / sleep / deadline / event wait
- result / user data / cleanup
- backend 自检接口

这意味着后续恢复任务时，不再需要先翻内部 spec 才能理解当前稳定的 coroutine 公共面。

## 17. Session Update (2026-03-14, coroutine + xnet public bridge notes)

为了把协程文档里的网络等待边界也补齐，这次继续把稳定的 `coroutine + xnet` 合同写进了：

- `docs/api-coroutine.md`
- `docs/api-coroutine.en.md`

当前公开说明已经覆盖：

- `xrtNetFutureWaitCo*`
- `xrtNetStreamWait*Co`
- `xrtNetDgramRecvCo*`
- `xrtNetListenerAcceptCo*`
- `xnetwaitsrc` 统一等待源

同时明确了几条最重要的合同：

- `readable` 是 paused-read buffered 路径
- `writable` 表达的是 backpressure relief
- `dgram recv` 是 one-shot 且与 `OnRecv` 互斥
- `listener accept` 基于真实 listener/worker 生命周期

这意味着协程侧已经不再缺少“如何和 xnet 一起使用”的公开说明。

## 18. Session Update (2026-03-14, sched-post hot path pass #1)

在协程和网络库共享的性能尾项里，这次先处理了 Linux 基线里最显眼的热点：

- `xrtCoSchedPost()` 每次跨线程唤醒都会分配并释放一个临时 post 节点

这次改成了 scheduler 内部复用 post node 的路径，避免高频 `malloc/free` 抖动：

- `lib/coroutine.h`
  - scheduler 新增 post freelist
  - `xrtCoSchedPost()` 优先复用 freelist node
  - `drain_posts()` 不再每次 `xrtFree`

实际验证：

- Windows 本地
  - `./build_test_coroutine_stage.sh` PASS
  - `run_coroutine_bench_windows.ps1` PASS
- Debian 13
  - `./build_test_coroutine_stage.sh` PASS
  - `run_coroutine_bench_linux.sh` PASS

这轮对 `bench_sched_post` 的影响是“有改善，但不是根治”：

- Windows
  - `760.260 ns/wakeup -> 687.510 ns/wakeup`
- Debian 13
  - `55343.819 ns/wakeup -> 53983.741 ns/wakeup`

当前判断：

- 这条优化是值得保留的低风险改进
- 但 Linux `sched_post` 的主要成本还不只在 heap churn，上层 condvar / wake / scheduling 路径仍需继续分析
