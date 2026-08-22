# 异步文件

异步文件体系把可能阻塞的文件系统操作提交到调用方拥有的有界 `xtaskpool`，并通过 `xfuture` 统一表达结果、错误、等待和取消。它不为每个操作创建线程，也不使用隐藏的全局执行器。

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

## 大小与刷新

- `xrtAsyncFileSize` 返回 Future 拥有的 `xfilesize`。
- `xrtAsyncFileResize` 返回 Future 拥有的 `xfilechange`，其 `Size` 是新大小。
- `xrtAsyncFileFlush` 把可写文件提交到稳定存储；只读文件为成功空操作。

## 整文件

整文件层提供：

- `xrtFileReadAllAsync` 与 `xrtFileReadAllLimitAsync`，成功值为 `xfiledata`。
- `xrtFileWriteAllAsync`，完整覆盖文件。
- `xrtFileAppendAsync`，使用操作系统追加语义完整写入。
- `xrtFileWriteAtomicAsync`，在同目录写入私有临时文件后原子发布。

三种写入都复制调用方数据，成功值为 `xfilechange`。受限读取会在已知或增长后的文件大小越过硬上限时失败。

## 文件管理

`xrtFileCopyAsync`、`xrtFileMoveAsync` 和 `xrtFileDeleteAsync` 与同步底座共享替换、跨卷移动和错误语义。成功 Future 没有值。

## 目录

基础目录层提供 `xrtDirCreateAsync`、`xrtDirCreateModeAsync`、`xrtDirCreateAllAsync`、`xrtDirCreateAllModeAsync` 和 `xrtDirRemoveAsync`。模式参数在 Windows 上被接受但忽略，在 POSIX 上使用权限低 12 位。

`xrtDirEmptyAsync` 成功值为 Future 拥有的 `xdirquery`，通过 `Empty` 判断目录是否为空。

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
