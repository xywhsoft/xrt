#ifndef XRT_INTERNAL_NET_H
#define XRT_INTERNAL_NET_H

#include "xrt_internal.h"

#if defined(XRT_FEATURE_NET)
	#if !defined(_WIN32) && !defined(_WIN64)
		#include <arpa/inet.h>
		#include <netinet/in.h>
		#include <sys/socket.h>
	#endif
#endif



#if defined(XRT_FEATURE_NET)

/* BASIC 保留失败、拒绝和丢弃计数；无原子模块的地址层不声明统计操作。 */
#if defined(XRT_FEATURE_ATOMIC) && \
	(XRT_NET_STATS_LEVEL >= XNET_STATS_BASIC)
	#define __xrtNetStatBasicAdd(pValue, iAmount) \
		((void)xrtAtomic64FetchAdd((pValue), (iAmount), XMEMORY_RELAXED))
	#define __xrtNetStatBasicStore32(pValue, iValue) \
		xrtAtomic32Store((pValue), (iValue), XMEMORY_RELEASE)
#else
	#define __xrtNetStatBasicAdd(pValue, iAmount) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iAmount); \
		} while ( false )
	#define __xrtNetStatBasicStore32(pValue, iValue) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iValue); \
		} while ( false )
#endif



/* FULL 增加吞吐、事件、命中率和高水位统计；较低级别没有热路径成本。 */
#if defined(XRT_FEATURE_ATOMIC) && \
	(XRT_NET_STATS_LEVEL >= XNET_STATS_FULL)
	#define __xrtNetStatFullAdd(pValue, iAmount) \
		((void)xrtAtomic64FetchAdd((pValue), (iAmount), XMEMORY_RELAXED))
	#define __xrtNetStatFullStore32(pValue, iValue) \
		xrtAtomic32Store((pValue), (iValue), XMEMORY_RELAXED)

	/* 以弱一致读取和 CAS 更新 64 位单调峰值。 */
	static inline void __xrtNetStatFullPeak64(
		xatomic64* pPeak,
		uint64 iValue
	)
	{
		uint64 iPeak = xrtAtomic64Load(pPeak, XMEMORY_RELAXED);

		while ( iValue > iPeak ) {
			if (
				xrtAtomic64CompareExchange(
					pPeak,
					&iPeak,
					iValue,
					XMEMORY_RELAXED,
					XMEMORY_RELAXED
				)
			) {
				break;
			}
		}
	}



	/* 以弱一致读取和 CAS 更新 32 位单调峰值。 */
	static inline void __xrtNetStatFullPeak32(
		xatomic32* pPeak,
		uint32 iValue
	)
	{
		uint32 iPeak = xrtAtomic32Load(pPeak, XMEMORY_RELAXED);

		while ( iValue > iPeak ) {
			if (
				xrtAtomic32CompareExchange(
					pPeak,
					&iPeak,
					iValue,
					XMEMORY_RELAXED,
					XMEMORY_RELAXED
				)
			) {
				break;
			}
		}
	}
#else
	#define __xrtNetStatFullAdd(pValue, iAmount) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iAmount); \
		} while ( false )
	#define __xrtNetStatFullStore32(pValue, iValue) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iValue); \
		} while ( false )
	#define __xrtNetStatFullPeak64(pValue, iValue) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iValue); \
		} while ( false )
	#define __xrtNetStatFullPeak32(pValue, iValue) \
		do { \
			(void)sizeof(pValue); \
			(void)sizeof(iValue); \
		} while ( false )
#endif




/* 设置网络模块结构化错误。 */
void __xrtNetSetError(xerrkind Kind, xneterror Code,
	cstr sOperation, cstr sMessage, int iSystemCode);



/* 无分配、无错误副作用地尝试解析一个不要求零结尾的数字 IP 视图。 */
bool __xrtNetAddrTryParse(
	xnetaddr* pAddr,
	xstrview IP,
	uint16 iPort
);



#if defined(XRT_FEATURE_NET_INTERFACE)

/* 无错误副作用地把不要求零结尾的接口名称转换为地址族索引。 */
bool __xrtNetInterfaceTryIndex(
	xstrview Name,
	xnetfamily Family,
	uint32* pIndex
);

#endif



/* 按需初始化平台网络运行时，进程生命周期内保持有效。 */
bool __xrtNetEnsure(void);

#endif

#endif
