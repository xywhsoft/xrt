# Regex 性能基准

`bench_regex.c` 分开测量四条路径：

- 编译同一命名捕获表达式并释放。
- 复用 matcher 遍历四个匹配及其捕获。
- 复用 set matcher 对四条规则做一次多模式分类。
- 复用 `xstrbuf` 完成四次命名/数字捕获替换。

基准不会把编译和执行混在同一数字里。matcher、set matcher 和输出构建器都在计时区外创建，匹配与替换段只测标准库推荐的高负载复用方式。

## 统一性能工具

```powershell
python tools/measure_performance.py `
	--config config/performance_profiles.json `
	--manifest config/modules.json `
	--profiles regex --smoke
```

绝对速率只在相同机器、编译器、架构、输入和迭代次数下比较。修改 BBRE 程序、执行上下文、捕获布局、集合合并、模板解析或字符串增长策略后，应重新记录四条路径。
