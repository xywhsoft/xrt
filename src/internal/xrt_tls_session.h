#ifndef XRT_INTERNAL_TLS_SESSION_H
#define XRT_INTERNAL_TLS_SESSION_H

#include "xrt_tls.h"
#include "xrt_net_buffer.h"



#if defined(XRT_FEATURE_TLS_SESSION)

/* 当前记录视图借用 Feed 或 Scratch，直到调用完成接口。 */
typedef struct xtlssessionrecord {
	xtlsrecordtype Type;
	xbytesview Data;
	bool Protected;
} xtlssessionrecord;



/* KeyUpdate 的派生结果只在密文成功排队后提交到活动会话。 */
typedef struct xtlssessionupdate {
	xtlsrecordkey ReadKey;
	xtlsrecordkey WriteKey;
	uint8 ReadTraffic[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 WriteTraffic[XTLS_TRANSCRIPT_HASH_MAX_SIZE];
	uint8 Message[XTLS_HANDSHAKE_HEADER_SIZE + 1u];
	size_t MessageSize;
} xtlssessionupdate;



/* 角色清理过程释放尾部状态持有的共享资源，不释放会话本体。 */
typedef void (*xtlssessioncleanproc)(
	xtlssession* pSession,
	ptr pRole
);



/* 会话仅由所属线程或 Worker 驱动，不在内部增加锁。 */
struct xtlssession {
	xtlscontext* Context;
	xnetbuf Feed;
	xnetbuf Send;
	xnetbuf Plain;
	xnetbuf Scratch;
	xtlsrecordkey ReadKey;
	xtlsrecordkey WriteKey;
	xtlssessionrecord Record;
	size_t RecordSize;
	xtlsrole Role;
	xtlsstate State;
	xtlsversion Version;
	xtlscipher Cipher;
	uint32 Wait;
	bool RecordPending;
	bool CloseSent;
	bool CloseReceived;
	bool TransportEof;
	bool FatalSent;
	bool PeerAlertSet;
	xbytesview Protocol;
	xtlsalertlevel PeerAlertLevel;
	xtlsalert PeerAlert;
	size_t AllocationSize;
	xtlssessioncleanproc Clean;
};



/* 为客户端和服务端专用入口创建公共会话本体，不预分配队列块。 */
xtlssession* __xrtTlsSessionCreate(
	const xtlscontext* pContext,
	xnetbufpool* pPool,
	xtlsrole Role
);



/* 以一次精确分配创建会话及角色尾部状态。 */
xtlssession* __xrtTlsSessionCreateSized(
	const xtlscontext* pContext,
	xnetbufpool* pPool,
	xtlsrole Role,
	size_t iRoleSize,
	xtlssessioncleanproc Clean
);



/* 返回会话尾部角色状态；普通会话返回 NULL。 */
ptr __xrtTlsSessionRoleData(xtlssession* pSession);



/* 按公开生命周期图推进状态；FAILED 可以从任意非终态进入。 */
bool __xrtTlsSessionSetState(xtlssession* pSession, xtlsstate State);



/* 尽力排队与当前根错误匹配的 fatal Alert，再进入失败终态。 */
xtlsresult __xrtTlsSessionFail(xtlssession* pSession);



/* 设置状态机当前等待原因组合。 */
bool __xrtTlsSessionSetWait(xtlssession* pSession, uint32 iWait);



/* 发布由角色状态稳定持有的协商 ALPN 协议，只允许成功发布一次。 */
bool __xrtTlsSessionSetProtocol(
	xtlssession* pSession,
	xbytesview Protocol
);



/* 原子发布最终协商版本与密码套件，只允许角色状态机成功提交一次。 */
bool __xrtTlsSessionNegotiated(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher
);



/* 借用完整输入密文队列，供记录解析器读取但不得直接追加。 */
const xnetbuf* __xrtTlsSessionFeedBuffer(const xtlssession* pSession);



/* 借用待发密文块链，只有同线程传输适配器可以移动其内容。 */
xnetbuf* __xrtTlsSessionSendBuffer(xtlssession* pSession);



/* 借用待应用消费的明文块链，不允许绕过会话安全消费接口。 */
const xnetbuf* __xrtTlsSessionPlainBuffer(const xtlssession* pSession);



/* 按需连续化应用即将读取的明文前缀，但不绕过安全消费接口。 */
xtlsresult __xrtTlsSessionPlainPullup(
	xtlssession* pSession,
	size_t iSize,
	xnetspan* pSpan
);



/* 把会话后续惰性分配切换到所属 Worker 缓冲池。 */
bool __xrtTlsSessionPool(xtlssession* pSession, xnetbufpool* pPool);



/* 通知会话待发密文已经完整转交给可靠传输队列。 */
bool __xrtTlsSessionSendMoved(xtlssession* pSession);



/* 精确消费协议状态机已经处理的输入密文。 */
bool __xrtTlsSessionFeedConsume(xtlssession* pSession, size_t iSize);



/* 把输入队列前缀收敛为一个连续 Span。 */
xtlsresult __xrtTlsSessionFeedPullup(
	xtlssession* pSession,
	size_t iSize,
	xnetspan* pSpan
);



/* 向密文发送队列原子复制数据并执行发送硬上限。 */
xtlsresult __xrtTlsSessionSend(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
);



/* 接管一块完整密文输出；失败时所有权仍归调用方。 */
xtlsresult __xrtTlsSessionSendTake(
	xtlssession* pSession,
	ptr pData,
	size_t iSize
);



/* 为记录编码器预留受发送硬上限约束的连续尾部空间。 */
xtlsresult __xrtTlsSessionSendReserve(
	xtlssession* pSession,
	size_t iMinimum,
	xnetwspan* pSpan
);



/* 提交记录编码器已经写入的密文字节。 */
bool __xrtTlsSessionSendCommit(xtlssession* pSession, size_t iSize);



/* 取消尚未提交的密文发送预留。 */
bool __xrtTlsSessionSendCancel(xtlssession* pSession);



/* 向应用明文队列原子复制数据并执行明文硬上限。 */
xtlsresult __xrtTlsSessionPlain(
	xtlssession* pSession,
	const void* pData,
	size_t iSize
);



/* 为记录解密器预留受明文硬上限约束的连续尾部空间。 */
xtlsresult __xrtTlsSessionPlainReserve(
	xtlssession* pSession,
	size_t iMinimum,
	xnetwspan* pSpan
);



/* 提交记录解密器已经写入的明文字节。 */
bool __xrtTlsSessionPlainCommit(xtlssession* pSession, size_t iSize);



/* 取消尚未提交的明文写入预留。 */
bool __xrtTlsSessionPlainCancel(xtlssession* pSession);



/* 原子替换当前接收记录密钥；失败时保留原密钥和序列号。 */
bool __xrtTlsSessionReadKey(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
);



/* 原子替换当前发送记录密钥；失败时保留原密钥和序列号。 */
bool __xrtTlsSessionWriteKey(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview Key,
	xbytesview Iv
);



/* 验证两侧记录密钥后一次替换收发 epoch，任一失败时均保留原状态。 */
bool __xrtTlsSessionKeys(
	xtlssession* pSession,
	xtlsversion Version,
	xtlscipher Cipher,
	xbytesview ReadKey,
	xbytesview ReadIv,
	xbytesview WriteKey,
	xbytesview WriteIv
);



/* 从 TLS 1.3 traffic secret 原子初始化一组记录密钥。 */
bool __xrtTls13RecordKey(
	xtlscipher Cipher,
	xbytesview Traffic,
	xtlsrecordkey* pKey
);



/* 检查发送硬上限是否还能原子容纳指定明文长度的受保护记录。 */
xtlsresult __xrtTlsSessionRecordWritable(
	xtlssession* pSession,
	size_t iPlaintext,
	size_t iPadding,
	cstr sOperation
);



/* 检查发送硬上限是否还能原子容纳一条 KeyUpdate 记录。 */
xtlsresult __xrtTlsSessionKeyUpdateWritable(
	xtlssession* pSession,
	cstr sOperation
);



/* 为收到的 KeyUpdate 派生下一代接收 epoch，并按需准备应答写 epoch。 */
bool __xrtTls13KeyUpdateReceive(
	xtlscipher Cipher,
	xbytesview ReadTraffic,
	xbytesview WriteTraffic,
	xtlskeyupdate Request,
	xtlssessionupdate* pNext,
	cstr sOperation
);



/* 为主动 KeyUpdate 派生下一代发送 epoch 和完整线路消息。 */
bool __xrtTls13KeyUpdateSend(
	xtlscipher Cipher,
	xbytesview WriteTraffic,
	xtlskeyupdate Request,
	xtlssessionupdate* pNext,
	cstr sOperation
);



/* 清除接收记录密钥及其序列号；空会话无操作。 */
void __xrtTlsSessionReadKeyClear(xtlssession* pSession);



/* 清除发送记录密钥及其序列号；空会话无操作。 */
void __xrtTlsSessionWriteKeyClear(xtlssession* pSession);



/* 排队一条未受保护记录，用于初始握手和兼容 CCS。 */
xtlsresult __xrtTlsSessionRecordPlain(
	xtlssession* pSession,
	xtlsrecordtype Type,
	uint16 iLegacyVersion,
	xbytesview Data
);



/* 使用当前发送密钥排队一条受保护记录。 */
xtlsresult __xrtTlsSessionRecordProtect(
	xtlssession* pSession,
	xtlsrecordtype Type,
	xbytesview Data,
	size_t iPadding
);



/* 读取但不消费下一条完整记录；重复调用返回同一挂起视图。 */
xtlsresult __xrtTlsSessionRecordNext(
	xtlssession* pSession,
	xtlssessionrecord* pRecord
);



/* 完成挂起记录；应用记录可把解密块零拷贝移入 Plain。 */
xtlsresult __xrtTlsSessionRecordFinish(
	xtlssession* pSession,
	bool bApplication
);



/* 处理并消费一条挂起 Alert，发布认证关闭或对端失败。 */
xtlsresult __xrtTlsSessionRecordAlert(
	xtlssession* pSession,
	const xtlssessionrecord* pRecord
);

#endif

#endif
