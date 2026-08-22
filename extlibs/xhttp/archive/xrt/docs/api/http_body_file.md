# HTTP 文件正文

## 定位

`http_body_file` 把异步文件读取适配为统一 `xhttpbody`，供 HTTP 客户端上传、
服务器响应和其他正文消费者复用。模块不设置状态码、媒体类型、`Range`、
`ETag` 或缓存字段；这些属于更高层的静态文件响应策略。

连续区间读取实现同时作为内部 cursor 提供给
`XRT_FEATURE_HTTP_STATIC_MULTIPART_BODY`。单区间文件 Body 与多范围静态 Body
共用同一异步提交、短读检测、Future 数据租约、取消和关闭路径，没有复制第二
套文件读取状态机。

模块依赖：

- `XRT_FEATURE_HTTP_BODY_ASYNC`
- `XRT_FEATURE_FILE_ASYNC`

## 读取配置

```c
#define XHTTP_BODY_FILE_READ_DEFAULT 65536u

typedef struct xhttpbodyfileconfig {
	size_t ReadSize;
} xhttpbodyfileconfig;

XRT_API void xrtHttpBodyFileConfigInit(
	xhttpbodyfileconfig* pConfig);
```

`ReadSize` 必须大于零，默认 64 KiB。它限制一次异步文件读取申请，不会在 Body、Reader 或连接中预留同等大小的固定缓冲；每次实际读取量为文件剩余长度、调用方 `MaxBytes` 和 `ReadSize` 的最小值。因此，大文件消费者即使传入 `SIZE_MAX` 也不会触发整段文件分配，高并发服务可以调小它控制每个活跃读取的瞬时内存，高吞吐顺序传输可以在自己的负载测试后调大。

初始化输出和构造输入都可以未对齐，但必须是完整且不回绕的内存区间。构造函数在返回前复制配置，之后调用方可以立即修改或释放原结构。零读取粒度会以 `XERR_ARGUMENT` 同步失败，并且采用入口不会接管文件。

## 分层入口

低层入口采用已经准备好的异步文件：

```c
xhttpbody* body = xrtHttpBodyFileAdopt(
	file, offset, length, NULL);
```

成功后正文独占 `file` 的关闭责任；失败时调用方仍然拥有它。文件必须可读，
偏移和长度必须落在跨平台 64 位文件偏移范围内。这个入口不查询路径或文件
大小，适合自定义存储、已经打开的句柄和调用方已知长度的场景。

高层入口在有界任务池中打开路径，并从同一个已打开句柄取得大小：

```c
xfuture* prepared = xrtHttpBodyFileFuture(pool, path, NULL);
xfuture* range = xrtHttpBodyFileRangeFuture(
	pool, path, offset, length, NULL);
```

因此不存在“先按路径查询大小、再打开另一个对象”的 TOCTOU 窗口。区间入口
采用严格语义：`offset + length` 超出已打开文件时，Future 以
`XHTTP_BODY_FILE_ERROR_RANGE` 失败，不静默裁剪。

## Future 所有权

准备 Future 成功值是借用的 `xhttpbody`。Future 拥有一个正文引用：

```c
xhttpbody* body = xrtHttpBodyRef(
	(xhttpbody*)xrtFutureValue(prepared)
);
xrtFutureDestroy(prepared);
```

把正文传给会增加引用的 Request 或 Reply 后，也可以直接释放准备 Future。
任务池由调用方拥有，必须存活到正文 Reader 关闭并且异步文件关闭完成。

## 读取与内存

文件正文不可重放，因为它独占一个已经打开的文件。`Open` 只分配 Reader，
不执行文件系统调用。每个 Reader 最多保留一次异步读 Future：

1. `Next` 提交读取并返回 `XHTTP_BODY_AGAIN`。
2. `Wait` 返回当前读取 Future 的独立引用。
3. 读取完成后，`Next` 发布不超过当前 `MaxBytes` 的 Chunk。
4. Chunk 通过 Future 引用保护连续结果缓冲，不复制文件数据。

没有每连接固定 8 KiB 或 64 KiB 缓冲。实际读取大小由正文消费者的
`MaxBytes`、配置的 `ReadSize` 和区间剩余长度共同决定；TCP 引用发送可以让 Chunk 晚于 Reader 和 Body 释放。

## 变化与背压

准备完成后文件若缩短，读取不会伪造声明长度，而是以
`XHTTP_BODY_FILE_ERROR_READ` 失败。文件增长不改变已经冻结的正文长度。

准备任务或读取任务达到任务池硬队列上限时，调用返回错误，cause 为
`XERR_AGAIN`。这是一条显式负载拒绝路径，不会无界堆积等待对象。服务端应为
文件 I/O 配置容量合适的专用池，并在响应提交前处理准备阶段的过载。

## 错误

错误域为 `http.body.file`：

- `XHTTP_BODY_FILE_ERROR_SUBMIT`
- `XHTTP_BODY_FILE_ERROR_OPEN`
- `XHTTP_BODY_FILE_ERROR_SIZE`
- `XHTTP_BODY_FILE_ERROR_RANGE`
- `XHTTP_BODY_FILE_ERROR_ADOPT`
- `XHTTP_BODY_FILE_ERROR_CREATE`
- `XHTTP_BODY_FILE_ERROR_READ`

文件、异步文件和任务池错误通过 `cause` 保留。
