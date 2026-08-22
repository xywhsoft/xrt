# Regex 性能基线（2026-07-31）

## 环境

- 系统：Windows 10 专业版 10.0.19045，x64
- 处理器：AMD Ryzen 5 5600，6 核 12 线程
- GCC：16.1.0，`-O3 -std=c11 -Wall -Wextra -Werror`
- TinyCC：0.9.27 x86_64，`-O2 -Wall -m64`
- 迭代次数：每项 100,000 次
- 统计方式：连续运行三次，记录每项吞吐量与耗时的中位数

## 结果

| 路径 | GCC 吞吐量（ops/s） | GCC 耗时（ns） | TinyCC 吞吐量（ops/s） | TinyCC 耗时（ns） |
| --- | ---: | ---: | ---: | ---: |
| 编译 | 310,056.464 | 322,521,900 | 63,858.991 | 1,565,950,200 |
| 复用 matcher 遍历 | 172,562.643 | 579,499,700 | 21,468.733 | 4,657,936,800 |
| 复用 set matcher | 21,126,016.690 | 4,733,500 | 3,299,132.328 | 30,311,000 |
| 复用输出缓冲替换 | 152,240.508 | 656,855,400 | 20,091.086 | 4,977,331,700 |

每轮校验和均为 `5,300,000`。

## 构建与运行

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\regex\bench_regex.c -o out\bench_regex.exe
out\bench_regex.exe 100000

E:\software\tcc\tcc.exe -O2 -Wall -m64 -I include `
	dev\bench\regex\bench_regex.c -o out\bench_regex_tcc.exe
out\bench_regex_tcc.exe 100000
```

## 比较规则

只在相同处理器、系统、编译器版本、优化参数、输入和迭代次数下比较同一列。修改 BBRE、执行上下文、捕获布局、集合合并、替换模板解析或字符串缓冲增长策略后，应重新连续采样三次。吞吐量中位数下降超过 10% 时，需要复测并分析；不能用 GCC 与 TinyCC 的绝对结果相互判断回退。
