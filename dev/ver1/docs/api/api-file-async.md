# Async File 异步文件模块

> 当前 XRT 的 `file_async` 主线负责“把文件与目录操作接到后台线程和 `future` 链路里”，而不是原生 IOCP / io_uring 风格的底层事件接口。

[返回索引](README.md)

---

## 1. 定位

这组 API 主要解决：

- 显式 offset 的异步读写
- 在后台线程里做文件 I/O
- 把整文件读写、复制、移动、删除接到 `xfuture*`
- 在工具链或批处理里组合多个文件任务

它更适合：

- agent 工作目录读写
- 批量报告生成
- `subprocess + file_async`
- `task / future` 驱动的一段式文件任务

它不应该被理解成：

- 原生事件驱动文件接口
- 可立即取消底层阻塞系统调用的 I/O 框架
- 共享 seek 指针语义的同步 `xfile` 替代品


## 2. 可用性与边界

当前源码主线里，这组 API 有 6 条值得先记住：

1. 公开声明当前挂在 `!defined(XRT_NO_NETWORK)` 分支下；也就是说，裁掉相应能力后，这整组 API 不会形成完整公开面。
2. 真正执行又依赖线程能力；如果编译时裁掉线程支持，当前实现会直接返回 `NULL`，并写入 `async file requires thread support.` 错误。
3. 当前实现是线程驱动封装，核心任务通过 `xTaskRunThread()` 跑到后台线程，不是原生 IOCP / io_uring。
4. 句柄层是显式 offset 语义，不共享同步文件的游标状态。
5. `xrtAsyncFileClose()` 的语义是“标记关闭并释放调用方引用”；已提交任务不会被强制回滚。
6. 路径便捷层的写接口会复制输入数据；调用方在 future 完成前不需要维持原缓冲区存活。


## 3. Flags 与结果对象

### 3.1 打开 Flags

```c
#define XAFILE_F_READ      0x0001u
#define XAFILE_F_WRITE     0x0002u
#define XAFILE_F_CREATE    0x0004u
#define XAFILE_F_TRUNCATE  0x0008u
```

使用边界：

- `XAFILE_F_READ`
	- 打开读能力
- `XAFILE_F_WRITE`
	- 打开写能力
- `XAFILE_F_CREATE`
	- 不存在时创建
- `XAFILE_F_TRUNCATE`
	- 截断文件

当前实现里还有一个真实校验：

- 如果设置了 `CREATE` 或 `TRUNCATE`
- 但没有 `WRITE`

会按配置错误处理。


### 3.2 共享 Flags

```c
#define XAFILE_SHARE_READ    0x0001u
#define XAFILE_SHARE_WRITE   0x0002u
#define XAFILE_SHARE_DELETE  0x0004u
```

默认行为：

- 如果 `iShareFlags == 0`
	- 当前实现会退到 `XAFILE_SHARE_READ`


### 3.3 `xasyncfileconfig`

```c
typedef struct {
	uint32 iFlags;
	uint32 iShareFlags;
	str sPath;
} xasyncfileconfig;
```

推荐做法：

1. `xrtAsyncFileConfigInit()`
2. 再设置 `iFlags / iShareFlags / sPath`


### 3.4 `xasyncfilebuf`

```c
typedef struct {
	ptr pData;
	size_t iSize;
	uint64 iOffset;
	bool bEOF;
} xasyncfilebuf;
```

常见来源：

- `xrtAsyncFileReadAt()`
- `xrtFileReadAllAsync()`
- `xrtFileGetAllAsync()`

释放方式：

- `xrtAsyncFileBufDestroy()`


### 3.5 `xasyncfileio`

```c
typedef struct {
	uint64 iValue;
	uint64 iOffset;
} xasyncfileio;
```

常见来源：

- `xrtAsyncFileWriteAt()`
- `xrtAsyncFileFlush()`
- `xrtAsyncFileGetSize()`
- `xrtAsyncFileSetSize()`
- 大多数路径操作 helper

推荐理解：

- `iValue`
	- 当前操作的数值结果
	- 对写入通常可理解为处理字节数
	- 对 `GetSize/SetSize` 通常可理解为文件大小
	- 对目录/复制/移动类 helper 则更接近底层同步 API 的返回数值
- `iOffset`
	- 与本次操作相关的 offset

释放方式：

- `xrtAsyncFileIoDestroy()`


## 4. 核心类型与入口

### 4.1 句柄层

```c
typedef struct xasyncfile_struct xasyncfile;

XXAPI void xrtAsyncFileConfigInit(xasyncfileconfig* pConfig);
XXAPI xasyncfile* xrtAsyncFileOpen(const xasyncfileconfig* pConfig);
XXAPI void xrtAsyncFileClose(xasyncfile* pFile);

XXAPI xfuture* xrtAsyncFileReadAt(xasyncfile* pFile, uint64 iOffset, size_t iSize);
XXAPI xfuture* xrtAsyncFileWriteAt(xasyncfile* pFile, uint64 iOffset, const void* pData, size_t iSize);
XXAPI xfuture* xrtAsyncFileFlush(xasyncfile* pFile);
XXAPI xfuture* xrtAsyncFileGetSize(xasyncfile* pFile);
XXAPI xfuture* xrtAsyncFileSetSize(xasyncfile* pFile, uint64 iSize);

XXAPI void xrtAsyncFileBufDestroy(xasyncfilebuf* pBuf);
XXAPI void xrtAsyncFileIoDestroy(xasyncfileio* pInfo);
```

补充说明：

- `xrtAsyncFileOpen()` 失败返回 `NULL`。
- 如果 `iFlags == 0`，当前实现会默认按 `XAFILE_F_READ` 打开。
- `ReadAt/WriteAt` 都是显式 offset，不依赖共享游标。
- `WriteAt()` 会在内部复制输入缓冲区后再把任务提交到后台线程。
- `xrtAsyncFileClose()` 不会把已经提交的 future 强行改成取消态。


### 4.2 路径便捷层

```c
XXAPI xfuture* xrtFileAppendAsync(str sPath, str sText, size_t iSize, int iCharset);
XXAPI xfuture* xrtFileReadAllAsync(str sPath, int iCharset);
XXAPI xfuture* xrtFileWriteAllAsync(str sPath, str sText, size_t iSize, int iCharset);
XXAPI xfuture* xrtFileGetAllAsync(str sPath);
XXAPI xfuture* xrtFilePutAllAsync(str sPath, const void* pData, size_t iSize);

XXAPI xfuture* xrtFileCopyAsync(str sSrc, str sDst, bool bReWrite);
XXAPI xfuture* xrtFileMoveAsync(str sSrc, str sDst, bool bReWrite);
XXAPI xfuture* xrtFileDeleteAsync(str sPath);

XXAPI xfuture* xrtDirCreateAsync(str sPath);
XXAPI xfuture* xrtDirCreateAllAsync(str sPath);
XXAPI xfuture* xrtDirCopyAsync(str sSrc, str sDst, bool bReWrite);
XXAPI xfuture* xrtDirMoveAsync(str sSrc, str sDst, bool bReWrite);
XXAPI xfuture* xrtDirDeleteAsync(str sPath);
```

补充说明：

- `xrtFileAppendAsync()` / `xrtFileWriteAllAsync()` 中，如果 `iSize == 0 && sText != NULL`，当前实现会自动按 `strlen()` 计算长度。
- `xrtFileReadAllAsync()` 走文本读取，受 `iCharset` 影响。
- `xrtFileGetAllAsync()` / `xrtFilePutAllAsync()` 面向原始字节。
- 目录类 helper 当前是“一段式任务”，不是流式目录遍历。
- 复制/移动类 helper 的 `bReWrite` 与同步文件/目录 API 保持同类语义。


## 5. Future value 约定

推荐按下面这张表理解 future 的 value：

| API | future value |
|-----|--------------|
| `xrtAsyncFileReadAt()` | `xasyncfilebuf*` |
| `xrtFileReadAllAsync()` | `xasyncfilebuf*` |
| `xrtFileGetAllAsync()` | `xasyncfilebuf*` |
| `xrtAsyncFileWriteAt()` | `xasyncfileio*` |
| `xrtAsyncFileFlush()` | `xasyncfileio*` |
| `xrtAsyncFileGetSize()` | `xasyncfileio*` |
| `xrtAsyncFileSetSize()` | `xasyncfileio*` |
| 大多数 `xrtFile*Async()` / `xrtDir*Async()` 写操作 | `xasyncfileio*` |

调用侧通常配合：

- `xFutureWaitValue()`
- `xFutureWaitValueTimeout()`

然后再按结果类型调用：

- `xrtAsyncFileBufDestroy()`
- `xrtAsyncFileIoDestroy()`


## 6. 推荐用法

### 6.1 要复用句柄、按 offset 读写

优先：

- `xasyncfile`

适合：

- 固定文件多段写入
- 偏移读写
- 同一文件上的多个异步任务


### 6.2 只想把整文件操作接到 future

优先：

- `xrtFileReadAllAsync()`
- `xrtFileWriteAllAsync()`
- `xrtFileGetAllAsync()`
- `xrtFilePutAllAsync()`


### 6.3 要做工作目录整理

优先：

- `xrtFileCopyAsync()`
- `xrtFileMoveAsync()`
- `xrtDirCreateAllAsync()`
- `xrtDirCopyAsync()`


## 7. 常见错误

1. 把它当作同步文件 `xfile` 的共享 seek 版本使用。
2. 拿到 `xasyncfilebuf*` / `xasyncfileio*` 以后忘记 destroy。
3. 写入 future 还没完成就误以为输入缓冲区必须一直保留。
4. 关闭文件后还继续提交新任务。
5. 忽略“当前实现依赖线程能力”的前提。
6. 把目录 helper 当成可中途取消、可增量枚举的高级框架。


## 8. 相关阅读

- [XRT 异步文件与目录操作入门](../guide/file-async-intro.md)
- [File](api-file.md)
- [Thread](api-thread.md)
- [Future / Task / Promise](api-future-task-promise.md)
- [用 Subprocess + File Async 写一个工具链流水线](../case/subprocess-file-async-pipeline.md)
