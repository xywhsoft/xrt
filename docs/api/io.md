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

### `xlineend`

行结束类型区分无终止符的末行、LF 与 CRLF。

```c
typedef enum xlineend {
	XLINE_END_NONE = 0,
	XLINE_END_LF,
	XLINE_END_CRLF
} xlineend;
```

| 值 | 语义 |
|---|---|
| `XLINE_END_NONE` | 无 |
| `XLINE_END_LF` | LF |
| `XLINE_END_CRLF` | CRLF 结尾 |

### `xlinenext`

行迭代结果明确区分正常结束、有效行和失败。

```c
typedef enum xlinenext {
	XLINE_NEXT_ERROR = -1,
	XLINE_NEXT_END = 0,
	XLINE_NEXT_LINE = 1
} xlinenext;
```

| 值 | 语义 |
|---|---|
| `XLINE_NEXT_ERROR` | 失败 |
| `XLINE_NEXT_END` | END |
| `XLINE_NEXT_LINE` | 已产出行 |

### `xlineview`

行内容借用到下一次迭代或销毁，不执行编码检查且不保证补零。

```c
typedef struct xlineview {
	xstrview Text;
	xlineend End;
} xlineview;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Text` | `xstrview` | 文本视图 |
| `End` | `xlineend` | 结束 |

### `xioerror`

IO 层稳定错误代码使用 xrt.io 域。

```c
typedef enum xioerror {
	XIO_ERROR_READ = 1,
	XIO_ERROR_WRITE,
	XIO_ERROR_SEEK,
	XIO_ERROR_TELL,
	XIO_ERROR_SIZE,
	XIO_ERROR_FLUSH,
	XIO_ERROR_CLOSE,
	XIO_ERROR_EOF,
	XIO_ERROR_NO_PROGRESS,
	XIO_ERROR_LIMIT,
	XIO_ERROR_CALLBACK
} xioerror;
```

| 值 | 语义 |
|---|---|
| `XIO_ERROR_READ` | 读方向 |
| `XIO_ERROR_WRITE` | 写方向 |
| `XIO_ERROR_SEEK` | 失败 |
| `XIO_ERROR_TELL` | 失败 |
| `XIO_ERROR_SIZE` | 尺寸 |
| `XIO_ERROR_FLUSH` | 刷新 |
| `XIO_ERROR_CLOSE` | 失败 |
| `XIO_ERROR_EOF` | 失败 |
| `XIO_ERROR_NO_PROGRESS` | 失败 |
| `XIO_ERROR_LIMIT` | 超限 |
| `XIO_ERROR_CALLBACK` | 回调失败 |

### `xreaderops`

Reader 回调表会在创建时复制；只有 Read 是必需过程。

```c
typedef struct xreaderops {
	xreadproc Read;
	xseekproc Seek;
	xtellproc Tell;
	xsizeproc Size;
	xcloseproc Close;
} xreaderops;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Read` | `xreadproc` | Read |
| `Seek` | `xseekproc` | Seek |
| `Tell` | `xtellproc` | Tell |
| `Size` | `xsizeproc` | 字节数 |
| `Close` | `xcloseproc` | Close |

### `xwriterops`

Writer 回调表会在创建时复制；只有 Write 是必需过程。

```c
typedef struct xwriterops {
	xwriteproc Write;
	xseekproc Seek;
	xtellproc Tell;
	xsizeproc Size;
	xflushproc Flush;
	xcloseproc Close;
} xwriterops;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Write` | `xwriteproc` | Write |
| `Seek` | `xseekproc` | Seek |
| `Tell` | `xtellproc` | Tell |
| `Size` | `xsizeproc` | 字节数 |
| `Flush` | `xflushproc` | Flush |
| `Close` | `xcloseproc` | Close |

### `xreader`

Reader 和 Writer 是同步字节 IO 对象；同一对象的操作必须由调用方串行化。

```c
typedef struct xreader xreader;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xwriter`

同步字节 Writer 对象（不透明）；同一对象的操作必须由调用方串行化。


```c
typedef struct xwriter xwriter;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xlinereader`

Line Reader 在通用 Reader 上提供有界流式行迭代。

```c
typedef struct xlinereader xlinereader;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xreadproc`

Read 和 Write 允许短操作；成功读取零字节只表示 EOF。

```c
typedef bool (*xreadproc)(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xwriteproc`

写入回调：向 `pContext` 写入至多 `iRequest` 字节，`pWritten` 返回实际写入数，`false` 表示失败。


```c
typedef bool (*xwriteproc)(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xseekproc`

可选定位、查询、刷新和关闭过程失败时必须设置当前错误。

```c
typedef bool (*xseekproc)(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xtellproc`

位置查询回调：返回 `pContext` 当前读写位置，`false` 表示对象不可寻址。


```c
typedef bool (*xtellproc)(ptr pContext, uint64* pPosition);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xsizeproc`

大小查询回调：返回 `pContext` 内容总字节数，`false` 表示大小不可知。


```c
typedef bool (*xsizeproc)(ptr pContext, uint64* pSize);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xflushproc`

冲刷回调：把 `pContext` 已缓冲数据提交到后端，`false` 表示失败。


```c
typedef bool (*xflushproc)(ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

### `xcloseproc`

关闭回调：释放 `pContext` 的后端资源；返回值保留供扩展，当前实现忽略。


```c
typedef bool (*xcloseproc)(ptr pContext);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

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
## API

### Reader 构造

### `xrtReaderCreate`

创建自定义 Reader；失败时 Context 所有权不变，销毁时调用一次 Close。

```c
xreader* xrtReaderCreate(
	const xreaderops* pOps,
	ptr pContext
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOps` | 输入 | 非空、静态存储 | 操作表 |
| `pContext` | 输入 | 成功后归 Reader | 自定义上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 自定义 Reader | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 构造](../../examples/io/stream_tour/main.c) · 观察

```c
	pCustom = xrtReaderCreate(&ReaderOps, &Mem);
```


### `xrtReaderFromMemory`

创建借用固定字节视图的可定位 Reader。

```c
xreader* xrtReaderFromMemory(xbytesview Data);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `Data` | 输入 | 借用、存活期覆盖 Reader | 字节视图 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 可定位 Reader（Size/Tell/Seek 全支持） | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/line · 内存](../../examples/io/line/main.c) · 观察

```c
	pReader = xrtReaderFromMemory(XRT_BYTES_LITERAL(sLog));
```


### `xrtReaderFromBuffer`

创建借用 Buffer 的 Reader；使用期间不得修改或销毁 Buffer。

```c
xreader* xrtReaderFromBuffer(const xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空、借用 | Buffer |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 可定位 Reader | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 缓冲](../../examples/io/stream_tour/main.c) · 观察

```c
			xreader* pBorrowed = xrtReaderFromBuffer(pBuffer);
```


### `xrtReaderTakeBuffer`

接管 Buffer 并创建 Reader；成功时把调用方槽清空。

```c
xreader* xrtReaderTakeBuffer(xbuffer** ppBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `ppBuffer` | 输入/输出 | 非空 | 成功后槽被清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Reader（拥有 Buffer） | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 缓冲](../../examples/io/stream_tour/main.c) · 观察

```c
		xreader* pBufReader = xrtReaderTakeBuffer(&pBuffer);
```


### `xrtReaderFromFile`

创建借用文件对象的 Reader。

```c
xreader* xrtReaderFromFile(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空、借用 | 文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 可定位 Reader | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 文件](../../examples/io/stream_tour/main.c) · 观察

```c
			xrtReaderFromFile(FileB) : NULL;
```


### `xrtReaderTakeFile`

接管文件对象并创建 Reader；成功时把调用方槽清空。

```c
xreader* xrtReaderTakeFile(xfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入/输出 | 非空 | 成功后槽被清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Reader（拥有文件） | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 文件](../../examples/io/stream_tour/main.c) · 观察

```c
			xreader* pTaken = xrtReaderTakeFile(&FileB);
```


### `xrtReaderOpen`

打开路径并创建拥有文件对象的 Reader。

```c
xreader* xrtReaderOpen(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Reader（拥有内部打开的文件） | — |
| `NULL` | 打开失败 | `xrt.io` 域错误 |

#### 错误

- `xrt.io` 域错误 — 打开失败
- `XERR_MEMORY`

#### 范例

[io/file · 文件](../../examples/io/file/main.c) · 观察

```c
	pReader = xrtReaderOpen(sPath);
```



### Reader 读取与复制

### `xrtReaderRead`

单次读取；成功读取零字节表示并锁定 EOF，直到下一次成功 Seek。

```c
bool xrtReaderRead(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `pBuffer` | 输出 | 非空（非零请求） | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 实际读取量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已读取（可短读；零 = EOF） | — |
| `false` | 参数或底层错误 | `*pRead` 语义不定 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 读取](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderRead(pReader, Out, 4u, &iRead) ||
```


### `xrtReaderReadFull`

持续读取到填满缓冲；提前 EOF 返回失败并保留实际读取量。

```c
bool xrtReaderReadFull(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `pBuffer` | 输出 | 非空 | 接收缓冲 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pRead` | 输出 | 可空 | 已读量（失败保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 缓冲已填满 | — |
| `false` | 提前 EOF 或底层错误 | `*pRead` 保留 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/file · 读取](../../examples/io/file/main.c) · 观察

```c
		 xrtReaderReadFull(pReader, arrData, sizeof(arrData), NULL) ) {
```


### `xrtReaderCopy`

持续复制到输入 EOF；使用固定大小栈缓冲且不随数据量分配。

```c
bool xrtReaderCopy(
	xreader* pReader,
	xwriter* pWriter,
	uint64* pCopied
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | 源 |
| `pWriter` | 输入 | 非空 | 目标 |
| `pCopied` | 输出 | 可空 | 累计复制量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制到 EOF | — |
| `false` | 底层错误 | `*pCopied` 保留 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/memory · 复制](../../examples/io/memory/main.c) · 观察

```c
		 xrtReaderCopy(pReader, pWriter, &iCopied) ) {
```


### `xrtReaderCopyN`

精确复制指定字节数；输入提前 EOF 时返回失败和已复制量。

```c
bool xrtReaderCopyN(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iSize,
	uint64* pCopied
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | 源 |
| `pWriter` | 输入 | 非空 | 目标 |
| `iSize` | 输入 | — | 精确字节数 |
| `pCopied` | 输出 | 可空 | 已复制量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已复制 iSize 字节 | — |
| `false` | 提前 EOF 或错误 | `*pCopied` 保留 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 复制](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderCopyN(pReader, pWriter, 5u, &uCopied) ||
```


### `xrtReaderCopyLimit`

在硬上限内复制到 EOF；超限时消费一个探测字节并返回范围错误。

```c
bool xrtReaderCopyLimit(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iLimit,
	uint64* pCopied
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | 源 |
| `pWriter` | 输入 | 非空 | 目标 |
| `iLimit` | 输入 | — | 硬上限 |
| `pCopied` | 输出 | 可空 | 已复制量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | EOF 且未超限 | — |
| `false` | 超限或错误 | 超限时 `*pCopied` 为 iLimit+1 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_RANGE` — 输入超过硬上限

#### 范例

[io/stream_tour · 复制](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderCopyLimit(pReader, pWriter, 64u, &uCopied) ||
```


### `xrtReaderReadAll`

在硬上限内读取到新 Buffer；超限时消费一个探测字节。

```c
xbuffer* xrtReaderReadAll(xreader* pReader, size_t iLimit);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `iLimit` | 输入 | — | 硬上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 拥有全部剩余内容的 Buffer | — |
| `NULL` | 超限、OOM 或读取失败 | — |

#### 错误

- `XERR_RANGE` — 内容超过上限
- `XERR_MEMORY`
- 底层读取错误

#### 范例

[io/stream_tour · 缓冲](../../examples/io/stream_tour/main.c) · 观察

```c
	pReadAll = xrtReaderReadAll(pReader, 64u);
```



### Reader 定位与查询

### `xrtReaderSeek`

移动 Reader 游标；成功后清除已锁定的 EOF。

```c
bool xrtReaderSeek(
	xreader* pReader,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `iOffset` | 输入 | 可负 | 偏移 |
| `Origin` | 输入 | — | 基准 |
| `pPosition` | 输出 | 可空 | 新位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 游标已移动、EOF 解锁 | — |
| `false` | 不支持或越界 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
	if ( !xrtReaderSeek(pReader, 2, XSEEK_START, &uPos) ||
		!xrtReaderRead(pReader, Out, 4u, &iRead) ||
		(memcmp(Out, "2345", 4u) != 0) ||
		!xrtReaderTell(pReader, &uPos) || (uPos != 6u) ||
		xrtReaderEOF(pReader) ) {
```


### `xrtReaderTell`

查询 Reader 游标；不支持时返回 `XERR_UNSUPPORTED`。

```c
bool xrtReaderTell(xreader* pReader, uint64* pPosition);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `pPosition` | 输出 | 非空 | 游标位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 位置已写出 | — |
| `false` | 不支持或参数错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderTell(pReader, &uPos) || (uPos != 4u) ) {
```


### `xrtReaderSize`

查询 Reader 当前总大小；不支持时返回 `XERR_UNSUPPORTED`。

```c
bool xrtReaderSize(xreader* pReader, uint64* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |
| `pSize` | 输出 | 非空 | 总大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 大小已写出 | — |
| `false` | 不支持或参数错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderSize(pReader, &uSize) ||
```


### `xrtReaderCanSeek`

判断 Reader 是否提供定位能力。

```c
bool xrtReaderCanSeek(const xreader* pReader);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 支持 Seek/Tell | — |
| `false` | 不支持或参数错误 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderCanSeek(pReader) ||
```


### `xrtReaderCanSize`

判断 Reader 是否提供大小查询能力。

```c
bool xrtReaderCanSize(const xreader* pReader);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 支持 Size | — |
| `false` | 不支持或参数错误 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtReaderCanSize(pReader) ||
```


### `xrtReaderEOF`

判断 Reader 是否已通过非零请求观察到 EOF。

```c
bool xrtReaderEOF(const xreader* pReader);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空 | Reader |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | EOF 已锁定 | — |
| `false` | 未观察或参数错误 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		xrtReaderEOF(pReader) ) {
```



### Reader 销毁

### `xrtReaderDestroy`

调用一次 Close 并销毁 Reader；Close 失败也一定释放对象。

```c
bool xrtReaderDestroy(xreader* pReader);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | Close 成功且已释放 | — |
| `false` | Close 失败（对象仍释放） | 错误经 `xrtGetError()` 报告 |

#### 错误

- 底层错误 — Close 失败；对象总是被释放

#### 范例

[io/file · 生命周期](../../examples/io/file/main.c) · 观察

```c
	xrtReaderDestroy(pReader);
```



### 行读取

### `xrtLineReaderCreate`

创建借用 Reader 的 Line Reader；最大行长只计算终止符之前的内容字节。

```c
xlinereader* xrtLineReaderCreate(
	xreader* pReader,
	size_t iMaxLine
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pReader` | 输入 | 非空、借用 | 底层 Reader |
| `iMaxLine` | 输入 | `> 0` | 内容字节上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Line Reader | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 行读取](../../examples/io/stream_tour/main.c) · 观察

```c
		pLines = xrtLineReaderCreate(pFileReader, 64u);
```


### `xrtLineReaderTake`

原子接管 Reader 槽；成功时清空来源，失败时所有权保持不变。

```c
xlinereader* xrtLineReaderTake(
	xreader** ppReader,
	size_t iMaxLine
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `ppReader` | 输入/输出 | 非空 | 成功后槽被清空 |
| `iMaxLine` | 输入 | `> 0` | 内容字节上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Line Reader（拥有底层） | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/line · 行读取](../../examples/io/line/main.c) · 观察

```c
	pLines = xrtLineReaderTake(&pReader, 1024u);
```


### `xrtLineReaderNext`

返回下一行借用视图；超限或底层读取失败后对象进入失败状态。

```c
xlinenext xrtLineReaderNext(
	xlinereader* pLines,
	xlineview* pLine
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLines` | 输入 | 非空 | Line Reader |
| `pLine` | 输出 | 非空 | 接收行视图（借用） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XLINE_NEXT_LINE` | 一行已发布 | — |
| `XLINE_NEXT_END` | EOF | 不设错 |
| `XLINE_NEXT_ERROR` | 超限或读取失败 | 对象锁定失败态 |

#### 错误

- 行超限或底层读取错误（对象进入失败状态）

#### 范例

[io/line · 行读取](../../examples/io/line/main.c) · 观察

```c
	while ( (Next = xrtLineReaderNext(pLines, &Line)) == XLINE_NEXT_LINE ) {
```


### `xrtLineReaderDestroy`

释放 Line Reader；接管模式同时销毁底层 Reader 并返回关闭结果。

```c
bool xrtLineReaderDestroy(xlinereader* pLines);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pLines` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已释放（接管模式含底层） | — |
| `false` | 底层 Close 失败（仍释放） | 错误经 `xrtGetError()` 报告 |

#### 错误

- 底层错误 — Close 失败；对象总是被释放

#### 范例

[io/line · 行读取](../../examples/io/line/main.c) · 观察

```c
	if ( !xrtLineReaderDestroy(pLines) ) {
```



### Writer 构造

### `xrtWriterCreate`

创建自定义 Writer；失败时 Context 所有权不变，销毁时调用一次 Close。

```c
xwriter* xrtWriterCreate(
	const xwriterops* pOps,
	ptr pContext
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pOps` | 输入 | 非空、静态存储 | 操作表 |
| `pContext` | 输入 | 成功后归 Writer | 自定义上下文 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 自定义 Writer | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 构造](../../examples/io/stream_tour/main.c) · 观察

```c
	pSink = xrtWriterCreate(&WriterOps, &Sink);
```


### `xrtWriterFromMemory`

创建借用固定容量的可定位 Writer；稀疏写入产生的空洞会填零。

```c
xwriter* xrtWriterFromMemory(ptr pData, size_t iCapacity);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pData` | 输入 | 非空、可写 | 输出缓冲 |
| `iCapacity` | 输入 | — | 容量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 可定位 Writer | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/memory · 内存](../../examples/io/memory/main.c) · 观察

```c
	xwriter* pWriter = xrtWriterFromMemory(
		arrOutput,
		sizeof(arrOutput)
	);
```


### `xrtWriterDiscard`

创建只统计并丢弃全部输入的 Writer。

```c
xwriter* xrtWriterDiscard(void);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| 无 | — | — | 无参数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 丢弃型 Writer | — |
| `NULL` | OOM | `XERR_MEMORY` |

#### 错误

- `XERR_MEMORY`

#### 范例

[io/stream_tour · 构造](../../examples/io/stream_tour/main.c) · 观察

```c
			xwriter* pDiscard = xrtWriterDiscard();
```


### `xrtWriterFromBuffer`

创建借用 Buffer 的 Writer，初始游标位于已有内容末尾。

```c
xwriter* xrtWriterFromBuffer(xbuffer* pBuffer);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pBuffer` | 输入 | 非空、可写、借用 | Buffer |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Writer（追加语义） | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/buffer · 缓冲](../../examples/io/buffer/main.c) · 观察

```c
	pWriter = xrtWriterFromBuffer(&Buffer);
```


### `xrtWriterFromFile`

创建借用文件对象的 Writer。

```c
xwriter* xrtWriterFromFile(xfile File);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `File` | 输入 | 非空、借用 | 文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Writer | — |
| `NULL` | 参数错误或 OOM | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 文件](../../examples/io/stream_tour/main.c) · 观察

```c
			xrtWriterFromFile(FileW) : NULL;
```


### `xrtWriterTakeFile`

接管文件对象并创建 Writer；成功时把调用方槽清空。

```c
xwriter* xrtWriterTakeFile(xfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入/输出 | 非空 | 成功后槽被清空 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Writer（拥有文件） | — |
| `NULL` | 参数错误或 OOM | 所有权不变 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 文件](../../examples/io/stream_tour/main.c) · 观察

```c
		pTakeWriter = xrtWriterTakeFile(&FileC);
```


### `xrtWriterOpen`

创建或截断路径并创建拥有文件对象的 Writer。

```c
xwriter* xrtWriterOpen(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | Writer（拥有内部打开的文件） | — |
| `NULL` | 打开失败 | `xrt.io` 域错误 |

#### 错误

- `xrt.io` 域错误
- `XERR_MEMORY`

#### 范例

[io/file · 文件](../../examples/io/file/main.c) · 观察

```c
	xwriter* pWriter = xrtWriterOpen(sPath);   /* 拥有式：Destroy 关文件 */
```


### `xrtWriterOpenAppend`

以操作系统追加语义打开路径并创建拥有文件对象的 Writer。

```c
xwriter* xrtWriterOpenAppend(cstr sPath);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `sPath` | 输入 | 非空、UTF-8 | 路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 追加语义 Writer | — |
| `NULL` | 打开失败 | `xrt.io` 域错误 |

#### 错误

- `xrt.io` 域错误
- `XERR_MEMORY`

#### 范例

[io/stream_tour · 文件](../../examples/io/stream_tour/main.c) · 观察

```c
	pAppend = xrtWriterOpenAppend("xrt-io-tour.tmp");
```



### Writer 写入

### `xrtWriterWrite`

单次写入，允许成功短写但拒绝非零请求不产生进展。

```c
bool xrtWriterWrite(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 实际写入量 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已写入（可短写；零进展拒绝） | — |
| `false` | 参数或底层错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 写入](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtWriterWrite(pAppend, "cd\n", 3u, &iRead) ||
```


### `xrtWriterWriteFull`

持续写入到全部完成；失败时保留实际写入量。

```c
bool xrtWriterWriteFull(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `pBuffer` | 输入 | 借用 | 写入数据 |
| `iRequest` | 输入 | — | 请求字节数 |
| `pWritten` | 输出 | 可空 | 已写量（失败保留） |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 全部写入 | — |
| `false` | 底层错误 | `*pWritten` 保留 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/buffer · 写入](../../examples/io/buffer/main.c) · 观察

```c
		 xrtWriterWriteFull(pWriter, "hello world", 11u, NULL) &&
```


### `xrtWriterFlush`

显式刷新 Writer；没有 Flush 回调时为空操作。

```c
bool xrtWriterFlush(xwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已刷新（或无回调） | — |
| `false` | 底层错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 写入](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtWriterFlush(pSink) || (Sink.Flushes != 1) ) {
```


### `xrtWriterWriteBuffer`

完整写入 Buffer 当前有效内容。

```c
bool xrtWriterWriteBuffer(
	xwriter* pWriter,
	const xbuffer* pBuffer
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `pBuffer` | 输入 | 非空 | 内容源 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 已完整写入 | — |
| `false` | 参数或底层错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 缓冲](../../examples/io/stream_tour/main.c) · 观察

```c
			if ( !xrtWriterWriteBuffer(pWriter, pBuffer) ||
				!xrtWriterTell(pWriter, &uPos) ||
				(uPos != 2u) ) {
```



### Writer 定位与销毁

### `xrtWriterSeek`

移动 Writer 游标；不支持时返回 `XERR_UNSUPPORTED`。

```c
bool xrtWriterSeek(
	xwriter* pWriter,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `iOffset` | 输入 | 可负 | 偏移 |
| `Origin` | 输入 | — | 基准 |
| `pPosition` | 输出 | 可空 | 新位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 游标已移动 | — |
| `false` | 不支持或越界 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/buffer · 定位](../../examples/io/buffer/main.c) · 观察

```c
		 xrtWriterSeek(pWriter, 6, XSEEK_START, NULL) &&
```


### `xrtWriterTell`

查询 Writer 游标。

```c
bool xrtWriterTell(xwriter* pWriter, uint64* pPosition);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `pPosition` | 输出 | 非空 | 游标位置 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 位置已写出 | — |
| `false` | 不支持或参数错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
				!xrtWriterTell(pWriter, &uPos) ||
```


### `xrtWriterSize`

查询 Writer 当前逻辑大小。

```c
bool xrtWriterSize(xwriter* pWriter, uint64* pSize);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |
| `pSize` | 输出 | 非空 | 逻辑大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 大小已写出 | — |
| `false` | 不支持或参数错误 | — |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法
- `XERR_UNSUPPORTED` — 该 Reader/Writer 不提供定位或大小查询

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtWriterSize(pAppend, &uSize) ||
```


### `xrtWriterCanSeek`

判断 Writer 是否提供定位能力。

```c
bool xrtWriterCanSeek(const xwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 支持 Seek/Tell | — |
| `false` | 不支持或参数错误 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
		!xrtWriterCanSeek(pAppend) ||
```


### `xrtWriterCanSize`

判断 Writer 是否提供大小查询能力。

```c
bool xrtWriterCanSize(const xwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 非空 | Writer |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | 支持 Size | — |
| `false` | 不支持或参数错误 | 纯查询 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[io/stream_tour · 定位](../../examples/io/stream_tour/main.c) · 观察

```c
			!xrtWriterCanSize(pFileWriter) ||
```


### `xrtWriterDestroy`

调用一次 Close 并销毁 Writer；不会隐式调用可能昂贵的 Flush。

```c
bool xrtWriterDestroy(xwriter* pWriter);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pWriter` | 输入 | 允许空 | 空 = 空操作 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `true` | Close 成功且已释放 | — |
| `false` | Close 失败（对象仍释放） | 错误经 `xrtGetError()` 报告 |

#### 错误

- 底层错误 — Close 失败；对象总是被释放

#### 范例

[io/buffer · 生命周期](../../examples/io/buffer/main.c) · 观察

```c
	xrtWriterDestroy(pWriter);
```


