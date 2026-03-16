# XRT Global TODO

状态: Active
最后更新: 2026-03-16

## 说明

本文件用于记录跨模块、跨阶段、需要长期跟踪的全局任务。

记录原则：

- 只记录真正影响 roadmap 的长期事项
- 明确区分 `blocked`、`deferred`、`ready`
- 对需要额外设备、平台或外部条件的事项，写清阻塞原因和解锁条件

## Global Backlog

### 1. Coroutine production backend hardware validation

状态: blocked

范围:

- ARM64 (`AAPCS64` / Darwin)
- RV64
- LA64

当前状态:

- 协程 backend 已完成设计与部分代码级收口
- 当前开发环境仅能对现有 Windows x64 / GCC 路径进行真实编译与运行验证
- ARM64 / RV64 / LA64 仍缺少真实设备验证，暂不能将这些目标声明为已验证 production backend

阻塞原因:

- 缺少对应平台的真实调试设备
- 缺少对应编译器/运行时环境下的 ABI 回归执行条件

解锁条件:

- 采购并接入对应平台测试设备
- 能在目标平台上执行最小 coroutine runtime harness
- 能执行浮点/向量寄存器保持、深栈、yield/resume、scheduler、sleep 等 ABI 回归

验收要求:

- ARM64: 验证 `x19-x30/sp` 与 `q8-q15`
- RV64: 验证 `s0-s11/sp/ra`，并补充浮点相关专项测试
- LA64: 验证 `fp/s0-s8/sp/ra`，并补充浮点相关专项测试

在无设备期间允许继续进行的工作:

- 补齐 spec、测试草案、harness、注释、平台矩阵
- 做当前主平台上的编译回归，防止未验证 backend 拖坏现有路径
- 审计 ABI 保存集合与初始栈约束

在无设备期间不建议继续投入过深的工作:

- 反复微调未验证平台的 inline asm 细节
- 宣称这些目标已经达到 production-ready
- 围绕未落地设备投入大量性能优化时间

建议处理策略:

- 先记录为全局阻塞项
- 当前主线继续推进其他模块重构
- 等设备到位后，回到本项执行集中验证

### 2. Listener accept wait-source lifecycle hardening

状态: done

Resolved state:

- listener native accept submissions now hold listener lifetime until the backend completion path releases the accept-op hold
- listener destroy now defers final free while an outstanding accept completion can still surface listener user data
- listener future and wait-source cancel races now abandon and destroy undelivered accepted streams instead of only closing them
- focused listener and stage sync regressions now cover registered-waiter destroy rejection and timeout-destroy with an outstanding accept op

范围:

- `xnet listener` accept watch 生命周期
- `listener future / wait-source` 取消竞态
- listener 相关 teardown 回归

当前状态:

- listener accept 已接入 `future / sync wait / coroutine wait / wait-source`
- 当前实现能通过现有功能回归，但代码审查确认仍有两个高风险生命周期缺口

问题 A:

- `__xnetListenerArmAcceptWatch()` 提交原生 accept op 后，没有让该 op 自身持有 listener 生命周期
- `xrtNetListenerDestroy()` 只检查 `iAsyncHoldCount`
- accept 完成事件稍后仍会通过 port event 的 `pUserData` 回到 `__xnetStreamOnPortEvents()`
- 在 `stop/destroy`、timeout cancel、server shutdown 等路径下，存在“底层 accept op 尚未完成，但 listener 已被 free”的 use-after-free 风险

问题 B:

- listener future 在 cancel 竞态下，如果 accept 已经产出 `xnetstream`
- 当前路径只会对该 stream 调 `xrtNetStreamClose()`
- 但不会调用 `xrtNetStreamDestroy()`
- 对于没有外部 owner 的 public listener wait 场景，这会留下不可达 accepted stream，对象泄露

建议处理方向:

- 为“已 armed 的 accept watch / 已提交的原生 accept op”引入独立生命周期 pin，而不是只依赖 future/waiter 自身的 hold
- 明确 listener accept 的 backend cancel/remove 协议，或保证 destroy 前等待所有 outstanding accept completion 回收
- 在 cancel-race 路径下，为未交付给外部 owner 的 accepted stream 增加明确的 destroy/cleanup 语义

建议补充回归:

- `listener timeout/deadline` 后立即 `destroy`，且期间没有新连接到来
- `listener cancel` 与 `accept completion` 竞争时，验证不会 UAF
- `listener cancel` 与 `accept completion` 竞争时，验证不会泄露 accepted stream
- `httpd/ws` server shutdown 时，验证 accept watch teardown 不依赖悬空 `pUserData`

解锁条件:

- listener destroy 不会在 outstanding accept op 仍持有旧 `pUserData` 时释放对象
- cancel-race 下不会遗留未 destroy 的 accepted stream
- 上述专项回归在当前主平台稳定通过
