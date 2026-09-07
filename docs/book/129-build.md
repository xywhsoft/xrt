---
num: 129
slug: build
title: 构建系统全解
volume: 卷十二 工程实践
type: practice
lead: 模块清单是唯一事实源、模块化与单头双轨编译、指纹缓存与并行任务、扩展库的 manifest 叠加——工具链的地基。
api: core, error
---

## 导读

卷十二从工具链开始。XRT 的构建不是手写 Makefile，而是一套**以 `config/modules.json` 为唯一事实源**的 Python 工具链：清单声明每个模块的头/源/测试/示例/文档（含依赖闭包），`tools/build.py` 据此驱动**模块化编译**（按模块选源，对象按闭包指纹缓存）与**单头回归**（`tools/amalgamate.py` 合成 `single/xrt.h` 后重编译全部测试）双轨验证。本章讲清四件事：**清单结构**（schema/依赖/测试资产三类：tests/single_tests/examples）；**build.py 的 CLI 面**（suite 选择/test 过滤/jobs 并行/cflag 注入/manifest 叠加——扩展库共享同一工具链）；**双轨的意义**（模块化证明"裁剪可编译"，单头证明"合并不冲突"——每轨都是发布门禁）；**指纹缓存**（依赖闭包指纹变了才重编——增量构建的正确性根基）。

## 引入

C 库的构建系统常见三种病：**清单漂移**（CMakeLists 与实际源文件各说各话——加文件忘了改清单，或反之）；**伪裁剪**（声称可裁剪但从不验证裁剪组合能编译——特性宏组合爆炸从未测试）；**单头失同步**（单头文件手维护，模块改了忘了合并）。XRT 的答案：**一份 JSON 清单做全部事实源**——build（编译什么）、amalgamate（合并什么）、package（打包什么）、measure（测什么）全部从同一份 modules.json 出发。"清单驱动一切"消灭了漂移的可能：源文件不在清单里就不参与编译，测试不在清单里就不进回归。

双轨的必要性展开说：**模块化轨**按你选的 suite（如 `core,queue`）取依赖闭包、只编译那些源——验证"这个裁剪组合能编能跑"；**单头轨**先把全部所选模块合并成一个 `single/xrt.h`（amalgamate 按拓扑序拼接、去重 include、内联本地头），再用 `XRT_IMPLEMENTATION` 单翻译单元编译同一批测试——验证"合并后的单头没有符号冲突/宏泄漏"。每条 PR 两轨都绿才可合并——这就是 SPEC 里"check + 被嵌入示例编译进入 CI"的实现面。

## 概念

### 清单：模块的声明卡片

```diagram flow
- modules.json（schema 1）：每个模块一张卡
  —— name / state / feature（特性宏）
  —— depends（依赖模块表——闭包计算的边）
  —— public_headers / internal_headers / sources
  —— tests / single_tests / examples（三类测试资产）
  —— benchmarks / docs（基准与文档归属）
- 扩展库叠加：extlibs/*/config/modules.json 经 --manifest 参数叠加
  —— xhttp/xws/xssh/xmail/xruntime 共享同一工具链
```

清单同时是**文档索引**（docs 字段）、**基准归属**（benchmarks 字段——queue 模块的卡上挂着 dev/bench/queue 的全部脚本与报告）——"事实源"的完整含义：编译、测试、打包、度量、文档清单，五面一卡。

### build.py 的 CLI 面

| 参数 | 用途 |
| --- | --- |
| `--suite core,queue` | 模块名/逗号组合/all——依赖闭包自动展开 |
| `--test test_queue_oom` | 只跑一个模块化测试 |
| `--start-test`/`--start-single-test` | 从指定测试**继续**回归（断点续跑——修一处跑剩余） |
| `--jobs 8` | 单头测试并行编译任务数（测试仍按清单序运行——确定性） |
| `--no-single`/`--no-examples` | 跳轨（只跑模块化/只跑测试门禁） |
| `--cflag`/`--ldflag` | 注入编译/链接参数（交叉场景/ sanitizer） |
| `--compiler tcc`/`--arch x86` | 编译器与架构选择（TinyCC 32 位也是门禁组合） |
| `--manifest path` | 叠加扩展库清单 |
| `--rebuild` | 忽略指纹强制重编 |

### 双轨编译与指纹缓存

**模块化轨**：suite→闭包→按模块编译对象（`-DXRT_FEATURE_*` 按清单 feature 字段生成）→链接→按清单序跑 tests；examples 逐个编译运行（教程嵌入的示例在此验证——SPEC 门禁的"被嵌入示例编译"）。**单头轨**：amalgamate 产 `single/xrt.h`（含声明版 `xrt_decl.h`）→single_tests 以 `#define XRT_IMPLEMENTATION` + include 单头编译→运行。**指纹缓存**：对象按"依赖闭包指纹"缓存——闭包内任何源/头变了指纹变、才重编；`--rebuild` 显式作废。**架构矩阵**：native/x86/x64 × gcc/tcc/cl——发布门禁覆盖 32 位 TinyCC（资源受限目标的代表）。

### 扩展库共享工具链

第 109–128 章的五个扩展库各自有 `config/modules.json`——`--manifest extlibs/xhttp/config/modules.json` 叠加后，同一 build.py 编译测试扩展库闭包（含"扩展所需的核心裁剪闭包"）。xruntime 的 README 展示完整门禁序列：build 回归、amalgamate 一致性检查（`--check`）、发布成熟度检查、package 验证——**库与扩展库一个工具链、一套纪律**。

## 示例

### 第一个完整程序：清单声明的示例

下面的程序来自 `examples/core/version_limits`——清单 examples 字段的一员（build 双轨都会编译它）：

```embed path="examples/core/version_limits/main.c" title="examples/core/version_limits/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/version_limits/main.c -lws2_32 -liphlpapi
version=2.0.0-dev
limits: depth=128 entries=100000 input=268435456
```

**刚才发生了什么。** ① 这个文件出现在 core 模块清单的 examples 数组里——`python tools/build.py --suite core` 会编译并运行它；第 3 章以来的教程嵌入示例全部同理（**教程代码就是门禁代码**——SPEC 红线的机器执行处）。② `xrtVersion()` 与 `xrtResourceLimitsInit` 是 core 模块公共头声明的 API——清单的 public_headers 字段与 docs/api 契约卡一一对应。③ 资源边界默认值（深度 128/条目 10 万/输入 256 MiB）是全部解析器的公共防线——第 132 章测试体系会回来用这些边界做 DoS 测试。

### 第二个完整程序：分配器装配示例

第二个程序来自 `examples/core/allocator_tour`——自定义分配器的接入验证：

```embed path="examples/core/allocator_tour/main.c" title="examples/core/allocator_tour/main.c"
```

```term
$ gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c examples/core/allocator_tour/main.c -lws2_32 -liphlpapi
allocator: swap -> at-family alloc/calloc/dup/realloc ok
allocator: locked after first alloc -> swap rejected ok
```

**刚才发生了什么。** ① `xrtSetAllocator` 换自定义分配器后走 at 族四入口（第 5 章的内存面在示例层的回归）。② **首分配后锁定**的语义验证：换过的分配器不能再换——这是嵌入宿主（第 134 章）的稳定性保证。③ 两行输出就是双轨编译的产物之一：清单驱动、门禁运行——你看到的每个教程示例都经过这条链。

## 契约

- **单一事实源**：modules.json 驱动 build/amalgamate/package/measure/doc——加源必先加清单。
- **三类资产**：tests（模块化）/single_tests（单头）/examples（教程与门禁）——各有轨。
- **闭包语义**：suite 展开为依赖闭包；编译/裁剪/打包都按闭包。
- **双轨门禁**：模块化证裁剪可编译、单头证合并不冲突——两轨都绿才可合。
- **确定性顺序**：测试按清单序运行（jobs 只并行编译不并行测试）。
- **指纹缓存**：对象按闭包指纹缓存；--rebuild 显式作废。
- **架构矩阵**：native/x86/x64 × gcc/tcc/cl——TinyCC 32 位是资源受限门禁。
- **断点续跑**：start-test/start-single-test 从断点继续——修一处跑剩余。
- **扩展叠加**：--manifest 叠加扩展清单——库与扩展一个工具链。
- **示例即门禁**：examples 双轨编译运行——教程嵌入代码零额外维护。

## 避坑

### 坑 1：加了源文件忘了加清单

症状：本地"编译通过"（IDE 全量编译），CI 模块化轨编进不完整闭包——链接失败或更糟（符号从别的模块漏进来）。

原因：IDE 不看清单；build.py 只编译清单内的文件。"本地过"与"门禁过"的差别就在清单。

```c bad
/* 新建 src/foo/bar.c 后直接提交——清单没有它 */
/* CI：模块化轨闭包不完整——链接失败 */
```

```c good
/* modules.json 的 foo 模块 sources 数组同步加入 */
/* PR diff 里清单条目与源文件同现——评审可见一致性 */
```

### 坑 2：只跑模块化轨就合并

症状：单头轨爆了——两个模块的内部符号/宏在合并后冲突，用户拿到的 single/xrt.h 编不过。

原因：模块化编译隔离了模块间的私有名冲突；只有单头合并能把它们暴露。两轨缺一不可。

```c bad
python tools/build.py --suite my_module --no-single && merge
/* 跳过单头轨合并——符号冲突到用户手里才爆 */
```

```c good
/* PR 合并前：双轨全绿——--no-single 只用于开发中快速迭代 */
python tools/build.py --suite my_module   /* 含单头轨 */
```

### 坑 3：--jobs 当成测试并行

症状：以为 --jobs 8 会并行跑测试——测试间有顺序依赖（共享端口/临时文件）时偶发互扰。

原因：--jobs 只并行**编译**任务；测试严格按清单序运行（确定性优先——失败可复现是门禁的价值）。

```c bad
/* 以为 --jobs 8 并行跑测试——测试共享资源偶发互扰 */
/* 事实：--jobs 只并行编译；测试严格按清单序 */
```

```c good
/* 想快：跑子集而不是并行 */
python tools/build.py --suite my_module --test test_queue_oom
/* 断点续跑：--start-test 从失败处继续 */
```

## 练习

### 基础：双轨跑通 core 套件

`python tools/build.py --suite core`（无 --no-single）——观察两轨输出。验收标准：模块化与单头测试全绿；examples 双轨各编译一次；能指出指纹缓存生效（二次运行跳过已编对象）。

### 进阶：裁剪组合验证

选 `--suite core,queue,queue_spsc` 编译并跑全部测试；再用 `--no-single` 对比时间。验收标准：闭包内不含未选模块的源；裁剪组合测试全绿。

### 挑战：扩展库门禁复现

按 xruntime README 的门禁序列逐条运行（build 回归/amalgamate --check/package --verify）。验收标准：理解每条命令验证什么；amalgamate --check 的"生成一致性"含义能用自己的话说明（重新生成与提交的 single 头零差异）。

## 速查

| 知识点 | 速查 |
| --- | --- |
| 事实源 | config/modules.json——编译/测试/打包/度量/文档五面一卡 |
| 三类资产 | tests（模块化）/single_tests（单头）/examples（教程+门禁） |
| 双轨 | 模块化证裁剪、单头证合并——两轨全绿才可合 |
| 指纹缓存 | 对象按闭包指纹；--rebuild 作废 |
| 确定性 | 测试按清单序；jobs 只并行编译 |
| 断点续跑 | start-test/start-single-test |
| 架构矩阵 | native/x86/x64 × gcc/tcc/cl——TinyCC 32 位是门禁 |
| 扩展叠加 | --manifest 叠加——五扩展库一工具链 |
| 示例即门禁 | examples 双轨编译——教程代码零第二实现 |
| cflag/ldflag | 注入编译链接参数（sanitizer/交叉） |
