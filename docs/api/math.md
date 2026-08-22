# Math API

## 设计契约

`XRT_FEATURE_MATH` 提供动态宿主和跨平台 C 代码需要的稳定数学补充层。它只依赖 core，不依赖随机数、线程或可变全局配置。

XRT 不重复包装全部 C 数学库。`sin`、`cos`、`sqrt` 等已有稳定 C 契约的函数仍直接使用 `<math.h>`；本模块保留的是常用组合操作、旧 CRT 兼容实现、统一特殊值语义和显式容差比较。

## 常量

```c
#define XRT_PI  3.14159265358979323846264338327950288
#define XRT_TAU (2.0 * XRT_PI)
#define XRT_E   2.71828182845904523536028747135266250
```

常量值是固定 API 契约，应用不能通过预处理器覆盖。无穷与 NaN 使用标准 `<math.h>` 的 `HUGE_VAL`、`INFINITY` 和 `NAN`，不再创建重复别名。

## 范围与符号

```c
double xrtMathMin(double fLeft, double fRight);
double xrtMathMax(double fLeft, double fRight);
double xrtMathClamp(double fValue, double fMin, double fMax);
int xrtMathSign(double fValue);
```

`xrtMathMin` 与 `xrtMathMax` 在任一参数为 NaN 时传播 NaN，而不是像 C `fmin/fmax` 那样忽略单个 NaN。两个参数都是零时，min 选择负零，max 选择正零。

`xrtMathClamp` 使用闭区间，并保留旧版便捷规则：边界写反时自动交换。值或边界为 NaN 时按 `value -> min -> max` 顺序传播第一个 NaN。`xrtMathSign` 对负数、正数返回 -1、1，对正负零和 NaN 返回 0。

## 数值转换

```c
double xrtMathTrunc(double fValue);
double xrtMathFract(double fValue);
double xrtMathMod(double fValue, double fDivisor);
double xrtMathRad(double fDegrees);
double xrtMathDeg(double fRadians);
```

- `xrtMathTrunc` 向零截断，并保留零、NaN、无穷。
- `xrtMathFract` 返回 `value - floor(value)`，因此 `xrtMathFract(-1.25) == 0.75`。
- `xrtMathMod` 保持 C `fmod` 的余数符号与特殊值规则。
- `xrtMathRad` 与 `xrtMathDeg` 使用固定 `XRT_PI` 转换角度。

## 特殊值

```c
bool xrtMathIsNaN(double fValue);
bool xrtMathIsInf(double fValue);
bool xrtMathIsFinite(double fValue);
```

三个函数提供跨 CRT 一致的布尔结果。`xrtMathIsInf` 不通过 `inf - inf` 判断，因此不会为了分类主动执行无效的无穷运算。

## 兼容数学函数

```c
double xrtMathLog2(double fValue);
double xrtMathExp2(double fValue);
double xrtMathLog1p(double fValue);
double xrtMathExpm1(double fValue);
double xrtMathCbrt(double fValue);
double xrtMathHypot(double fX, double fY);
```

这些导出函数为动态宿主和缺少部分 C99 符号的旧 Windows CRT 提供统一入口：

- `xrtMathLog1p` 与 `xrtMathExpm1` 对接近零的输入使用短级数，避免先舍入掉微小差值，并保留输入负零的符号。
- `xrtMathCbrt` 保留负数符号，并在 `pow` 初值后执行牛顿修正。
- `xrtMathHypot` 先按绝对值缩放，降低直接计算 `x*x + y*y` 的上溢和下溢风险；任一参数为无穷时结果为正无穷，即使另一参数为 NaN。

函数的定义域、舍入、errno 和浮点异常继续遵循平台 C 数学环境。XRT 只固定上述组合语义，不伪造任意精度承诺。

## 显式容差比较

### `xrtMathNear`

```c
bool xrtMathNear(double fLeft, double fRight,
	double fAbsoluteTolerance, double fRelativeTolerance);
```

浮点近似比较同时接受绝对容差与相对容差，规则与 Python `math.isclose` 一致：

```text
abs(left - right) <= absoluteTolerance
或
abs(left - right) <= relativeTolerance * max(abs(left), abs(right))
```

绝对容差处理零附近，相对容差处理不同数量级的普通值。两个值按浮点规则完全相等时返回 true，因此同号无穷彼此相等；NaN 永远不相近，一个有限值与一个无穷也不相近。

容差必须非负且不能为 NaN。无效容差返回 false 并设置 `XERR_ARGUMENT`，即使被比较的两个值完全相等也不会忽略参数错误。

```c
bool bMeasured = xrtMathNear(100.0, 100.05, 0.0, 0.001);
bool bNearZero = xrtMathNear(0.0, 1e-10, 1e-9, 0.0);
```

### `xrtMathIntNear`

```c
bool xrtMathIntNear(int64 iLeft, int64 iRight, uint64 iTolerance);
```

整数比较只使用明确的无符号绝对差容差，不引入模糊“百分比整数”模式。计算覆盖从 `INT64_MIN` 到 `INT64_MAX` 的最大差 `UINT64_MAX`，不会执行有符号溢出。

## 线程与全局状态

所有数学函数都是无状态函数，可以并发调用。近似比较不再读取 `xCore` 的可变模式和容差，因此库、线程、协程和测试不会互相改变数学判断规则。

## 旧 API 决策

- 删除 `XRT_APPROX_DIFF`、`XRT_APPROX_PERCENT` 和 `xCore` 容差字段。
- `xrtIntApprox` 改为显式无溢出的 `xrtMathIntNear`。
- `xrtNumApprox` 改为同时表达绝对与相对容差的 `xrtMathNear`。
- 常量缩短为 `XRT_PI`、`XRT_TAU`、`XRT_E`；删除对标准无穷和 NaN 的重复定义。
- 保留旧版稳定的 fract、hypot 和小值级数思路，同时修复整数差值溢出、`INT64_MIN` 取绝对值溢出、隐藏全局竞态和特殊值含糊问题。

## 完整示例

- `examples/math/helpers/main.c`：范围、负数小数部分、角度和稳定 hypot。
- `examples/math/near/main.c`：绝对、相对和整数容差。
