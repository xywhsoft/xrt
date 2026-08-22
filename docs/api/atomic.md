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

`xrtAtomicIsLockFree(iSize)` 查询自然对齐的指定字节宽度是否由当前目标无锁实现。当前公开标量只使用 4 字节、8 字节和指针宽度；其他宽度返回 `false`。查询采用保守口径：后端不能静态证明无锁时返回 `false`，但这不表示对应 Atomic API 失去原子语义。需要对外宣称 lock-free 的算法必须先检查其最大原子宽度，不能把“操作语义原子”误写成“实现一定无锁”。

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

```c
uint32 xrtAtomic32Load(const xatomic32* pAtomic, xmemoryorder iOrder);
void xrtAtomic32Store(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
uint32 xrtAtomic32Exchange(xatomic32* pAtomic, uint32 iValue, xmemoryorder iOrder);
bool xrtAtomic32CompareExchange(
	xatomic32* pAtomic,
	uint32* pExpected,
	uint32 iDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);
```

32 位和 64 位类型都提供 `Load`、`Store`、`Exchange`、`CompareExchange`、`FetchAdd`、`FetchSub`、`FetchAnd`、`FetchOr`、`FetchXor`。`Fetch*` 返回修改前的值，无符号加减按模宽度回绕。

强比较交换不产生伪失败。成功时写入 `Desired`；失败时对象不变，并把当前实际值写回 `Expected`，因此调用方可直接继续 CAS 循环。`Expected` 必须指向调用方独占的普通标量，不得与原子对象重叠；它本身的并发保护由调用方负责。

原生后端直接使用平台的加法和按位读改写指令。必须走 CAS 的退化路径会复用失败操作返回的实际值，不会在每次竞争失败后额外执行一次原子加载。

## 原子指针

```c
ptr xrtAtomicPtrLoad(const xatomicptr* pAtomic, xmemoryorder iOrder);
void xrtAtomicPtrStore(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);
ptr xrtAtomicPtrExchange(xatomicptr* pAtomic, ptr pValue, xmemoryorder iOrder);
bool xrtAtomicPtrCompareExchange(
	xatomicptr* pAtomic,
	ptr* pExpected,
	ptr pDesired,
	xmemoryorder iSuccess,
	xmemoryorder iFailure
);
```

空指针是合法值。`CompareExchange` 通过布尔结果区分成功与失败，不依赖返回指针是否为空。Atomic 只管理指针槽，不负责目标对象的引用计数、生命周期或内存回收；无锁链表仍必须单独解决 ABA 和安全回收问题。

## 栅栏与自旋

`xrtAtomicThreadFence()` 建立线程间栅栏，`xrtAtomicSignalFence()` 只约束编译器对当前线程与信号处理器可见访问的重排。`xrtAtomicPause()` 是短自旋提示，不让出时间片、不等待事件，也不保证公平性；长等待应使用后续同步原语或任务等待源。

## 错误

空地址、错误对齐、空 `Expected` 或非法内存顺序设置 `XERR_ARGUMENT`。失败的写操作不修改对象。成功操作不会清除调用前已有错误。

完整可编译示例位于 `examples/core/atomic`，并发计数与单头文件测试分别位于 `tests/core/test_atomic_threads.c` 和 `tests/single/test_single_atomic.c`。
