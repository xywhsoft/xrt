# Value Collection 性能基准

`bench_value_collection.c` 固化高级 Value 容器的五条热路径：

- Array 失败原子 Extend 的来源元素吞吐。
- Object 失败原子 Merge 的来源元素吞吐。
- 两个不相交 Set 的 Union 结果元素吞吐。
- `IsDisjoint` 扫描较小集合的元素吞吐。
- 两个 COW 外壳共享同一 Map backing 时，Replace Merge 恒等操作吞吐。

默认参数为每侧 10,000 项、100 轮批量操作、1,000 轮关系判断和
1,000,000 轮共享 backing 恒等合并。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\value_collection\bench_value_collection.c `
	-o out\bench_value_collection.exe
out\bench_value_collection.exe 10000 100 1000 1000000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\value_collection\bench_value_collection.c `
	-o out\bench_value_collection_tcc.exe
out\bench_value_collection_tcc.exe 10000 100 1000 1000000
```

程序逐轮验证结果并输出防消除校验和。只能在相同机器、工具链、架构、
参数和电源状态下比较绝对速率；跨机器结果只作为数量级参考。

当前环境基线见 `VALUE_COLLECTION_BENCH_20260729.md`。
