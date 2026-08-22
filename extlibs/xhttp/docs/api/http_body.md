# HTTP 正文源

## 定位

`http_body` 为客户端上传、服务器流式响应和 multipart 编码提供统一正文来源。
它不解析 HTTP/1 分帧，也不拥有网络连接。

## 所有权

- `xhttpbody` 是线程安全引用对象。
- `xhttpbodyreader` 是单消费者对象，不允许并发调用。
- `xhttpbodychunk` 拥有独立数据租约，必须调用
  `xrtHttpBodyChunkRelease()`。
- Chunk 可以晚于 Reader 和 Body 释放，适合 TCP 引用发送。
- `Borrow` 不延长外部内存生命周期；`Copy`、`Take` 和 `Reference` 提供拥有路径。
- `Reference` 必须提供释放过程；只借用数据应使用 `Borrow`。

## 固定正文

```c
xhttpbody* body = xrtHttpBodyCopy(
	(xbytesview){ (const uint8*)json, json_size }
);
```

`Empty`、`Copy`、`Borrow`、`Take` 和 `Reference` 创建的正文都可重放，已知长度
等于输入字节数。`Copy` 把描述符和正文副本放在一个按实际长度分配的紧凑块中，
不会为同一份固定正文建立第二个数据分配。

`xrtHttpBodyView` 可以借用固定正文的连续字节，不打开 Reader，也不复制数据。
自定义、文件和变换正文不保证连续存储，此时它返回 `false`、清空输出视图，并保持
线程原有错误不变。返回视图的生命周期不超过 Body，调用方不能修改或释放底层数据。

所有输出参数都必须与其读取的对象状态分离。`View` 的输出描述符、`Next` 的 Chunk
描述符，以及 `Read` 的输出缓冲和长度槽都不能覆盖 Reader、Body 或固定正文的底层
字节；`Read` 的输出缓冲和长度槽也不能相互覆盖。别名参数错误不会推进 Reader，
也不会先清空输出长度。该限制保证固定 Body 在重放和并发打开时始终保持不可变。

## 自定义来源

```c
xhttpbodyops ops = {
	open_body,
	destroy_factory
};

xhttpbody* body = xrtHttpBodyCreate(
	&ops,
	factory,
	content_length,
	XHTTP_BODY_REPLAYABLE
);
```

`Open` 返回独立的 `xhttpbodyreaderops` 和 Reader 上下文。可重放来源必须允许
并发打开独立 Reader；不可重放来源一生最多尝试一次打开。`Open` 失败时来源
必须自行回收本次尝试创建的全部 Reader 资源。XRT 会在进入 `Open` 前隔离调用线程
已有错误：来源发布的错误会被准确保留，即使它与进入调用前的错误对象相同；来源未
设置错误时才补充 `http.body` 来源错误。

`Next` 的规则：

- `DATA` 必须返回 `1..MaxBytes` 字节以及非空释放过程。
- `EOF`、`AGAIN` 和 `ERROR` 不得附带 Chunk。
- 已知长度必须与全部 DATA 的总和完全一致。
- `ERROR` 应设置结构化错误；未设置时 XRT 补充 `http.body` 来源错误。

来源违反规则后，Reader 固定为失败终态，后续调用重放同一个错误。

`Destroy`、Reader `Close` 和 Chunk `Release` 都是无返回清理边界。XRT 在调用这些
用户过程时保存并恢复调用线程的当前错误，因此正常销毁和 OOM 回滚不会被清理回调
产生的次要错误覆盖。回调仍应完成全部资源释放，不能依赖错误返回影响清理流程。

## 异步来源

启用 `XHTTP_FEATURE_HTTP_BODY_ASYNC` 后，Reader 操作可以提供 `Wait`。来源只有在
同时提供 `Wait` 时才能返回 `AGAIN`：

```c
status = xrtHttpBodyNext(reader, limit, &chunk);
if ( status == XHTTP_BODY_AGAIN ) {
	xfuture* ready = xrtHttpBodyReaderWait(reader);
}
```

调用方拥有返回的 Future，可以用统一 Future API 等待、取消或在协程中挂起。
每个 `AGAIN` 只能成功取得一次 Future，重复 Wait 是状态错误。Future 完成只是可读性
提示，调用方必须重新执行 `Next`，并允许来源因竞争再次返回 `AGAIN`。销毁调用方持有的
Future 只释放这一个等待引用，不取消 Reader；来源返回的 Future 必须独立持有自身所需
状态，即使 Reader 随后关闭也仍可安全完成或销毁。Wait 创建失败会把 Reader 固化为
失败终态，后续 `Next` 或 Wait 都重放同一个结构化错误。

在调用 `Next` 后直接再次轮询是允许的，但高层传输不应忙等；收到 `AGAIN` 后应领取
Future 并把执行权交还调度器。同步等待、任务和协程都消费同一个 Future 契约，不建立
第二套正文状态机。

## 有界生产流

启用 `XHTTP_FEATURE_HTTP_BODY_STREAM` 后，`xrtHttpBodyStreamCreate()` 同时返回一个
未知长度、不可重放的 `xhttpbody` 和一个并发生产端：

```c
xhttpbodystreamconfig config;
xhttpbodystream* stream;
xhttpbody* body;

xrtHttpBodyStreamConfigInit(&config);
config.MaxBytes = 1024 * 1024;
config.MaxChunks = 256;
body = xrtHttpBodyStreamCreate(&config, &stream);
```

`stream` 输出不能为空，也不能覆盖传入的非空 `config`；参数失败不会修改重叠存储。
配置与输出句柄可以位于完整但未对齐的存储中，地址范围回绕会在任何读写前失败。
对象只保存配置副本。配置和临时输出在创建返回后都不再被借用。

Stream 是多生产者、单消费者队列。`xrtHttpBodyStreamRef()` 为每个生产者建立独立
引用；最后一个生产端调用 `xrtHttpBodyStreamDestroy()` 时自动发布正常 EOF。需要提前
结束全部生产者时调用 `xrtHttpBodyStreamClose()`；永久生产失败使用
`xrtHttpBodyStreamFail()`，其 Cause 会保留在 `xrt.http.body.stream` 错误链中。

`MaxBytes` 与 `MaxChunks` 是硬预算，不是通知水位。预算包含并发写入已经预留的空间、
排队节点和 Reader 尚未释放的活动 Chunk 租约，因此慢消费者不会让内存无限增长。
一次写入形成一个队列节点；Reader 按 `MaxBytes` 切分节点时，必须释放最后一个切片后才
归还该节点的全部预算。单次写入大于 `MaxBytes` 是范围错误，预算暂时不足则返回
`XHTTP_BODY_STREAM_AGAIN`。

三种写入路径的所有权不同：

- `xrtHttpBodyStreamWrite()` 复制输入，成功前后都不接管调用方数据。
- `xrtHttpBodyStreamWriteRef()` 仅在 `OK` 时接管释放过程；`AGAIN`、`CLOSED` 和
  `ERROR` 仍由调用方负责。
- `xrtHttpBodyStreamWriteTake()` 仅在 `OK` 时接管由 `xrtMalloc()` 分配的数据。

三条路径都只接受非空、不回绕且不覆盖 Stream 内部状态的字节范围。该限制阻止调用方
把不透明对象本身误作载荷并在消费时破坏同步状态。`WriteRef` 和 `WriteTake` 的任何
非 `OK` 结果都严格保持调用方所有权，释放回调也不会执行。

生产者遇到 `AGAIN` 后调用 `xrtHttpBodyStreamWaitWritable()`。返回值是当前可写代际的
共享 Future。一次失败写入会打开真实背压代际；即使当前还剩少量字节或节点槽，Future
也至少等待下一次预算释放，避免原 Chunk 放不下时形成 `AGAIN -> 立即完成 -> AGAIN` 的
忙循环。成功仍只表示容量发生过可写进展，不保证竞争生产者的原写入能够放入，因此醒来
后必须重试。调用方不再等待时只释放自己的 Future 引用，不应取消共享的可写代际。
生产端关闭时 Future 进入 `CLOSED`，失败时进入 `FAILED`，消费者释放 Reader 时同样关闭
全部生产等待。

Future 创建、Future 完成、引用释放回调以及错误处理器通知都发生在 Stream 互斥锁
之外，因此这些外部路径可以重入 `xrtHttpBodyStreamInfo()`，不会与生产队列形成锁递归。
同一代际可以被多个生产者共享；某个调用方销毁自己的 Future 引用不会取消其他等待者。

Reader 继续使用通用 `AGAIN + xrtHttpBodyReaderWait()` 契约。Wait 创建失败会按通用
Body 规则把该 Reader 固化为失败终态；销毁 Reader 会关闭唯一消费端、丢弃尚未租出的
节点并释放其外部所有权。已经借出的 Chunk 可晚于 Reader、Body 和全部生产端释放，
其内部引用保证底层 Stream 存活到租约归还。

`xrtHttpBodyStreamInfo()` 返回并发一致的预算和生命周期快照。`WrittenBytes` 与
`ReadBytes` 是累计受理量与累计借出量，达到 `UINT64_MAX` 后饱和，不发生回绕。
输出可以未对齐，但必须是完整且与 Stream 内部存储分离的范围；快照一次发布，结构填充
字节也会确定化。默认预算是 1 MiB 和 256 个 Chunk；对象本身不预分配这些载荷空间。
最后一个生产引用销毁时的自动 EOF 是清理动作，不覆盖调用线程已有错误；
需要观察关闭失败时应显式调用 `xrtHttpBodyStreamClose()`。

完整示例见 `examples/http/body_stream/main.c`。

## 流式变换

内部 `http_body_transform` 集中实现来源推进、输出块所有权、Chunk 租约、错误和异步
等待组合，不公开第二套正文 API。`XHTTP_FEATURE_HTTP_BODY_DEFLATE` 和
`XHTTP_FEATURE_HTTP_BODY_INFLATE` 分别提供 `xrtHttpBodyDeflate` 与
`xrtHttpBodyInflate`，算法模块只通过薄适配器接入这一套状态机。

两种变换都接受任意 Body 来源，按需在每个已打开 Reader 中创建独立编解码器，并继承
来源的重放能力。结果长度为 `XHTTP_BODY_UNKNOWN`，输出块按实际数据分配，不给 Body
对象预留固定传输缓冲。Deflate 和 Inflate 分别提供尚未交付输出的 `QueueLimit`；
Inflate 还默认带 64 MiB 解码总量上限，防止高压缩比线路表示造成无界瞬时队列或总量。

启用 `XHTTP_FEATURE_HTTP_BODY_DECODE` 后，`xrtHttpBodyDecodeFields` 可以把公共
Content-Encoding 计划组合成逆序 Inflate Body。未知编码会返回完整原始 Body，
不会进行部分解码；该便利层仍不修改 Header、Vary、摘要或 ETag，这些表示元数据由
独立的 HTTP 响应或客户端策略层处理。完整 API 与边界见
`http_body_deflate.md`、`http_body_inflate.md` 和 `http_body_decode.md`。

## 错误

正文来源错误域为 `http.body`：

- `XHTTP_BODY_ERROR_REOPEN`
- `XHTTP_BODY_ERROR_SOURCE`
- `XHTTP_BODY_ERROR_CONTRACT`
- `XHTTP_BODY_ERROR_LENGTH`

参数、状态、内存和范围错误仍使用 XRT 通用错误类别。
