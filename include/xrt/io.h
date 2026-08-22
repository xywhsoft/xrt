#ifndef XRT_IO_H
#define XRT_IO_H

#include <xrt/error.h>

#if defined(XRT_FEATURE_IO_BUFFER)
	#include <xrt/buffer.h>
#endif

#if defined(XRT_FEATURE_IO_FILE)
	#include <xrt/file.h>
#endif



#if defined(XRT_FEATURE_IO_BUFFER) && \
	(!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_BUFFER))
	#error "XRT IO buffer adapters require IO and buffer support"
#endif

#if defined(XRT_FEATURE_IO_FILE) && \
	(!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_FILE))
	#error "XRT IO file adapters require IO and file support"
#endif

#if defined(XRT_FEATURE_IO_LINE) && \
	(!defined(XRT_FEATURE_IO) || !defined(XRT_FEATURE_BUFFER))
	#error "XRT line readers require IO and buffer support"
#endif



#if defined(XRT_FEATURE_IO)

/* Reader 和 Writer 是同步字节 IO 对象；同一对象的操作必须由调用方串行化。 */
typedef struct xreader xreader;
typedef struct xwriter xwriter;



#if defined(XRT_FEATURE_IO_LINE)

/* Line Reader 在通用 Reader 上提供有界流式行迭代。 */
typedef struct xlinereader xlinereader;



/* 行结束类型区分无终止符的末行、LF 与 CRLF。 */
typedef enum xlineend {
	XLINE_END_NONE = 0,
	XLINE_END_LF,
	XLINE_END_CRLF
} xlineend;



/* 行迭代结果明确区分正常结束、有效行和失败。 */
typedef enum xlinenext {
	XLINE_NEXT_ERROR = -1,
	XLINE_NEXT_END = 0,
	XLINE_NEXT_LINE = 1
} xlinenext;



/* 行内容借用到下一次迭代或销毁，不执行编码检查且不保证补零。 */
typedef struct xlineview {
	xstrview Text;
	xlineend End;
} xlineview;

#endif



/* IO 层稳定错误代码使用 xrt.io 域。 */
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



/* Read 和 Write 允许短操作；成功读取零字节只表示 EOF。 */
typedef bool (*xreadproc)(
	ptr pContext,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);

typedef bool (*xwriteproc)(
	ptr pContext,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);



/* 可选定位、查询、刷新和关闭过程失败时必须设置当前错误。 */
typedef bool (*xseekproc)(
	ptr pContext,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);

typedef bool (*xtellproc)(ptr pContext, uint64* pPosition);
typedef bool (*xsizeproc)(ptr pContext, uint64* pSize);
typedef bool (*xflushproc)(ptr pContext);
typedef bool (*xcloseproc)(ptr pContext);



/* Reader 回调表会在创建时复制；只有 Read 是必需过程。 */
typedef struct xreaderops {
	xreadproc Read;
	xseekproc Seek;
	xtellproc Tell;
	xsizeproc Size;
	xcloseproc Close;
} xreaderops;



/* Writer 回调表会在创建时复制；只有 Write 是必需过程。 */
typedef struct xwriterops {
	xwriteproc Write;
	xseekproc Seek;
	xtellproc Tell;
	xsizeproc Size;
	xflushproc Flush;
	xcloseproc Close;
} xwriterops;



XRT_EXTERN_C_BEGIN



/* 创建自定义 Reader；失败时 Context 所有权不变，销毁时调用一次 Close。 */
XRT_API xreader* xrtReaderCreate(
	const xreaderops* pOps,
	ptr pContext
);



/* 创建自定义 Writer；失败时 Context 所有权不变，销毁时调用一次 Close。 */
XRT_API xwriter* xrtWriterCreate(
	const xwriterops* pOps,
	ptr pContext
);



/* 创建借用固定字节视图的可定位 Reader。 */
XRT_API xreader* xrtReaderFromMemory(xbytesview Data);



/* 创建借用固定容量的可定位 Writer；稀疏写入产生的空洞会填零。 */
XRT_API xwriter* xrtWriterFromMemory(ptr pData, size_t iCapacity);



/* 创建只统计并丢弃全部输入的 Writer。 */
XRT_API xwriter* xrtWriterDiscard(void);



/* 单次读取；成功读取零字节表示并锁定 EOF，直到下一次成功 Seek。 */
XRT_API bool xrtReaderRead(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);



/* 持续读取到填满缓冲；提前 EOF 返回失败并保留实际读取量。 */
XRT_API bool xrtReaderReadFull(
	xreader* pReader,
	ptr pBuffer,
	size_t iRequest,
	size_t* pRead
);



/* 持续复制到输入 EOF，使用固定大小栈缓冲且不随数据量分配。 */
XRT_API bool xrtReaderCopy(
	xreader* pReader,
	xwriter* pWriter,
	uint64* pCopied
);



/* 精确复制指定字节数；输入提前 EOF 时返回失败和已复制量。 */
XRT_API bool xrtReaderCopyN(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iSize,
	uint64* pCopied
);



/* 在硬上限内复制到 EOF；超限时消费一个探测字节并返回范围错误。 */
XRT_API bool xrtReaderCopyLimit(
	xreader* pReader,
	xwriter* pWriter,
	uint64 iLimit,
	uint64* pCopied
);



/* 移动 Reader 游标；成功后清除已经锁定的 EOF。 */
XRT_API bool xrtReaderSeek(
	xreader* pReader,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);



/* 查询 Reader 游标；不支持时返回 XERR_UNSUPPORTED。 */
XRT_API bool xrtReaderTell(xreader* pReader, uint64* pPosition);



/* 查询 Reader 当前总大小；不支持时返回 XERR_UNSUPPORTED。 */
XRT_API bool xrtReaderSize(xreader* pReader, uint64* pSize);



/* 判断 Reader 是否提供定位或大小查询能力。 */
XRT_API bool xrtReaderCanSeek(const xreader* pReader);
XRT_API bool xrtReaderCanSize(const xreader* pReader);



/* 判断 Reader 是否已经通过非零请求观察到 EOF。 */
XRT_API bool xrtReaderEOF(const xreader* pReader);



/* 调用一次 Close 并销毁 Reader；Close 失败也一定释放对象。 */
XRT_API bool xrtReaderDestroy(xreader* pReader);



#if defined(XRT_FEATURE_IO_LINE)

/* 创建借用 Reader 的 Line Reader；最大行长只计算终止符之前的内容字节。 */
XRT_API xlinereader* xrtLineReaderCreate(
	xreader* pReader,
	size_t iMaxLine
);



/* 原子接管 Reader 槽；成功时清空来源，失败时所有权保持不变。 */
XRT_API xlinereader* xrtLineReaderTake(
	xreader** ppReader,
	size_t iMaxLine
);



/* 返回下一行借用视图；超限或底层读取失败后对象进入失败状态。 */
XRT_API xlinenext xrtLineReaderNext(
	xlinereader* pLines,
	xlineview* pLine
);



/* 释放 Line Reader；接管模式同时销毁底层 Reader 并返回关闭结果。 */
XRT_API bool xrtLineReaderDestroy(xlinereader* pLines);

#endif



/* 单次写入，允许成功短写但拒绝非零请求不产生进展。 */
XRT_API bool xrtWriterWrite(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);



/* 持续写入到全部完成；失败时保留实际写入量。 */
XRT_API bool xrtWriterWriteFull(
	xwriter* pWriter,
	const void* pBuffer,
	size_t iRequest,
	size_t* pWritten
);



/* 移动 Writer 游标；不支持时返回 XERR_UNSUPPORTED。 */
XRT_API bool xrtWriterSeek(
	xwriter* pWriter,
	int64 iOffset,
	xseek Origin,
	uint64* pPosition
);



/* 查询 Writer 游标或当前逻辑大小。 */
XRT_API bool xrtWriterTell(xwriter* pWriter, uint64* pPosition);
XRT_API bool xrtWriterSize(xwriter* pWriter, uint64* pSize);



/* 判断 Writer 是否提供定位或大小查询能力。 */
XRT_API bool xrtWriterCanSeek(const xwriter* pWriter);
XRT_API bool xrtWriterCanSize(const xwriter* pWriter);



/* 显式刷新 Writer；没有 Flush 回调时为空操作。 */
XRT_API bool xrtWriterFlush(xwriter* pWriter);



/* 调用一次 Close 并销毁 Writer；不会隐式调用可能昂贵的 Flush。 */
XRT_API bool xrtWriterDestroy(xwriter* pWriter);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_IO_BUFFER)

XRT_EXTERN_C_BEGIN



/* 创建借用 Buffer 的 Reader；使用期间不得修改或销毁 Buffer。 */
XRT_API xreader* xrtReaderFromBuffer(const xbuffer* pBuffer);



/* 接管 Buffer 并创建 Reader；成功时把调用方槽清空。 */
XRT_API xreader* xrtReaderTakeBuffer(xbuffer** ppBuffer);



/* 创建借用 Buffer 的 Writer，初始游标位于已有内容末尾。 */
XRT_API xwriter* xrtWriterFromBuffer(xbuffer* pBuffer);



/* 在硬上限内读取到新 Buffer；超限时消费一个探测字节。 */
XRT_API xbuffer* xrtReaderReadAll(xreader* pReader, size_t iLimit);



/* 完整写入 Buffer 当前有效内容。 */
XRT_API bool xrtWriterWriteBuffer(
	xwriter* pWriter,
	const xbuffer* pBuffer
);



XRT_EXTERN_C_END

#endif



#if defined(XRT_FEATURE_IO_FILE)

XRT_EXTERN_C_BEGIN



/* 创建借用文件对象的 Reader 或 Writer。 */
XRT_API xreader* xrtReaderFromFile(xfile File);
XRT_API xwriter* xrtWriterFromFile(xfile File);



/* 接管文件对象；成功时把调用方槽清空。 */
XRT_API xreader* xrtReaderTakeFile(xfile* pFile);
XRT_API xwriter* xrtWriterTakeFile(xfile* pFile);



/* 打开路径并创建拥有文件对象的 Reader。 */
XRT_API xreader* xrtReaderOpen(cstr sPath);



/* 创建或截断路径并创建拥有文件对象的 Writer。 */
XRT_API xwriter* xrtWriterOpen(cstr sPath);



/* 以操作系统追加语义打开路径并创建拥有文件对象的 Writer。 */
XRT_API xwriter* xrtWriterOpenAppend(cstr sPath);



XRT_EXTERN_C_END

#endif

#endif
