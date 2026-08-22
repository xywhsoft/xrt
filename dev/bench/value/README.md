# Value 性能基准

`bench_value.c` 固化两个核心热路径：

- 创建、精确读取并释放 1,000,000 个整数 Value。
- 对等价整数/浮点 Value 执行 10,000,000 次 Hash 和 ScalarEqual。

第一项同时覆盖 XRT 小对象尺寸类和原子引用释放；第二项覆盖公开参数、输出别名门禁、
跨数值类型规范哈希与相等，不包含容器或图遍历。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\value\bench_value.c -o out\bench_value.exe
out\bench_value.exe 1000000 10000000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\value\bench_value.c -o out\bench_value_tcc.exe
out\bench_value_tcc.exe 1000000 10000000
```

程序逐次验证 Getter、Hash 和 ScalarEqual，并输出防消除校验和。不同机器的绝对速率
不可直接比较；同一发布环境应使用相同编译器、架构和参数与稳定版本比较。

当前环境基线见 `VALUE_BENCH_20260729.md`。
