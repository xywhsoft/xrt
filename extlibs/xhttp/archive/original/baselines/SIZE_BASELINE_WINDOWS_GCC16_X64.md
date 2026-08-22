# XRT Windows GCC 16 x64 体积基线

日期：2026-08-09

本报告是 `dev/bench/size/SIZE_BASELINE_WINDOWS_GCC16_X64.json` 的人类可读快照。JSON
文件是自动比较的唯一权威输入；本报告用于评审环境、测量口径和各功能闭包的实际代价。

## 环境

| 项目 | 值 |
| --- | --- |
| 报告 schema | 2 |
| 平台 | Windows `win32` / `amd64` |
| 目标架构 | x64 |
| 编译器 | GCC 16.1.0 |
| 节区工具 | GNU Binutils `size` 2.47.20260726 |
| 优化 | `-O2` |
| strip | 否 |
| 源码提交 | `51d3492b3868440acb46595bb9ef2682bd30bd54` |
| 工作树 | 有未提交修改 |
| 源码指纹 | `fdc4c8f8d36e81e8f0ee48be727c922b8653718a6f28c234bade47d6efd49af1` |

当前仓库正处于整体重构期，因此这份数据是当前工作树的开发基线，不是最终发布提交的
不可变证明。生成发布候选时必须在干净工作树上重新生成，并由 JSON 中的
`source_revision`、`source_dirty` 和 `source_fingerprint` 证明源码身份。

## 测量结果

单位均为字节。`text`、`data` 和 `bss` 来自 GNU `size`；`file` 是产物实际文件大小。
静态库的节区值是全部成员之和，不是归档索引本身的大小。

| 功能闭包 | 产物 | text | data | bss | file |
| --- | --- | ---: | ---: | ---: | ---: |
| core | 单头对象 | 108116 | 992 | 2624 | 141885 |
| core | 静态库 | 10728 | 976 | 2672 | 26438 |
| core | 动态库 | 20097 | 1152 | 2960 | 59845 |
| console | 单头对象 | 111052 | 992 | 2752 | 146847 |
| console | 静态库 | 13196 | 976 | 2800 | 32032 |
| console | 动态库 | 23014 | 1152 | 3088 | 64784 |
| foundation | 单头对象 | 215184 | 992 | 2688 | 288849 |
| foundation | 静态库 | 112628 | 976 | 2736 | 190236 |
| foundation | 动态库 | 157100 | 1232 | 5600 | 232315 |
| concurrency | 单头对象 | 191556 | 1000 | 2688 | 267602 |
| concurrency | 静态库 | 83832 | 1032 | 2768 | 182218 |
| concurrency | 动态库 | 104928 | 1208 | 3056 | 179394 |
| network | 单头对象 | 395044 | 1000 | 2688 | 528809 |
| network | 静态库 | 245996 | 1000 | 2752 | 448672 |
| network | 动态库 | 271190 | 1176 | 3024 | 376648 |
| http_websocket | 单头对象 | 764640 | 1000 | 2720 | 1013547 |
| http_websocket | 静态库 | 588124 | 1000 | 2784 | 1020902 |
| http_websocket | 动态库 | 657281 | 1256 | 5632 | 839885 |
| all | 单头对象 | 3357020 | 1184 | 32640 | 4386332 |
| all | 静态库 | 2964452 | 1192 | 32832 | 5133800 |
| all | 动态库 | 3183835 | 2008 | 35776 | 3905336 |

## 回归阈值

- `text`：最多增长 10%。
- `data`、`bss`：最多增长 20%。
- 产物文件：最多增长 15%。

阈值只负责阻止未解释的大幅增长。任何增长仍需要对应到功能或实现变动，不能通过放宽
阈值或直接覆盖基线绕过分析。

## 复现

生成当前报告：

```powershell
python tools\measure_size.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --report out\size\gnu\x64\size-report.json
```

与固定基线比较：

```powershell
python tools\measure_size.py --compiler E:\software\w64devkit\bin\gcc.exe --arch x64 --baseline dev\bench\size\SIZE_BASELINE_WINDOWS_GCC16_X64.json --check
```

比较要求平台、架构、编译器族与版本、`size` 版本、优化和 strip 策略完全一致。不同环境
必须建立独立基线，不能用本报告的绝对值直接判定回归。
