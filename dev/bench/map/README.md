# Map 性能基准

`bench_map.c` 迁移旧版 `Dict` 的百万插入、千万命中查询资产，用于发布前比较
Map 热路径、哈希实现、桶扩容和堆分配器变更。基准默认预留容量，使插入结果主要反映
条目分配、键复制与哈希成本；查询仍包含旧版基线使用的十进制键构造成本。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\map\bench_map.c -o out\bench_map.exe
out\bench_map.exe 1000000 10000000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\map\bench_map.c -o out\bench_map_tcc.exe
out\bench_map_tcc.exe 1000000 10000000
```

参数依次为插入数量和查询数量。程序会验证键数、每次查询的值与最终校验和；
任何功能错误返回非零。不同机器的绝对速率不能直接作为统一阈值，同一发布环境应保留
编译器、架构、参数和结果，再与上一个稳定版本比较。

当前基线见 `MAP_BENCH_20260728.md`。
