---
num: 132
slug: testing
title: 测试、故障注入与模糊测试
volume: 卷十二 工程实践
type: practice
lead: testRequire 断言族、线程/负向/变异/逐分配点 OOM 的测试形态矩阵、LibFuzzer 风格的结构化模糊——门禁的四种武器。
api: core, memory_debug
---

## 导读

前十一章的每个 API 都被一群测试护着——本章讲那群测试的**形态学**。XRT 的测试体系（挂在清单 tests 字段、build.py 双轨运行）有四种武器：**正向测试**（`testRequire(条件, "消息")` 断言族——简洁到一行的验收）；**线程测试**（`_threads` 后缀——并发语义的实测）；**负向与变异测试**（`_negative`/`_mutation` 后缀——畸形输入稳定性：全字节变异往返、边界种子）；**逐分配点 OOM 测试**（`_oom` 后缀——185 个文件！失败分配器逐点注入，验证"失败不破坏状态、错误正确报告"）。**模糊测试**（fuzz/）：LibFuzzer 风格入口+仓库内的**结构化模糊**（确定性移位寄存器噪声+语法种子先行——`test_http_auth_fuzz` 的形态）。四种武器覆盖"对的行为、并发下的行为、错输入下的行为、没资源时的行为"四个象限。

## 引入

为什么 OOM 测试有 185 个文件？因为**内存耗尽是最容易被跳过的路径**：开发者写代码时假设分配成功（正常环境确实成功），测试也跑在内存充足的机器上——于是"分配失败后的清理"成为纯粹的未测试区。而恰恰这条路径藏着最多的泄漏与状态损坏（半初始化结构、部分构建的容器）。XRT 的纪律是**逐分配点验证**：失败分配器（如 `testMapOomAlloc`——"允许状态下转发到底层，限量用尽返回 NULL"）让被测代码的每一次分配按序失败——第 N 次失败跑一轮，循环到全部点都轮过——"分配失败不破坏状态"成为被穷举的事实而非假设。

变异测试的动机同源但对象不同：**解析器的健壮性**。HTTP 头、percent 编码、TLS 报文——真实网络会送来一切字节序列；测试自己造的几十个用例永远不够。变异测试把合法输入的**每个字节逐个翻转**再整轮随机噪声——"任意输入不崩溃、失败走结构化错误"是被机器证明的。模糊测试再加一层：语法种子（真实协议形态）+确定性随机变长——在 CI 里跑出人类想不到的组合。

## 概念

### 断言族与测试形态矩阵

```diagram flow
- 正向（test 名）：testRequire(条件, "说明")——失败即打印消息退出
- 线程（_threads）：并发语义实测——SPSC 压测/取消竞态/错误槽隔离
- 负向（_negative）：非法输入的明确拒绝——错误类别与码断言
- 变异（_mutation）：全字节变异+大小写混合——往返稳定性
- OOM（_oom）：失败分配器逐点注入——185 个文件的矩阵
- 模糊（_fuzz）：语法种子+确定性噪声——结构化探索
```

`testRequire` 是全库统一断言（测试框架极简：失败打印消息+行号+退出非零——门禁只认退出码）。形态后缀是**命名约定**——清单里一眼看出每个模块的覆盖象限（queue 的卡：test_queue/test_queue_spsc_threads/test_queue_spsc_oom——三象限齐）。

### 逐分配点 OOM：失败分配器模式

```diagram flow
- 装配：自定义分配器（Fail 标志/Limited 限量/Remaining 余量）
- 循环：第 N 点失败 → 跑被测流程 → 断言（返回失败/状态未变/错误=XERR_MEMORY+域）
  → N+1 → 直到流程不再分配
- 附加验证：错误报告本身不申请内存（OOM 中的错误也要能报——test_oom 的核心断言）
```

第 6 章（内存调试）讲过失败分配器的**工具面**（`xrtMemDebugFailAfter`）；测试体系用它+自写限量分配器覆盖每个模块。**"错误报告不申请内存"**是反复出现的断言（`xrtErrorCreate` 失败时错误槽保留 OOM 类别——错误系统在资源枯竭时仍活着）：第 4 章"最小依赖"承诺的测试面。

### 变异测试：往返与稳定

两种形态：**全字节变异**（合法向量 → 每字节逐个 ±1/翻转 → 重解析/重解码——拒绝或成功都行但不崩溃不泄漏，成功者与原语义一致）；**噪声往返**（确定性随机输入——如 percent 的 6000 轮"任意字节+大小写混合转义都能稳定往返"）。**确定性**（固定种子）是关键：失败可复现——模糊发现的问题报告种子即可重放。

### 模糊：种子+噪声+不变量

`test_http_auth_fuzz` 的结构是模板：① **语法种子先行**（真实协议形态的字符串数组——空串/标准 Basic/Bearer/Digest 复杂参数——已知边界先过）；② **确定性噪声**（移位寄存器变长随机输入——探索人类想不到的组合）；③ **不变量断言**（解析要么成功要么结构化失败——abort 带行号的 `FuzzAbort` 报告契约违反位置，"避免优化器合并冷分支后丢失诊断"——连编译器优化都考虑了）。**fuzz/ 目录**（xhttp 的 http_auth/http_route/http_router/http_sse 四个 LibFuzzer 风格入口）：外部模糊器对接的持久入口——CI 快跑内置模糊、离线深跑外部模糊器，同一套不变量。

## 示例

### 第一个完整程序：故障注入的日常面

下面的程序来自 `examples/memory/fail_inject`——失败分配器的应用层姿势：

```embed path="examples/memory/fail_inject/main.c" title="examples/memory/fail_inject/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/memory/fail_inject/main.c -lws2_32 -liphlpapi
enabled=1
fail-after-2: 1st=ok 2nd=NULL triggered=1
event[0]: alloc
cleared: 3rd=ok triggered-still=0
reset=1
```

**刚才发生了什么。** ① `xrtMemDebugFailAfter(N)` 的**实测语义**：前 N 次成功、第 N+1 次失败——输出里 1st=ok 2nd=NULL 就是 FailAfter(1)。② **事件流**（event[0]: alloc）——调试堆的按序事件访问：注入的失败也进事件流（审计完整）。③ `FailClear` 连"已触发"标志一并复位（cleared 后 3rd=ok 且 triggered-still=0）——**_oom 测试变体全部建立在这组入口上**（头注释原话：测试与应用共用同一实现——又一处"无第二实现"）。

### 第二个完整程序：泄漏现场的定位

第二个程序来自 `examples/memory/debug`——Reset/Snapshot/VisitLive 的三件套：

```embed path="examples/memory/debug/main.c" title="examples/memory/debug/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single impl.c examples/memory/debug/main.c -lws2_32 -liphlpapi
live_count=1 live_bytes=64 peak_bytes=64
live address=000002bd... size=64 site=examples/memory/debug/main.c:30
alloc_count=1 free_count=1 events=2
```

**刚才发生了什么。** ① **泄漏现场的标准姿势**（头注释）：怀疑段前 `Reset`、后 `Snapshot`——`LiveCount` 不归零 + `VisitLive` 打出的 `site`（**精确到文件:行**）就是泄漏点。② 输出里 alloc_count=1 free_count=1 但 live_count=1——一次 alloc 一次 free 之后还有活块？这是示例刻意留下的分配（演示VisitLive 能看见它）——计数对账思维：三种计数（分配/释放/存活）交叉验证分配纪律。③ 更强的能力（隔离区捕获双释放/UAF）在 JSON 报告形态（`debug_report` 示例）——测试与运维共用同套数据。

### 测试金字塔的第三层：变异与模糊的分界

把 XRT 的测试想成金字塔：底层是海量正向断言（每模块数十条 testRequire——行为逐条钉死）；中层是象限扩展（threads/oom——两个高成本象限各一组）；顶层是变异与模糊——**输入空间的机械探索**。变异与模糊的分界值得记住：变异以**已知合法输入**为锚（每个字节都翻一遍——保证"合法附近的非法"全过）；模糊以**语法模板**为锚（随机组长的类真实输入——探索"结构上像但从未见过"的区域）。前者防回归（改坏了旧边界），后者防未知（藏着没写过的边界）。二者都以确定性为生命线——这也是它们能进 CI 的前提（几分钟跑完、失败必复现），真随机的深探索留给离线外部模糊器。

## 契约

- **形态命名**：`_threads`/`_negative`/`_mutation`/`_oom`/`_fuzz` 后缀——覆盖象限在清单可见。
- **断言极简**：testRequire 族——失败打印+非零退出；门禁只认退出码。
- **OOM 逐点**：失败分配器逐点轮转；断言失败返回+状态不变+XERR_MEMORY+正确域。
- **错误系统存活**：OOM 中的错误报告不申请内存——错误槽保留 OOM 类别。
- **变异确定性**：固定种子——失败可复现（种子即重放凭证）。
- **不变量优先**：模糊断言"成功或结构化失败"二选一——崩溃/泄漏是契约违反。
- **种子先行**：语法边界种子在随机噪声之前——已知边界必过。
- **双模模糊**：内置确定性（CI）+外部模糊器入口（离线深跑）——同一套不变量。
- **示例即测试件**：fail_inject/debug 示例与 _oom 测试共用入口——无第二实现。

## 避坑

### 坑 1：只写正向测试

症状：代码合并后线上崩——并发竞态/畸形输入/内存不足三条路径全没测过。

原因：正向测试只覆盖"对的行为"象限；四象限（对/并发/错输入/没资源）需要四种武器——后三种正是事故的来源。

```c bad
testRequire(parse(str) == OK, "parse");
/* 并发？畸形？OOM？——全部未测 */
```

```c good
/* 四象限各至少一组：正向+threads+mutation/negative+oom */
testRequire(parse(str) == OK, "parse");
/* tests/xxx/test_xxx_threads.c / _mutation.c / _oom.c 同模块挂清单 */
```

### 坑 2：模糊测试用真随机（不可复现）

症状：CI 偶发失败——本地重跑又过（种子不同），报告无法定位。

原因：模糊的价值在**可复现**——固定种子的确定性噪声让每次失败都能重放。真随机只用于离线探索（发现后转固定种子用例入库）。

```c bad
srand(time(NULL));            /* 每次不同——失败即丢失 */
run_fuzz(random_input());
```

```c good
uint32 State = UINT32_C(0x9E3779B9);  /* 固定种子 */
run_fuzz(noise(&State));               /* 失败种子可重放 */
```

### 坑 3：OOM 测试断言"返回失败"就完

症状：OOM 后状态损坏——下次调用崩溃；或错误类别不对（把 OOM 报成参数错误）。

原因：OOM 测试的完整断言是三件：失败返回+**状态未变**（对象可继续用或可安全销毁）+错误正确（XERR_MEMORY+域）。只查返回值放过一半的 bug。

```c bad
testRequire(map_insert(...) == false, "oom fails");
/* 状态？错误？——没查 */
```

```c good
testRequire(map_insert(...) == false, "oom fails");
testRequire(xrtErrorKind(xrtGetError()) == XERR_MEMORY, "kind");
testRequire(map_still_consistent(&Map), "state intact");  /* 状态未变 */
testRequire(map_destroy_path_clean(&Map), "可安全清理");   /* 收尾不二次崩 */
```

## 练习

### 基础：跑一个模块的全形态

`build.py --suite queue`——观察 threads/oom 变体的运行。验收标准：能对每个测试文件说出它覆盖的象限。

### 进阶：给自定义模块写四件套

写一个含分配的小模块（如简易缓存），配齐正向/threads/mutation/oom 四个测试文件挂清单。验收标准：故意在 OOM 路径留泄漏——oom 测试能抓到（用 debug 堆对账）。

### 挑战：结构化模糊器

仿 `test_http_auth_fuzz`：语法种子（≥8 个真实形态）+ 确定性噪声 5000 轮+不变量（解析成功或结构化失败）。验收标准：注入一个会崩溃的 bug（如越界读）——模糊器在 ≤1000 轮内抓到；输出带可重放种子。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 四武器 | 正向（testRequire）/线程（_threads）/变异负向（_mutation/_negative）/OOM（_oom） |
| 模糊 | _fuzz 内置+fuzz/ 外部入口——语法种子先行+确定性噪声 |
| OOM 模式 | 失败分配器逐点轮转；三件断言（返回/状态/错误） |
| 错误存活 | OOM 中错误报告不申请内存——错误槽保留类别 |
| 变异形态 | 全字节逐翻转+确定性噪声往返 |
| 确定性 | 固定种子——失败可复现；真随机只离线探索 |
| 不变量 | "成功或结构化失败"二选一——崩溃泄漏即违反 |
| 应用面 | fail_inject/debug 示例与测试共用入口——无第二实现 |
| 命名约定 | 形态后缀——清单里覆盖象限可见 |
| 规模事实 | _oom 185 个文件——逐分配点验证是纪律不是口号 |
