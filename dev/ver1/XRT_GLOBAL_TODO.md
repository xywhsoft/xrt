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

## Global Backlog

### 2. 基础运行时（批次 A）关键缺陷

状态: ready

来源: [MODULE_ASSESSMENT.md](dev/MODULE_ASSESSMENT.md) 批次 A 评估

这些缺陷不依赖外部条件，可立即动手；但部分项会牵动其他批次，需提前对齐：

- **time.h 时区转换 bug**: `xrtUTCToLocal/LocalToUTC` 复用"当前偏移"转换任意时间戳，对 DST 区域的历史/未来时间戳结果错误。影响 HTTP Date、TLS 证书有效期、日志时间戳等所有跨时区场景。修复时需联合 xhttp/xnettls 回归。
- **charset.h UTF-8 校验不严**: `xrtUTF8to16/xrtUTF8to32/xrtIsUTF8` 不拒绝 overlong encoding、代理区码点、超出 U+10FFFF 的码点。是 UTF-8 smuggling 安全风险。修复后会让原本"宽容接受"的输入变成拒绝，属于 breaking 行为变更，需在版本说明中标注。影响 json.h、xhttp_util.h。
- **type.h callable refcount 非原子**: 跨线程 future 回调持有 callable 时存在数据竞争。改为原子操作是纯内部改动，不影响 API。
- **xid.h `xrtMakeXID` 大小疑似不匹配**: `xrtMalloc(24)` 但 `xid` 结构是 4 个字段，若每字段 8 字节则需 32 字节。需先核对 `xid` 类型定义确认是否为真实越界。

### 3. P0 主干在基础运行时的接入点

状态: ready

来源: [MODULE_ASSESSMENT.md](dev/MODULE_ASSESSMENT.md) §1.3

三大主干在批次 A 的具体落点已明确，可在批次 A 内部预留接口，等正式推进主干时直接挂接：

- **统一错误模型**: 落点 `base.h`，扩展 `xrtThreadData` 增加 `xrtErrorContext` 槽，新增 `xrtSetErrorEx/xrtGetLastErrorEx`，旧 `xrtSetError` 作为薄包装保持兼容
- **统一执行上下文**: 落点 `time.h`，内部预留 `xrtDeadlineFromTimeout/xrtDeadlineExpired` helper
- **统一资源生命周期**: 落点 `type.h`，为 `xrt_type_ops` 补 `move` 实现，先做 string 类型
