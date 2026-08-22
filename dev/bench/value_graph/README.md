# Value Graph 性能基准

`bench_value_graph.c` 固化 Value 图层的四条关键路径：

- 1,000 个独立 Object 节点的身份保留深克隆。
- 两个独立大图的结构相等。
- 每层重复引用同一子节点的共享 DAG 相等。
- 不可变标量的无分配深克隆快路。

默认执行 100 轮大图深克隆、1,000 轮大图比较、100,000 轮 24 层
共享 DAG 比较和 1,000,000 轮标量深克隆。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\value_graph\bench_value_graph.c `
	-o out\bench_value_graph.exe
out\bench_value_graph.exe 1000 100 1000 100000 1000000 24
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\value_graph\bench_value_graph.c `
	-o out\bench_value_graph_tcc.exe
out\bench_value_graph_tcc.exe 1000 100 1000 100000 1000000 24
```

程序在计时前验证两组图相等，并逐轮检查结果和输出防消除校验和。
只能在相同机器、工具链、架构、参数和电源状态下比较绝对速率。

当前环境基线见 `VALUE_GRAPH_BENCH_20260729.md`。
