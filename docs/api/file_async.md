# 异步文件

异步文件体系把可能阻塞的文件系统操作提交到调用方拥有的有界 `xtaskpool`，并通过 `xfuture` 统一表达结果、错误、等待和取消。它不为每个操作创建线程，也不使用隐藏的全局执行器。

## 类型与常量

### `xfiledata`

读取结果及其 Data 都由 Future 拥有，Future 释放前保持有效。

```c
typedef struct xfiledata {
	bytes Data;
	size_t Size;
	uint64 Offset;
	bool End;
} xfiledata;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Data` | `bytes` | 数据 |
| `Size` | `size_t` | 字节数 |
| `Offset` | `uint64` | 偏移量 |
| `End` | `bool` | 结束 |

### `xfilechange`

写入、查询大小和修改大小统一返回偏移与字节数。

```c
typedef struct xfilechange {
	uint64 Offset;
	uint64 Size;
} xfilechange;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Offset` | `uint64` | 偏移量 |
| `Size` | `uint64` | 字节数 |

### `xfilesize`

文件或目录树大小查询使用独立结果，避免混入写入偏移语义。

```c
typedef struct xfilesize {
	uint64 Size;
} xfilesize;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Size` | `uint64` | 字节数 |

### `xdirquery`

目录属性查询结果由 Future 拥有。

```c
typedef struct xdirquery {
	bool Empty;
} xdirquery;
```

| 字段 | 类型 | 语义 |
|---|---|---|
| `Empty` | `bool` | Empty |

### `xfileasyncerror`

异步文件错误保留外层操作，并通过 cause 保留文件或任务池错误。

```c
typedef enum xfileasyncerror {
	XFILE_ASYNC_ERROR_OPEN = 1,
	XFILE_ASYNC_ERROR_SUBMIT,
	XFILE_ASYNC_ERROR_READ,
	XFILE_ASYNC_ERROR_WRITE,
	XFILE_ASYNC_ERROR_FLUSH,
	XFILE_ASYNC_ERROR_SIZE,
	XFILE_ASYNC_ERROR_RESIZE,
	XFILE_ASYNC_ERROR_CLOSE,
	XFILE_ASYNC_ERROR_COPY,
	XFILE_ASYNC_ERROR_MOVE,
	XFILE_ASYNC_ERROR_DELETE,
	XFILE_ASYNC_ERROR_CREATE,
	XFILE_ASYNC_ERROR_TREE,
	XFILE_ASYNC_ERROR_QUERY
} xfileasyncerror;
```

| 值 | 语义 |
|---|---|
| `XFILE_ASYNC_ERROR_OPEN` | 失败 |
| `XFILE_ASYNC_ERROR_SUBMIT` | 失败 |
| `XFILE_ASYNC_ERROR_READ` | 读方向 |
| `XFILE_ASYNC_ERROR_WRITE` | 写方向 |
| `XFILE_ASYNC_ERROR_FLUSH` | 刷新 |
| `XFILE_ASYNC_ERROR_SIZE` | 尺寸 |
| `XFILE_ASYNC_ERROR_RESIZE` | 失败 |
| `XFILE_ASYNC_ERROR_CLOSE` | 失败 |
| `XFILE_ASYNC_ERROR_COPY` | 失败 |
| `XFILE_ASYNC_ERROR_MOVE` | 失败 |
| `XFILE_ASYNC_ERROR_DELETE` | Delete失败 |
| `XFILE_ASYNC_ERROR_CREATE` | 创建 |
| `XFILE_ASYNC_ERROR_TREE` | 失败 |
| `XFILE_ASYNC_ERROR_QUERY` | 重叠查询非法 |

### `xasyncfile`

异步文件对象绑定一个有界任务池，并在关闭前保留全部已受理操作。

```c
typedef struct xasyncfile xasyncfile;
```

不透明句柄或别名；生命周期与所有权见各使用方 API 节。

### `xfileasyncreleaseproc`

零复制写入受理后，在数据不再被任务使用时执行一次释放过程。

```c
typedef void (*xfileasyncreleaseproc)(
	ptr pContext,
	cbytes pData,
	size_t iSize
);
```

回调类型；参数与返回语义见签名及各使用方 API 节。

## 裁剪层

功能按真实依赖拆分，应用只启用需要的层：

- `XRT_FEATURE_FILE_ASYNC_COMMON`：任务提交、路径快照和结构化错误公共层，只依赖 `task_pool`。
- `XRT_FEATURE_FILE_ASYNC`：已打开文件的绝对偏移读写、大小、调整、刷新和关闭，只依赖 `file` 与公共层。
- `XRT_FEATURE_FILE_ASYNC_WHOLE`：整文件读取、覆盖、追加和原子写，只依赖 `file_whole` 与公共层。
- `XRT_FEATURE_FILE_ASYNC_MANAGE`：文件复制、移动和删除，只依赖 `file_whole` 与公共层。
- `XRT_FEATURE_DIR_ASYNC`：目录创建、递归创建、空目录删除和空状态查询，只依赖 `dir` 与公共层。
- `XRT_FEATURE_FILE_TREE_ASYNC`：目录树复制、移动、删除、清理、统计和大小，只依赖 `file_tree` 与公共层。

旧版把这些能力放在一个路径任务大分派器和全局线程体系中。当前实现保留其完整场景，但每一层直接复用已经验证的同步底座，避免重复平台代码，也允许基础目录异步操作不引入递归遍历、链接和整文件复制。

## 任务池

所有异步入口显式接收 `xtaskpool*`：

```c
xtaskpoolconfig Config = { 4, 1024, 0 };
xtaskpool* pPool = xrtTaskPoolCreate(&Config);
```

任务池必须存活到全部 Future 完成。队列达到硬上限时，提交同步返回 `NULL`，错误域为 `xrt.file.async`、代码为 `XFILE_ASYNC_ERROR_SUBMIT`，其 `cause` 保留任务池的 `XERR_AGAIN`。调用方可以等待负载下降后重试。

路径、复制型写入数据和目录树选项都在提交返回前取得快照。提交成功后，调用方可以立即修改或释放原始参数。

## 文件对象

```c
xfileoptions Options;
xrtFileOptionsInit(&Options);
Options.Flags = XFILE_READ | XFILE_WRITE | XFILE_CREATE;

xasyncfile* pFile = xrtAsyncFileOpen(pPool, "data.bin", &Options);
```

`xrtAsyncFileOpen` 同步完成参数校验和原生文件打开。`XFILE_APPEND` 与绝对偏移写入语义冲突，因此文件对象拒绝该标志；追加写入使用 `xrtFileAppendAsync`。

已经打开的文件可以转交给异步对象：

```c
xfile File = xrtOpen("data.bin", XFILE_READ);
xasyncfile* pFile = xrtAsyncFileAdopt(pPool, File);
```

`Adopt` 成功后接管唯一关闭责任，失败时所有权仍归调用方。`xrtAsyncFileFlags` 返回采用时冻结的文件标志。

```c
xfuture* pClose = xrtAsyncFileClose(pFile);
xrtFutureWait(pClose);
xrtFutureDestroy(pClose);
xrtTaskPoolDestroy(pPool);
```

调用 `Close` 后，调用方立即失去 `xasyncfile*` 所有权。关闭 Future 在全部已受理操作终止且原生文件关闭后完成。关闭使用任务池的资源回收通道，不占普通队列槽位，因此队列已满或任务池已关闭接收普通任务时仍能收尾。

### `xrtAsyncFileOpen`

同步完成参数校验和原生文件打开；拒绝与绝对偏移写入冲突的 `XFILE_APPEND` 标志。

```c
xasyncfile* xrtAsyncFileOpen(
	xtaskpool* pPool,
	cstr sPath,
	const xfileoptions* pOptions
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空、UTF-8 | 文件路径 |
| `pOptions` | 输入 | 允许空 | 打开选项；不得含 `XFILE_APPEND` |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 异步文件对象 | — |
| `NULL` | 打开失败 | 错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_OPEN` — 打开失败，cause 保留底层错误
- `XERR_ARGUMENT` — 含 `XFILE_APPEND` 标志或参数非法

#### 范例

[file/async · 打开与读写](../../examples/file/async/main.c) · 观察

```c
	pFile = xrtAsyncFileOpen(
		pPool,
		"xrt-async-example.txt",
		&Options
	);
```


### `xrtAsyncFileAdopt`

采用已经打开的文件，并把唯一关闭责任转交给异步文件对象；失败时调用方仍然拥有 `File`。

```c
xasyncfile* xrtAsyncFileAdopt(
	xtaskpool* pPool,
	xfile File
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `File` | 输入 | 非空 | 已打开的 `xfile`；成功后只能经 `xrtAsyncFileClose` 关闭 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 异步文件对象（已接管 File） | — |
| `NULL` | 采用失败 | 所有权仍归调用方 |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/async_tour · Adopt](../../examples/file/async_tour/main.c) · 观察

```c
	if ( (File == NULL) ||
		((pFile = xrtAsyncFileAdopt(pPool, File)) == NULL) ) {
```


### `xrtAsyncFileFlags`

返回异步文件采用时保存的打开标志；失败返回 0。

```c
uint32 xrtAsyncFileFlags(const xasyncfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| `XFILE_*` 组合 | 采用时冻结的标志 | — |
| `0` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `XERR_ARGUMENT` — 指针为空或参数非法

#### 范例

[file/async_tour · Adopt](../../examples/file/async_tour/main.c) · 观察

```c
	iFlags = xrtAsyncFileFlags(pFile);
```


### `xrtAsyncFileClose`

停止接收新操作并释放对象所有权；Future 在全部已受理操作结束且原生文件关闭后完成。关闭走任务池资源回收通道，不占普通队列槽位。

```c
xfuture* xrtAsyncFileClose(xasyncfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 成功后调用方立即失去所有权 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 关闭 Future（成功无值） | — |
| `NULL` | 参数非法 | `XERR_ARGUMENT` |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_CLOSE` — 原生关闭失败（cause 保留底层错误）

#### 范例

[file/async_tour · Close](../../examples/file/async_tour/main.c) · 观察

```c
		xfuture* pClosing = xrtAsyncFileClose(pFile);

		if ( pClosing == NULL ) goto Cleanup;
		pFile = NULL;  /* Close 已受理并释放对象所有权。 */
```


## 定位读写

```c
xfuture* pWrite = xrtAsyncFileWriteAt(
	pFile,
	4096,
	XRT_BYTES_LITERAL("payload")
);
```

`xrtAsyncFileWriteAt` 复制输入。成功值是 Future 拥有的 `xfilechange`，其中 `Offset` 是请求偏移，`Size` 是完整写入字节数。

高吞吐场景可避免复制：

- `xrtAsyncFileWriteAtRef` 借用外部缓冲。提交成功后，释放回调在成功、失败或取消终态后执行一次；提交失败不转移所有权。
- `xrtAsyncFileWriteAtTake` 接管由 `xrtMalloc` 家族分配的缓冲，并在终态后通过 `xrtFree` 释放；提交失败仍由调用方释放。
- 零长度写入不转移外部缓冲所有权。

```c
xfuture* pRead = xrtAsyncFileReadAt(pFile, 4096, 8192);
xrtFutureWait(pRead);
xfiledata* pData = (xfiledata*)xrtFutureValue(pRead);
```

读取成功值 `xfiledata` 及其 `Data` 都由 Future 拥有：

- `Offset`：请求的绝对偏移。
- `Size`：实际读取字节数。
- `End`：非零请求发生短读，即本次到达 EOF。
- `Data[Size]`：额外补零，便于协议检查；数据仍按二进制长度处理。

零长度读取不探测 EOF，返回 `Size == 0`、`End == false`。

Windows 使用 `OVERLAPPED` 定位 I/O，POSIX 使用 `pread`/`pwrite`。同一文件对象的不同绝对偏移操作可以并行，不共享也不修改文件游标。任务池只保证受理和终态，不保证相关任务的执行顺序；存在数据依赖时，调用方必须等待前一个 Future，或使用 Future continuation 后再提交下一步。

### `xrtAsyncFileReadAt`

从绝对偏移读取最多 `iSize` 字节；成功值为 Future 拥有的 `xfiledata`（`End` 标记短读 EOF，`Data` 额外补零）。同一对象不同偏移可并行，不共享游标。

```c
xfuture* xrtAsyncFileReadAt(
	xasyncfile* pFile,
	uint64 iOffset,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `iSize` | 输入 | — | 最多读取字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_READ` — 读取失败

#### 范例

[file/async · 打开与读写](../../examples/file/async/main.c) · 观察

```c
	pRead = xrtAsyncFileReadAt(pFile, 0, 64);
	pData = (xfiledata*)waitValue(pRead);
```


### `xrtAsyncFileWriteAt`

从绝对偏移完整写入；提交前复制数据，返回后调用方可以立即释放或修改源缓冲。成功值为 `xfilechange`。

```c
xfuture* xrtAsyncFileWriteAt(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `Data` | 输入 | 借用、提交时复制 | 写入数据 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_WRITE` — 写入失败

#### 范例

[file/async · 打开与读写](../../examples/file/async/main.c) · 观察

```c
	pWrite = xrtAsyncFileWriteAt(
		pFile,
		0,
		XRT_BYTES_LITERAL("hello async file")
	);
```


### `xrtAsyncFileWriteAtRef`

零复制受理外部数据；提交成功后释放责任转移（释放回调在成功/失败/取消终态后恰好一次），提交失败仍归调用方。零长度不转移所有权。

```c
xfuture* xrtAsyncFileWriteAtRef(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data,
	xfileasyncreleaseproc pRelease,
	ptr pContext
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `Data` | 输入 | 借用至终态 | 非空数据必须提供释放过程 |
| `pRelease` | 输入 | 非空数据时非空 | 释放回调 |
| `pContext` | 输入 | 任意值 | 原样传给回调 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_tour · WriteAtRef](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtAsyncFileWriteAtRef(pFile, 8u,
			(xbytesview) { (cbytes)"REF-EXTRA", 9u },
			exampleRelease, NULL), &pValue) ||
```


### `xrtAsyncFileWriteAtTake`

零复制接管由 `xrtMalloc` 家族分配的非空数据；终态后经 `xrtFree` 释放。提交失败时所有权仍归调用方；`NULL,0` 表示空写入。

```c
xfuture* xrtAsyncFileWriteAtTake(
	xasyncfile* pFile,
	uint64 iOffset,
	bytes pData,
	size_t iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |
| `iOffset` | 输入 | — | 绝对偏移 |
| `pData` | 输入 | `xrtMalloc` 产物；受理即转移 | 被接管缓冲 |
| `iSize` | 输入 | — | 字节数 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_tour · WriteAtTake](../../examples/file/async_tour/main.c) · 观察

```c
		xfuture* pSubmitted = xrtAsyncFileWriteAtTake(pFile, 17u, pTake, 5u);
```


### `xrtAsyncFileFlush`

把已受理写入提交到稳定存储；只读文件直接成功。

```c
xfuture* xrtAsyncFileFlush(xasyncfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_FLUSH` — 刷新失败

#### 范例

[file/async_tour · Ref+Flush](../../examples/file/async_tour/main.c) · 观察

```c
		!exampleWaitValue(&pFuture, xrtAsyncFileFlush(pFile), NULL) ) {
```


## 大小与刷新

- `xrtAsyncFileSize` 返回 Future 拥有的 `xfilesize`。
- `xrtAsyncFileResize` 返回 Future 拥有的 `xfilechange`，其 `Size` 是新大小。
- `xrtAsyncFileFlush` 把可写文件提交到稳定存储；只读文件为成功空操作。

### `xrtAsyncFileSize`

查询当前文件大小；成功值为 Future 拥有的 `xfilesize`。

```c
xfuture* xrtAsyncFileSize(xasyncfile* pFile);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_SIZE` — 查询失败

#### 范例

[file/async_tour · Size/Resize](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtAsyncFileSize(pFile), &pValue) ||
```


### `xrtAsyncFileResize`

修改文件大小；成功值为 `xfilechange`（`Size` 为新大小）。

```c
xfuture* xrtAsyncFileResize(
	xasyncfile* pFile,
	uint64 iSize
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pFile` | 输入 | 非空 | 异步文件对象 |
| `iSize` | 输入 | — | 新大小 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_RESIZE` — 调整失败

#### 范例

[file/async_tour · Size/Resize](../../examples/file/async_tour/main.c) · 观察

```c
		!exampleWaitValue(&pFuture, xrtAsyncFileResize(pFile, 16u),
			&pValue) ) {
```


## 整文件

整文件层提供：

- `xrtFileReadAllAsync` 与 `xrtFileReadAllLimitAsync`，成功值为 `xfiledata`。
- `xrtFileWriteAllAsync`，完整覆盖文件。
- `xrtFileAppendAsync`，使用操作系统追加语义完整写入。
- `xrtFileWriteAtomicAsync`，在同目录写入私有临时文件后原子发布。

三种写入都复制调用方数据，成功值为 `xfilechange`。受限读取会在已知或增长后的文件大小越过硬上限时失败。

### `xrtFileReadAllAsync`

在任务池线程中读取整个文件；成功值为 Future 拥有的 `xfiledata`（含额外零字节）。

```c
xfuture* xrtFileReadAllAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空（提交时快照） | 文件路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_whole · 整读](../../examples/file/async_whole/main.c) · 观察

```c
	pRead = xrtFileReadAllAsync(pPool, sPath);
```


### `xrtFileReadAllLimitAsync`

在硬上限内读取整个文件；文件超限时 Future 失败。

```c
xfuture* xrtFileReadAllLimitAsync(
	xtaskpool* pPool,
	cstr sPath,
	size_t iLimit
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 文件路径 |
| `iLimit` | 输入 | — | 源文件字节硬上限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `XERR_RANGE` — 文件越过上限（Future 失败）

#### 范例

[file/async_tour · 限读](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileReadAllLimitAsync(pPool,
			"xrt-async-whole.tmp", 64u), &pValue) ||
```


### `xrtFileWriteAllAsync`

复制输入并在任务池线程中完整覆盖文件；成功值为 `xfilechange`。

```c
xfuture* xrtFileWriteAllAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目标路径 |
| `Data` | 输入 | 借用、提交时复制 | 完整内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_tour · 整写](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileWriteAllAsync(pPool,
			"xrt-async-whole.tmp",
			(xbytesview) { (cbytes)"whole-file", 10u }),
			NULL) ) {
```


### `xrtFileAppendAsync`

复制输入并使用操作系统追加语义完整写入。

```c
xfuture* xrtFileAppendAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目标路径 |
| `Data` | 输入 | 借用、提交时复制 | 追加内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_tour · 追加](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileAppendAsync(pPool,
			"xrt-async-tour.tmp",
			(xbytesview) { (cbytes)"tail", 4u }), NULL) ) {
```


### `xrtFileWriteAtomicAsync`

复制输入并通过同目录临时文件原子发布；成功值为 `xfilechange`。

```c
xfuture* xrtFileWriteAtomicAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目标路径 |
| `Data` | 输入 | 借用、提交时复制 | 完整内容 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`

#### 范例

[file/async_whole · 原子写](../../examples/file/async_whole/main.c) · 观察

```c
	pWrite = xrtFileWriteAtomicAsync(
		pPool,
		sPath,
		XRT_BYTES_LITERAL("hello async whole file")
	);
```


## 文件管理

`xrtFileCopyAsync`、`xrtFileMoveAsync` 和 `xrtFileDeleteAsync` 与同步底座共享替换、跨卷移动和错误语义。成功 Future 没有值。

### `xrtFileCopyAsync`

在任务池线程中复制文件；与同步底座共享替换语义，成功 Future 没有值。

```c
xfuture* xrtFileCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sSource` | 输入 | 非空（快照） | 源文件 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换已存在的普通文件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_COPY` — cause 保留 `xrt.file` 错误

#### 范例

[file/async_manage · 复制删除](../../examples/file/async_manage/main.c) · 观察

```c
	pCopy = xrtFileCopyAsync(pPool, sSource, sTarget, true);
```


### `xrtFileMoveAsync`

在任务池线程中移动文件；成功 Future 没有值。

```c
xfuture* xrtFileMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sSource` | 输入 | 非空 | 源文件 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换语义 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_MOVE`

#### 范例

[file/async_tour · 改名](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileMoveAsync(pPool,
			"xrt-async-whole.tmp",
			"xrt-async-moved.tmp", true), NULL) ) {
```


### `xrtFileDeleteAsync`

在任务池线程中删除文件；成功 Future 没有值。

```c
xfuture* xrtFileDeleteAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目标文件 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_DELETE`

#### 范例

[file/async_manage · 复制删除](../../examples/file/async_manage/main.c) · 观察

```c
	pDelete = xrtFileDeleteAsync(pPool, sTarget);
```


## 目录

基础目录层提供 `xrtDirCreateAsync`、`xrtDirCreateModeAsync`、`xrtDirCreateAllAsync`、`xrtDirCreateAllModeAsync` 和 `xrtDirRemoveAsync`。模式参数在 Windows 上被接受但忽略，在 POSIX 上使用权限低 12 位。

`xrtDirEmptyAsync` 成功值为 Future 拥有的 `xdirquery`，通过 `Empty` 判断目录是否为空。

### `xrtDirCreateAsync`

在任务池线程中使用平台默认模式创建一个目录。

```c
xfuture* xrtDirCreateAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空（快照） | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_CREATE`

#### 范例

[file/dir_async · 建删](../../examples/file/dir_async/main.c) · 观察

```c
	pCreate = xrtDirCreateAsync(pPool, sPath);
```


### `xrtDirCreateModeAsync`

在任务池线程中使用显式 POSIX 模式创建一个目录；Windows 接受但忽略模式。

```c
xfuture* xrtDirCreateModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |
| `iMode` | 输入 | 低 12 位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_CREATE`

#### 范例

[file/async_tour · 目录族](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirCreateModeAsync(pPool,
			"xrt-async-dir", 0755), NULL) ||
```


### `xrtDirCreateAllAsync`

在任务池线程中使用平台默认模式创建全部缺失目录。

```c
xfuture* xrtDirCreateAllAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 多级路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_CREATE`

#### 范例

[file/async_tour · 目录族](../../examples/file/async_tour/main.c) · 观察

```c
		!exampleWaitValue(&pFuture, xrtDirCreateAllAsync(pPool,
			"xrt-async-deep/a/b"), NULL) ||
```


### `xrtDirCreateAllModeAsync`

在任务池线程中使用显式 POSIX 模式创建全部缺失目录。

```c
xfuture* xrtDirCreateAllModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 多级路径 |
| `iMode` | 输入 | 低 12 位 | POSIX 权限 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_CREATE`

#### 范例

[file/async_tour · 目录族](../../examples/file/async_tour/main.c) · 观察

```c
		!exampleWaitValue(&pFuture, xrtDirCreateAllModeAsync(pPool,
			"xrt-async-deep2/x/y", 0755), NULL) ||
```


### `xrtDirRemoveAsync`

在任务池线程中删除一个空目录。

```c
xfuture* xrtDirRemoveAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 空目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_CREATE`

#### 范例

[file/dir_async · 建删](../../examples/file/dir_async/main.c) · 观察

```c
	pRemove = xrtDirRemoveAsync(pPool, sPath);
```


### `xrtDirEmptyAsync`

查询目录是否为空；成功值为 Future 拥有的 `xdirquery`（`Empty` 字段判定）。

```c
xfuture* xrtDirEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_QUERY`

#### 范例

[file/async_tour · 目录族](../../examples/file/async_tour/main.c) · 观察

```c
		!exampleWaitValue(&pFuture, xrtDirEmptyAsync(pPool,
			"xrt-async-deep/a/b"), NULL) ) {
```


## 目录树

递归目录树层直接复用同步层的 `xtreecopyoptions` 与 `xwalkstats`：

- `xrtFileTreeCopyAsync`：高级复制，成功值是源树统计。
- `xrtDirCopyAsync`：常用复制，`bReplace` 为真时合并目录并替换冲突对象。
- `xrtFileTreeRemoveAsync`：高级后序删除，成功值是处理统计。
- `xrtDirRemoveAllAsync` 与 `xrtDirCleanAsync`：删除根或只清空内容。
- `xrtDirMoveAsync`：优先同卷改名，跨卷时复制成功后删除源树。
- `xrtDirStatsAsync`：返回 `xwalkstats`。
- `xrtDirSizeAsync`：返回 `xfilesize`。
- `xrtDirEnsureEmptyAsync`：创建缺失目录，或清空已有目录。

复制、删除和统计结果由 Future 拥有。移动和确保为空成功时没有值。

### `xrtFileTreeCopyAsync`

使用高级选项异步复制目录树；成功值为源树的 `xwalkstats`。选项在提交时快照。

```c
xfuture* xrtFileTreeCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	const xtreecopyoptions* pOptions
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sSource` | 输入 | 非空（快照） | 源树根 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `pOptions` | 输入 | 允许空 | 空 = 默认保守选项 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE`

#### 范例

[file/async_tour · 目录树](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileTreeCopyAsync(pPool,
			"xrt-async-deep2", "xrt-async-tree-copy", NULL),
		NULL) ) {
```


### `xrtDirCopyAsync`

常用目录复制；`bReplace` 为真时合并目录并替换冲突对象。成功值为源树统计。

```c
xfuture* xrtDirCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sSource` | 输入 | 非空 | 源树 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 合并/替换语义 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE`

#### 范例

[file/tree_async · 复制删除](../../examples/file/tree_async/main.c) · 观察

```c
	pCopy = xrtDirCopyAsync(pPool, sSource, sTarget, false);
```


### `xrtFileTreeRemoveAsync`

后序异步删除目录树；成功值为处理结果 `xwalkstats`。

```c
xfuture* xrtFileTreeRemoveAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bKeepRoot
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 树根路径 |
| `bKeepRoot` | 输入 | — | 保留根目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE`

#### 范例

[file/async_tour · 目录树](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtFileTreeRemoveAsync(pPool,
			"xrt-async-tree-copy", true), NULL) ||
```


### `xrtDirRemoveAllAsync`

递归删除目录及全部内容；成功值为处理统计。

```c
xfuture* xrtDirRemoveAllAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 树根路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE`

#### 范例

[file/tree_async · 复制删除](../../examples/file/tree_async/main.c) · 观察

```c
	pRemove = xrtDirRemoveAllAsync(pPool, sTarget);
```


### `xrtDirCleanAsync`

删除目录全部内容并保留根；成功值为处理统计。

```c
xfuture* xrtDirCleanAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE`

#### 范例

[file/async_tour · Clean](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirCleanAsync(pPool,
			"xrt-async-deep-moved"), NULL) ) {
```


### `xrtDirMoveAsync`

异步移动目录树；优先同卷改名，跨卷时复制成功后删除源树。成功 Future 没有值。

```c
xfuture* xrtDirMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sSource` | 输入 | 非空 | 源树 |
| `sTarget` | 输入 | 非空 | 目标路径 |
| `bReplace` | 输入 | — | 替换语义 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_MOVE`

#### 范例

[file/async_tour · Move](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirMoveAsync(pPool,
			"xrt-async-deep", "xrt-async-deep-moved", true),
		NULL) ) {
```


### `xrtDirStatsAsync`

异步统计目录树；成功值为 `xwalkstats`。

```c
xfuture* xrtDirStatsAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |
| `bRecursive` | 输入 | — | 深入子目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_QUERY`

#### 范例

[file/async_tour · 统计](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirStatsAsync(pPool,
			"xrt-async-dir", true), &pValue) ||
```


### `xrtDirSizeAsync`

异步计算普通文件总字节数；成功值为 `xfilesize`。

```c
xfuture* xrtDirSizeAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |
| `bRecursive` | 输入 | — | 深入子目录 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future；结果/错误/取消经 Future 表达 | — |
| `NULL` | 受理前失败（参数/队列满/池关闭） | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_SUBMIT` — 队列达到硬上限（cause 保留任务池 `XERR_AGAIN`）
- 受理后失败保存在 Future 中，经 `xrtFutureError` 读取；底层错误在 `xrtErrorCause`
- `xrt.file.async` / `XFILE_ASYNC_ERROR_QUERY`

#### 范例

[file/async_tour · 统计](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirSizeAsync(pPool,
			"xrt-async-dir", true), &pValue) ||
```


### `xrtDirEnsureEmptyAsync`

异步创建缺失目录，或清空已有目录并保留根；成功 Future 没有值。

```c
xfuture* xrtDirEnsureEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
);
```

#### 参数

| 参数 | 方向 | 约束 | 说明 |
|---|---|---|---|
| `pPool` | 输入 | 非空、须存活到全部 Future 完成 | 调用方拥有的有界任务池 |
| `sPath` | 输入 | 非空 | 目录路径 |

#### 返回值

| 返回 | 含义 | 失败时状态 |
|---|---|---|
| 非空 | 已受理的 Future | — |
| `NULL` | 受理前失败 | 线程错误经 `xrtGetError()` 报告 |

#### 错误

- `xrt.file.async` / `XFILE_ASYNC_ERROR_TREE` — cause 保留 `xrt.tree` 错误

#### 范例

[file/async_tour · EnsureEmpty](../../examples/file/async_tour/main.c) · 观察

```c
	if ( !exampleWaitValue(&pFuture, xrtDirEnsureEmptyAsync(pPool,
			"xrt-async-dir"), NULL) ||
```

## 取消

取消是协作请求：

- 排队任务在开始前观察到取消时进入 `XFUTURE_CANCELLED`，任务参数和外部缓冲责任完整回收。
- 已进入阻塞文件系统调用的任务通常要等待该调用返回，不承诺强制中断。
- 取消 Future 不等于关闭 `xasyncfile`；对象仍须通过 `xrtAsyncFileClose` 收尾。

## 错误

异步文件错误域为 `xrt.file.async`，稳定代码由 `xfileasyncerror` 定义：

- `OPEN`、`SUBMIT`、`READ`、`WRITE`、`FLUSH`、`SIZE`、`RESIZE`、`CLOSE`
- `COPY`、`MOVE`、`DELETE`、`CREATE`、`TREE`、`QUERY`

同步参数、范围、溢出和受理前 OOM 直接返回 `NULL` 并设置线程错误。任务已受理后的失败保存在 Future 中，通过 `xrtFutureError` 读取。外层错误保留操作语义，底层 `xrt.file`、`xrt.dir`、`xrt.tree` 或任务池错误保存在 `xrtErrorCause` 中。

## 示例

- `examples/file/async/main.c`
- `examples/file/async_whole/main.c`
- `examples/file/async_manage/main.c`
- `examples/file/dir_async/main.c`
- `examples/file/tree_async/main.c`
