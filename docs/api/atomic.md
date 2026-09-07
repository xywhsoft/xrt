# Atomic

Atomic 提供可裁剪的 32 位、64 位和指针原子操作，是无锁队列、引用对象、调度器和网络状态机的共同底座。它不分配内存，不隐式阻塞，也不接管原子指针指向的对象。

## 裁剪

启用 `XRT_FEATURE_ATOMIC` 即可使用本模块，只依赖 `core`。未启用时不声明原子类型和函数，也不会编译平台原子实现。

## 类型与初始化

```c
typedef struct xatomic32 {
	volatile uint32 Value;
} xatomic32;

typedef struct xatomic64 {
	volatile uint64 Value;
} xatomic64;

typedef struct xatomicptr {
	ptr volatile Value;
} xatomicptr;
```

`xatomic64` 声明为至少 8 字节对齐，`xatomicptr` 按指针宽度对齐。对象发布给其他执行流后，只能通过 Atomic API 访问 `Value`；不能混用普通读写、`memcpy` 或直接赋值。

TinyCC x86 不会把成员类型的 8 字节对齐要求传播到栈上外层结构。XRT 对正常对齐对象仍使用无锁指令；对该编译器产生的 4 字节对齐 `xatomic64` 使用内部 64 槽分片锁，并提供不弱于请求的内存顺序。其他编译器仍拒绝未按类型自然对齐的 64 位对象。

自动和动态对象在发布前使用 `xrtAtomic32Init()`、`xrtAtomic64Init()`、`xrtAtomicPtrInit()`。静态对象使用 `XRT_ATOMIC32_INIT()`、`XRT_ATOMIC64_INIT()`、`XRT_ATOMICPTR_INIT()`：

```c
static xatomic64 RequestCount = XRT_ATOMIC64_INIT(0);
```

初始化不是并发操作。对象仍被其他线程访问时，不得重新初始化、移动或销毁。

### `xrtAtomicIsLockFree`

查询自然对齐的指定字节宽度是否由当前目标无锁实现。

```c
bool xrtAtomicIsLockFree(size_t iSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iSize` | 输入 | — | 字节宽度；当前公开标量只认 4、8 与指针宽度 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 该宽度由目标无锁实现（保守口径：静态可证明才为真） | — |
| `false` | 其他宽度，或后端无法静态证明无锁 | 纯查询，不设置错误 |

#### 错误

- 无 — 查询采用保守口径：返回 `false` 不表示对应 Atomic API 失去原子语义。需要对外宣称 lock-free 的算法必须先检查其最大原子宽度。

#### 范例

[core/atomic_tour · 杂项](../../examples/core/atomic_tour/main.c) · 发布无锁算法前先检查宽度

```c
if ( !xrtAtomicIsLockFree(sizeof(uint32)) ||
	!xrtAtomicIsLockFree(sizeof(uint64)) ) {
	goto Cleanup;
}
```

### `xrtAtomic32Init`

把 32 位原子对象置为指定值。仅用于发布前的单线程初始化，不是并发操作。

```c
void xrtAtomic32Init(xatomic32* pAtomic, uint32 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输出 | 建议非空 | 目标对象；空或未对齐指针是空操作 |
| `iValue` | 输入 | — | 初始值 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 地址非法时静默忽略，不设置错误 |

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 初始化后立即交换取旧值

```c
xrtAtomic32Init(&A32, 0x0Fu);
iOld32 = xrtAtomic32Exchange(&A32, 0x3Cu, XMEMORY_ACQ_REL);
```

### `xrtAtomic64Init`

把 64 位原子对象置为指定值。仅用于发布前的单线程初始化。

```c
void xrtAtomic64Init(xatomic64* pAtomic, uint64 iValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输出 | 建议非空、8 字节对齐 | 目标对象；地址非法时是空操作 |
| `iValue` | 输入 | — | 初始值 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 地址非法时静默忽略，不设置错误 |

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 初始 100，交换为 50

```c
xrtAtomic64Init(&A64, UINT64_C(100));
iOld64 = xrtAtomic64Exchange(&A64, UINT64_C(50),
	XMEMORY_ACQ_REL);
```

### `xrtAtomicPtrInit`

把原子指针对象置为指定值。仅用于发布前的单线程初始化；空指针是合法值。

```c
void xrtAtomicPtrInit(xatomicptr* pAtomic, ptr pValue);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输出 | 建议非空 | 目标对象 |
| `pValue` | 输入 | 允许空 | 初始指针值 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 地址非法时静默忽略，不设置错误 |

#### 范例

[core/atomic_tour · 指针](../../examples/core/atomic_tour/main.c) · 发布后再交换

```c
xrtAtomicPtrInit(&APtr, (ptr)&s_Target[0]);
pOld = xrtAtomicPtrExchange(&APtr, (ptr)&s_Target[1],
	XMEMORY_ACQ_REL);
```

### `xmemoryorder`

原子操作的内存顺序与 C11 语义一致。

```c
typedef enum xmemoryorder {
	XMEMORY_RELAXED = 0,
	XMEMORY_ACQUIRE = 1,
	XMEMORY_RELEASE = 2,
	XMEMORY_ACQ_REL = 3,
	XMEMORY_SEQ_CST = 4
} xmemoryorder;
```

| 值 | 语义 |
|---|---|
| `XMEMORY_RELAXED` | RELAXED |
| `XMEMORY_ACQUIRE` | ACQUIRE |
| `XMEMORY_RELEASE` | RELEASE |
| `XMEMORY_ACQ_REL` | ACQREL |
| `XMEMORY_SEQ_CST` | 顺序一致（默认最强） |

### 常量总表

| 常量 | 值 | 语义 |
|---|---|---|
| `XRT_ATOMIC32_INIT` | `(iValue) { (uint32)(iValue) }` | 静态原子对象初始化器只能用于对象定义。 |
| `XRT_ATOMIC64_INIT` | `(iValue) { (uint64)(iValue) }` | ATOMIC64初始化 |
| `XRT_ATOMICPTR_INIT` | `(pValue) { (ptr)(pValue) }` | ATOMICPTR初始化 |

## 内存顺序

```c
typedef enum xmemoryorder {
	XMEMORY_RELAXED,
	XMEMORY_ACQUIRE,
	XMEMORY_RELEASE,
	XMEMORY_ACQ_REL,
	XMEMORY_SEQ_CST
} xmemoryorder;
```

- `RELAXED` 只保证该值原子更新，适合独立统计计数。
- `ACQUIRE` 用于读取已经发布的数据。
- `RELEASE` 用于发布此前写入的数据。
- `ACQ_REL` 用于同时读取并发布的读改写操作。
- `SEQ_CST` 提供最直观的全局顺序，适合控制路径和尚未证明可放宽的代码。

加载只接受 `RELAXED`、`ACQUIRE`、`SEQ_CST`；存储只接受 `RELAXED`、`RELEASE`、`SEQ_CST`。比较交换失败顺序不能包含 Release，并且不能强于成功顺序。非法组合设置 `XERR_ARGUMENT`，对象保持不变。

GCC 和 Clang 直接使用对应的内存顺序。只提供全栅栏原语的平台会采用更强顺序，绝不会弱化公开合同。

## 32 位与 64 位整数

32 位和 64 位类型都提供 `Load`、`Store`、`Exchange`、`CompareExchange`、`FetchAdd`、`FetchSub`、`FetchAnd`、`FetchOr`、`FetchXor`。`Fetch*` 返回修改前的值，无符号加减按模宽度回绕。

强比较交换不产生伪失败。成功时写入 `Desired`；失败时对象不变，并把当前实际值写回 `Expected`，因此调用方可直接继续 CAS 循环。`Expected` 必须指向调用方独占的普通标量，不得与原子对象重叠；它本身的并发保护由调用方负责。

原生后端直接使用平台的加法和按位读改写指令。必须走 CAS 的退化路径会复用失败操作返回的实际值，不会在每次竞争失败后额外执行一次原子加载。

### `xrtAtomic32Load`

原子读取 32 位值。

```c
uint32 xrtAtomic32Load(const xatomic32* pAtomic, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、4 字节对齐 | 目标对象 |
| `iOrder` | 输入 | `RELAXED`/`ACQUIRE`/`SEQ_CST` | 加载允许的顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 当前值 | 原子快照 | 参数非法时返回 `0`；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空/未对齐，或顺序不是三种加载顺序之一

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · ACQUIRE 配对发布写

```c
if ( (iOld32 != 0x0Fu) ||
	(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x3Cu) ) {
```

### `xrtAtomic32Store`

原子写入 32 位值。

```c
void xrtAtomic32Store(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 新值 |
| `iOrder` | 输入 | `RELAXED`/`RELEASE`/`SEQ_CST` | 存储允许的顺序 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 参数非法时不写入、不设置错误 |

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · RELEASE 发布后紧接读改写

```c
xrtAtomic32Store(&A32, 0x33u, XMEMORY_RELEASE);
iOld32 = xrtAtomic32FetchAdd(&A32, 0x0Cu, XMEMORY_ACQ_REL);
```

### `xrtAtomic32Exchange`

原子替换 32 位值并返回旧值。

```c
uint32 xrtAtomic32Exchange(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 新值 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 旧值 | 替换前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 交换取旧值再核对

```c
xrtAtomic32Init(&A32, 0x0Fu);
iOld32 = xrtAtomic32Exchange(&A32, 0x3Cu, XMEMORY_ACQ_REL);
```

### `xrtAtomic32CompareExchange`

强比较交换：当前值等于 `*pExpected` 时写入 `iDesired`；失败时把实际值写回 `*pExpected`，不产生伪失败。

```c
bool xrtAtomic32CompareExchange(
	xatomic32* pAtomic,
	uint32* pExpected,
	uint32 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `pExpected` | 输入/输出 | 非空、调用方独占 | 期望值；失败时被改写为实际值 |
| `iDesired` | 输入 | — | 成功时写入的值 |
| `iSuccess` | 输入 | 五种顺序均可 | 成功路径顺序 |
| `iFailure` | 输入 | 加载顺序，且不强于成功 | 失败路径顺序；不能含 Release |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 匹配并已写入 `iDesired` | — |
| `false` | 不匹配（对象不变，`*pExpected` 已回写）或参数非法 | 参数非法时对象与 `*pExpected` 均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空/未对齐、`pExpected` 为空、失败顺序含 Release 或强于成功顺序

#### 范例

[core/atomic_tour · CAS](../../examples/core/atomic_tour/main.c) · 失败路径把实际值 0x0A 回写

```c
iExpected = 0x99u;  /* 期望错误：失败并把实际值 0x0A 写回 */
if ( xrtAtomic32CompareExchange(&A32, &iExpected, 0x0Bu,
		XMEMORY_ACQ_REL, XMEMORY_ACQUIRE) ||
	(iExpected != 0x0Au) ) {
	goto Cleanup;
}
```

### `xrtAtomic32FetchAdd`

原子加并返回旧值；无符号按模 2^32 回绕。

```c
uint32 xrtAtomic32FetchAdd(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 加数 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 加法前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 0x33 + 0x0C = 0x3F

```c
iOld32 = xrtAtomic32FetchAdd(&A32, 0x0Cu, XMEMORY_ACQ_REL);
if ( (iOld32 != 0x33u) ||
	(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x3Fu) ) {
```

### `xrtAtomic32FetchSub`

原子减并返回旧值；无符号按模 2^32 回绕。

```c
uint32 xrtAtomic32FetchSub(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 减数 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 减法前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 0x3F − 0x0F = 0x30

```c
iOld32 = xrtAtomic32FetchSub(&A32, 0x0Fu, XMEMORY_ACQ_REL);
if ( (iOld32 != 0x3Fu) ||
	(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x30u) ) {
```

### `xrtAtomic32FetchAnd`

原子按位与并返回旧值。

```c
uint32 xrtAtomic32FetchAnd(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 与运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 0x3C & 0x0F = 0x0C

```c
iOldAnd = xrtAtomic32FetchAnd(&A32, 0x0Fu, XMEMORY_ACQ_REL);
if ( iOldAnd != 0x3Cu ) {
```

### `xrtAtomic32FetchOr`

原子按位或并返回旧值。

```c
uint32 xrtAtomic32FetchOr(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 置位掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 或运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 0x0C | 0x06 = 0x0E

```c
iOldOr = xrtAtomic32FetchOr(&A32, 0x06u, XMEMORY_ACQ_REL);
if ( iOldOr != 0x0Cu ) {
```

### `xrtAtomic32FetchXor`

原子按位异或并返回旧值。

```c
uint32 xrtAtomic32FetchXor(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、4 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 翻转掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 异或运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · RMW 补集](../../examples/core/atomic_tour/main.c) · 0x0E ^ 0x0B = 0x05

```c
iOld32 = xrtAtomic32FetchXor(&A32, 0x0Bu, XMEMORY_ACQ_REL);
if ( (iOld32 != 0x0Eu) ||
	(xrtAtomic32Load(&A32, XMEMORY_ACQUIRE) != 0x05u) ) {
```

### `xrtAtomic64Load`

原子读取 64 位值。顺序与错误契约同 32 位形态。

```c
uint64 xrtAtomic64Load(const xatomic64* pAtomic, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、8 字节对齐 | 目标对象 |
| `iOrder` | 输入 | `RELAXED`/`ACQUIRE`/`SEQ_CST` | 加载允许的顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 当前值 | 原子快照 | 参数非法时返回 `0`；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空/未按 8 字节对齐，或顺序非法

#### 范例

[core/atomic · 计数器](../../examples/core/atomic/main.c) · 纯计数读用 RELAXED 足够

```c
printf("counter=%llu\n", (unsigned long long)xrtAtomic64Load(&Counter, XMEMORY_RELAXED));
```

### `xrtAtomic64Store`

原子写入 64 位值。失败时不写入、不设置错误。

```c
void xrtAtomic64Store(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 新值 |
| `iOrder` | 输入 | `RELAXED`/`RELEASE`/`SEQ_CST` | 存储允许的顺序 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 参数非法时不写入、不设置错误 |

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · RELEASE 写入后 ACQUIRE 核对

```c
xrtAtomic64Store(&A64, UINT64_C(7), XMEMORY_RELEASE);
if ( xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(7) ) {
```

### `xrtAtomic64Exchange`

原子替换 64 位值并返回旧值。

```c
uint64 xrtAtomic64Exchange(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 新值 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 替换前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 100 换 50，取回 100

```c
xrtAtomic64Init(&A64, UINT64_C(100));
iOld64 = xrtAtomic64Exchange(&A64, UINT64_C(50),
	XMEMORY_ACQ_REL);
```

### `xrtAtomic64CompareExchange`

64 位强比较交换；失败时把实际值写回 `*pExpected`，不产生伪失败。

```c
bool xrtAtomic64CompareExchange(
	xatomic64* pAtomic,
	uint64* pExpected,
	uint64 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `pExpected` | 输入/输出 | 非空、调用方独占 | 期望值；失败时被改写为实际值 |
| `iDesired` | 输入 | — | 成功时写入的值 |
| `iSuccess` | 输入 | 五种顺序均可 | 成功路径顺序 |
| `iFailure` | 输入 | 加载顺序，且不强于成功 | 失败路径顺序；不能含 Release |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 匹配并已写入 `iDesired` | — |
| `false` | 不匹配或参数非法 | 不匹配时 `*pExpected` 已回写；参数非法时对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空/未对齐、`pExpected` 为空、失败顺序含 Release 或强于成功顺序

#### 范例

[core/atomic · CAS](../../examples/core/atomic/main.c) · 成功序 ACQ_REL + 失败序 ACQUIRE

```c
if (
	xrtAtomic64CompareExchange(
		&Counter,
		&iExpected,
		10u,
		XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE
	)
) {
```

### `xrtAtomic64FetchAdd`

原子加并返回旧值；无符号按模 2^64 回绕。

```c
uint64 xrtAtomic64FetchAdd(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 加数 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 加法前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic · 计数器](../../examples/core/atomic/main.c) · 纯计数用 RELAXED

```c
(void)xrtAtomic64FetchAdd(&Counter, 1u, XMEMORY_RELAXED);
```

### `xrtAtomic64FetchSub`

原子减并返回旧值；无符号按模 2^64 回绕。

```c
uint64 xrtAtomic64FetchSub(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 减数 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 减法前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 50 − 20 = 30

```c
iOld64 = xrtAtomic64FetchSub(&A64, UINT64_C(20),
	XMEMORY_ACQ_REL);
if ( (iOld64 != UINT64_C(50)) ||
	(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(30)) ) {
```

### `xrtAtomic64FetchAnd`

原子按位与并返回旧值。

```c
uint64 xrtAtomic64FetchAnd(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 与运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 30 & 28 = 28

```c
iOld64 = xrtAtomic64FetchAnd(&A64, UINT64_C(0x1C),
	XMEMORY_ACQ_REL);
if ( (iOld64 != UINT64_C(30)) ||
	(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(28)) ) {
```

### `xrtAtomic64FetchOr`

原子按位或并返回旧值。

```c
uint64 xrtAtomic64FetchOr(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 置位掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 或运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 28 | 3 = 31

```c
iOld64 = xrtAtomic64FetchOr(&A64, UINT64_C(3),
	XMEMORY_ACQ_REL);
if ( (iOld64 != UINT64_C(28)) ||
	(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != UINT64_C(31)) ) {
```

### `xrtAtomic64FetchXor`

原子按位异或并返回旧值。

```c
uint64 xrtAtomic64FetchXor(xatomic64* pAtomic, uint64 iValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、8 字节对齐 | 目标对象 |
| `iValue` | 输入 | — | 翻转掩码 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧值 | 异或运算前的原子快照 | 参数非法时返回 `0` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 64 位](../../examples/core/atomic_tour/main.c) · 31 ^ 31 = 0

```c
iOld64 = xrtAtomic64FetchXor(&A64, UINT64_C(31),
	XMEMORY_ACQ_REL);
if ( (iOld64 != UINT64_C(31)) ||
	(xrtAtomic64Load(&A64, XMEMORY_ACQUIRE) != 0u) ) {
```

## 原子指针

空指针是合法值。`CompareExchange` 通过布尔结果区分成功与失败，不依赖返回指针是否为空。Atomic 只管理指针槽，不负责目标对象的引用计数、生命周期或内存回收；无锁链表仍必须单独解决 ABA 和安全回收问题。

### `xrtAtomicPtrLoad`

原子读取指针值。

```c
ptr xrtAtomicPtrLoad(const xatomicptr* pAtomic, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、按指针对齐 | 目标对象 |
| `iOrder` | 输入 | `RELAXED`/`ACQUIRE`/`SEQ_CST` | 加载允许的顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 当前指针 | 原子快照；空指针是合法值 | 参数非法时返回 `NULL`；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 指针](../../examples/core/atomic_tour/main.c) · ACQUIRE 读取交换结果

```c
if ( (pOld != (ptr)&s_Target[0]) ||
	(xrtAtomicPtrLoad(&APtr, XMEMORY_ACQUIRE) !=
		(ptr)&s_Target[1]) ) {
```

### `xrtAtomicPtrStore`

原子写入指针值。失败时不写入、不设置错误。

```c
void xrtAtomicPtrStore(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入 | 非空、按指针对齐 | 目标对象 |
| `pValue` | 输入 | 允许空 | 新指针值 |
| `iOrder` | 输入 | `RELAXED`/`RELEASE`/`SEQ_CST` | 存储允许的顺序 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 参数非法时不写入、不设置错误 |

#### 范例

[core/atomic_tour · 指针](../../examples/core/atomic_tour/main.c) · CAS 后直接写回

```c
xrtAtomicPtrStore(&APtr, (ptr)&s_Target[1], XMEMORY_RELEASE);
if ( xrtAtomicPtrLoad(&APtr, XMEMORY_ACQUIRE) !=
	(ptr)&s_Target[1] ) {
```

### `xrtAtomicPtrExchange`

原子替换指针并返回旧值。

```c
ptr xrtAtomicPtrExchange(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、按指针对齐 | 目标对象 |
| `pValue` | 输入 | 允许空 | 新指针值 |
| `iOrder` | 输入 | 五种顺序均可 | 读改写顺序 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| 旧指针 | 替换前的原子快照 | 参数非法时返回 `NULL` 且对象不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针非法或顺序非法

#### 范例

[core/atomic_tour · 指针](../../examples/core/atomic_tour/main.c) · 发布后立即交换

```c
xrtAtomicPtrInit(&APtr, (ptr)&s_Target[0]);
pOld = xrtAtomicPtrExchange(&APtr, (ptr)&s_Target[1],
	XMEMORY_ACQ_REL);
```

### `xrtAtomicPtrCompareExchange`

指针强比较交换；失败时把实际指针写回 `*pExpected`，不产生伪失败。

```c
bool xrtAtomicPtrCompareExchange(
	xatomicptr* pAtomic,
	ptr* pExpected,
	ptr pDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pAtomic` | 输入/输出 | 非空、按指针对齐 | 目标对象 |
| `pExpected` | 输入/输出 | 非空、调用方独占 | 期望指针；失败时被改写为实际值 |
| `pDesired` | 输入 | 允许空 | 成功时写入的指针 |
| `iSuccess` | 输入 | 五种顺序均可 | 成功路径顺序 |
| `iFailure` | 输入 | 加载顺序，且不强于成功 | 失败路径顺序；不能含 Release |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|---|
| `true` | 匹配并已写入 `pDesired` | — |
| `false` | 不匹配（对象不变，`*pExpected` 已回写）或参数非法 | 参数非法时对象与 `*pExpected` 均不变；错误经 `xrtGetError()` 报告 |

#### 错误

- `XERR_ARGUMENT` — 指针为空、`pExpected` 为空、失败顺序含 Release 或强于成功顺序

#### 范例

[core/atomic_tour · 指针](../../examples/core/atomic_tour/main.c) · 错误期望触发失败回写

```c
pExpected = (ptr)&s_Target[0];  /* 错误期望：失败回写 */
if ( xrtAtomicPtrCompareExchange(&APtr, &pExpected,
		(ptr)&s_Target[0], XMEMORY_ACQ_REL,
		XMEMORY_ACQUIRE) ||
	(pExpected != (ptr)&s_Target[1]) ) {
	goto Cleanup;
}
```

## 栅栏与自旋

### `xrtAtomicThreadFence`

建立线程间栅栏；参数非法时是空操作。

```c
void xrtAtomicThreadFence(xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iOrder` | 输入 | 五种顺序均可 | 栅栏强度；`RELAXED` 无同步效果 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 非法顺序静默忽略，不设置错误 |

#### 范例

[core/atomic_tour · 杂项](../../examples/core/atomic_tour/main.c) · 与读改写配对

```c
xrtAtomicThreadFence(XMEMORY_ACQ_REL);
xrtAtomicSignalFence(XMEMORY_SEQ_CST);
xrtAtomicPause();
```

### `xrtAtomicSignalFence`

只约束编译器对当前线程与信号处理器可见访问的重排，不生成 CPU 栅栏。

```c
void xrtAtomicSignalFence(xmemoryorder iOrder);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iOrder` | 输入 | 五种顺序均可 | 编译器重排约束强度 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 非法顺序静默忽略，不设置错误 |

#### 范例

[core/atomic_tour · 杂项](../../examples/core/atomic_tour/main.c) · 三件套并置

```c
xrtAtomicThreadFence(XMEMORY_ACQ_REL);
xrtAtomicSignalFence(XMEMORY_SEQ_CST);
xrtAtomicPause();
```

### `xrtAtomicPause`

短自旋提示：不让出时间片、不等待事件，也不保证公平性。长等待应使用后续同步原语或任务等待源。

```c
void xrtAtomicPause(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 |
|---|---|
| 无 | 纯提示，不会失败 |

#### 范例

[core/atomic_tour · 杂项](../../examples/core/atomic_tour/main.c) · 自旋循环体内插入

```c
xrtAtomicThreadFence(XMEMORY_ACQ_REL);
xrtAtomicSignalFence(XMEMORY_SEQ_CST);
xrtAtomicPause();
```

## 错误

空地址、错误对齐、空 `Expected` 或非法内存顺序设置 `XERR_ARGUMENT`。失败的写操作不修改对象。成功操作不会清除调用前已有错误。`Init`、`Store`、栅栏与 `Pause` 对非法参数一律静默忽略（空操作），读改写族在参数非法时返回零值/假并设置错误。

| 约束 | 适用函数 |
|---|---|
| 加载顺序（`RELAXED`/`ACQUIRE`/`SEQ_CST`） | 全部 `Load` |
| 存储顺序（`RELAXED`/`RELEASE`/`SEQ_CST`） | 全部 `Store` |
| 五种顺序 | `Exchange`、`Fetch*`、`ThreadFence`、`SignalFence` |
| 失败顺序不得含 Release、不得强于成功顺序 | 全部 `CompareExchange` |

完整可编译示例位于 `examples/core/atomic`，并发计数与单头文件测试分别位于 `tests/core/test_atomic_threads.c` 和 `tests/single/test_single_atomic.c`。
