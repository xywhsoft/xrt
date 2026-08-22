#ifndef XRT_XID_H
#define XRT_XID_H

#include <xrt/error.h>
#include <xrt/time.h>



#if defined(XRT_FEATURE_XID) && \
	(!defined(XRT_FEATURE_TIME) || \
	 !defined(XRT_FEATURE_RANDOM_SECURE) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XRT_FEATURE_XID requires time, random_secure and codec_base64"
#endif



#if defined(XRT_FEATURE_XID)

#define XID_BINARY_SIZE		24u
#define XID_TEXT_SIZE		32u
#define XID_TEXT_CAPACITY	(XID_TEXT_SIZE + 1u)



/* XID 是按时间排序的 192 位值；字节布局和宿主端序无关。 */
typedef struct xid {
	uint8 Data[XID_BINARY_SIZE];
} xid;



/* 静态零值初始化器只能用于对象定义。 */
#define XID_ZERO { { 0 } }



/* XID 文本解析错误使用稳定域 xrt.xid。 */
typedef enum xiderror {
	XID_ERROR_FORMAT = 1
} xiderror;



XRT_EXTERN_C_BEGIN



/* 生成一个使用当前 Unix 微秒和 128 位系统安全随机数的 XID。 */
XRT_API bool xrtXidMake(xid* pXid);



/* 批量生成 XID，一次取得整批安全随机字节以降低系统调用成本。 */
XRT_API bool xrtXidMakeMany(xid* pXids, size_t iCount);



/* 生成一个由 xrtFree 释放的 32 字符 XID。 */
XRT_API str xrtXidMakeString(void);



/* 把 XID 写为 32 字符有序 URL-safe 文本，并在末尾补零。 */
XRT_API bool xrtXidWrite(
	const xid* pXid,
	char* sOutput,
	size_t iCapacity
);



/* 创建由 xrtFree 释放的 XID 文本。 */
XRT_API str xrtXidFormat(const xid* pXid);



/* 严格解析完整的 32 字符 XID；失败时不修改输出值。 */
XRT_API bool xrtXidParse(xstrview Text, xid* pXid);



/* 提取生成时间；任意 24 字节值都可以按稳定布局解释。 */
XRT_API bool xrtXidTime(const xid* pXid, xtime* pTime);



/* 按时间前缀和随机后缀执行三态字典序比较。 */
XRT_API int xrtXidCompare(const xid* pLeft, const xid* pRight);



/* 判断两个 XID 的全部 24 字节是否相同。 */
XRT_API bool xrtXidEqual(const xid* pLeft, const xid* pRight);



/* 判断 XID 是否为全零值。 */
XRT_API bool xrtXidIsZero(const xid* pXid);



/* 从 xrt.xid 格式错误的机器数据中读取文本字节位置。 */
XRT_API bool xrtXidErrorOffset(
	const xerror* pError,
	size_t* pOffset
);



XRT_EXTERN_C_END

#endif

#endif
