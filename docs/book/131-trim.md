---
num: 131
slug: trim
title: 特性裁剪与体积控制
volume: 卷十二 工程实践
type: practice
lead: 正向模块选择与闭包展开、ALL+EXCLUDE 的排除语义、粒度原则、体积档案与符号禁入——把库裁到目标放得下。
api: core, error
---

## 导读

第 130 章讲了构建怎么编译裁剪组合；本章讲**裁剪怎么声明、怎么验证体积真的小了**。三块：**正向选择模型**（`XRT_MODULE_*` 只声明根模块、依赖闭包自动展开——"用户不需要知道模块当前依赖哪些内部特性，也不应复制依赖表"）；**ALL+EXCLUDE 的排除语义**（`XRT_MODULE_ALL`+`XRT_EXCLUDE_MEMORY_DEBUG`——全量减诊断的精确表达；显式根模块优先于排除）；**粒度原则**（一个公开模块=一小组有共同状态/依赖/场景的能力——不按函数拆宏也不做总开关；新增能力先问"能否独立测试/裁剪/说明所有权"）。**体积验证**：`config/size_profiles.json` 的档案（每档 suite+`forbid_symbols` 符号禁入表）配 `tools/measure_size.py`——core 档禁止出现正则/压缩/网络/协议族的任何符号——**裁剪不是声明而是被验证的事实**。

## 引入

嵌入式目标放不下整个库？裁掉不需要的部分——听起来简单，历史实现的三种病：**手抄依赖表**（用户自己列全部需要的宏——模块内部依赖一变全库用户的抄写全错）；**隐式全量**（不选就给你全部——"最小程序"偷偷带上 TLS）；**虚假粒度**（按函数拆宏——宏数量爆炸没人用；或一个总宏——裁剪等于全或无）。XRT 的三副药：闭包展开（声明根、展开依赖——表只在库内维护一份）；**无选择=只有 Core**（"没有定义任何 XRT_MODULE_* 时只提供无特性宏的 Core 契约"——最小集成可预测，新模块不会悄悄加大现有程序）；**模块粒度三问**（独立测试/独立裁剪/独立说明所有权——满足才立新模块）。

`forbid_symbols` 的设计值得强调：体积档案不只是"编这个 suite"，还**断言产物里不出现别家的符号**——core 档链接后如果冒出 TLS 族符号（依赖误拉），measure 直接失败。裁剪错误在门禁暴露，不在线上体积膨胀时发现。

## 概念

### 正向选择与闭包

```diagram flow
- 声明：#define XRT_MODULE_STRING_SPLIT（根模块）→ #include "xrt.h"
- 展开：features.h 按模块清单展开闭包——string 基础+切分自动带上
  ——用户不复制依赖表；内部依赖演进对用户透明
- 实现单元：XRT_IMPLEMENTATION 恰好一个翻译单元定义
- 一致性：模块化链接时全部 TU 用一致模块集合
```

**时序纪律**：选择宏必须在首次 include `xrt.h`/`features.h` 前定义——include 之后的 define 无效（展开已经发生）。

### ALL 与 EXCLUDE 的语义

`XRT_MODULE_ALL`：需要完整运行库的宿主/编译器/调试工具用。`XRT_EXCLUDE_MEMORY_DEBUG`：ALL 减内存调试（同时排除 memory_debug 与 memory_debug_report、保留 memory_stats）——"完整生产功能但不要侵入式诊断"的精确表达。**优先级规则**：EXCLUDE 只抑制 ALL 的隐式选择；**显式定义的根模块仍然优先**（ALL+EXCLUDE_X+显式 MODULE_X=X 进来）。**默认忠告**（契约原文）："普通应用不应默认选择全部模块"。

**ALL+EXCLUDE 的组合语义**用一次推演记住：`XRT_MODULE_ALL` 把清单里全部模块隐式选上；`XRT_EXCLUDE_MEMORY_DEBUG` 从隐式集合里剔除两个诊断模块——此刻产物=全量减诊断；再显式 `#define XRT_MODULE_MEMORY_DEBUG`——显式根模块**加回**优先于排除（三态叠加的求值顺序：显式>ALL 的隐式>EXCLUDE 的剔除）。这个优先级设计让"全量减一"与"精确点名"可以共存，宿主在 ALL 基础上逐模块微调而不失去默认方便性。

### 粒度原则：模块三问

一个公开模块=一小组**共同状态、共同依赖、共同场景**的能力。反面：按单函数拆宏（宏爆炸）/整体系一个总开关（全或无）。Queue 家族与网络族是正面样本：Queue 的基础/SPSC/MPSC/MPMC/等待/取消/Select/协程桥接分别选择；网络的 Engine/各后端/TCP/UDP/同步/Future/DNS/代理/TLS 各自成闭包。**新增公共能力先过三问**：能否独立测试？能否独立裁剪？能否独立说明所有权？——满足就立新模块，不扩大已有总宏（粒度不退化的机制）。

### 体积档案与符号禁入

```diagram flow
- 档案：size_profiles.json 每档 {name, suite, forbid_symbols}
  ——core 档禁正则引擎前缀、压缩内部前缀（regex/压缩的私有符号）
  与网络/HTTP/WebSocket/TLS 各族的公开符号前缀
- 度量：measure_size.py 按档编译 → 记录环境基线（平台/编译器/优化/stripped）
  → 断言禁入符号零出现 → 尺寸报告（含基线防跨环境误比）
- 门禁：任一禁入符号出现即失败——依赖误拉当场暴露
```

**环境基线**的意义：二进制尺寸跨编译器/优化级/平台不可比——报告记录 `platform/machine/compiler_family/compiler_version/arch/optimization/stripped` 七要素，趋势对比只在同基线内进行（第 136 章度量方法的前奏）。

## 示例

### 第一个完整程序：最小 Core 集成

下面的程序来自 `examples/core/version_limits`——无特性宏的 Core 契约下运行：

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**刚才发生了什么。** ① 编译行**没有 XRT_MODULE_ALL**——无选择=只有 Core：版本查询与资源边界是 Core 契约的一部分（结构化错误/错误槽同属），而 printf 错误构造是 `XRT_MODULE_ERROR_FORMAT` 独立选择——最小程序不带格式化运行时（契约原文的例子）。② 对照第 130 章的编译行（有 ALL）：**同一示例两种集成形态**——ALL 是教程的省心形态、无选择是最小集成的事实声明。③ 体积视角：这个形态的产物就是 core 体积档——体积档的符号禁入断言它不含正则/压缩/网络任何符号。

### 第二个完整程序：分配器替换的最小集成

第二个程序来自 `examples/core/allocator_tour`——Core 闭包内的宿主接入：

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**刚才发生了什么。** ① 同样无 ALL 的最小形态——`xrtSetAllocator` 是 Core 内存面（第 5 章）的一部分。② **首分配锁定的裁剪意义**：嵌入宿主（第 135 章）在 main 一进来就换分配器——之后库内全部分配走宿主；锁定保证多模块/多 TU 场景不会再被换掉（集成稳定性）。③ 裁剪+分配器替换的组合就是嵌入式集成的标准姿势：小体积+内存归宿主管——两个示例合起来是最小集成的完整画像。

### 与宿主裁剪观的对照（设计观察）

裁剪问题在宿主语言世界（C++ 模板实例化、Rust feature 门控）同样存在，对照能看清取舍。**Rust features**： Cargo.toml 声明特性+依赖闭包——模型几乎同构（正向选择+闭包展开），差异在 XRT 的 forbid_symbols 断言（产物级验证）比多数生态更严格。**C++ 模板**：按实例化自动"裁剪"——零声明成本，但裁剪结果不可预测（实例化蔓延）、无法断言（没有"禁入符号"的等价物）。XRT 选择显式声明+产物断言：多写一行宏，换来"裁了什么"成为可验证事实——嵌入式交付里这个确定性比省事值钱。

## 契约

- **正向选择**：XRT_MODULE_* 声明根模块；闭包按清单展开；用户不复制依赖表。
- **时序**：选择宏在首次 include 前；XRT_IMPLEMENTATION 恰一 TU；链接 TU 集合一致。
- **无选择语义**：只有无特性宏的 Core 契约——新模块不悄悄加大现有程序。
- **EXCLUDE 语义**：抑制 ALL 的隐式选择；显式根模块优先；MEMORY_DEBUG 排除带 report 留 stats。
- **粒度三问**：独立测试/独立裁剪/独立说明所有权——满足才立新模块。
- **粒度样本**：Queue 八件与网络族逐项独立——不按函数拆也不做总开关。
- **体积档案**：{name, suite, forbid_symbols}；禁入符号出现即门禁失败。
- **环境基线**：七要素（平台/机器/编译器族/版本/架构/优化/strip）——趋势只比同基线。
- **普通应用忠告**：不应默认 ALL——ALL 是宿主/工具的形态。

## 避坑

### 坑 1：include 之后才 define 模块宏

症状：模块的 API 不可见（编译器报未声明）——明明 define 了。

原因：展开在首次 include 时发生；之后的 define 不回头。宏必须在一切 xrt.h/features.h include 之前（命令行 -D 或文件最顶部）。

```c bad
#include "xrt.h"                 /* 先 include：Core 展开已定 */
#define XRT_MODULE_QUEUE         /* 迟到的选择：无效 */
```

```c good
#define XRT_MODULE_QUEUE         /* 顶部声明 */
#define XRT_IMPLEMENTATION
#include "xrt.h"
```

### 坑 2：抄依赖表（手动列全部宏）

症状：库升级后编译断——内部依赖变了，你抄的表过时。

原因：依赖闭包是库的私事——契约明说"不应复制依赖表"。只声明你要的根（STRING_SPLIT 而不是 STRING+STRING_SPLIT 的内部组合）。

```c bad
/* 抄出来的"闭包" */
#define XRT_MODULE_STRING
#define MY_SPLIT_LEGACY_PATCH /* 抄旧版依赖的手工补丁宏 */
```

```c good
#define XRT_MODULE_STRING_SPLIT  /* 只声明根——闭包库内展开 */
```

### 坑 3：体积对比跨了环境

症状："这次改动让二进制大了 30%"——一查基线，上次 gcc 这次 tcc。

原因：尺寸跨编译器/优化级/strip 不可比；measure_size 的七要素基线就是防这个。趋势对比必须同基线（同档同环境），或用符号禁入断言代替绝对尺寸。

```c good
/* 查报告基线七要素一致再比；体积门禁用 forbid_symbols（环境无关） */
```

## 练习

### 基础：三种集成形态对拍

同一示例分别以无选择/`MODULE_STRING_SPLIT`/`MODULE_ALL` 编译——对比产物符号表（nm/findstr）与尺寸。验收标准：三形态符号集递增；无选择形态零非 Core 符号。

### 进阶：EXCLUDE 语义验证

`MODULE_ALL`+`XRT_EXCLUDE_MEMORY_DEBUG` 编译——确认内存调试 API 缺席而 stats 仍在；再加显式 `XRT_MODULE_MEMORY_DEBUG`——确认显式优先（调试 API 回来）。验收标准：三步符号检查与契约一致。

### 挑战：自定义体积档

为"只要容器+字符串"的目标写新档案：suite 定根模块、forbid_symbols 列网络/协议族符号——跑 measure_size 验证。故意把一个依赖误拉进 suite——观察禁入断言失败。验收标准：正常档全绿；误拉档在禁入符号处失败（错误信息指出符号名）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 选择模型 | 正向根模块+闭包展开——依赖表不抄 |
| 时序 | 宏在首次 include 前；IMPLEMENTATION 恰一 TU |
| 无选择 | 只有 Core 契约——最小集成可预测 |
| EXCLUDE | 抑制 ALL 隐式；显式根优先；MD 排除带 report 留 stats |
| 粒度三问 | 独立测试/裁剪/所有权——满足才立模块 |
| 粒度样本 | Queue 八件、网络族逐项——不函数拆宏不总开关 |
| 体积档案 | name+suite+forbid_symbols——禁入零出现 |
| 环境基线 | 七要素——趋势只比同基线 |
| 忠告 | 普通应用不 ALL——ALL 归宿主/工具 |
