# Wait

`wait` 定义线程、协程、Future、任务和网络共用的等待结果与 deadline 口径。
启用宏为 `XRT_FEATURE_WAIT`，依赖 `XRT_FEATURE_TIME`。

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

```c
xdeadline xrtDeadlineAfter(uint64 iTimeout);
```

使用当前单调时钟和相对微秒数构造 deadline。加法溢出或 `iTimeout == UINT64_MAX`
时返回 `XRT_DEADLINE_NEVER`。相对超时应在重试循环外只转换一次，避免伪唤醒延长总超时。

### `xrtDeadlineExpired`

```c
bool xrtDeadlineExpired(xdeadline iDeadline);
```

deadline 已到时返回 `true`；`XRT_DEADLINE_NEVER` 永远返回 `false`。

### `xrtDeadlineRemaining`

```c
uint64 xrtDeadlineRemaining(xdeadline iDeadline);
```

返回剩余微秒数。已到期返回 `0`，`XRT_DEADLINE_NEVER` 返回 `UINT64_MAX`。

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
