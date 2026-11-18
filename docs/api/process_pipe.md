# Process Pipe API

选择 XRT_MODULE_PROCESS_PIPE 以启动一对子进程标准流。父端使用非阻塞 I/O，
子端保持阻塞式标准输入和输出。该模块复用 Process 的参数、环境、句柄继承
与进程回收，每个管道对象只有一个调用方驱动者。

Windows 创建本机字节模式命名管道，在父端启用重叠 I/O，并在执行子进程前核对
连接属于当前进程。POSIX 使用 socketpair 承载子进程 stdin，以便在截断时抑制
SIGPIPE，stdout 使用普通管道。

[Windows 匿名管道](https://learn.microsoft.com/en-us/windows/win32/ipc/anonymous-pipe-operations)
[Windows 重叠管道 I/O](https://learn.microsoft.com/en-us/windows/win32/ipc/synchronous-and-overlapped-input-and-output)
[Linux send](https://man7.org/linux/man-pages/man2/send.2.html)

## 类型与常量

### `xprocesspipe`

持有父端 stdin 和 stdout、唤醒信号以及 Windows 上每方向一个固定缓冲区
及完成对象。不同管道对象可由不同线程驱动。管道句柄与进程对象分别释放。

### `xprocesspipeio`

| 结果 | 意义 |
| --- | --- |
| `XPROCESS_PIPE_ERROR` | 本地参数或 I/O 错误，保留 Count |
| `XPROCESS_PIPE_AGAIN` | 暂无进展，Count 为零 |
| `XPROCESS_PIPE_PROGRESS` | 完成 Count 字节的连续前缀 |
| `XPROCESS_PIPE_EOF` | 子端输出关闭且已读完，Count 为零 |

`XPROCESS_PIPE_MAX_CHUNK` 为单次读写上限，1 MiB。Windows 按配置的
ChunkBytes 为每个方向分配一份暂存。POSIX 直接使用调用方提供的缓冲区。

## 函数

### `xrtProcessPipeSpawn`

接收借用的启动配置，返回之前配置及其引用均须有效。使用阻塞式 stdin/stdout 子端
替代配置中的两项输入，保留 stderr 策略。调用方负责排空另行捕获的 stderr 管道。

```c
XRT_API bool xrtProcessPipeSpawn(const xprocessconfig* Config,size_t ChunkBytes,
    xprocess** Process,xprocesspipe** Pipe);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Config` | 进程参数和环境等配置。 |
| `ChunkBytes` | 单次读写的正数上限。 |
| `Process` | 进程对象输出。 |
| `Pipe` | 父端管道输出。 |

#### 返回值

成功写出两个独立拥有的对象并返回 true。失败保留两个输出槽。

#### 错误

无效配置、Terminal 模式、内存耗尽、原生管道创建或进程启动失败均返回 false。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeRead`

非阻塞读取 stdout。在 AGAIN 后继续使用相同 Size，调用方可以替换 Data 指针。EOF 表示已排空。

```c
XRT_API xprocesspipeio xrtProcessPipeRead(xprocesspipe* Pipe,bytes Data,size_t Size,size_t* Count);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 一个有效的父端管道对象。 |
| `Data` | 调用方接收缓冲区。 |
| `Size` | 1 到 ChunkBytes 字节。 |
| `Count` | 输出本次完成的字节数。错误时保持原值。 |

#### 返回值

返回 xprocesspipeio 中的读写结果。

#### 错误

参数错误与平台 I/O 失败返回 XPROCESS_PIPE_ERROR。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeWrite`

非阻塞写入 stdin。在 AGAIN 后以相同 Size 和内容重试。Windows 可以在返回 AGAIN
前复制并提交请求，PROGRESS 确认该请求完成的前缀。部分进展后推进游标。

```c
XRT_API xprocesspipeio xrtProcessPipeWrite(xprocesspipe* Pipe,cbytes Data,size_t Size,size_t* Count);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 一个有效的父端管道对象。 |
| `Data` | 借用到调用返回的发送字节。 |
| `Size` | 1 到 ChunkBytes 字节。 |
| `Count` | 输出本次完成的字节数。错误时保持原值。 |

#### 返回值

返回 xprocesspipeio 中的读写结果。

#### 错误

参数错误、输入已关闭与平台 I/O 失败返回 XPROCESS_PIPE_ERROR。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeWait`

等待所选方向或唤醒。XWAIT_OK 后尝试非阻塞读写。Windows 的请求方向尚未提交 I/O 时
可以立即返回 XWAIT_OK，以便驱动者提交首次操作。

```c
XRT_API xwaitresult xrtProcessPipeWait(xprocesspipe* Pipe,bool Read,bool Write,int64 Timeout);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 一个有效的父端管道对象。 |
| `Read` | 关注读取方向。 |
| `Write` | 关注写入方向。 |
| `Timeout` | 相对毫秒预算。-1 为无限等待，0 为立即尝试。 |

#### 返回值

返回 XWAIT_OK、XWAIT_TIMEOUT 或 XWAIT_ERROR。

#### 错误

无效参数、时钟或等待失败返回 XWAIT_ERROR。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeWake`

跨线程非阻塞唤醒，允许合并多个信号。销毁之前须使其它线程退出唤醒调用。

```c
XRT_API bool xrtProcessPipeWake(xprocesspipe* Pipe);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 一个有效的父端管道对象。 |

#### 返回值

成功返回 true。

#### 错误

传入空指针或设置唤醒失败时返回 false。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeCloseInput`

幂等关闭父端 stdin 并保留 stdout。Windows 先取消并等待已提交的写入物理退出。
完整写入后执行关闭，可让子进程读完数据后观察 EOF。

```c
XRT_API bool xrtProcessPipeCloseInput(xprocesspipe* Pipe);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 一个有效的父端管道对象。 |

#### 返回值

完成关闭后返回 true。

#### 错误

传入空指针或平台关闭失败时返回 false。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。


### `xrtProcessPipeDestroy`

关闭父端句柄并释放唤醒资源。Windows 等待已提交的读写物理结束后释放内存。
子进程由调用方通过独立的 Process 结果管理。

```c
XRT_API void xrtProcessPipeDestroy(xprocesspipe* Pipe);
```

#### 参数

| 参数 | 语义 |
| --- | --- |
| `Pipe` | 有效的管道对象，或空指针。 |

#### 返回值

无返回值。

#### 范例

[启动并交换字节后回收子进程](../../examples/process/pipe/main.c)。
