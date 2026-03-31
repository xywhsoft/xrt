# XRT Guide: Async File and Directory Operations

> A practical introduction to `xrtAsyncFileOpen()`, `xrtAsyncFileReadAt()`, `xrtAsyncFileWriteAt()`, and the path-based async helpers such as `xrtFileReadAllAsync()` and `xrtFileWriteAllAsync()`.

[中文](file-async-intro.md) | [Back to Guides](README.en.md)

---

## What This Page Explains

This async file layer is meant to complement the existing synchronous file API when you want file work to participate in the XRT `future / task` pipeline:

- run file IO on background worker threads
- wait through `xfuture*`
- read and write by explicit offset instead of a shared cursor
- use one-shot async helpers for whole-file and directory operations

It fits well for tool pipelines, agent workspaces, and background batch jobs.

---

## The Two Entry Layers

### 1. The handle layer: `xasyncfile`

Use this when you want:

- an explicit file handle
- offset-based reads and writes
- multiple async operations on the same file

Main entry points:

- `xrtAsyncFileConfigInit()`
- `xrtAsyncFileOpen()`
- `xrtAsyncFileReadAt()`
- `xrtAsyncFileWriteAt()`
- `xrtAsyncFileGetSize()`
- `xrtAsyncFileSetSize()`
- `xrtAsyncFileFlush()`
- `xrtAsyncFileClose()`

### 2. The path helper layer

Use this when you want:

- one-shot whole-file reads and writes
- copy / move / delete operations
- directory create / copy / move / delete operations

Common helpers:

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

## Minimal Handle Example

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

## Minimal Whole-File Example

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

## Result Ownership

Most async file futures resolve to heap objects:

- `xrtAsyncFileReadAt()`, `xrtFileReadAllAsync()`, and `xrtFileGetAllAsync()` return `xasyncfilebuf*`
- `xrtAsyncFileWriteAt()`, `xrtAsyncFileGetSize()`, `xrtAsyncFileSetSize()`, and most path helpers return `xasyncfileio*`

Free them with:

- `xrtAsyncFileBufDestroy()`
- `xrtAsyncFileIoDestroy()`

---

## Important Behavior Notes

- `v1` is currently thread-backed and built on top of `xTaskRunThread()`.
- The handle layer is explicitly offset-based and does not share the synchronous `xfile` cursor model.
- `xrtAsyncFileClose()` means "reject new work and release the caller reference"; already-submitted operations continue until completion.
- Directory helpers are one-shot async wrappers around the current synchronous directory APIs.
- This version does not yet provide native IOCP / io_uring backends and does not guarantee that cancellation interrupts a blocking syscall immediately.

---

## Recommended Usage

- Prefer `xasyncfile` when you need block-oriented or offset-oriented file work.
- Prefer `xrtFile*Async()` when you only need to plug whole-file operations into a future chain.
- After `xFutureWaitValue*()`, consume the result object first and then destroy it with the matching helper.
- For batch workloads, combine multiple file futures with `xFutureWhenAll()`.

---

## Related Reading

- [File](../api/api-file.en.md)
- [Thread](../api/api-thread.en.md)
- [Future / Task / Promise](../api/api-future-task-promise.en.md)
- [Subprocess and Tool Execution](subprocess-intro.en.md)
