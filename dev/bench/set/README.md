# Set 性能基准

`bench_set.c` 固化百万个 `uint64` 元素插入和千万次命中查询，用于比较 Set
条目分配、稳定哈希、桶链与容量策略的变化。基准在计时前预留容量，因此插入结果主要
反映每元素条目分配、复制和哈希成本，不包含桶数组扩容。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\set\bench_set.c -o out\bench_set.exe
out\bench_set.exe 1000000 10000000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\set\bench_set.c -o out\bench_set_tcc.exe
out\bench_set_tcc.exe 1000000 10000000
```

参数依次为插入数量和查询数量。程序验证最终元素数、每次命中的规范元素和最终校验和；
任何功能错误返回非零。绝对速率只用于同一机器、编译器、架构和参数下的版本比较，
不能作为跨环境统一阈值。

当前环境基线见 `SET_BENCH_20260728.md`。
