# JSON 性能基准

`bench_json.c` 使用同一份包含对象、数组、数字和 Unicode 转义的 JSON，独立测量：

- 严格解析并构造 `xvalue` DOM。
- `xrtJsonVisit` 直接事件访问，不构造 DOM。
- 把已构造 DOM 紧凑序列化为内存文本。

三段热路径分开计时，便于判断回归来自扫描/解码、Value 容器还是输出扩容。基准逐次校验返回值并输出校验和。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\json\bench_json.c -o out\bench_json.exe
out\bench_json.exe 100000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\json\bench_json.c -o out\bench_json_tcc.exe
out\bench_json_tcc.exe 100000
```

绝对速率只在相同机器、编译器、架构和参数下比较。修改 JSON 扫描、字符串解码、Value 容器、数字转换、增量写入器或 Buffer 增长策略后，应重新记录基线。
