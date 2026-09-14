---
num: 132
slug: package
title: 打包、分发与单头
volume: 卷十二 工程实践
type: practice
lead: static/shared 双产物命名矩阵、真实消费者验证、amalgamate 的拓扑合成与一致性检查——从源码树到可交付物。
api: core, error
---

## 导读

第 130 章编译、第 131 章裁剪；本章把成果变成**可交付物**。`tools/package.py` 从清单出发打包两种产物：**static**（`libxrt.a`/`xrt.lib`——按闭包归档）与 **shared**（`xrt.dll`+导入库/`libxrt.so`/`libxrt.dylib`——按平台三态命名）；`--verify` 做发布级验证——**真实链接并运行最小发布库消费者**（不是"链接成功"而是"消费者跑通"）。**单头**（`single/xrt.h` + 声明版 `xrt_decl.h`）：`tools/amalgamate.py` 按模块拓扑序合成——本地 include 内联、选择宏重排、`--check` 断言**重新生成与提交文件零差异**（单头不漂移的机器保证）。发布成熟度检查（`check_release_maturity.py`）与扩展库独立打包（各自 manifest 的 libxruntime 等）收尾。

## 引入

"打包"听起来只是归档——真实的坑在**验证的深度**。浅验证：产物生成、大小合理。深验证：**消费者视角**——静态库真的能被第三方工程链接并运行吗（缺符号/重复符号/ABI 不匹配都是链接期或运行期才爆）？动态库的导入库真的对得上导出吗？单头真的与模块化源**行为一致**吗（手维护的单头必然漂移——改了源忘了重新生成）？XRT 的答案全是机械化：`--verify` 编译并**运行**最小消费者（发布语义的端到端）；`amalgamate --check` 重生成对比（单头漂移零容忍——SPEC"手工编辑 wwwroot 禁止"的同款纪律施于代码）；成熟度检查汇总全部红线。

单头的价值值得再述：**一个文件 + 一个 include** 就是完整集成——没有构建系统对接、没有链接配置、没有头文件路径——脚本、快速原型、教学场景（本书第 3 章起所有 term 的编译行就是单头形态）的首选。代价是编译期（全库进一个 TU）与不能选择性编译（裁剪宏仍有效——`XRT_MODULE_*` 在单头内照样工作，第 131 章的裁剪语义无损）。

## 概念

### 产物矩阵与命名

| kind | Windows/MSVC | Windows/GNU | macOS | Linux |
| --- | --- | --- | --- | --- |
| static | `xrt.lib` | `libxrt.a` | `libxrt.a` | `libxrt.a` |
| shared | `xrt.dll`+`xrt.lib`（导入库） | `xrt.dll`+`libxrt.dll.a` | `libxrt.dylib` | `libxrt.so` |

**闭包裁剪进产物**：`--suite` 决定打包哪些模块（闭包展开）——"只要网络的动态库"与"全量静态库"都是一条命令的事。产物名带产品前缀（扩展库打包时 `libxruntime.a` 等——各自 manifest 的 product 字段）。

### verify：消费者端到端

```diagram flow
- 打包：闭包源 → 编译对象 → 归档（static）/链接（shared）
- 验证：生成最小消费者程序（include 公共头 + 调用基础 API）
  → 对产物真实链接 → 运行 → 断言输出
- 语义：发布物从消费者视角"活着"——不是文件存在而是可用
```

`--verify` 的深度在于**运行**：链接成功只证明符号齐；运行成功才证明初始化/ABI/运行时行为正确。这与教程门禁的"示例编译并运行"（第 130 章）同一哲学——**可执行的事实才算数**。

### amalgamate：单头的合成

```diagram flow
- 输入：清单（模块拓扑序）+ 模块源
- 合成：本地 include 内联（仓库内 #include 展开进单文件）
  ——系统 include 保留 —— 选择宏区按依赖序重排
  —— 生成实现版 single/xrt.h 与声明版 single/xrt_decl.h
- 一致性：amalgamate --check 重新生成并与提交文件逐字节对比
  ——零差异才过——单头漂移在门禁暴露
```

**声明版**（`xrt_decl.h`）的意义：纯声明单头——多 TU 工程的"只想要原型"形态（链接库产物时用声明、单实现 TU 用完整版）。**拓扑序**是合成正确性的根基：模块间依赖决定拼接顺序——被依赖者在前（amalgamate 复用清单的拓扑排序——同一份事实源的又一次消费）。

### 发布门禁全链

xruntime README 的序列即模板：`build.py`（回归）→ `amalgamate --check`（单头一致）→ `check_release_maturity.py --release`（成熟度红线汇总）→ `package.py --kind static --verify` / `--kind shared --verify`（双产物+消费者验证）。**每条命令验证一个正交维度**——失败信息能直接指到维度。

## 示例

### 第一个完整程序：作为消费者的最小集成

下面的程序来自 `examples/core/version_limits`——它可以以单头、静态库、动态库三种形态消费：

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**刚才发生了什么。** ① 这条编译行是**单头消费者形态**：`-I single -include xrt.h` + `impl.c`（定义 `XRT_IMPLEMENTATION` 的实现 TU——教程的固定搭配）；`package --verify` 生成的最小消费者与此同构。② 换库形态：编 `libxrt.a` 后消费者的编译变为 `-I include` + `main.c -lxrt`（声明来自公共头、实现来自库）——**同一份 main 三种消费**，行为必须一致（verify 的隐含断言）。③ 无选择=Core（第 131 章）在这里兑现：这个消费者对最小产物（core 闭包）也能链接运行。

### 第二个完整程序：分配器定制的消费者

第二个程序来自 `examples/core/allocator_tour`——展示消费者侧的内存接管：

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**刚才发生了什么。** ① 首行换分配器后锁定的语义对**库产物同样成立**：静态/动态库内的全部分配都走宿主分配器（发布库"内存归宿主"的集成契约）。② 这个示例在门禁里的角色正是"消费者多样性"——除了 verify 的最小消费者，examples 全集都是真实消费者（第 130 章 examples 即门禁）；allocator 接入是其中最深的定制路径。③ 打包视角的裁剪：core 闭包产物已覆盖本示例全部符号——最小发布面够用。

### 三类消费者的选型

发布物的三种形态对应三类消费者，选型一页看清。**单头**（`single/xrt.h`）：脚本、快速原型、教学、小工具——零构建系统成本，一个 include 即完整；代价是单 TU 全量编译与重编译整个单头的编译时间。**静态库**（`libxrt.a`）：正式工程默认——链接即冻结（无运行时依赖）、闭包裁剪精确到模块、与宿主构建系统天然集成；代价是每平台每配置一个产物。**动态库**（`so/dll/dylib`）：多程序共享一份实现、可独立升级（ABI 稳定时）——C 库场景较少但插件宿主常用；代价是导入库/符号导出的平台矩阵与部署分发。本书教程通篇单头（term 编译行可证），是因为读者复制即运行；真实项目按部署形态选——三者行为一致（verify 的隐含断言）使切换零成本。

## 契约

- **产物矩阵**：static/shared × 四平台命名规则；shared 在 Windows 带导入库；产品名随 manifest。
- **闭包裁剪**：--suite 决定产物内容；闭包展开同 build。
- **verify 深度**：生成最小消费者→真实链接→运行→断言——可执行事实。
- **单头合成**：拓扑序拼接；本地 include 内联、系统 include 保留；选择宏按依赖重排。
- **双版本单头**：实现版 xrt.h / 声明版 xrt_decl.h——多 TU 形态用声明。
- **一致性检查**：--check 重生成对比零差异——单头漂移零容忍。
- **成熟度汇总**：check_release_maturity 汇总红线——发布是结论不是猜测。
- **扩展打包**：扩展库各自 manifest 打各自产物（libxruntime 等）——闭包含所需核心裁剪。
- **互斥链接**：不能同时链接两份含相同核心闭包的库（xruntime README 明示）。

## 避坑

### 坑 1：改了源忘了重新生成单头

症状：模块化测试全绿、单头用户编译失败或行为不同——single/xrt.h 是旧的。

原因：单头是**生成物**——手改单头或忘了 amalgamate，提交的与源不一致。`--check` 在 CI 拦截；本地习惯是改源后立即重生成。

```c bad
/* 手改 single/xrt.h 修一个笔误——下次生成被覆盖，且 CI --check 失败 */
```

```c good
/* 改模块源 → python tools/amalgamate.py → 提交源与生成的单头同 PR */
```

### 坑 2：同时链接库与扩展库的重复闭包

症状：符号重复定义链接错误，或更糟——两份核心状态（分配器各一套）运行期错乱。

原因：扩展库产物（如 libxruntime）已包含所需核心闭包——再链一份全量 libxrt 就是两份实现。契约明示"不能再同时链接另一份包含相同 XRT 闭包的库"。

```c bad
gcc app.c -lxruntime -lxrt   /* 两份 core：重复符号/双状态 */
```

```c good
/* 用 libxruntime（含其核心闭包）——不再链 libxrt；
   或源码形态集成扩展（扩展的单头含所需核心） */
```

### 坑 3：拿产物尺寸当发布质量指标

症状："新版本大了 5% 要回滚"——没查基线（编译器/优化/strip 不同）。

原因：尺寸对比必须同环境基线（第 131 章七要素）；发布质量的红线是 verify+成熟度检查（功能与一致性），尺寸只是趋势参考。

```c bad
if ( size(new) > size(old) * 1.05 ) { rollback(); }  /* 跨环境无意义 */
```

```c good
/* 同基线趋势监控 + forbid_symbols 结构断言 + verify 功能红线 */
```

## 练习

### 基础：双产物打包验证

`package.py --suite core --kind static --verify` 与 `--kind shared --verify`——检查产物名与消费者验证输出。验收标准：两产物生成、verify 各自跑通；导入库（Windows）存在。

### 进阶：裁剪产物

`--suite core,queue,queue_spsc` 打静态库；用第 21 章 SPSC 示例作消费者链接运行。验收标准：消费者通过；产物不含未选模块符号（nm 对照）。

### 挑战：单头一致性演练

改一个模块源的注释 → `amalgamate --check`（应失败——单头旧）→ 重新生成 → `--check` 过 → 双轨回归。验收标准：全流程闭环；理解 --check 比较的语义（重生成 vs 提交文件）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 产物矩阵 | static（libxrt.a/xrt.lib）/shared（dll+导入库/so/dylib） |
| 闭包进产物 | --suite 决定内容——最小发布面一条命令 |
| verify 深度 | 消费者真实链接+运行——可执行事实 |
| 单头合成 | 拓扑序拼接、本地 include 内联、宏重排 |
| 双版本 | xrt.h 实现 / xrt_decl.h 声明——多 TU 用声明 |
| 一致性 | --check 零差异——单头漂移零容忍 |
| 成熟度 | check_release_maturity 汇总红线 |
| 扩展打包 | 各自 manifest 各自产物——闭包含核心 |
| 互斥 | 不双链含相同闭包的两库 |
| 单头价值 | 一文件一 include 全集成——脚本/原型/教学 |
