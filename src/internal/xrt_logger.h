#ifndef XRT_INTERNAL_LOGGER_H
#define XRT_INTERNAL_LOGGER_H



#if defined(XRT_FEATURE_LOGGER_CORE)

/* 创建带日志域、操作名和可选原因链的结构化错误，并接管原因引用。 */
xerror* __xrtLogErrorCreate(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage,
	xerror* pCause
);



/* 用当前线程错误作为原因，设置一条新的日志域结构化错误。 */
void __xrtLogErrorSet(
	xerrkind Kind,
	xlogerror Code,
	cstr sOperation,
	cstr sMessage
);




/* 验证 Sink 写回调身份，并返回只在 Sink 生命周期内有效的用户数据。 */
bool __xrtLogSinkData(
	const xlogsink* pSink,
	xlogsinkwriteproc pWrite,
	ptr* ppUserData
);



/* 计算一条拥有型记录所需的连续存储，包含记录、字段和全部文本。 */
bool __xrtLogOwnedSize(
	const xlogrecord* pRecord,
	size_t* pSize
);



/* 把借用记录深拷贝到调用方连续存储，成功后由 Clear 释放错误字段引用。 */
bool __xrtLogOwnedCopy(
	const xlogrecord* pSource,
	xlogrecord* pTarget,
	size_t iCapacity
);



/* 释放拥有型记录持有的错误字段引用；不释放记录存储。 */
void __xrtLogOwnedClear(xlogrecord* pRecord);

#endif



#endif
