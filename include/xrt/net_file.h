#ifndef XRT_NET_FILE_H
#define XRT_NET_FILE_H

#include <xrt/file.h>
#include <xrt/net.h>



#if defined(XRT_FEATURE_NET_FILE) && \
	(!defined(XRT_FEATURE_FILE) || !defined(XRT_FEATURE_NET_ENGINE))
	#error "XRT native network file support requires file and network engine support"
#endif

#if defined(XRT_FEATURE_NET_FILE) && defined(__linux__) && \
	!defined(__ANDROID__) && !defined(XRT_FEATURE_NET_PORT_URING)
	#error "XRT native network file support requires io_uring on Linux"
#endif



XRT_EXTERN_C_BEGIN



#if defined(XRT_FEATURE_NET_FILE)

/*
	打开可提交到完成端口的文件，并自动附加 XFILE_ASYNC。
	对象由调用方拥有；全部在途操作终结后使用 xrtClose 关闭。
*/
XRT_API xfile xrtNetFileOpen(
	cstr sPath,
	const xfileoptions* pOptions
);



/*
	从绝对偏移读取到调用方缓冲，返回非零操作标识。
	只能在所属 Worker 执行；文件、缓冲和 Completion 保持到唯一终态。
*/
XRT_API uint64 xrtNetFileRead(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
);



/*
	把调用方缓冲写入绝对偏移，返回非零操作标识。
	热路径不复制载荷；文件、缓冲和 Completion 保持到唯一终态。
*/
XRT_API uint64 xrtNetFileWrite(
	xnetworker* pWorker,
	xfile File,
	uint64 iOffset,
	const void* pData,
	size_t iSize,
	xnetcompletion* pCompletion
);



/* 在所属 Worker 请求取消操作；原操作仍通过 Completion 产生唯一终态。 */
XRT_API bool xrtNetFileCancel(
	xnetworker* pWorker,
	uint64 Id
);

#endif



XRT_EXTERN_C_END

#endif
