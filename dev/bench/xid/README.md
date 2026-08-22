# XID benchmark

基准分别测量单个安全生成、256 项批量安全生成、固定缓冲写入、严格解析和分配式格式化。生成指标包含系统安全随机源与当前时间读取，批量指标以生成的 ID 数量而不是批次数量计算。

```powershell
$env:PATH = 'E:\software\w64devkit\bin;' + $env:PATH
gcc -O3 -std=c11 -Wall -Wextra -Werror -m64 dev/bench/xid/bench_xid.c -o out/xid_bench.exe
out/xid_bench.exe 200000
```

`checksum` 只用于阻止编译器删除热循环，不是性能结果。比较不同提交时应使用相同机器、编译器、优化级别和迭代次数。
