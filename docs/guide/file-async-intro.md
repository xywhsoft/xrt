# XRT 异步文件与目录操作入门

> 目标：快速上手 `xrtAsyncFileOpen()`、`xrtAsyncFileReadAt()`、`xrtAsyncFileWriteAt()`，以及 `xrtFileReadAllAsync()` / `xrtFileWriteAllAsync()` 这一组异步文件能力。

[English](file-async-intro.en.md) | [返回教学文档](README.md)

---

## 1. 这组 API 解决什么问题

当你已经有一套同步文件 API，但又希望把文件读写接到 `future / task` 链路里时，这组接口正好补上这一层：

- 在后台线程里执行文件读写
- 用 `xfuture*` 等待结果
- 做显式 offset 的读写，而不是依赖共享文件游标
- 提供整文件与目录操作的异步便捷函数

它比较适合：

- 工具链里的异步文件读写
- AI Agent 的工作目录读写
- 和 `xTaskRunThread()`、`xFutureWhenAll()` 组合的批处理任务

---

## 2. 两层入口

### 2.1 句柄层 `xasyncfile`

适合你想要：

- 显式打开文件
- 按 offset 读写
- 多个异步操作复用同一个文件句柄

常用入口：

- `xrtAsyncFileConfigInit()`
- `xrtAsyncFileOpen()`
- `xrtAsyncFileReadAt()`
- `xrtAsyncFileWriteAt()`
- `xrtAsyncFileGetSize()`
- `xrtAsyncFileSetSize()`
- `xrtAsyncFileFlush()`
- `xrtAsyncFileClose()`

### 2.2 路径便捷层

适合你想要：

- 一次性整文件读写
- 复制 / 移动 / 删除
- 目录创建、复制、移动、删除

常用入口：

- `xrtFileReadAllAsync()`
- `xrtFileWriteAllAsync()`
- `xrtFileGetAllAsync()`
- `xrtFilePutAllAsync()`
- `xrtFileCopyAsync()`
- `xrtFileMoveAsync()`
- `xrtFileDeleteAsync()`
- `xrtDirCreateAllAsync()`
- `xrtDirCopyAsync()`

---

## 3. 最小句柄示例

```c
#include "xrt.h"

int main(void)
{
	xasyncfileconfig tConfig;
	xasyncfile* pFile;
	xfuture* pFuture;
	xasyncfileio* pWriteInfo;
	xasyncfilebuf* pReadInfo;

	xrtInit();

	xrtAsyncFileConfigInit(&tConfig);
	tConfig.iFlags = XAFILE_F_READ | XAFILE_F_WRITE | XAFILE_F_CREATE | XAFILE_F_TRUNCATE;
	tConfig.sPath = (str)"demo_async.txt";

	pFile = xrtAsyncFileOpen(&tConfig);
	if ( pFile == NULL ) {
		xrtUnit();
		return 1;
	}

	pFuture = xrtAsyncFileWriteAt(pFile, 0, "hello async file", 16);
	pWriteInfo = (xasyncfileio*)xFutureWaitValueTimeout(pFuture, 3000);
	xFutureRelease(pFuture);
	xrtAsyncFileIoDestroy(pWriteInfo);

	pFuture = xrtAsyncFileReadAt(pFile, 0, 16);
	pReadInfo = (xasyncfilebuf*)xFutureWaitValueTimeout(pFuture, 3000);
	if ( pReadInfo ) {
		printf("read: %s\n", (char*)pReadInfo->pData);
		xrtAsyncFileBufDestroy(pReadInfo);
	}
	xFutureRelease(pFuture);

	xrtAsyncFileClose(pFile);
	xrtUnit();
	return 0;
}
```

---

## 4. 最小整文件示例

```c
xfuture* pWrite = xrtFileWriteAllAsync((str)"demo.txt", (str)"hello\n", 0, XRT_CP_UTF8);
xfuture* pRead;
xasyncfilebuf* pText;

xFutureWaitTimeout(pWrite, 3000);
xFutureRelease(pWrite);

pRead = xrtFileReadAllAsync((str)"demo.txt", XRT_CP_UTF8);
pText = (xasyncfilebuf*)xFutureWaitValueTimeout(pRead, 3000);
if ( pText ) {
	printf("text = %s\n", (char*)pText->pData);
	xrtAsyncFileBufDestroy(pText);
}
xFutureRelease(pRead);
```

---

## 5. 结果对象怎么释放

这组异步接口的 future value 通常是堆对象：

- `xrtAsyncFileReadAt()` / `xrtFileReadAllAsync()` / `xrtFileGetAllAsync()` 返回 `xasyncfilebuf*`
- `xrtAsyncFileWriteAt()` / `xrtAsyncFileGetSize()` / `xrtAsyncFileSetSize()` 以及大多数路径操作返回 `xasyncfileio*`

对应释放函数：

- `xrtAsyncFileBufDestroy()`
- `xrtAsyncFileIoDestroy()`

---

## 6. 几个关键语义

- `v1` 当前是线程驱动实现，底层通过 `xTaskRunThread()` 跑到后台线程。
- 句柄层是显式 offset 语义，不共享同步 `xfile` 的游标状态。
- `xrtAsyncFileClose()` 的语义是“拒绝新操作并释放调用方引用”；已经提交的异步任务会继续跑完。
- 目录类 helper 目前是对现有同步目录 API 的异步封装，适合一段式任务，不是流式目录遍历。
- 这版没有做原生 IOCP / io_uring，也不保证取消会立刻打断阻塞中的系统调用。

---

## 7. 推荐用法

- 需要按块、按偏移处理时，优先用 `xasyncfile`
- 只想把整文件读写接进 future 链路时，优先用 `xrtFile*Async()`
- 需要拿输出数据时，统一在 `xFutureWaitValue*()` 后读取结果对象，再调用对应 destroy helper
- 如果要批量并发多个文件任务，可以配合 `xFutureWhenAll()`

---

## 8. 延伸阅读

- [File](../api/api-file.md)
- [Thread](../api/api-thread.md)
- [Future / Task / Promise](../api/api-future-task-promise.md)
- [XRT 子进程与工具执行入门](subprocess-intro.md)
