# Wait API

`XRT_FEATURE_WAIT` 依赖 `XRT_FEATURE_TIME`，统一等待结果与相对毫秒参数。

```c
#define XRT_WAIT_FOREVER INT64_C(-1)
typedef enum xwaitresult {
    XWAIT_ERROR = -1,
    XWAIT_OK = 0,
    XWAIT_TIMEOUT = 1,
    XWAIT_CANCELLED = 2,
    XWAIT_CLOSED = 3
} xwaitresult;
```

线程、条件变量、事件、信号量、Future、协程、任务、进程和网络等待使用 `int64` 相对毫秒。0 表示非阻塞检查，`XRT_WAIT_FOREVER` 表示无限等待；其它负数返回参数错误。调度定时器同样使用相对毫秒，不支持无限到期点。

```c
xwaitresult Result = xrtEventWaitFor(Event, 50);
Result = xrtFutureWaitFor(Future, XRT_WAIT_FOREVER);
```

`XWAIT_TIMEOUT`、`XWAIT_CANCELLED`、`XWAIT_CLOSED` 是正常控制结果，不覆盖已有线程错误。`XWAIT_ERROR` 才表示需要读取 `xrtGetError()`。多步操作在内部使用同一个 Timer 预算，重试不会重新获得全部等待时间；超长原生等待会分段检查，有限等待不会变成无限等待。

不提供公开截止时间类型或截止点转换函数。测量耗时直接使用两次 `xrtTimer()` 的差，详见 [Time API](time.md)。

### `xwaitresult`

等待结果：成功、超时、取消和错误。超时是正常等待结局，不能一律视为线程错误。

```c
typedef enum xwaitresult {
    XWAIT_ERROR = -1, XWAIT_OK = 0, XWAIT_TIMEOUT = 1,
    XWAIT_CANCELLED = 2, XWAIT_CLOSED = 3
} xwaitresult;
```
