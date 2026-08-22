# Random API

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

### `XRT_RNG_INITIALIZER`

```c
xrng Rng = XRT_RNG_INITIALIZER;
```

静态初始化得到固定有效状态，适合无需自定义 seed 的确定性路径。多数程序仍应调用 `xrtRngSeed`，明确记录 seed 与 stream。

### `xrtRngSeed`

```c
void xrtRngSeed(xrng* pRng, uint64 iSeed, uint64 iStream);
```

按 PCG 推荐过程初始化或重置状态。相同 seed、stream 和调用序列在支持的平台、指针宽度和编译器上产生相同结果。不同并行任务应使用不同 stream，不能只在共享状态上并发调用。

PCG32 的增量由 `(stream << 1) | 1` 构造，因此 stream 的低 63 位标识序列，最高位不参与选择。需要自动分配流编号时，应在 `[0, 2^63)` 内生成唯一编号。

空状态指针设置 `XERR_ARGUMENT`。成功后 guard 和奇数增量约束同时建立。

### `xrtRng32` 与 `xrtRng64`

```c
uint32 xrtRng32(xrng* pRng);
uint64 xrtRng64(xrng* pRng);
```

`xrtRng32` 保留旧版已经使用的 PCG XSH RR 32 位序列。`xrtRng64` 从同一状态连续取两个 32 位字，以第一次结果为低位、第二次结果为高位；不再要求调用方维护两个不相关状态。

未初始化、被修改或增量为偶数的状态设置 `XERR_STATE`，返回零且不推进状态。

### `xrtRngBytes`

```c
bool xrtRngBytes(xrng* pRng, ptr pData, size_t iSize);
```

按小端顺序展开连续 PCG32 结果，零分配填满任意长度缓冲。相同状态在不同 CPU 字节序上得到同样的字节序列；每四个输出字节消费一个 32 位结果，尾部不足四字节仍消费完整结果。空区间允许空指针且不推进状态，输出不能与 `xrng` 重叠。

该接口用于可复现测试数据、模拟和非安全协议夹具；名字中的 `Bytes` 不代表密码安全。安全字节必须使用 `xrtSecureRandom`。

## 有界整数

### `xrtRngBelow32` 与 `xrtRngBelow64`

```c
uint32 xrtRngBelow32(xrng* pRng, uint32 iBound);
uint64 xrtRngBelow64(xrng* pRng, uint64 iBound);
```

使用拒绝采样生成 `[0, iBound)`，不会产生 `% iBound` 的模偏差。`iBound == 0` 设置 `XERR_ARGUMENT`，并保证状态不变。边界为 1 时结果恒为零。

### `xrtRngRange`

```c
int64 xrtRngRange(xrng* pRng, int64 iMin, int64 iMax);
```

生成半开区间 `[iMin, iMax)`。这与 Python `randrange`、Go 的有界随机和常见容器下标语义一致，适合数组索引、采样和循环范围。必须满足 `iMin < iMax`；空区间或反向区间设置 `XERR_ARGUMENT`，状态不变。

实现根据宽度选择 32 位或 64 位采样，完整覆盖跨零区间和接近 `INT64_MIN`、`INT64_MAX` 的区间，所有宽度计算都使用无溢出位模式运算。

### `xrtRngRangeClosed`

```c
int64 xrtRngRangeClosed(xrng* pRng, int64 iMin, int64 iMax);
```

生成闭区间 `[iMin, iMax]`，适合骰子、离散等级和明确包含上界的业务规则。允许 `iMin == iMax`，并支持整个 `[INT64_MIN, INT64_MAX]` 域。反向区间设置 `XERR_ARGUMENT`，不会自动交换边界，以免把调用错误静默变成另一种业务语义。

```c
xrng Rng;

xrtRngSeed(&Rng, 2026, 7);
int64 iIndex = xrtRngRange(&Rng, 0, itemCount);
int64 iDice = xrtRngRangeClosed(&Rng, 1, 6);
```

## 单位实数

### `xrtRngReal`

```c
double xrtRngReal(xrng* pRng);
```

使用一个 64 位随机字的高 53 位生成 `[0.0, 1.0)`，与双精度尾数能力匹配。结果可能为 0.0，永远小于 1.0。需要闭区间或其他分布时，调用方应在此均匀原语上构建明确算法。

## 数组洗牌

```c
bool xrtRngShuffle(xrng* pRng,
	ptr pData, size_t iCount, size_t iItemSize);
```

使用无偏有界采样执行原地 Fisher-Yates 洗牌，支持任意固定大小元素且不分配内存。函数先验证状态、空指针、元素大小、总字节数溢出和数组/状态重叠，再推进随机状态；失败时数组和状态都不变。空数组允许空指针并直接成功。

这个原语承接旧文档中反复手写的洗牌与随机采样底座。抽取前 N 项可以先复制数据、调用 `xrtRngShuffle`，再读取前 N 项；权重采样和特定概率分布仍由上层按业务规则组合。

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

## 完整示例

- `examples/math/random/main.c`：显式状态、闭区间和单位实数。
- `examples/math/random_secure/main.c`：操作系统安全随机源和敏感缓冲清理。
- `examples/math/random_secure_text/main.c`：URL-safe 密码安全随机令牌。
- `examples/math/thread_random/main.c`：当前线程便捷状态。
- `examples/math/random_text/main.c`：显式状态可复现随机文本。
- `examples/math/thread_random_text/main.c`：当前线程便捷随机文本。

PCG 来源与许可证见 `docs/THIRD_PARTY.md`。
