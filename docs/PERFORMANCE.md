# 性能与基准

性能数据只对测试时的源码版本、平台、编译器、配置和负载有效。XRT 的目标是在不牺牲错误处理、取消、背压和资源回收语义的前提下提供可预测的性能；单次微基准或跨平台数字不能作为部署承诺。

## 运行基准

性能配置由 `config/performance_profiles.json` 管理，使用 `tools/measure_performance.py` 执行。先使用 smoke 模式确认本机环境与入口正确，再在固定机器和工具链上与已登记基线比较：

```text
python tools/measure_performance.py --compiler gcc --arch x64 --smoke
python tools/measure_performance.py --compiler <compiler> --arch x64 --baseline <baseline.json> --check
```

运行结果应记录提交版本、操作系统、CPU、编译器与优化参数、输入矩阵、原始样本和统计方法。基准输出放在构建目录中，可通过 `python tools/clean.py --apply` 删除；不要把遗留可执行文件或缓存当作性能证据。

## 解读结果

- 吞吐、平均延迟和尾延迟应分别比较；一个指标改善不能掩盖另一个指标的退化。
- 先检查输入是否相同、模块是否相同、是否启用同一后端，再比较数字。
- 网络、TLS、文件和调度结果受内核、驱动、负载与拓扑影响，必须在实际部署环境复测。
- 任何优化都必须保留 API 的错误、所有权、取消和关闭契约，并通过相应回归测试。

## 发布使用

正式发布使用固定环境的多轮样本和已登记阈值判断回归。交叉编译、仅能链接，或只运行一次 smoke，都只能证明构建路径可用，不能证明目标环境的性能。发布步骤和其他验证要求见[构建与发布](BUILD.md)及[发布支持](RELEASE_STATUS.md)。
