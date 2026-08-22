# XSON 性能基准

`bench_xson.c` 使用同一份包含 bytes、time、int-map、set、非有限浮点和 Unicode
转义的 XSON，独立测量：

- 严格解析并构造 `xvalue` DOM。
- `xrtXsonVisit` 直接事件访问，不构造 DOM。
- 把已构造 DOM 紧凑序列化为内存文本。

三段路径分开计时。读取路径包含 Base64 解码和时间解析，写出路径包含流式 Base64
编码和 UTC 时间规范化。基准逐次验证返回值并输出校验和。

## Windows GCC

```powershell
E:\software\w64devkit\bin\gcc.exe -O3 -std=c11 -Wall -Wextra -Werror `
	-I include dev\bench\xson\bench_xson.c -o out\bench_xson.exe
out\bench_xson.exe 100000
```

## TinyCC

```powershell
E:\software\tcc\tcc.exe -O2 -Wall -m32 -I include `
	dev\bench\xson\bench_xson.c -o out\bench_xson_tcc.exe
out\bench_xson_tcc.exe 100000
```

绝对速率只在相同机器、编译器、架构、输入和参数下比较。修改共享 reader/writer、
Base64、时间文本、Value 容器或 Buffer 增长策略后，应重新记录基线。
