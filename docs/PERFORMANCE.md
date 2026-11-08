# 性能与基准

性能数据只对测试时的源码版本、平台、编译器、配置和负载有效。XRT 的目标是在不牺牲错误处理、取消、背压和资源回收语义的前提下提供可预测的性能；单次微基准或跨平台数字不能作为部署承诺。

## 运行基准

性能配置由 `config/performance_profiles.json` 管理，使用 `tools/measure_performance.py` 执行。先使用 smoke 模式确认本机环境与入口正确，再在固定机器和工具链上与已登记基线比较：

```text
python tools/measure_performance.py --compiler gcc --arch x64 --smoke
python tools/measure_performance.py --compiler <compiler> --arch x64 --baseline <baseline.json> --check
```

运行结果应记录提交版本、操作系统、CPU、编译器与优化参数、输入矩阵、原始样本和统计方法。基准输出放在构建目录中，可通过 `python tools/clean.py --apply` 删除；不要把遗留可执行文件或缓存当作性能证据。

当中位数为零而离散度非零，或有限比值计算溢出时，派生的 `relative_mad`、`relative_range`、`relative_central_range` 可以为正无穷。JSON 报告将这类比值明确编码为字符串 `"inf"`，原始样本和绝对统计保持不变；读取比值时使用 `float(value)`。工具在编码前仍以无限值执行原有噪声门禁，不将它改成零或跳过失败；原始样本中的 NaN/Infinity 仍被拒绝。

每项 benchmark 的可选 `inputs` 列表应登记影响工作负载的仓库文件，例如本地对端头文件、测试数据和驱动脚本。使用仓库相对路径；必须列全传递使用的夹具。工具将这些文件与 benchmark 源码一起计入 `benchmark_fingerprint`，并在报告中保留声明列表。文件缺失、目录、越出仓库的路径和重复声明会被拒绝。生产实现仍由 `source_fingerprint` 单独标识，便于比较实现变化。

修正工作负载或其输入声明后，旧指纹的基线应当拒绝比较；应按原验收规则重新测量，不能只更新旧报告的指纹。共享工作区中同时使用 Windows 和 WSL 时，应使用不同输出目录（例如 Windows `--arch native`、WSL `--arch x64`）或独立工作区，避免共用对象缓存。

## 解读结果

- 吞吐、平均延迟和尾延迟应分别比较；一个指标改善不能掩盖另一个指标的退化。
- 先检查输入是否相同、模块是否相同、是否启用同一后端，再比较数字。
- 网络、TLS、文件和调度结果受内核、驱动、负载与拓扑影响，必须在实际部署环境复测。
- 任何优化都必须保留 API 的错误、所有权、取消和关闭契约，并通过相应回归测试。

## 发布使用

正式发布使用固定环境的多轮样本和已登记阈值判断回归。交叉编译、仅能链接，或只运行一次 smoke，都只能证明构建路径可用，不能证明目标环境的性能。发布步骤和其他验证要求见[构建与发布](BUILD.md)及[发布支持](RELEASE_STATUS.md)。
