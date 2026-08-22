#ifndef XRT_FILE_ASYNC_H
#define XRT_FILE_ASYNC_H

#include <xrt/file.h>
#include <xrt/task.h>



#if defined(XRT_FEATURE_FILE_ASYNC_COMMON) && \
	!defined(XRT_FEATURE_TASK_POOL)
	#error "XRT async file common support requires task-pool support"
#endif

#if defined(XRT_FEATURE_FILE_ASYNC) && \
	(!defined(XRT_FEATURE_FILE) || \
	 !defined(XRT_FEATURE_FILE_ASYNC_COMMON))
	#error "XRT async file support requires file and async-file-common support"
#endif

#if defined(XRT_FEATURE_FILE_ASYNC_WHOLE) && \
	(!defined(XRT_FEATURE_FILE_WHOLE) || \
	 !defined(XRT_FEATURE_FILE_ASYNC_COMMON))
	#error "XRT async whole-file support requires whole-file and async-file-common support"
#endif

#if defined(XRT_FEATURE_FILE_ASYNC_MANAGE) && \
	(!defined(XRT_FEATURE_FILE_WHOLE) || \
	 !defined(XRT_FEATURE_FILE_ASYNC_COMMON))
	#error "XRT async file management requires whole-file and async-file-common support"
#endif

#if defined(XRT_FEATURE_DIR_ASYNC) && \
	(!defined(XRT_FEATURE_DIR) || \
	 !defined(XRT_FEATURE_FILE_ASYNC_COMMON))
	#error "XRT async directory support requires directory and async-file-common support"
#endif

#if defined(XRT_FEATURE_FILE_TREE_ASYNC) && \
	(!defined(XRT_FEATURE_FILE_TREE) || \
	 !defined(XRT_FEATURE_FILE_ASYNC_COMMON))
	#error "XRT async file-tree support requires file-tree and async-file-common support"
#endif



#if defined(XRT_FEATURE_FILE_ASYNC_COMMON)

/* 异步文件对象绑定一个有界任务池，并在关闭前保留全部已受理操作。 */
typedef struct xasyncfile xasyncfile;



/* 读取结果及其 Data 都由 Future 拥有，Future 释放前保持有效。 */
typedef struct xfiledata {
	bytes Data;
	size_t Size;
	uint64 Offset;
	bool End;
} xfiledata;



/* 写入、查询大小和修改大小统一返回偏移与字节数。 */
typedef struct xfilechange {
	uint64 Offset;
	uint64 Size;
} xfilechange;



/* 文件或目录树大小查询使用独立结果，避免混入写入偏移语义。 */
typedef struct xfilesize {
	uint64 Size;
} xfilesize;



/* 目录属性查询结果由 Future 拥有。 */
typedef struct xdirquery {
	bool Empty;
} xdirquery;



/* 零复制写入受理后，在数据不再被任务使用时执行一次释放过程。 */
typedef void (*xfileasyncreleaseproc)(
	ptr pContext,
	cbytes pData,
	size_t iSize
);



/* 异步文件错误保留外层操作，并通过 cause 保留文件或任务池错误。 */
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

#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_FILE_ASYNC)

/*
	同步打开异步文件对象。
	任务池由调用方拥有，并且必须存活到 xrtAsyncFileClose 返回的 Future 完成。
*/
XRT_API xasyncfile* xrtAsyncFileOpen(
	xtaskpool* pPool,
	cstr sPath,
	const xfileoptions* pOptions
);



/*
	采用已经打开的文件，并把唯一关闭责任转交给异步文件对象。
	失败时调用方仍然拥有 File；成功后只能通过 xrtAsyncFileClose 关闭。
*/
XRT_API xasyncfile* xrtAsyncFileAdopt(
	xtaskpool* pPool,
	xfile File
);



/* 返回异步文件采用时保存的打开标志；失败返回 0。 */
XRT_API uint32 xrtAsyncFileFlags(const xasyncfile* pFile);



/*
	停止接收新操作并释放调用方的对象所有权。
	返回的 Future 在全部已受理操作结束且原生文件关闭后完成。
	关闭过程通过任务池资源回收通道执行，不在调用线程执行文件系统操作。
*/
XRT_API xfuture* xrtAsyncFileClose(xasyncfile* pFile);



/*
	从绝对偏移读取最多 iSize 字节。
	成功 Future 的值为借用的 xfiledata；非零请求发生短读时以 End 标记 EOF。
*/
XRT_API xfuture* xrtAsyncFileReadAt(
	xasyncfile* pFile,
	uint64 iOffset,
	size_t iSize
);



/*
	从绝对偏移完整写入 Data。
	提交前复制数据，函数返回后调用方可以立即释放或修改源缓冲。
*/
XRT_API xfuture* xrtAsyncFileWriteAt(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data
);



/*
	零复制受理外部数据；成功后释放责任转移，失败时仍归调用方。
	非空数据必须提供释放过程；零长度不转移所有权。
*/
XRT_API xfuture* xrtAsyncFileWriteAtRef(
	xasyncfile* pFile,
	uint64 iOffset,
	xbytesview Data,
	xfileasyncreleaseproc pRelease,
	ptr pContext
);



/*
	零复制接管由 xrtMalloc 家族分配的非空数据。
	提交失败时所有权仍归调用方；NULL,0 表示空写入。
*/
XRT_API xfuture* xrtAsyncFileWriteAtTake(
	xasyncfile* pFile,
	uint64 iOffset,
	bytes pData,
	size_t iSize
);



/* 把已受理写入提交到稳定存储；只读文件直接成功。 */
XRT_API xfuture* xrtAsyncFileFlush(xasyncfile* pFile);



/* 查询当前文件大小；成功 Future 的值为借用的 xfilesize。 */
XRT_API xfuture* xrtAsyncFileSize(xasyncfile* pFile);



/* 修改文件大小；成功 Future 的值记录新大小。 */
XRT_API xfuture* xrtAsyncFileResize(
	xasyncfile* pFile,
	uint64 iSize
);

#endif



#if defined(XRT_FEATURE_DIR_ASYNC)

/* 在任务池线程中使用平台默认模式创建一个目录。 */
XRT_API xfuture* xrtDirCreateAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 在任务池线程中使用显式 POSIX 模式创建一个目录。 */
XRT_API xfuture* xrtDirCreateModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
);



/* 在任务池线程中使用平台默认模式创建全部缺失目录。 */
XRT_API xfuture* xrtDirCreateAllAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 在任务池线程中使用显式 POSIX 模式创建全部缺失目录。 */
XRT_API xfuture* xrtDirCreateAllModeAsync(
	xtaskpool* pPool,
	cstr sPath,
	uint32 iMode
);



/* 在任务池线程中删除一个空目录。 */
XRT_API xfuture* xrtDirRemoveAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 查询目录是否为空；成功 Future 的值为借用的 xdirquery。 */
XRT_API xfuture* xrtDirEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
);

#endif



#if defined(XRT_FEATURE_FILE_TREE_ASYNC)

/* 使用高级选项异步复制目录树；成功值为源树的 xwalkstats。 */
XRT_API xfuture* xrtFileTreeCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	const xtreecopyoptions* pOptions
);



/* 常用目录复制；成功值为源树的 xwalkstats。 */
XRT_API xfuture* xrtDirCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);



/* 后序异步删除目录树；成功值为处理结果 xwalkstats。 */
XRT_API xfuture* xrtFileTreeRemoveAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bKeepRoot
);



/* 递归删除目录及全部内容；成功值为处理结果 xwalkstats。 */
XRT_API xfuture* xrtDirRemoveAllAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 删除目录全部内容并保留根；成功值为处理结果 xwalkstats。 */
XRT_API xfuture* xrtDirCleanAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 异步移动目录树；成功 Future 没有值。 */
XRT_API xfuture* xrtDirMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);



/* 异步统计目录树；成功值为 xwalkstats。 */
XRT_API xfuture* xrtDirStatsAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
);



/* 异步计算普通文件总字节数；成功值为 xfilesize。 */
XRT_API xfuture* xrtDirSizeAsync(
	xtaskpool* pPool,
	cstr sPath,
	bool bRecursive
);



/* 异步创建缺失目录，或清空已有目录并保留根。 */
XRT_API xfuture* xrtDirEnsureEmptyAsync(
	xtaskpool* pPool,
	cstr sPath
);

#endif



#if defined(XRT_FEATURE_FILE_ASYNC_WHOLE)

/* 在任务池线程中读取整个文件；成功值为 Future 拥有的 xfiledata。 */
XRT_API xfuture* xrtFileReadAllAsync(
	xtaskpool* pPool,
	cstr sPath
);



/* 在硬上限内读取整个文件；文件超限时 Future 失败。 */
XRT_API xfuture* xrtFileReadAllLimitAsync(
	xtaskpool* pPool,
	cstr sPath,
	size_t iLimit
);



/* 复制输入并在任务池线程中完整覆盖文件。 */
XRT_API xfuture* xrtFileWriteAllAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);



/* 复制输入并使用操作系统追加语义完整写入。 */
XRT_API xfuture* xrtFileAppendAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);



/* 复制输入并通过同目录临时文件原子发布。 */
XRT_API xfuture* xrtFileWriteAtomicAsync(
	xtaskpool* pPool,
	cstr sPath,
	xbytesview Data
);

#endif



#if defined(XRT_FEATURE_FILE_ASYNC_MANAGE)

/* 在任务池线程中复制文件；成功 Future 没有值。 */
XRT_API xfuture* xrtFileCopyAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);



/* 在任务池线程中移动文件；成功 Future 没有值。 */
XRT_API xfuture* xrtFileMoveAsync(
	xtaskpool* pPool,
	cstr sSource,
	cstr sTarget,
	bool bReplace
);



/* 在任务池线程中删除文件；成功 Future 没有值。 */
XRT_API xfuture* xrtFileDeleteAsync(
	xtaskpool* pPool,
	cstr sPath
);

#endif



XRT_EXTERN_C_END

#endif
