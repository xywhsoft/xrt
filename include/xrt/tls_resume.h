#ifndef XRT_TLS_RESUME_H
#define XRT_TLS_RESUME_H

#include <xrt/tls.h>
#include <xrt/time.h>



#if defined(XRT_FEATURE_TLS_RESUME) && \
	(!defined(XRT_FEATURE_TLS) || !defined(XRT_FEATURE_TIME) || \
	 !defined(XRT_FEATURE_CRYPTO_CORE))
	#error "XRT_FEATURE_TLS_RESUME requires TLS, time and crypto core support"
#endif



#if defined(XRT_FEATURE_TLS_RESUME)

typedef struct xtlsresume xtlsresume;



/*
	恢复配置在创建期间借用全部视图；成功后对象持有一份不可变深拷贝。
	PeerIdentity 是调用方定义的已认证对端标识，可为空，但不参与线路编码。
*/
typedef struct xtlsresumeconfig {
	xtlsversion Version;
	xtlscipher Cipher;
	xbytesview Ticket;
	xbytesview Secret;
	xstrview ServerName;
	xbytesview Protocol;
	xbytesview PeerIdentity;
	uint32 Lifetime;
	uint32 AgeAdd;
	uint32 MaxEarlyData;
	xtime IssuedAt;
} xtlsresumeconfig;



/* 信息快照中的视图由恢复对象持有，只能在对象引用存活期间借用。 */
typedef struct xtlsresumeinfo {
	xtlsversion Version;
	xtlscipher Cipher;
	xbytesview Ticket;
	xbytesview Secret;
	xstrview ServerName;
	xbytesview Protocol;
	xbytesview PeerIdentity;
	uint32 Lifetime;
	uint32 AgeAdd;
	uint32 MaxEarlyData;
	xtime IssuedAt;
	xtime ExpiresAt;
} xtlsresumeinfo;



XRT_EXTERN_C_BEGIN



/* 初始化 TLS 1.3 恢复配置，并把签发时间设为当前墙钟时间。 */
XRT_API void xrtTlsResumeConfigInit(xtlsresumeconfig* pConfig);



/* 创建单次精确分配、深拷贝且可跨线程共享的恢复对象。 */
XRT_API xtlsresume* xrtTlsResumeCreate(
	const xtlsresumeconfig* pConfig
);



/* 增加恢复对象引用；对象内容在全部引用之间保持只读。 */
XRT_API xtlsresume* xrtTlsResumeRetain(const xtlsresume* pResume);



/* 释放恢复对象，并在最后一个引用结束时清除票据、PSK 与全部元数据。 */
XRT_API void xrtTlsResumeRelease(xtlsresume* pResume);



/* 发布恢复对象的只读信息快照；输出视图不得超过对象引用生命周期。 */
XRT_API bool xrtTlsResumeInfo(
	const xtlsresume* pResume,
	xtlsresumeinfo* pInfo
);



/* 判断给定墙钟时刻是否位于票据的半开有效区间内。 */
XRT_API bool xrtTlsResumeValidAt(
	const xtlsresume* pResume,
	xtime iNow
);



/* 计算 TLS 1.3 ClientHello 使用的混淆票据年龄，过期时不修改输出。 */
XRT_API bool xrtTlsResumeTicketAge(
	const xtlsresume* pResume,
	xtime iNow,
	uint32* pAge
);



XRT_EXTERN_C_END

#endif

#endif
