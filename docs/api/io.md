# IO

`io` 是同步、字节优先的通用输入输出层。它用于文件、内存、Buffer、压缩器和格式处理器之间的同步组合，不替代 TCP、TLS、HTTP 等带有等待、取消和背压语义的异步流对象。

## 裁剪

| 根模块 | 能力 | 依赖 |
| --- | --- | --- |
| `XRT_MODULE_IO` | 自定义 Reader/Writer、内存适配器、Copy | `core` |
| `XRT_MODULE_IO_BUFFER` | Buffer Reader/Writer、ReadAll | `io`, `buffer` |
| `XRT_MODULE_IO_FILE` | 文件借用/接管/路径构造器 | `io`, `file` |
| `XRT_MODULE_IO_LINE` | 有界流式逐行读取 | `io`, `buffer` |

各模块不反向依赖。只使用内存 IO 不会带入 Buffer、路径、时间或文件系统；Line Reader 不会拉入文件适配器或字符串高级功能。

## 对象契约

`xreader` 和 `xwriter` 是不透明对象。同一对象不提供内部并发串行化；多个线程或协程共享时，调用方必须在外部同步。

Reader 的非零 `Read` 成功返回零字节表示永久 EOF。EOF 会被对象锁定，后续读取直接返回零；成功 `xrtReaderSeek` 后解除。Writer 的非零写入不能成功返回零字节，该结果会转换为 `XIO_ERROR_NO_PROGRESS`，从而保证 Full 和 Copy 循环不会卡死。

`xrtReaderDestroy` 和 `xrtWriterDestroy` 对空指针为空操作。销毁一定释放对象，即使 `Close` 失败。Writer 销毁不会隐式调用 `xrtWriterFlush`：对文件而言，Flush 可能执行昂贵的持久化提交，必须由调用方明确选择。

回调收到的缓冲只在本次调用期间有效。回调返回 `false` 时应设置当前 `xerror`；没有设置时，IO 层补充 `xrt.io` 错误。回调返回超过请求量的计数属于实现契约错误。

## 类型

`xseek` 是文件与通用 IO 共用的定位原点枚举：`XSEEK_START`、`XSEEK_CURRENT` 和 `XSEEK_END` 分别以开头、当前游标和末尾为基准。

`xreadproc` 和 `xwriteproc` 分别执行一次允许短操作的同步读写。`xseekproc`、`xtellproc`、`xsizeproc`、`xflushproc` 和 `xcloseproc` 是可选能力。

`xreaderops` 只有 `Read` 必需，`xwriterops` 只有 `Write` 必需。创建函数复制整个回调表，因此回调表本身可以位于栈上；`Context` 的生命周期由调用方和可选 `Close` 共同决定。

`xioerror` 使用稳定域 `xrt.io`：

| 代码 | 含义 |
| --- | --- |
| `XIO_ERROR_READ` | Reader 回调失败 |
| `XIO_ERROR_WRITE` | Writer 回调失败 |
| `XIO_ERROR_SEEK` | 定位失败或不支持 |
| `XIO_ERROR_TELL` | 游标查询失败或不支持 |
| `XIO_ERROR_SIZE` | 大小查询失败或不支持 |
| `XIO_ERROR_FLUSH` | 显式刷新失败 |
| `XIO_ERROR_CLOSE` | 销毁时关闭失败 |
| `XIO_ERROR_EOF` | Full 或 CopyN 提前遇到 EOF |
| `XIO_ERROR_NO_PROGRESS` | Writer 对非零请求未消费数据 |
| `XIO_ERROR_LIMIT` | ReadAll、CopyLimit 或 Line Reader 超过硬上限 |
| `XIO_ERROR_CALLBACK` | 回调返回不合法计数 |

## 创建

### `xrtReaderCreate`

```c
xreader* xrtReaderCreate(const xreaderops* pOps, ptr pContext);
```

创建自定义 Reader。创建失败不会调用 `Close`，也不会消费 Context。销毁成功创建的 Reader 时至多调用一次 `Close`。

### `xrtWriterCreate`

```c
xwriter* xrtWriterCreate(const xwriterops* pOps, ptr pContext);
```

创建自定义 Writer，所有权规则与 Reader 一致。

### `xrtReaderFromMemory`

```c
xreader* xrtReaderFromMemory(xbytesview Data);
```

创建可定位、可查询大小的内存 Reader。对象只借用 Data，Data 必须保持有效且不得在 Reader 生存期内改变。

### `xrtWriterFromMemory`

```c
xwriter* xrtWriterFromMemory(ptr pData, size_t iCapacity);
```

创建固定容量内存 Writer。初始游标和逻辑大小都是零；Seek 可以移到容量范围内，随后写入会把逻辑大小之外的空洞填零。容量不足时允许完成可容纳的短写。

### `xrtWriterDiscard`

```c
xwriter* xrtWriterDiscard(void);
```

创建丢弃 Writer。它不保存数据，`xrtWriterTell` 和 `xrtWriterSize` 返回累计消费量。

## Reader

### `xrtReaderRead`

执行一次读操作。`pRead` 可空；失败时不会把回调的未验证计数发布给调用方。

### `xrtReaderReadFull`

持续处理短读，直到填满请求或失败。提前 EOF 返回 `XIO_ERROR_EOF`，`pRead` 保留已经完成的字节数。

### `xrtReaderCopy`

使用 16 KiB 固定栈缓冲复制到 EOF，不随输入总量分配内存。`pCopied` 在失败时也返回已经写入目标的字节数。

### `xrtReaderCopyN`

精确复制指定 `uint64` 长度。Reader 提前结束时返回 `XIO_ERROR_EOF`。

### `xrtReaderCopyLimit`

在硬上限内复制到 EOF。复制量恰好达到上限时读取一个探测字节：探测到 EOF 成功，探测到数据则消费该字节并返回 `XIO_ERROR_LIMIT`。

### `xrtReaderSeek`

按 `XSEEK_START`、`XSEEK_CURRENT` 或 `XSEEK_END` 移动游标。输出位置可空；成功后解除 EOF 锁定。

### `xrtReaderTell` 和 `xrtReaderSize`

查询当前位置和总大小。输出参数必需；适配器不提供对应能力时返回 `XERR_UNSUPPORTED`。

### `xrtReaderCanSeek`、`xrtReaderCanSize` 和 `xrtReaderEOF`

无失败地查询可选能力和 EOF 状态。空对象返回 `false`。

### `xrtReaderDestroy`

调用可选 Close 并释放 Reader。Close 失败仍然释放对象并返回 `false`。

## Line Reader

```c
typedef enum xlineend {
	XLINE_END_NONE = 0,
	XLINE_END_LF,
	XLINE_END_CRLF
} xlineend;

typedef enum xlinenext {
	XLINE_NEXT_ERROR = -1,
	XLINE_NEXT_END = 0,
	XLINE_NEXT_LINE = 1
} xlinenext;

typedef struct xlineview {
	xstrview Text;
	xlineend End;
} xlineview;
```

Line Reader 是通用 `xreader` 上的可选动态缓冲层。对象本身不包含固定 4K/8K 数组；第一次读取时才按需申请容量，长行按几何策略增长，并始终受调用方设置的内容字节上限约束。

`xlineview.Text` 不执行 UTF-8 校验，可以包含零字节，也不保证额外补零。视图借用内部缓冲，只在下一次 `xrtLineReaderNext` 或销毁前有效。LF 和 CRLF 会从内容中剥离并通过 `End` 明确返回；输入末尾没有终止符时返回 `XLINE_END_NONE`。终止换行之后不会额外产生一条空行，而输入中的真实空行会正常返回。

### `xrtLineReaderCreate`

```c
xlinereader* xrtLineReaderCreate(xreader* pReader, size_t iMaxLine);
```

创建借用底层 Reader 的行迭代器。`iMaxLine` 是剥离 LF 或 CRLF 后允许的最大内容字节数，必须大于零。Line Reader 会预读底层输入；提前销毁借用对象时，尚未发布的预读字节不会退回底层 Reader，因此同一数据流应持续通过 Line Reader 消费。

### `xrtLineReaderTake`

```c
xlinereader* xrtLineReaderTake(xreader** ppReader, size_t iMaxLine);
```

原子接管 Reader。成功后清空来源槽，销毁 Line Reader 时同时销毁底层 Reader；构造失败时槽和所有权保持不变。完整处理单一输入时优先使用该入口。

### `xrtLineReaderNext`

```c
xlinenext xrtLineReaderNext(xlinereader* pLines, xlineview* pLine);
```

返回下一行、正常结束或失败。短读以及跨读取边界的 CRLF 会被正确合并。超限返回 `XERR_RANGE / xrt.io / XIO_ERROR_LIMIT`；OOM 和底层读取错误保持原始结构化错误。流式错误发生后对象进入失败状态，后续迭代返回 `XERR_STATE`，但仍可安全销毁。

### `xrtLineReaderDestroy`

释放动态缓冲。借用模式不关闭底层 Reader；接管模式调用一次 `xrtReaderDestroy` 并传播关闭结果。空对象销毁成功执行空操作。

## Writer

### `xrtWriterWrite`

执行一次允许短写的操作。非零请求成功消费零字节会返回 `XIO_ERROR_NO_PROGRESS`。

### `xrtWriterWriteFull`

持续处理短写直到全部完成；`pWritten` 在失败时保留已消费量。

### `xrtWriterSeek`、`xrtWriterTell` 和 `xrtWriterSize`

定位或查询 Writer。固定内存 Writer 和 Buffer Writer 的 Size 是已经形成的逻辑内容长度，不是容量。

### `xrtWriterCanSeek` 和 `xrtWriterCanSize`

无失败地查询 Writer 可选能力。

### `xrtWriterFlush`

显式调用 Flush 回调；没有 Flush 回调时成功执行空操作。文件适配器把它映射为 `xrtFlush`。

### `xrtWriterDestroy`

调用可选 Close 并释放 Writer，不隐式调用 Flush。

## Buffer 适配器

### `xrtReaderFromBuffer`

创建借用 Buffer 的 Reader。Reader 生存期内不得修改、扩容、清空或销毁 Buffer。

### `xrtReaderTakeBuffer`

接管 `xbuffer**` 中的对象。成功时清空调用方槽，销毁 Reader 时销毁 Buffer；失败时槽和所有权保持不变。

### `xrtWriterFromBuffer`

创建借用 Buffer 的 Writer，初始游标在已有内容末尾。支持覆盖、追加和稀疏写，使用完整 `size_t` 容量范围。

### `xrtReaderReadAll`

在包含式硬上限内创建并返回新 Buffer。达到上限后使用一个探测字节区分精确长度和超限输入；零上限只接受空输入。

### `xrtWriterWriteBuffer`

完整写入 Buffer 当前有效内容。来源可以是目标 Buffer Writer 所引用 Buffer 的有效区域，底层 Buffer 保证自别名安全。

## 文件适配器

### `xrtReaderFromFile` 和 `xrtWriterFromFile`

创建借用文件对象的适配器。销毁适配器不会关闭文件；构造时会检查文件是否具有所需访问能力。

### `xrtReaderTakeFile` 和 `xrtWriterTakeFile`

接管 `xfile*` 槽。构造成功才清空槽，销毁适配器时关闭文件；构造失败保持文件所有权不变。

### `xrtReaderOpen`

以读取方式打开路径并返回拥有文件的 Reader。

### `xrtWriterOpen`

以创建、写入和截断方式打开路径并返回拥有文件的 Writer。

### `xrtWriterOpenAppend`

以操作系统追加语义创建拥有文件的 Writer。每次写入都位于文件末尾，即使共享游标曾被 Seek 修改。

## 示例

```c
unsigned char output[64];
xreader* reader = xrtReaderFromMemory(XRT_BYTES_LITERAL("hello"));
xwriter* writer = xrtWriterFromMemory(output, sizeof(output));
uint64 copied;

if ( !xrtReaderCopy(reader, writer, &copied) ) {
	/* 读取 xrtGetError() */
}
xrtReaderDestroy(reader);
xrtWriterDestroy(writer);
```

完整范例位于 `examples/io/memory`、`examples/io/buffer`、`examples/io/file` 和 `examples/io/line`。
