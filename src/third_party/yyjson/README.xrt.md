# yyjson 数值内核

- 上游：<https://github.com/ibireme/yyjson>
- 基线：`0.12.0`，提交 `9365ddc7061033df656578bf86040048b5b5531a`
- 许可证：MIT
- XRT 使用位置：`src/text/number_float_core.c`

XRT 没有引入 yyjson 的 JSON DOM、读取器或写出器，只精炼复用了以下经过验证的数值资产：

- Eisel-Lemire 风格的 double 快速读取路径；
- 固定 64 个字的 BigInt 精确舍入后备路径；
- Schubfach 最短往返写出路径；
- 读取与写出共用的 128 位十进制幂表。

XRT 自行负责显式长度输入、语法、数字分隔符、错误、容量、分配和公开 API。这样可以保留成熟数值算法，同时避免把 JSON 组件和其内部类型耦合到数值基础层。
