# Math API

Math 提供动态宿主和跨平台 C 代码需要的稳定数学补充层：整数饱和运算、安全除法与比较；只依赖 core，无随机数或可变全局配置。

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

## API

### `xrtMathMin`

返回两个浮点数中较小者；NaN 参与比较返回另一操作数之外的非确定侧（实现按 IEEE minNum 语义）。

```c
double xrtMathMin(double fLeft, double fRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fLeft` | 输入 | — | 左值 |
| `fRight` | 输入 | — | 右值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 较小者 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 极值](../../examples/math/tour/main.c) · 观察

```c
	if ( (xrtMathMin(2.0, 3.0) != 2.0) ||
		(xrtMathMin(3.0, 2.0) != 2.0) ||
		(xrtMathMax(2.0, 3.0) != 3.0) ||
		(xrtMathMax(3.0, 2.0) != 3.0) ||
		(xrtMathMin(-1.0, 1.0) != -1.0) ) {
```


### `xrtMathMax`

返回两个浮点数中较大者。

```c
double xrtMathMax(double fLeft, double fRight);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fLeft` | 输入 | — | 左值 |
| `fRight` | 输入 | — | 右值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 较大者 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 极值](../../examples/math/tour/main.c) · 观察

```c
		(xrtMathMax(2.0, 3.0) != 3.0) ||
```


### `xrtMathClamp`

把值限制在 `[fMin, fMax]` 区间内。

```c
double xrtMathClamp(double fValue, double fMin, double fMax);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 待限制值 |
| `fMin` | 输入 | `<= fMax` | 下界 |
| `fMax` | 输入 | — | 上界 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 限制后的值 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/helpers · 极值](../../examples/math/helpers/main.c) · 观察

```c
	printf("clamp: %.1f\n", xrtMathClamp(12.0, 0.0, 10.0));  /* 超上界→10 */
```


### `xrtMathSign`

返回浮点数的符号：负数为 -1、零为 0、正数为 +1。

```c
int xrtMathSign(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `-1/0/+1` | 符号 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 基础](../../examples/math/tour/main.c) · 观察

```c
	if ( (xrtMathSign(-5.0) != -1) ||
		(xrtMathSign(5.0) != 1) ||
		(xrtMathSign(0.0) != 0) ) {
```


### `xrtMathTrunc`

向零截断小数部分。

```c
double xrtMathTrunc(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 截断后的整值 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 基础](../../examples/math/tour/main.c) · 观察

```c
	if ( (xrtMathTrunc(2.7) != 2.0) ||
		(xrtMathTrunc(-2.7) != -2.0) ) {
```


### `xrtMathFract`

返回小数部分 `x - trunc(x)`，保持符号。

```c
double xrtMathFract(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 带符号小数部分 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/helpers · 基础](../../examples/math/helpers/main.c) · 观察

```c
	printf("fract: %.2f\n", xrtMathFract(-1.25));           /* -1.25→0.75 */
```


### `xrtMathMod`

带符号取模（结果符号随被除数）；除数为零返回 NaN。

```c
double xrtMathMod(double fValue, double fDivisor);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 被除数 |
| `fDivisor` | 输入 | 非零 | 除数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | `fValue - divisor * trunc(fValue/divisor)` | — |

#### 错误

- 无 — 纯函数（除零返回 NaN）

#### 范例

[math/tour · 基础](../../examples/math/tour/main.c) · 观察

```c
	if ( !exampleNear(xrtMathMod(7.0, 3.0), 1.0) ||
		!exampleNear(xrtMathMod(-7.0, 3.0), -1.0) ) {
```


### `xrtMathRad`

角度转弧度。

```c
double xrtMathRad(double fDegrees);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fDegrees` | 输入 | — | 角度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 弧度 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 基础](../../examples/math/tour/main.c) · 观察

```c
	if ( !exampleNear(xrtMathRad(180.0), 3.14159265358979) ) {
```


### `xrtMathDeg`

弧度转角度。

```c
double xrtMathDeg(double fRadians);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fRadians` | 输入 | — | 弧度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 角度 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/helpers · 基础](../../examples/math/helpers/main.c) · 观察

```c
	printf("angle: %.1f\n", xrtMathDeg(XRT_PI));            /* π rad→180° */
```


### `xrtMathIsNaN`

判断是否 NaN。

```c
bool xrtMathIsNaN(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是 NaN | — |
| `false` | 非 NaN | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[math/tour · 分类](../../examples/math/tour/main.c) · 观察

```c
		if ( !xrtMathIsNaN(fNaN) ||
			xrtMathIsNaN(1.0) ||
			xrtMathIsNaN(fInf) ||
			!xrtMathIsInf(fInf) ||
			xrtMathIsInf(1.0) ||
			xrtMathIsInf(fNaN) ||
			!xrtMathIsFinite(1.5) ||
			xrtMathIsFinite(fNaN) ||
			xrtMathIsFinite(fInf) ) {
```


### `xrtMathIsInf`

判断是否无穷大（正或负）。

```c
bool xrtMathIsInf(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 是无穷 | — |
| `false` | 有限或 NaN | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[math/tour · 分类](../../examples/math/tour/main.c) · 观察

```c
			!xrtMathIsInf(fInf) ||
```


### `xrtMathIsFinite`

判断是否有限值（非 NaN 且非无穷）。

```c
bool xrtMathIsFinite(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 有限 | — |
| `false` | NaN 或无穷 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[math/tour · 分类](../../examples/math/tour/main.c) · 观察

```c
			!xrtMathIsFinite(1.5) ||
```


### `xrtMathLog2`

以 2 为底的对数；非正值返回 NaN。

```c
double xrtMathLog2(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | `> 0` | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | log2 | — |

#### 错误

- 无 — 域外返回 NaN

#### 范例

[math/tour · 函数](../../examples/math/tour/main.c) · 观察

```c
	if ( !exampleNear(xrtMathLog2(8.0), 3.0) ||
		!exampleNear(xrtMathLog2(1.0), 0.0) ||
		!exampleNear(xrtMathExp2(10.0), 1024.0) ||
		!exampleNear(xrtMathLog1p(0.0), 0.0) ||
		!exampleNear(xrtMathExpm1(0.0), 0.0) ||
		!exampleNear(xrtMathLog1p(1.0), 0.693147180559945) ||
		!exampleNear(xrtMathExpm1(1.0), 1.718281828459045) ) {
```


### `xrtMathExp2`

2 的幂；溢出返回无穷。

```c
double xrtMathExp2(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 指数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 2^x | — |

#### 错误

- 无 — 溢出返回无穷

#### 范例

[math/tour · 函数](../../examples/math/tour/main.c) · 观察

```c
		!exampleNear(xrtMathExp2(10.0), 1024.0) ||
```


### `xrtMathLog1p`

计算 `ln(1 + x)`，x 接近零时保持精度。

```c
double xrtMathLog1p(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | `> -1` | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | ln(1+x) | — |

#### 错误

- 无 — 域外返回 NaN

#### 范例

[math/tour · 函数](../../examples/math/tour/main.c) · 观察

```c
		!exampleNear(xrtMathLog1p(0.0), 0.0) ||
```


### `xrtMathExpm1`

计算 `e^x - 1`，x 接近零时保持精度。

```c
double xrtMathExpm1(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | e^x - 1 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 函数](../../examples/math/tour/main.c) · 观察

```c
		!exampleNear(xrtMathExpm1(0.0), 0.0) ||
```


### `xrtMathCbrt`

立方根；负数返回负根（与 `pow(x, 1/3)` 不同）。

```c
double xrtMathCbrt(double fValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fValue` | 输入 | — | 输入值 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 带符号立方根 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/tour · 函数](../../examples/math/tour/main.c) · 观察

```c
	if ( !exampleNear(xrtMathCbrt(27.0), 3.0) ||
		!exampleNear(xrtMathCbrt(-27.0), -3.0) ||
		!exampleNear(xrtMathCbrt(0.0), 0.0) ) {
```


### `xrtMathHypot`

计算 `sqrt(x² + y²)`，中间过程不上溢。

```c
double xrtMathHypot(double fX, double fY);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fX` | 输入 | — | X |
| `fY` | 输入 | — | Y |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 数值 | 欧氏长度 | — |

#### 错误

- 无 — 纯函数

#### 范例

[math/helpers · 函数](../../examples/math/helpers/main.c) · 观察

```c
	printf("hypot: %.1f\n", xrtMathHypot(3.0, 4.0));        /* 勾股 3-4-5 */
```


### `xrtMathNear`

使用显式绝对与相对容差比较两个浮点数：`|L-R| <= max(abs, rel * max(|L|,|R|))`。

```c
bool xrtMathNear(double fLeft, double fRight,
	double fAbsoluteTolerance, double fRelativeTolerance);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `fLeft` | 输入 | — | 左值 |
| `fRight` | 输入 | — | 右值 |
| `fAbsoluteTolerance` | 输入 | `>= 0` | 绝对容差 |
| `fRelativeTolerance` | 输入 | `>= 0` | 相对容差 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 在容差内相等 | — |
| `false` | 超出容差 | 纯谓词 |

#### 错误

- 无 — 纯谓词

#### 范例

[math/tour · 比较](../../examples/math/tour/main.c) · 观察

```c
	return xrtMathNear(fLeft, fRight, 1e-9, 0.0);
```


### `xrtMathIntNear`

使用无符号绝对差容差比较两个 int64，计算过程不会溢出。

```c
bool xrtMathIntNear(int64 iLeft, int64 iRight, uint64 iTolerance);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLeft` | 输入 | — | 左值 |
| `iRight` | 输入 | — | 右值 |
| `iTolerance` | 输入 | — | 容差 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | `‖L-R‖ <= iTolerance`（含 INT64_MIN 边界） | — |
| `false` | 超出容差 | 纯谓词 |

#### 错误

- 无 — 纯谓词（不依赖浮点）

#### 范例

[math/near · 比较](../../examples/math/near/main.c) · 观察

```c
		xrtMathIntNear(1000, 1003, 5) ? "near" : "different");
```

## 模块契约：错误

数学函数为纯计算：合法输入不设置错误；非法参数（如近似比较的容差为负、量化步长为零）设置 `XERR_ARGUMENT`，经 `xrtGetError()` 取得。

## 模块契约：所有权

全部函数按值传入传出，无动态所有权。
