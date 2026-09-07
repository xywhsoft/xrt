# Spin

`spin` 提供不记录所有者、不可递归的短临界区锁。它先使用处理器暂停提示自旋，持续竞争时主动让出时间片；等待时间不可预测或临界区可能阻塞时，应改用 `mutex`。

## 裁剪

启用 `XRT_FEATURE_SPIN`，模块依赖 `atomic`。未启用时不声明类型和函数，也不编译实现。

## 生命周期

栈上对象使用 `xrtSpinInit()` 和 `xrtSpinUnit()`，静态对象使用 `XRT_SPIN_INIT`，动态对象使用 `xrtSpinCreate()` 和 `xrtSpinDestroy()`。释放仍被持有的锁会失败并设置 `XERR_STATE`。

```c
xspinlock Lock;

if ( !xrtSpinInit(&Lock) ) {
	return false;
}
if ( !xrtSpinLock(&Lock) ) {
	return false;
}
/* 短小且不阻塞的共享状态更新。 */
(void)xrtSpinUnlock(&Lock);
(void)xrtSpinUnit(&Lock);
```

## 进入与退出

`xrtSpinLock()` 等待直到获得锁。`xrtSpinTryLock()` 只尝试一次，锁繁忙时返回 `false` 且不设置错误。`xrtSpinUnlock()` 使用 Release 顺序发布临界区内的写入。

锁不记录线程所有者，调用方必须保证只有持有者执行解锁；同一执行流重复进入会永久等待。协程、任务或网络 Worker 不应在持锁期间执行阻塞、等待或用户回调。

完整示例位于 `examples/concurrency/spin`，竞争回归位于 `tests/concurrency/test_spin_threads.c`。

## API

### `xrtSpinInit`

初始化调用方提供的自旋锁存储。

```c
bool xrtSpinInit(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输出 | 非空 | 接收自旋锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已初始化 | — |
| `false` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 初始化

```c
	if ( !xrtSpinInit(&Spin) ) {
```

### `xrtSpinUnit`

释放自旋锁状态；锁仍被持有时失败。

```c
bool xrtSpinUnit(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输入 | 非空、未持有 | 目标自旋锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 锁仍被持有 | `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 锁仍被持有、未初始化或状态非法

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 释放状态

```c
	if ( !xrtSpinUnlock(&Spin) || !xrtSpinUnit(&Spin) ) {
```

### `xrtSpinCreate`

创建一个动态分配的自旋锁。

```c
xspinlock* xrtSpinCreate(void)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无参数 | — | — | — |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 自旋锁 | — |
| `NULL` | 分配失败 | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY` — 分配失败

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 堆创建

```c
		xspinlock* pHeap = xrtSpinCreate();
```

### `xrtSpinDestroy`

释放动态自旋锁；空指针视为空操作。

```c
bool xrtSpinDestroy(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放 | — |
| `false` | 锁仍被持有 | `XERR_STATE` |

#### 错误

- `XERR_STATE` — 锁仍被持有时空指针之外路径失败

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 销毁

```c
			!xrtSpinDestroy(pHeap) ) {
```

### `xrtSpinLock`

自适应等待并进入短临界区。

```c
bool xrtSpinLock(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输入 | 非空、已初始化 | 目标自旋锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已进入临界区 | — |
| `false` | 参数或状态非法 | `XERR_ARGUMENT` / `XERR_STATE` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 锁仍被持有、未初始化或状态非法

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 加锁

```c
	if ( !xrtSpinLock(&Spin) ) {
```

### `xrtSpinTryLock`

尝试进入短临界区；锁繁忙时不设置错误。

```c
bool xrtSpinTryLock(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输入 | 非空、已初始化 | 目标自旋锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已进入临界区 | — |
| `false` | 锁繁忙或参数非法 | 忙碌不设错 |

#### 错误

- 锁繁忙返回 `false` 且不设置错误；参数或状态非法 `XERR_ARGUMENT` / `XERR_STATE`

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 尝试加锁

```c
			xrtSpinTryLock(pHeap) ||  /* 已持有：Try 必失败 */
```

### `xrtSpinUnlock`

离开短临界区。

```c
bool xrtSpinUnlock(xspinlock* pSpin)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pSpin` | 输入 | 非空、已持有 | 目标自旋锁 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已离开临界区 | — |
| `false` | 未持有或参数非法 | `XERR_STATE` / `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_STATE` — 锁仍被持有、未初始化或状态非法

#### 范例

[spin](../../examples/concurrency/spin/main.c) · 解锁

```c
	if ( !xrtSpinUnlock(&Spin) || !xrtSpinUnit(&Spin) ) {
```
