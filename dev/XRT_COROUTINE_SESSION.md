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
| M4 Backend rewrite/audit | in_progress | 70% | 当前主平台已跑通，非 x86 真机验证未完成 |
| M5 Public API compat layer | in_progress | 80% | 标准 runtime API 已成型，但尚未完全收口 |
| M6 xnet integration | in_progress | 75% | future/stream/dgram 等待链已接通，accept/socket-level 仍未完成 |
| M7 Tests, docs, benchmarks | todo | 20% | 仅有专项测试和 spec，公开文档/基准/默认主测试流未完成 |

一句话总结：

- 协程“能用”的部分已经很多
- 协程“可以宣称完成”的部分还不够
- 当前最缺的是：真机 backend 验证、最后一段 xnet 等待源、主测试流/文档/benchmark


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
- 但仍主要依赖专用 harness
- 主测试流、公开文档、基准测试没有完成收口

未完成的具体事项：

- 协程测试尚未作为“已完成交付”纳入默认主测试流
- 协程重构的公开文档尚未补齐
- 协程与 xnet 集成的公开说明尚未补齐
- benchmark 尚未完成：
  - context switch 成本
  - coroutine create/destroy 成本
  - timer 唤醒吞吐
  - post queue 跨线程唤醒成本
  - 与 thread-per-task 的对比


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
