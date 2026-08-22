# Value Container 性能基准

`bench_value_container.c` 固化四类容器热路径：

- 预留容量后的 Array 标量追加。
- 二进制键 Object 的插入和查询。
- 100,000 项 Array 的首次 COW 分离吞吐。
- 64 层共享 DAG 的环检测，验证复杂度按唯一 backing 数量增长。

默认参数为 1,000,000 次 Array 追加、200,000 次 Object 插入与查询、100 次
COW 分离和 1,000 次 DAG 检查。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\value_container\bench_value_container.c `
	-o out\bench_value_container.exe
out\bench_value_container.exe 1000000 200000 100 1000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\value_container\bench_value_container.c `
	-o out\bench_value_container_tcc.exe
out\bench_value_container_tcc.exe 1000000 200000 100 1000
```

程序逐项验证操作结果并输出防消除校验和。同一发布环境应固定编译器、架构、参数
和 CPU 电源状态后比较；跨机器绝对速率只作参考。

当前环境基线见 `VALUE_CONTAINER_BENCH_20260729.md`。
