---
num: 134
slug: embed
title: 嵌入宿主程序
volume: 卷十二 工程实践
type: practice
lead: 分配器接管与首分配锁、错误槽的宿主映射、模块裁剪的宿主选择、stats 的生产常开——把 XRT 装进你的程序而不喧宾夺主。
api: core, memory_stats, error
---

## 导读

"嵌入"是 XRT 设计里的第一公民视角——教程每一章的 API 都活在"宿主程序的一部分"这个假设下。本章把散落各章的集成点收拢成完整手册：**内存**（`xrtSetAllocator` 在 main 一进来就接管——之后库内全部分配走宿主；首分配后锁定保稳定）；**错误**（线程错误槽与宿主异常体系的映射——每个"返回值宣告失败"都是宿主异常的候选源；错误对象可跨层携带）；**裁剪**（宿主只选需要的根模块——第 130 章裁剪语义的宿主侧应用；最小集成不偷偷变大）；**观测**（`memory_stats` 生产常开的近零开销计数——宿主的健康检查面板直读）；**形态选型**（单头/静态/动态——第 131 章三类消费者的宿主决策）。嵌入的最高目标：**库是宿主的零件，不是平行王国**——内存归你管、错误归你翻译、体积归你定。

## 引入

宿主集成最常见的失败形态是"库想当主人"：自作主张 malloc（宿主的内存预算失控）、报错只打印（宿主的错误体系接不住）、体积全量拖入（嵌入式放不下）、全局状态自说自话（多实例/多版本共存崩溃）。XRT 的每个集成点都是对这些失败的正面回答——而且都被测试锁定（分配器替换有 `allocator_tour` 的双轨验证、错误映射有 `_threads` 的槽隔离验证、裁剪有体积档断言、stats 有开销基线）。

值得先建立**接管的时序观**：main 的第一件事换分配器（锁定前）→ 初始化宿主设施 → 按需开 stats → 业务运行期错误槽随线程走 → 收尾时 stats 快照+Drain 全部库对象。顺序错了（如首分配后再换分配器）就是第 5 章讲过的被拒场景——本章把它们串成时间线。

## 概念

### 内存接管：分配器与锁定

```diagram flow
- 时机：main 一进来（任何 XRT 分配之前）→ xrtSetAllocator(&宿主分配器)
- 接管后：库内全部分配（含池/缓冲内部）走宿主——预算、统计、arena 归宿主管
- 锁定：首次分配后 SetAllocator 被拒——多 TU/多模块场景不会再被换
- 形态：at 族四入口（Alloc/Calloc/Realloc/Dup）+ 回调上下文
```

宿主分配器的常见形态：**arena**（请求级 bump 分配——请求结束整块释放，第 7 章的宿主应用）、**预算计数**（限量分配器——库的内存上限归宿主策略）、**代理统计**（转发系统分配器+记账——宿主面板直读库的消耗）。**锁定的意义**再强调：两个库模块（或宿主的两处代码）都试图换分配器——先到先得、后者被拒——集成稳定性优先于灵活性。

### 错误映射：从错误槽到宿主体系

XRT 的失败报告是"返回值 + 线程错误槽"（第 4 章）——宿主侧的映射三问：**在哪接**（每个 XRT 调用点检查返回值——宏/包装函数统一接）；**怎么翻译**（`xrtErrorKind` → 宿主异常类型/错误码；`xrtErrorMessage` → 人类消息；原因链 → 日志展开）；**怎么携带**（跨线程先 `TakeError`——第 4 章的携带语义在宿主任务队列里兑现）。**错误槽隔离**：宿主多线程各持各槽——`_threads` 测试验证的隔离正是宿主并发安全的保证。

### 裁剪与体积：宿主的选择权

宿主声明根模块（第 130 章）——"我只要容器+字符串"的宿主产物不含网络任何符号（forbid_symbols 断言）。**演进安全**：无选择=Core——XRT 未来加模块不会悄悄加大宿主产物（契约明示这条规则的目的）。**多 TU 一致**：宿主的全部 TU 用一致模块集合（模块化链接时）——构建系统统一定义（CMake 的 target_compile_definitions 一处声明）。

### 观测：stats 的生产常开

`memory_stats`（第 6 章介绍过）的宿主定位是**健康面板的数据源**：`xrtMemStatsEnable(true)` 一次、周期 `Snapshot` 读数——`malloc_calls`（业务分配次数）/`pooled`（池命中）/`direct`（直通）/`backing`（打到系统的次数）四读数构成内存健康的最小指标集。**近零开销**是常开的前提（只计数不记录——对照 memory_debug 的全记录高开销）；泄漏检测（live 不归零）在长运行服务的告警阈值里用。

### 三形态的宿主决策

单头（脚本/原型——一个 include）/静态库（正式工程——链接冻结）/动态库（插件宿主——独立升级）——第 131 章的消费者选型在宿主语境重述为集成决策：**宿主的构建系统对接成本**与**分发形态**决定选择；行为一致性（verify 断言）保证切换无感。

## 示例

### 第一个完整程序：stats 常开的健康面板

下面的程序来自 `examples/memory/stats`——生产形态的观测样本：

```embed path="examples/memory/stats/main.c" title="examples/memory/stats/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/stats/main.c -lws2_32 -liphlpapi
before=0 after=1
malloc_calls=2 pooled=1 direct=1 backing=3
```

**刚才发生了什么。** ① `xrtMemStatsEnable(true)` + `Reset`——开关查询（before/after）再清零：**先确认再行动**的观测纪律。② 两次业务分配（48B 小块 + 4096B 大块）后快照读数：`pooled=1`（小块命中池——零系统调用）、`direct=1`（大块超阈值直通）、`backing=3`（系统侧实际 3 次——直通 1 + 池内部预热 2）——**业务视角与系统视角的对账**正是容量规划的原料。③ 宿主面板周期执行"快照+读数"就是库内存健康的全部监控——常开代价近零（计数的自增）。

### 第二个完整程序：分配器接管的完整验证

第二个程序来自 `examples/core/allocator_tour`——宿主内存接管的验收：

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**刚才发生了什么。** ① 换分配器后**at 族四入口全验**（Alloc/Calloc/Dup/Realloc）——宿主分配器要实现的完整面（不只是 Alloc——Realloc 的语义常被漏）。② 第二行的锁定验证：二次 SetAllocator 被拒——**宿主集成的时序契约**（必须在首分配前）从错误变成被测事实。③ 这两个示例合起来是宿主集成的"Hello World+"：接管（本例）+ 观测（上例）——内存主权与可见性同时到位。

### 宿主清单：集成的验收顺序

把本章收拢成一张**上线前清单**——按依赖顺序逐项打勾：① 分配器已在 main 首行接管（at 族四入口全实现、锁定语义已知悉）；② 模块裁剪已声明（根模块清单进构建系统、体积档验证过禁入符号）；③ 错误映射层就位（Kind 翻译表、原因链入日志、跨线程 TakeError 路径）；④ 观测已常开（stats enable + 周期快照 + 四读数告警阈值）；⑤ 诊断形态已定（生产 EXCLUDE 排除、排障构建可切换）；⑥ 产物形态已选（单头/静态/动态——与宿主构建系统对接验证）；⑦ 收尾路径已演练（库对象全 Drain、stats 终快照、错误槽清空）。七项全勾，XRT 就以"零件"身份融入宿主——内存可见、错误可译、体积可控、行为可验——嵌入的完成态不是"能编译"而是"主权清晰"。

## 契约

- **接管时序**：SetAllocator 在任何 XRT 分配之前（main 首行级别）；首分配后锁定；at 族四入口全实现。
- **内存主权**：接管后库内全部分配（含池内部）走宿主——预算/统计/arena 归宿主。
- **错误映射**：调用点查返回值→Kind 翻译→Message 展示→原因链入日志；跨线程先 TakeError。
- **槽隔离**：宿主线程各持各错误槽——并发安全由 _threads 测试锁定。
- **裁剪主权**：宿主声明根模块；无选择=Core 不变大；多 TU 集合一致（构建系统一处定义）。
- **观测常开**：stats 只计数近零开销；四读数（calls/pooled/direct/backing）为健康最小集。
- **形态一致**：单头/静态/动态行为由 verify 断言——宿主切换零成本。
- **诊断可拔**：MEMORY_DEBUG 排除语义（第 130 章）——生产全功能减诊断。

## 避坑

### 坑 1：初始化库之后才换分配器

症状：SetAllocator 返回失败——库的内部结构（池/首次缓存）已经用默认分配器建了。

原因：锁定契约。宿主的 main 第一件事就是换——任何"先初始化点东西"的顺序都可能踩线。

```c bad
int main(void) {
	init_something_using_xrt();   /* 内部已分配 */
	xrtSetAllocator(&MyAlloc);    /* 被拒：首分配已发生 */
}
```

```c good
int main(void) {
	xrtSetAllocator(&MyAlloc);    /* 第一件事 */
	init_something_using_xrt();   /* 全部走宿主分配器 */
}
```

### 坑 2：宿主包装函数吞掉错误细节

症状：宿主侧只见"失败"——类别/域/原因链全丢——重试策略只能瞎猜。

原因：映射层偷懒（只传 bool）。Kind 是机器可读的决策依据（第 4 章）——TIMEOUT 重试、AGAIN 等待、ARGUMENT 报 bug——吞掉它等于把结构化错误降级成布尔。

```c bad
bool my_json_parse(str s, xvalue** out) {
	if ( !xrtJsonParse(...) ) { return false; }   /* 细节全丢 */
}
```

```c good
bool my_json_parse(str s, xvalue** out, my_error* e) {
	if ( !xrtJsonParse(...) ) {
		fill_host_error(e, xrtTakeError());  /* Kind/域/链全量翻译 */
		return false;
	}
}
```

### 坑 3：stats 与 debug 傻傻分不清（全开）

症状：生产环境开了 memory_debug（每笔分配记录明细）——性能掉一截，内存翻倍。

原因：两者定位不同：stats 计数（常开）、debug 明细（排障时短开）。生产的默认组合是 stats 开 + debug 排除（`XRT_EXCLUDE_MEMORY_DEBUG`——第 130 章的组合语义）。

```c bad
#define XRT_MODULE_ALL   /* 含 memory_debug——生产全明细记录 */
```

```c good
#define XRT_MODULE_ALL
#define XRT_EXCLUDE_MEMORY_DEBUG   /* 生产：stats 留、明细去 */
/* 排障构建再显式加 XRT_MODULE_MEMORY_DEBUG */
```

## 练习

### 基础：请求级 arena 集成

宿主 arena（bump 分配 + 请求结束整释）作 XRT 分配器：一个请求内跑容器+字符串业务。验收标准：库内零系统分配（backing=0 或仅 arena 大块）；请求结束一次释放；stats 对账。

### 进阶：错误映射层

写宿主映射函数（Kind→宿主错误码、Message+原因链→日志结构）+ 包装三个常用调用。验收标准：六种 Kind 各走一遍映射正确；跨线程任务携带错误（TakeError 路径）无损。

### 挑战：多租户嵌入

同进程两个"租户"（独立业务模块）各自裁剪、共享一个 XRT 实例：统一分配器（预算按租户记账——上下文区分）、错误各线程隔离、stats 分租户读数。验收标准：租户 A 的 OOM 不影响 B；错误不串槽；体积为两租户闭包并集。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 最高目标 | 库是宿主零件——内存/错误/体积的主权在宿主 |
| 接管时序 | SetAllocator 在任何 XRT 分配前；at 族四入口全实现 |
| 锁定 | 首分配后不可换——集成稳定优先 |
| 错误映射 | 返回值点→Kind 翻译→链入日志；跨线程 TakeError |
| 槽隔离 | 线程各持槽——_threads 测试锁定的宿主并发保证 |
| 裁剪主权 | 声明根模块；无选择=Core；演进不偷偷变大 |
| stats 常开 | 近零开销四读数：calls/pooled/direct/backing |
| 诊断可拔 | ALL+EXCLUDE_MEMORY_DEBUG——生产全功能减明细 |
| 形态决策 | 单头/静态/动态——行为一致切换零成本 |
