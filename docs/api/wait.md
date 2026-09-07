# Wait

`wait` 定义线程、协程、Future、任务和网络共用的等待结果与 deadline 口径。
启用宏为 `XRT_FEATURE_WAIT`，依赖 `XRT_FEATURE_TIME`。

## 类型与常量

### `xwaitresult`

等待结果把正常控制流与真正错误分开表达。

```c
typedef enum xwaitresult {
	XWAIT_ERROR = -1,
	XWAIT_OK = 0,
	XWAIT_TIMEOUT = 1,
	XWAIT_CANCELLED = 2,
	XWAIT_CLOSED = 3
} xwaitresult;
```

| 值 | 语义 |
|---|---|
| `XWAIT_ERROR` | 失败 |
| `XWAIT_OK` | 成功 |
| `XWAIT_TIMEOUT` | 超时 |
| `XWAIT_CANCELLED` | 已取消 |
| `XWAIT_CLOSED` | 已关闭 |

### `xdeadline`

截止时间使用 xrtClock 的单调微秒刻度。

```c
typedef uint64 xdeadline;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

## 时间单位

`xdeadline` 是 `xrtClock()` 使用的单调微秒刻度。它只适合计算进程内经过时间，
不能与 Unix 时间、UTC 时间或本地时间互换。所有名称以 `For` 结尾的等待接收相对微秒数，
以 `Until` 结尾的等待接收绝对 `xdeadline`。

```c
typedef uint64 xdeadline;
```

`XRT_DEADLINE_NEVER` 表示永不超时。把 `UINT64_MAX` 作为相对超时传给
`xrtDeadlineAfter()` 也会得到这个值。

## 等待结果

`xwaitresult` 是所有可等待模块共用的结果类型：

| 值 | 含义 | 是否设置错误 |
|---|---|---|
| `XWAIT_OK` | 操作完成 | 否 |
| `XWAIT_TIMEOUT` | deadline 已到 | 否 |
| `XWAIT_CANCELLED` | 操作被协作取消 | 否 |
| `XWAIT_CLOSED` | 等待源已经关闭 | 否 |
| `XWAIT_ERROR` | 参数、状态或平台调用失败 | 是 |

超时、取消和关闭属于正常控制流，不覆盖当前线程的错误。只有 `XWAIT_ERROR` 才应读取
`xrtErrorGet()` 或 `xrtGetError()`。

## 函数

### `xrtDeadlineAfter`

从当前单调时钟和相对微秒数构造截止时间，溢出时返回 `NEVER`。

```c
xdeadline xrtDeadlineAfter(uint64 iTimeout)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iTimeout` | 输入 | — | 相对微秒数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 截止时间 | 可传给 `*WaitUntil` 族 | — |
| `NEVER` | 溢出 | 不设错误 |

#### 错误

- 无 — 纯计算，溢出饱和为 `NEVER`

#### 范例

[deadline](../../examples/concurrency/deadline/main.c) · 构造截止时间

```c
	xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(50000));
```

### `xrtDeadlineExpired`

判断截止时间是否已经到达；`NEVER` 永远不会到达。

```c
bool xrtDeadlineExpired(xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` / `false` | 是否已到达 | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[deadline](../../examples/concurrency/deadline/main.c) · 到达判断

```c
	printf("expired: %s\n", xrtDeadlineExpired(iDeadline) ? "yes" : "no");
```

### `xrtDeadlineRemaining`

返回截止时间前剩余微秒数；已到达返回零，`NEVER` 返回 `UINT64_MAX`。

```c
uint64 xrtDeadlineRemaining(xdeadline iDeadline)
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `iDeadline` | 输入 | — | 截止时间 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `>= 0` | 剩余微秒；已到达为 0，`NEVER` 为 `UINT64_MAX` | — |

#### 错误

- 无 — 纯查询，不设置错误

#### 范例

[deadline](../../examples/concurrency/deadline/main.c) · 剩余时间

```c
		(unsigned long long)xrtDeadlineRemaining(iDeadline));
```

## 模块契约：所有权

截止时间为纯值类型，按值传递，无所有权语义。

## 示例

```c
xdeadline iDeadline = xrtDeadlineAfter(UINT64_C(250000));

while ( !operationReady() ) {
	if ( xrtDeadlineExpired(iDeadline) ) {
		return XWAIT_TIMEOUT;
	}
	waitOnce(xrtDeadlineRemaining(iDeadline));
}
return XWAIT_OK;
```

可运行示例见 `examples/concurrency/deadline/main.c`，边界测试见
`tests/concurrency/test_wait.c`。
