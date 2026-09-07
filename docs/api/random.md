# Random API

Random 提供四个层次：操作系统密码安全随机、可复现显式状态、线程便捷态（及其显式别名）与随机文本；非密码层绝不用于密钥材料。

## 类型与常量

### `xrandomerror`

操作系统安全随机源稳定错误代码。

```c
typedef enum xrandomerror {
	XRANDOM_ERROR_SYSTEM = 1
} xrandomerror;
```

| 值 | 语义 |
|---|---|
| `XRANDOM_ERROR_SYSTEM` | 系统随机源失败 |

## 设计契约

XRT 把伪随机数分成两个可独立裁剪的层次：

- `XRT_FEATURE_RANDOM_SECURE`：直接使用操作系统安全随机源，不依赖 PCG、密码算法或线程运行时。
- `XRT_FEATURE_RANDOM_SECURE_TEXT`：在系统安全随机源上生成无偏 ASCII 令牌，只依赖 `random_secure`。
- `XRT_FEATURE_RANDOM`：调用方持有的 PCG32 显式状态，不分配内存，不依赖线程、任务、时间或密码模块。
- `XRT_FEATURE_RANDOM_DEFAULT`：当前线程便捷状态，依赖 `XRT_FEATURE_RANDOM`，首次使用时自动播种，也允许显式播种以复现结果。
- `XRT_FEATURE_RANDOM_TEXT`：在显式状态上生成可复现 ASCII 文本，依赖 `random`。
- `XRT_FEATURE_RANDOM_TEXT_DEFAULT`：在当前线程状态上生成便捷随机文本，依赖 `random_text` 与 `random_default`。

显式状态是算法原语，也是测试、模拟、游戏、随机采样和可复现构建应优先使用的路径。便捷层只转发到同一套原语，不维护第二种随机算法。

这里的 PCG 是统计用途伪随机数，不是密码随机源。TLS、WebSocket、令牌、密码、密钥、nonce、UUID v4、临时资源名称和其他安全边界必须使用 `xrtSecureRandom` 或建立在其上的 Helper；不能使用 `xrtRng*` 或 `xrtRand*`。

## 系统安全随机

```c
typedef enum xrandomerror {
	XRANDOM_ERROR_SYSTEM = 1
} xrandomerror;

bool xrtSecureRandom(ptr pData, size_t iSize);
```

Windows 使用系统 CNG `BCryptGenRandom`，通过线程安全的一次性运行时解析保持单头文件无需额外导入库；成功后缓存系统入口。Linux 优先使用 `getrandom`，仅在旧内核不支持时回退 `/dev/urandom`，其他 POSIX 平台使用 `/dev/urandom`。旧版中已经压实的短读、`EINTR`、旧内核回退、并发初始化和整缓冲清零边界全部保留。

函数只有在完整填满缓冲后才返回 `true`。系统源失败时返回 `false`、清零整个输出，并设置 `xrt.random` / `XRANDOM_ERROR_SYSTEM` 结构化错误；绝不退化为 PCG、时间或进程号。空区间直接成功并允许空指针。

```c
uint8 Token[32];

if ( !xrtSecureRandom(Token, sizeof(Token)) ) {
	return false;
}
/* 使用 Token。 */
xrtSecureZero(Token, sizeof(Token));
```

`random_secure` 是独立基础模块。密码密钥生成、TLS、临时文件、原子写和目录树暂存共同依赖它，不会让文件体系反向拉入 `crypto_core`。

### 安全文本与令牌

```c
bool xrtSecureText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength);
str xrtSecureStringFrom(xstrview Alphabet, size_t iLength);
str xrtSecureString(size_t iLength);
```

`xrtSecureText` 是零分配基础接口，写入固定长度文本并补零。`xrtSecureStringFrom` 分配使用自定义字母表的字符串；`xrtSecureString` 使用 `0-9A-Za-z-_` 组成的 URL-safe 64 字符字母表，适合会话标识、CSRF token、Cookie token 和 URL token。返回的字符串由 `xrtFree` 释放；敏感令牌在释放前应使用 `xrtSecureZero` 清理。

这些接口保证随机源安全、字符采样无偏和输出长度固定，但不保证“至少包含一个大写字母、一个小写字母、一个数字和一个符号”之类的密码策略。需要类别约束时，应用层应分别从每个必选类别生成至少一个字符，再从完整字母表补足并使用安全随机源执行无偏洗牌；不能只在生成后检查一次便返回可能不合规的结果。

自定义字母表必须包含 1 至 94 个互不重复的可见 ASCII 字符。实现按块读取系统熵并执行拒绝采样，不使用有偏的直接取模。参数、容量和输出/字母表重叠会在写入前完成检查；系统熵失败时整个输出被清零。`random_secure_text` 不依赖 PCG、默认线程状态或完整密码算法模块。

```c
str sToken = xrtSecureString(32);

if ( sToken == NULL ) {
	return false;
}
/* 使用 sToken。 */
xrtSecureZero(sToken, 33);
xrtFree(sToken);
```

### `xrtSecureRandom`

使用操作系统密码安全随机源填满缓冲；失败时清零整个输出。

```c
bool xrtSecureRandom(ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输出 | 非空 | 接收缓冲 |
| `iSize` | 输入 | > 0 | 缓冲字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已填满密码安全随机字节 | — |
| `false` | 系统随机源失败，输出已清零 | `XERR_IO` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或长度为零
- `XERR_IO` — 操作系统随机源读取失败

#### 范例

[random_secure](../../examples/math/random_secure/main.c) · 系统安全随机

```c
	if ( !xrtSecureRandom(arrId, sizeof(arrId)) ) {
```

### `xrtSecureText`

使用操作系统安全随机源和自定义字母表写入随机文本并补零。

```c
bool xrtSecureText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | > `iLength` | 容量，须含末尾零 |
| `iLength` | 输入 | — | 文本长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 失败 | `XERR_ARGUMENT` / `XERR_IO` |

#### 错误

- `XERR_ARGUMENT` — 字母表为空、容量不足或指针为空
- `XERR_IO` — 操作系统随机源读取失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 安全随机文本

```c
	if ( !xrtSecureText(SV(sHex), TextA, sizeof(TextA), 8u) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
```

### `xrtSecureStringFrom`

使用自定义字母表创建由 `xrtFree` 释放的密码安全随机字符串。

```c
str xrtSecureStringFrom(xstrview Alphabet, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_IO` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 字母表为空或长度非法
- `XERR_IO` — 系统随机源失败
- `XERR_MEMORY` — 分配失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 安全随机字符串（自定义字母表）

```c
	sGenerated = xrtSecureStringFrom(SV(sHex), 8u);
```

### `xrtSecureString`

使用 URL-safe 64 字符字母表创建密码安全随机字符串。

```c
str xrtSecureString(size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_IO` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 长度非法
- `XERR_IO` — 系统随机源失败
- `XERR_MEMORY` — 分配失败

#### 范例

[random_secure_text](../../examples/math/random_secure_text/main.c) · 安全随机字符串

```c
	str sToken = xrtSecureString(32);   /* 32 字符 ≈ 192 位熵 */
```

## 显式状态

### `xrng`

```c
typedef struct xrng {
	uint64 State;
	uint64 Increment;
	uint32 Guard;
	uint32 Reserved;
} xrng;
```

状态可放在栈、对象、任务或用户自定义线程上下文中，不需要创建和销毁堆对象。字段公开是为了让 C 用户控制存储、复制状态和嵌入结构，不代表允许直接修改。每个状态由一个线程或外部同步保护；不同状态可以完全并行。

按值复制 `xrng` 会复制当前位置，两个副本随后产生相同序列。这是分支模拟和可复现测试的明确能力。

| 字段 | 类型 | 语义 |
|---|---|---|
| `State` | `uint64` | 状态字 |
| `Increment` | `uint64` | 流增量（须为奇数） |
| `Guard` | `uint32` | 守卫字（Ready 校验） |

### `XRT_RNG_INITIALIZER`

```c
xrng Rng = XRT_RNG_INITIALIZER;
```

静态初始化得到固定有效状态，适合无需自定义 seed 的确定性路径。多数程序仍应调用 `xrtRngSeed`，明确记录 seed 与 stream。

## 有界整数

## 单位实数

## 数组洗牌

```c
bool xrtRngShuffle(xrng* pRng,
	ptr pData, size_t iCount, size_t iItemSize);
```

使用无偏有界采样执行原地 Fisher-Yates 洗牌，支持任意固定大小元素且不分配内存。函数先验证状态、空指针、元素大小、总字节数溢出和数组/状态重叠，再推进随机状态；失败时数组和状态都不变。空数组允许空指针并直接成功。

这个原语承接旧文档中反复手写的洗牌与随机采样底座。抽取前 N 项可以先复制数据、调用 `xrtRngShuffle`，再读取前 N 项；权重采样和特定概率分布仍由上层按业务规则组合。

### `xrtRngSeed`

用 seed 和 stream 初始化或重置一个显式随机数状态。

```c
void xrtRngSeed(xrng* pRng, uint64 iSeed, uint64 iStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输出 | 非空 | 显式状态 |
| `iSeed` | 输入 | — | 种子 |
| `iStream` | 输入 | — | 流序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已初始化 | — |

#### 错误

- `XERR_ARGUMENT` — 状态指针为空

#### 范例

[random](../../examples/math/random/main.c) · 初始化状态

```c
	xrtRngSeed(&Rng, 2026, 7);       /* 种子 2026，流 7 */
```

### `xrtRngReady`

判断显式随机数状态是否已经初始化且内部约束自洽。

```c
bool xrtRngReady(const xrng* pRng)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 非空 | 显式状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否可用 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 状态自检

```c
	if ( !xrtRngReady(&RngA) ||
		(xrtRngReady(NULL)) ) {
```

### `xrtRng32`

从显式状态生成一个 32 位伪随机数。

```c
uint32 xrtRng32(xrng* pRng)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 32 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random](../../examples/math/random/main.c) · 32 位生成

```c
	printf("explicit: %u\n", (unsigned int)xrtRng32(&Rng));
```

### `xrtRng64`

从同一个显式状态连续生成并组合一个 64 位伪随机数。

```c
uint64 xrtRng64(xrng* pRng)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 64 位生成

```c
		if ( xrtRng64(&RngA) != xrtRng64(&RngB) ) {
```

### `xrtRngBytes`

按稳定的小端字节顺序填充缓冲区；同一状态在所有平台产生相同结果。

```c
bool xrtRngBytes(xrng* pRng, ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `pData` | 输出 | 非空 | 接收缓冲 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已填充 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 状态未初始化、缓冲为空或长度为零

#### 范例

[random](../../examples/math/random/main.c) · 字节填充

```c
	if ( !xrtRngBytes(&Rng, arrBytes, sizeof(arrBytes)) ||
		 !xrtRngShuffle(&Rng, arrOrder,
			sizeof(arrOrder) / sizeof(arrOrder[0]), sizeof(arrOrder[0])) ) {
```

### `xrtRngBelow32`

无偏生成 `[0, iBound)` 内的 32 位整数。

```c
uint32 xrtRngBelow32(xrng* pRng, uint32 iBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `iBound` | 输入 | > 0 | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 32 位值 | 无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 32 位无偏上界

```c
		uint32 uA = xrtRngBelow32(&RngA, 100u);
```

### `xrtRngBelow64`

无偏生成 `[0, iBound)` 内的 64 位整数。

```c
uint64 xrtRngBelow64(xrng* pRng, uint64 iBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `iBound` | 输入 | > 0 | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 64 位无偏上界

```c
			uint64 uA = xrtRngBelow64(&RngA,
				UINT64_C(1000000));
```

### `xrtRngRange`

无偏生成半开区间 `[iMin, iMax)` 内的整数。

```c
int64 xrtRngRange(xrng* pRng, int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | > `iMin` | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 半开区间

```c
		int64 iA = xrtRngRange(&RngA, -100, 100);
```

### `xrtRngRangeClosed`

无偏生成闭区间 `[iMin, iMax]` 内的整数，包括完整 int64 域。

```c
int64 xrtRngRangeClosed(xrng* pRng, int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | >= `iMin` | 上界（含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random](../../examples/math/random/main.c) · 闭区间

```c
	printf("dice    : %lld\n", (long long)xrtRngRangeClosed(&Rng, 1, 6));
```

### `xrtRngReal`

生成半开区间 `[0.0, 1.0)` 内具有 53 位精度的双精度数。

```c
double xrtRngReal(xrng* pRng)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| double | `[0.0, 1.0)` 内随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random](../../examples/math/random/main.c) · 单位实数

```c
	printf("real    : %.12f\n", xrtRngReal(&Rng));
```

### `xrtRngShuffle`

使用 Fisher-Yates 算法原地打乱定长元素数组，不执行内存分配。

```c
bool xrtRngShuffle(xrng* pRng,
	ptr pData, size_t iCount, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `pData` | 输入/输出 | 非空 | 元素数组 |
| `iCount` | 输入 | — | 元素数量 |
| `iItemSize` | 输入 | > 0 | 单元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已打乱 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` / `XERR_OVERFLOW` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_OVERFLOW` — `iCount * iItemSize` 溢出
- `XERR_STATE` — 状态未初始化

#### 范例

[random](../../examples/math/random/main.c) · 数组洗牌

```c
		 !xrtRngShuffle(&Rng, arrOrder,
			sizeof(arrOrder) / sizeof(arrOrder[0]), sizeof(arrOrder[0])) ) {
```

### `xrtRngText`

把可复现随机文本写入调用方缓冲区并补零。

```c
bool xrtRngText(xrng* pRng, xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | > `iLength` | 容量 |
| `iLength` | 输入 | — | 文本长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 状态未初始化、字母表为空或容量不足

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 随机文本

```c
	if ( !xrtRngText(&RngA, SV(sHex), TextA, sizeof(TextA), 8u) ||
		!xrtRngText(&RngB, SV(sHex), TextB, sizeof(TextB), 8u) ||
		(memcmp(TextA, TextB, 8u) != 0) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
```

### `xrtRngStringFrom`

使用自定义字母表创建由 `xrtFree` 释放的可复现随机字符串。

```c
str xrtRngStringFrom(xrng* pRng, xstrview Alphabet, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 状态未初始化或字母表为空
- `XERR_MEMORY` — 分配失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 随机字符串（自定义字母表）

```c
	sGenerated = xrtRngStringFrom(&RngA, SV(sHex), 12u);
```

### `xrtRngString`

使用 URL-safe 64 字符字母表创建可复现随机字符串。

```c
str xrtRngString(xrng* pRng, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pRng` | 输入 | 已初始化 | 显式状态 |
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 状态未初始化或长度非法
- `XERR_MEMORY` — 分配失败

#### 范例

[random_text](../../examples/math/random_text/main.c) · 随机字符串

```c
	sText = xrtRngString(&Rng, 24);
```

## 当前线程便捷层

```c
void xrtRandSeed(uint64 iSeed, uint64 iStream);
uint32 xrtRand32(void);
uint64 xrtRand64(void);
bool xrtRandBytes(ptr pData, size_t iSize);
uint64 xrtRandBelow(uint64 iBound);
int64 xrtRandRange(int64 iMin, int64 iMax);
int64 xrtRandRangeClosed(int64 iMin, int64 iMax);
double xrtRandReal(void);
bool xrtRandShuffle(ptr pData, size_t iCount, size_t iItemSize);
```

这些函数使用当前线程独立状态，不要求调用 `xrtInit`，也不要求线程先附着到任务运行时。多个线程之间没有共享随机锁；每个线程第一次调用时自动播种。自动种子只用于避免普通线程默认得到同一序列，不提供不可预测性。

调用 `xrtRandSeed` 可把当前线程重置到可复现序列。其余便捷函数和同 seed/stream 的显式状态逐值一致。

```c
xrtRandSeed(42, 54);
uint32 iValue = xrtRand32();
int64 iChoice = xrtRandRange(0, 10);
```

库和框架代码通常应接收或持有显式 `xrng`，避免隐式消费调用者线程的序列。便捷层主要服务脚本语言内建函数和短小应用。

## 随机文本

`xrtRngText` 是零分配基础 API，把固定长度文本写入调用方缓冲区并补零；容量必须至少为 `length + 1`。`xrtRngStringFrom` 创建使用自定义字母表的独立字符串，`xrtRngString` 使用 `0-9A-Za-z-_` 组成的 URL-safe 64 字符默认字母表。对应的 `xrtRandText`、`xrtRandStringFrom`、`xrtRandString` 使用当前线程状态。

自定义字母表必须包含 1 至 94 个互不重复的可见 ASCII 字符。该约束保证输出始终是有效、无嵌入零的 UTF-8 文本，并防止重复字符无意改变概率权重。每个字符都通过拒绝采样选择，不产生 `random % alphabet_size` 的模偏差。

参数、容量、字母表和重叠会在推进 RNG 前完整检查；失败不改变输出或随机状态。调用方缓冲区不能与字母表或 `xrng` 状态重叠。长度为零的分配便捷函数仍返回可释放的独立空字符串。

这些 API 继承 PCG 的“可复现但可预测”属性，只适用于测试数据、模拟、游戏、明确不承担安全边界的名称和随机采样。令牌、Cookie 密钥、WebSocket nonce、密码、UUID v4、会话标识和临时资源必须使用 `xrtSecureRandom`、`xrtSecureText` 或 `xrtSecureString`。

## 错误与状态

- 所有返回数值的失败路径通过当前执行上下文的 `xerror` 补充表达错误。
- 参数或状态校验失败不会推进随机状态。
- 零是合法随机结果，不能只凭返回值判断成功；需要区分时先清理并检查 `xrtGetError()`。
- 成功调用不会主动清除调用前已有错误。

## 旧 API 决策

- `xrand` 改为 `xrng`，显式状态统一使用 `xrtRng*`，当前线程便捷层统一使用 `xrtRand*`。
- 删除 `Ex`、`Obj` 和线程专用版本族。栈上 `xrng` 已覆盖独立对象，不需要堆分配、`ptr` 强转和专用销毁函数。
- 64 位输出只消费一个状态，不再要求 low/high 两个生成器。
- 原来的区间函数同时承担“自动交换边界”和“闭区间”两种隐藏规则；现在半开与闭区间分名表达，反向边界明确失败。
- 修复完整 32 位闭区间宽度转换为零后执行除零的问题，并把范围扩展到完整 `int64`。
- 删除普通伪随机数可生成安全 UUID/GUID 的旧文档建议。

### `xrtRandSeed`

重置当前线程的快速伪随机数状态；与 `xrtFastRand*` 别名族共享同一线程状态。

```c
void xrtRandSeed(uint64 iSeed, uint64 iStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSeed` | 输入 | — | 种子 |
| `iStream` | 输入 | — | 流序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已重置 | — |

#### 错误

- 无 — 重置不失败

#### 范例

[thread_random](../../examples/math/thread_random/main.c) · 重置线程状态

```c
	xrtRandSeed(2026, 7);             /* 重置本线程状态 → 序列可复现 */
```

### `xrtRand32`

从当前线程状态生成一个非密码学 32 位伪随机数。

```c
uint32 xrtRand32(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 32 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[thread_random](../../examples/math/thread_random/main.c) · 32 位生成

```c
	printf("value: %u\n", (unsigned int)xrtRand32());
```

### `xrtRand64`

从当前线程状态生成一个非密码学 64 位伪随机数。

```c
uint64 xrtRand64(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 64 位生成

```c
	(void)xrtRand64();
```

### `xrtRandBytes`

使用当前线程非密码学随机状态按稳定的小端顺序填充字节。

```c
bool xrtRandBytes(ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输出 | 非空 | 接收缓冲 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已填充 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 缓冲为空或长度为零

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 字节填充

```c
	if ( !xrtRandBytes(Buffer, sizeof(Buffer)) ) {
```

### `xrtRandBelow`

从当前线程状态无偏生成 `[0, iBound)` 内的整数。

```c
uint64 xrtRandBelow(uint64 iBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iBound` | 输入 | > 0 | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 无偏上界

```c
	if ( (xrtRandBelow(10u) >= 10u) ||
		(xrtRandBelow(1u) != 0u) ) {
```

### `xrtRandRange`

从当前线程状态无偏生成半开区间 `[iMin, iMax)` 内的整数。

```c
int64 xrtRandRange(int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | > `iMin` | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 半开区间

```c
			iMax = xrtRandRange(-5, 5);
```

### `xrtRandRangeClosed`

从当前线程状态无偏生成闭区间 `[iMin, iMax]` 内的整数。

```c
int64 xrtRandRangeClosed(int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | >= `iMin` | 上界（含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[thread_random](../../examples/math/thread_random/main.c) · 闭区间

```c
	printf("dice : %lld\n", (long long)xrtRandRangeClosed(1, 6));
```

### `xrtRandReal`

从当前线程状态生成 `[0.0, 1.0)` 内的双精度数。

```c
double xrtRandReal(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| double | `[0.0, 1.0)` 内随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[thread_random](../../examples/math/thread_random/main.c) · 单位实数

```c
	printf("real : %.12f\n", xrtRandReal());
```

### `xrtRandShuffle`

使用当前线程随机状态原地打乱定长元素数组。

```c
bool xrtRandShuffle(ptr pData, size_t iCount, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入/输出 | 非空 | 元素数组 |
| `iCount` | 输入 | — | 元素数量 |
| `iItemSize` | 输入 | > 0 | 单元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已打乱 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` / `XERR_OVERFLOW` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_OVERFLOW` — 尺寸乘法溢出

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 数组洗牌

```c
		if ( !xrtRandShuffle(Numbers, 6u, sizeof(int)) ) {
```

### `xrtRandText`

使用当前线程随机状态把文本写入调用方缓冲区并补零。

```c
bool xrtRandText(xstrview Alphabet,
	char* sOutput, size_t iCapacity, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `sOutput` | 输出 | 非空 | 输出缓冲 |
| `iCapacity` | 输入 | > `iLength` | 容量 |
| `iLength` | 输入 | — | 文本长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入并补零 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 字母表为空、容量不足或指针为空

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 随机文本

```c
	if ( !xrtRandText(SV(sHex), TextA, sizeof(TextA), 8u) ||
		!exampleInAlphabet(TextA, 8u, SV(sHex)) ) {
```

### `xrtRandStringFrom`

使用当前线程随机状态和自定义字母表创建随机字符串。

```c
str xrtRandStringFrom(xstrview Alphabet, size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Alphabet` | 输入 | 借用、非空 | 字母表 |
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 字母表为空或长度非法
- `XERR_MEMORY` — 分配失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 随机字符串（自定义字母表）

```c
	sGenerated = xrtRandStringFrom(SV(sHex), 16u);
```

### `xrtRandString`

使用当前线程随机状态和默认字母表创建随机字符串。

```c
str xrtRandString(size_t iLength)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iLength` | 输入 | — | 字符串长度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 零结尾字符串，`xrtFree` 释放 | — |
| `NULL` | 失败 | `XERR_ARGUMENT` / `XERR_MEMORY` |

#### 错误

- `XERR_ARGUMENT` — 长度非法
- `XERR_MEMORY` — 分配失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 随机字符串

```c
	sGenerated = xrtRandString(12u);
```

### `xrtFastRandSeed`

重置当前线程的快速伪随机数状态；旧 `xrtRand*` 名称是本族的兼容别名。

```c
void xrtFastRandSeed(uint64 iSeed, uint64 iStream)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSeed` | 输入 | — | 种子 |
| `iStream` | 输入 | — | 流序号 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 无 | 已重置 | — |

#### 错误

- 无 — 重置不失败

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 重置线程状态

```c
	xrtFastRandSeed(12345u, 67890u);
```

### `xrtFastRand32`

从当前线程状态生成一个非密码学 32 位伪随机数。

```c
uint32 xrtFastRand32(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 32 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 32 位生成

```c
	(void)xrtFastRand32();
```

### `xrtFastRand64`

从当前线程状态生成一个非密码学 64 位伪随机数。

```c
uint64 xrtFastRand64(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 伪随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 64 位生成

```c
	(void)xrtFastRand64();
```

### `xrtFastRandBytes`

使用当前线程非密码学随机状态按稳定的小端顺序填充字节。

```c
bool xrtFastRandBytes(ptr pData, size_t iSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输出 | 非空 | 接收缓冲 |
| `iSize` | 输入 | > 0 | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已填充 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 缓冲为空或长度为零

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 字节填充

```c
	if ( !xrtFastRandBytes(BytesA, sizeof(BytesA)) ) {
```

### `xrtFastRandBelow`

从当前线程状态无偏生成 `[0, iBound)` 内的整数。

```c
uint64 xrtFastRandBelow(uint64 iBound)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iBound` | 输入 | > 0 | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 无偏上界

```c
		if ( (xrtFastRandBelow(7u) >= 7u) ||
			(iRange < -3) || (iRange > 3) ||
			(xrtFastRandRangeClosed(1, 1) != 1) ) {
```

### `xrtFastRandRange`

从当前线程状态无偏生成半开区间 `[iMin, iMax)` 内的整数。

```c
int64 xrtFastRandRange(int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | > `iMin` | 上界（不含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 半开区间

```c
		int64 iRange = xrtFastRandRange(-3, 3);
```

### `xrtFastRandRangeClosed`

从当前线程状态无偏生成闭区间 `[iMin, iMax]` 内的整数。

```c
int64 xrtFastRandRangeClosed(int64 iMin, int64 iMax)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iMin` | 输入 | — | 下界（含） |
| `iMax` | 输入 | >= `iMin` | 上界（含） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 64 位值 | 区间内无偏随机数 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空、边界为零或区间为空
- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 闭区间

```c
			(xrtFastRandRangeClosed(1, 1) != 1) ) {
```

### `xrtFastRandReal`

从当前线程状态生成 `[0.0, 1.0)` 内的双精度数。

```c
double xrtFastRandReal(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| double | `[0.0, 1.0)` 内随机数 | — |

#### 错误

- 无错误设置约束 — 本族不是密码学安全随机源，绝不可用于密钥、nonce、token、会话标识或任何攻击者可以猜测的值

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 单位实数

```c
		double dReal = xrtFastRandReal();
```

### `xrtFastRandShuffle`

使用当前线程随机状态原地打乱定长元素数组。

```c
bool xrtFastRandShuffle(ptr pData, size_t iCount, size_t iItemSize)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入/输出 | 非空 | 元素数组 |
| `iCount` | 输入 | — | 元素数量 |
| `iItemSize` | 输入 | > 0 | 单元素字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已打乱 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` / `XERR_OVERFLOW` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或元素大小为零
- `XERR_OVERFLOW` — 尺寸乘法溢出

#### 范例

[random_tour](../../examples/math/random_tour/main.c) · 数组洗牌

```c
		if ( !xrtFastRandShuffle(Numbers, 4u, sizeof(int)) ) {
```

## 完整示例

- `examples/math/random/main.c`：显式状态、闭区间和单位实数。
- `examples/math/random_secure/main.c`：操作系统安全随机源和敏感缓冲清理。
- `examples/math/random_secure_text/main.c`：URL-safe 密码安全随机令牌。
- `examples/math/thread_random/main.c`：当前线程便捷状态。
- `examples/math/random_text/main.c`：显式状态可复现随机文本。
- `examples/math/thread_random_text/main.c`：当前线程便捷随机文本。

PCG 来源与许可证见 `docs/THIRD_PARTY.md`。
