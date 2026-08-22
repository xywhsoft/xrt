# Value Collection 性能基线 2026-07-29

## 环境

- 系统：Microsoft Windows 10 专业版
- CPU：AMD Ryzen 5 5600 6-Core Processor
- GCC：16.1.0，`-O3 -std=c11`，x64
- TinyCC：0.9.27，`-O2 -m32`
- 参数：每侧 10,000 项，100 轮批量操作，1,000 轮关系判断，
  1,000,000 轮共享 backing 恒等合并

## 结果

| 编译器 | Array Extend | Object Merge | Set Union | Set IsDisjoint | 共享 Map 恒等合并 |
| --- | ---: | ---: | ---: | ---: | ---: |
| GCC x64 | 19,025,296.034 items/s | 4,626,635.169 items/s | 13,850,367.554 items/s | 47,414,173.234 items/s | 113,372,257.809 ops/s |
| TinyCC `-m32` | 4,414,835.614 items/s | 959,565.355 items/s | 4,239,573.194 items/s | 17,957,161.037 items/s | 35,602,519.234 ops/s |

两次运行的校验和均为 `7001000`。

## 计量口径

- Array Extend 与 Object Merge 只计来源项数，不把准备阶段复制的目标项重复计入。
- Set Union 计结果集合总项数。
- Set IsDisjoint 使用不相交集合，因此每轮完整扫描较小一侧。
- 共享 Map 恒等合并使用两个独立 Value 外壳和同一 COW backing，验证
  Replace Merge 按 backing 身份走 `O(1)` 路径。

## 使用

本文件是同一机器、工具链、架构和参数下的回归起点。修改批量提交、
COW、Map/Set、引用计数、回调保护或内存池后应重新记录；跨机器绝对速率
只作为数量级参考。
