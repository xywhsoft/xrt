/*
 * MIT License
 *
 * Copyright (c) 2025 xLeaves [xywhsoft] <xywhsoft@qq.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/* 此文件由 tools/amalgamate.py 生成，请勿直接修改。 */
/* Supply XRT and selected extension dependencies before this header. */
#if !defined(XRT_CORE_H)
#error "xpop3 requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XPOP3_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XPOP3_IMPLEMENTATION) && \
	!defined(_WIN32) && !defined(_WIN64)
	#if defined(__linux__) && !defined(_GNU_SOURCE)
		#define _GNU_SOURCE 1
	#endif
	#if defined(__APPLE__) && !defined(_DARWIN_C_SOURCE)
		#define _DARWIN_C_SOURCE 1
	#endif
	#if !defined(_POSIX_C_SOURCE)
		#define _POSIX_C_SOURCE 200809L
	#endif
	#if !defined(_FILE_OFFSET_BITS)
		#define _FILE_OFFSET_BITS 64
	#endif
#endif
#ifndef XPOP3_SINGLE_HEADER_H
#define XPOP3_SINGLE_HEADER_H
#define XPOP3_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xpop3/include/xpop3/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XPOP3_FEATURES_H
#define XPOP3_FEATURES_H

/* pop3_message 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_MESSAGE)
#ifndef XPOP3_FEATURE_POP3_MESSAGE
#define XPOP3_FEATURE_POP3_MESSAGE
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_TREE
#define XMAIL_MODULE_MAIL_TREE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* pop3_auth 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_AUTH)
#ifndef XPOP3_FEATURE_POP3_AUTH
#define XPOP3_FEATURE_POP3_AUTH
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* pop3_client_tls 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_CLIENT_TLS)
#ifndef XPOP3_FEATURE_POP3_CLIENT_TLS
#define XPOP3_FEATURE_POP3_CLIENT_TLS
#endif
#ifndef XPOP3_MODULE_POP3_CLIENT
#define XPOP3_MODULE_POP3_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* pop3_client 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3_CLIENT)
#ifndef XPOP3_FEATURE_POP3_CLIENT
#define XPOP3_FEATURE_POP3_CLIENT
#endif
#ifndef XPOP3_MODULE_POP3
#define XPOP3_MODULE_POP3
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* pop3 及其直接依赖。 */
#if defined(XPOP3_MODULE_ALL) || defined(XPOP3_MODULE_POP3)
#ifndef XPOP3_FEATURE_POP3
#define XPOP3_FEATURE_POP3
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XPOP3_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xpop3/include/xrt/pop3.h */
/* ========================================================================== */

#ifndef XRT_POP3_H
#define XRT_POP3_H




#if defined(XPOP3_FEATURE_POP3) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XPOP3_FEATURE_POP3 requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XPOP3_FEATURE_POP3)

#define XPOP3_COMMAND_MAX 512u
#define XPOP3_AUTH_COMMAND_MAX 255u
#define XPOP3_AUTH_RESPONSE_MAX 12288u

#define XPOP3_CAP_TOP UINT32_C(0x00000001)
#define XPOP3_CAP_USER UINT32_C(0x00000002)
#define XPOP3_CAP_SASL UINT32_C(0x00000004)
#define XPOP3_CAP_RESP_CODES UINT32_C(0x00000008)
#define XPOP3_CAP_LOGIN_DELAY UINT32_C(0x00000010)
#define XPOP3_CAP_PIPELINING UINT32_C(0x00000020)
#define XPOP3_CAP_EXPIRE UINT32_C(0x00000040)
#define XPOP3_CAP_UIDL UINT32_C(0x00000080)
#define XPOP3_CAP_IMPLEMENTATION UINT32_C(0x00000100)
#define XPOP3_CAP_STLS UINT32_C(0x00000200)

#define XPOP3_SASL_PLAIN UINT32_C(0x00000001)
#define XPOP3_SASL_XOAUTH2 UINT32_C(0x00000002)
#define XPOP3_SASL_OAUTHBEARER UINT32_C(0x00000004)



/* POP3 状态行借用输入，Text 不包含状态指示符和其后的空白。 */
typedef struct xpop3replyview {
	xstrview Source;
	xstrview Text;
	bool Ok;
} xpop3replyview;



/* STAT 响应使用 64 位消息数量和总字节数。 */
typedef struct xpop3stat {
	uint64 Messages;
	uint64 Bytes;
} xpop3stat;



/* LIST 行包含 1 起始消息序号和字节数。 */
typedef struct xpop3listview {
	uint64 Message;
	uint64 Bytes;
} xpop3listview;



/* UIDL 行的唯一 ID 借用输入行。 */
typedef struct xpop3uidlview {
	uint64 Message;
	xstrview Id;
} xpop3uidlview;



/* CAPA 行的名称和参数均借用输入。 */
typedef struct xpop3capabilityview {
	xstrview Source;
	xstrview Name;
	xstrview Parameters;
} xpop3capabilityview;



XRT_EXTERN_C_BEGIN



/* 解析 `+OK` 或 `-ERR` 状态行。 */
XRT_API bool xrtPop3ReplyParse(xstrview Line, xpop3replyview* pReply);



/* 解析成功 STAT 响应中的消息数量和总字节数。 */
XRT_API bool xrtPop3StatParse(xstrview Line, xpop3stat* pStat);



/* 解析多行 LIST 响应中的一项。 */
XRT_API bool xrtPop3ListParse(xstrview Line, xpop3listview* pItem);



/* 解析多行 UIDL 响应中的一项。 */
XRT_API bool xrtPop3UidlParse(xstrview Line, xpop3uidlview* pItem);



/* 解析一条 CAPA 能力行。 */
XRT_API bool xrtPop3CapabilityParse(
	xstrview Line,
	xpop3capabilityview* pCapability
);



/* 返回已知 POP3 能力名称的稳定标记，未知扩展返回零。 */
XRT_API uint32 xrtPop3Capability(xstrview Name);



/* 返回内置 SASL 机制名称的稳定标记，未知机制返回零。 */
XRT_API uint32 xrtPop3SaslMechanism(xstrview Name);



/* 安全写出 `Verb [Arguments]\r\n`，拒绝控制字符和超长命令。 */
XRT_API bool xrtPop3CommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 POP3 命令行。 */
XRT_API str xrtPop3Command(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xpop3/include/xrt/pop3_client.h */
/* ========================================================================== */

#ifndef XRT_POP3_CLIENT_H
#define XRT_POP3_CLIENT_H




#if defined(XPOP3_FEATURE_POP3_CLIENT) && \
	(!defined(XPOP3_FEATURE_POP3) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XPOP3_FEATURE_POP3_CLIENT requires POP3 and mail net"
#endif

#if defined(XPOP3_FEATURE_POP3_CLIENT_TLS) && \
	(!defined(XPOP3_FEATURE_POP3_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XPOP3_FEATURE_POP3_CLIENT_TLS requires POP3 client and mail net TLS"
#endif



#if defined(XPOP3_FEATURE_POP3_CLIENT)

typedef struct xpop3client xpop3client;



/* POP3 状态显式区分认证、事务和未消费完的多行响应。 */
typedef enum xpop3clientstate {
	XPOP3_CLIENT_AUTHORIZATION = 0,
	XPOP3_CLIENT_TRANSACTION,
	XPOP3_CLIENT_MULTILINE,
	XPOP3_CLIENT_UPDATE,
	XPOP3_CLIENT_CLOSED,
	XPOP3_CLIENT_FAILED
} xpop3clientstate;



/* 配置借用共享网络对象，只在 Open 期间读取。 */
typedef struct xpop3clientconfig {
	xmailnetconfig Net;
	bool ReadCapabilities;
} xpop3clientconfig;



/* 最后状态行借用 Client，下一次 Receive 或销毁后失效。 */
typedef struct xpop3reply {
	bool Ok;
	xstrview Source;
	xstrview Text;
} xpop3reply;



XRT_EXTERN_C_BEGIN



/* 初始化明文 110 端口并默认读取 CAPA。 */
XRT_API void xrtPop3ClientConfigInit(xpop3clientconfig* pConfig);



/* 验证网络配置和 STLS 所需的能力读取设置。 */
XRT_API bool xrtPop3ClientConfigValid(const xpop3clientconfig* pConfig);



/* 建立连接，验证 greeting，读取 CAPA 并按需完成 STLS。 */
XRT_API xpop3client* xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回当前 POP3 状态。 */
XRT_API xpop3clientstate xrtPop3ClientState(const xpop3client* pClient);



/* 返回最近一次 CAPA 的已知能力位集。 */
XRT_API uint32 xrtPop3ClientCapabilities(const xpop3client* pClient);



/* 返回最近一次 CAPA 中 SASL 参数列出的已知认证机制。 */
XRT_API uint32 xrtPop3ClientSaslMechanisms(const xpop3client* pClient);



/* 返回当前会话实际使用的传输安全级别。 */
XRT_API xmailsecurity xrtPop3ClientSecurity(const xpop3client* pClient);



/* 取得最近有效状态行的稳定借用视图；畸形行不会覆盖它。 */
XRT_API bool xrtPop3ClientLastReply(
	const xpop3client* pClient,
	xpop3reply* pReply
);



/* 发送一条不含 CRLF 的低层 POP3 行。 */
XRT_API bool xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送可超过普通命令上限的 SASL 响应或取消行。 */
XRT_API bool xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取一条不含 CRLF 的原始线路，用于 SASL continuation 等扩展。 */
XRT_API bool xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取并验证一条 +OK 或 -ERR 状态行。 */
XRT_API bool xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 构建并发送命令，然后读取状态行。 */
XRT_API bool xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送预期多行数据的命令；+OK 后进入 MULTILINE。 */
XRT_API bool xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 逐行去除 dot transparency；END 自动恢复命令前状态。 */
XRT_API xmailnext xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询邮箱消息数量和总字节数。 */
XRT_API bool xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询一条消息的 LIST 大小。 */
XRT_API bool xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取全部 LIST 项。 */
XRT_API bool xrtPop3ClientListAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 查询一条消息的 UIDL。 */
XRT_API bool xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取全部 UIDL 项。 */
XRT_API bool xrtPop3ClientUidlAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取一封完整邮件。 */
XRT_API bool xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始流式读取邮件字段和指定数量的正文行。 */
XRT_API bool xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 标记一条消息在 UPDATE 阶段删除。 */
XRT_API bool xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 清除当前会话的删除标记。 */
XRT_API bool xrtPop3ClientReset(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证事务连接仍可交换命令。 */
XRT_API bool xrtPop3ClientNoop(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送 QUIT，验证成功状态并认证关闭传输。 */
XRT_API bool xrtPop3ClientQuit(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不发送 QUIT，直接正常关闭传输。 */
XRT_API bool xrtPop3ClientClose(
	xpop3client* pClient,
	xdeadline iDeadline
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtPop3ClientAbort(xpop3client* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtPop3ClientDestroy(xpop3client* pClient);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xpop3/include/xrt/pop3_auth.h */
/* ========================================================================== */

#ifndef XRT_POP3_AUTH_H
#define XRT_POP3_AUTH_H




#if defined(XPOP3_FEATURE_POP3_AUTH) && \
	(!defined(XPOP3_FEATURE_POP3_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XPOP3_FEATURE_POP3_AUTH requires POP3 client and Base64"
#endif



#if defined(XPOP3_FEATURE_POP3_AUTH)

/* USER/PASS 保留传统命令，其余机制使用 RFC 5034 SASL 交换。 */
typedef enum xpop3authmethod {
	XPOP3_AUTH_USER_PASS = 0,
	XPOP3_AUTH_PLAIN,
	XPOP3_AUTH_XOAUTH2,
	XPOP3_AUTH_OAUTHBEARER
} xpop3authmethod;



/* 凭据只在 Auth 调用期间借用，结束前所有临时副本都会被清零。 */
typedef struct xpop3authconfig {
	xpop3authmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} xpop3authconfig;

XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、初始响应开启和明文凭据关闭的安全默认值。 */
XRT_API void xrtPop3AuthConfigInit(xpop3authconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtPop3AuthConfigValid(const xpop3authconfig* pConfig);



/* 在 AUTHORIZATION 会话上执行 USER/PASS 或配置的 SASL 机制。 */
XRT_API bool xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 使用 USER/PASS 认证；默认应拒绝明文传输，显式参数用于受控兼容场景。 */
XRT_API bool xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xpop3/include/xrt/pop3_message.h */
/* ========================================================================== */

#ifndef XRT_POP3_MESSAGE_H
#define XRT_POP3_MESSAGE_H




#if defined(XPOP3_FEATURE_POP3_MESSAGE) && \
	(!defined(XPOP3_FEATURE_POP3_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_TREE) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XPOP3_FEATURE_POP3_MESSAGE requires POP3 client, mail tree and buffer"
#endif



#if defined(XPOP3_FEATURE_POP3_MESSAGE)

#define XPOP3_MESSAGE_BYTES_DEFAULT XMAIL_TREE_SOURCE_BYTES_DEFAULT



XRT_EXTERN_C_BEGIN



/* 流式读取完整邮件并恢复每一行的 CRLF；零预算使用默认上限。 */
XRT_API bool xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 流式读取字段和指定数量正文行，并恢复每一行的 CRLF。 */
XRT_API bool xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取完整邮件并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取 TOP 结果并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 按 MIME 树预算读取并解析完整邮件；成功结果不依赖网络缓冲。 */
XRT_API bool xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xpop3/include/xpop3.h */
/* ========================================================================== */

#ifndef XPOP3_H
#define XPOP3_H


#if defined(XPOP3_FEATURE_POP3_CLIENT) || \
	defined(XPOP3_FEATURE_POP3_CLIENT_TLS)
#endif

#if defined(XPOP3_FEATURE_POP3_AUTH)
#endif

#if defined(XPOP3_FEATURE_POP3_MESSAGE)
#endif

#endif

#endif

#if defined(XPOP3_IMPLEMENTATION) && !defined(XPOP3_IMPLEMENTATION_ONCE)
#define XPOP3_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xpop3/src/internal/xrt_mail.h */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3) || \
	defined(XPOP3_FEATURE_POP3_CLIENT) || \
	defined(XPOP3_FEATURE_POP3_CLIENT_TLS) || \
	defined(XPOP3_FEATURE_POP3_AUTH) || \
	defined(XPOP3_FEATURE_POP3_MESSAGE)
#ifndef XPOP3_INTERNAL_XRT_MAIL_H_BRIDGE_H
#define XPOP3_INTERNAL_XRT_MAIL_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xpop3/src/internal/xrt_mail_net.h */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_CLIENT) || \
	defined(XPOP3_FEATURE_POP3_CLIENT_TLS) || \
	defined(XPOP3_FEATURE_POP3_AUTH)
#ifndef XPOP3_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H
#define XPOP3_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xpop3/src/internal/xrt_pop3_client.h */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_CLIENT) || \
	defined(XPOP3_FEATURE_POP3_AUTH)
#ifndef XRT_INTERNAL_POP3_CLIENT_H
#define XRT_INTERNAL_POP3_CLIENT_H




#if defined(XPOP3_FEATURE_POP3_CLIENT)

/* POP3 认证扩展只共享状态对象，不复制客户端实现。 */
struct xpop3client {
	__xmailtransport Transport;
	xpop3clientstate State;
	xpop3clientstate ReturnState;
	uint32 Capabilities;
	uint32 SaslMechanisms;
	__xmailtext Reply;
};



bool __xrtPop3ClientReplySave(
	xpop3client* pClient,
	xstrview Line,
	xpop3reply* pReply
);



bool __xrtPop3ClientFail(xpop3client* pClient);



bool __xrtPop3ClientAuthorize(xpop3client* pClient);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xpop3/src/internal/xrt_mail_auth.h */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_AUTH)
#ifndef XPOP3_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H
#define XPOP3_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* source: extlibs/xpop3/src/pop3/pop3.c */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3)



#if defined(XPOP3_FEATURE_POP3)

/* 跳过 POP3 token 之间的空格和制表符。 */
static size_t __xrtPop3Space(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) &&
		 ((Text.Data[iPosition] == ' ') || (Text.Data[iPosition] == '\t')) ) {
		iPosition++;
	}
	return iPosition;
}



/* 严格读取一个十进制 uint64 token。 */
static bool __xrtPop3Uint64(
	xstrview Text,
	size_t* pPosition,
	uint64* pValue
)
{
	size_t iPosition = *pPosition;
	uint64 iValue = 0;
	size_t iDigits = 0;

	while ( iPosition < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			break;
		}
		if ( iValue > ((UINT64_MAX -
			(uint64)(iByte - (unsigned char)'0')) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
		iPosition++;
		iDigits++;
	}
	if ( iDigits == 0 ) {
		return false;
	}
	*pPosition = iPosition;
	*pValue = iValue;
	return true;
}



/* 解析 POP3 状态行。 */
XRT_API bool xrtPop3ReplyParse(xstrview Line, xpop3replyview* pReply)
{
	xpop3replyview Reply;
	size_t iPrefix;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 xrtMemRangesOverlap(pReply, sizeof(*pReply), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (Line.Size >= 3u) &&
		 (memcmp(Line.Data, "+OK", 3u) == 0) ) {
		Reply.Ok = true;
		iPrefix = 3u;
	} else if ( (Line.Size >= 4u) &&
		 (memcmp(Line.Data, "-ERR", 4u) == 0) ) {
		Reply.Ok = false;
		iPrefix = 4u;
	} else {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 status line"
		);
		return false;
	}
	if ( (iPrefix < Line.Size) && (Line.Data[iPrefix] != ' ') &&
		 (Line.Data[iPrefix] != '\t') ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 status separator"
		);
		return false;
	}
	for ( size_t i = iPrefix; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"POP3 status contains a control character"
			);
			return false;
		}
	}
	iPrefix = __xrtPop3Space(Line, iPrefix);
	Reply.Source = Line;
	Reply.Text = __xrtMailSlice(Line, iPrefix, Line.Size - iPrefix);
	*pReply = Reply;
	return true;
}



/* 解析两个无符号十进制 token，并要求行尾无额外内容。 */
static bool __xrtPop3Pair(
	xstrview Text,
	uint64* pFirst,
	uint64* pSecond
)
{
	size_t iPosition = __xrtPop3Space(Text, 0);
	uint64 iFirst;
	uint64 iSecond;

	if ( !__xrtPop3Uint64(Text, &iPosition, &iFirst) ) {
		return false;
	}
	if ( (iPosition == Text.Size) ||
		 ((Text.Data[iPosition] != ' ') && (Text.Data[iPosition] != '\t')) ) {
		return false;
	}
	iPosition = __xrtPop3Space(Text, iPosition);
	if ( !__xrtPop3Uint64(Text, &iPosition, &iSecond) ) {
		return false;
	}
	iPosition = __xrtPop3Space(Text, iPosition);
	if ( iPosition != Text.Size ) {
		return false;
	}
	*pFirst = iFirst;
	*pSecond = iSecond;
	return true;
}



/* 解析 STAT 成功响应。 */
XRT_API bool xrtPop3StatParse(xstrview Line, xpop3stat* pStat)
{
	xpop3replyview Reply;
	xpop3stat Stat;

	if ( !xrtMemRangeValid(pStat, sizeof(*pStat)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ReplyParse(Line, &Reply) ) {
		return false;
	}
	if ( !Reply.Ok ||
		 !__xrtPop3Pair(Reply.Text, &Stat.Messages, &Stat.Bytes) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 STAT response"
		);
		return false;
	}
	*pStat = Stat;
	return true;
}



/* 解析 LIST 响应项。 */
XRT_API bool xrtPop3ListParse(xstrview Line, xpop3listview* pItem)
{
	xpop3listview Item;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		 xrtMemRangesOverlap(pItem, sizeof(*pItem), Line.Data, Line.Size) ||
		 !__xrtPop3Pair(Line, &Item.Message, &Item.Bytes) ||
		 (Item.Message == 0) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 LIST item"
		);
		return false;
	}
	*pItem = Item;
	return true;
}



/* 解析 UIDL 响应项。 */
XRT_API bool xrtPop3UidlParse(xstrview Line, xpop3uidlview* pItem)
{
	xpop3uidlview Item;
	size_t iPosition;
	size_t iStart;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		 xrtMemRangesOverlap(pItem, sizeof(*pItem), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iPosition = __xrtPop3Space(Line, 0);
	if ( !__xrtPop3Uint64(Line, &iPosition, &Item.Message) ||
		 (Item.Message == 0) || (iPosition == Line.Size) ||
		 ((Line.Data[iPosition] != ' ') && (Line.Data[iPosition] != '\t')) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 UIDL item"
		);
		return false;
	}
	iPosition = __xrtPop3Space(Line, iPosition);
	iStart = iPosition;
	while ( iPosition < Line.Size ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition];

		if ( (iByte <= 32u) || (iByte >= 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid POP3 unique ID"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == iStart ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty POP3 unique ID"
		);
		return false;
	}
	Item.Id = __xrtMailSlice(Line, iStart, iPosition - iStart);
	*pItem = Item;
	return true;
}



/* 解析 CAPA 行名称和参数。 */
XRT_API bool xrtPop3CapabilityParse(
	xstrview Line,
	xpop3capabilityview* pCapability
)
{
	xpop3capabilityview Capability;
	size_t iPosition = 0;
	size_t iParameters;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pCapability, sizeof(*pCapability)) ||
		 xrtMemRangesOverlap(pCapability, sizeof(*pCapability), Line.Data,
			Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( (iPosition < Line.Size) &&
		 (Line.Data[iPosition] != ' ') && (Line.Data[iPosition] != '\t') ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition];

		/* CAPA tags are extensible printable ASCII tokens, excluding '.'.
		 * Digits and punctuation in unknown tags must not reject the session. */
		if ( (iByte < 33u) || (iByte > 126u) || (iByte == '.') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid POP3 capability name"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty POP3 capability name"
		);
		return false;
	}
	iParameters = __xrtPop3Space(Line, iPosition);
	for ( size_t i = iParameters; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"POP3 capability contains a control character"
			);
			return false;
		}
	}
	Capability.Source = Line;
	Capability.Name = __xrtMailSlice(Line, 0, iPosition);
	Capability.Parameters = __xrtMailSlice(
		Line,
		iParameters,
		Line.Size - iParameters
	);
	*pCapability = Capability;
	return true;
}



/* 查找 POP3 能力名称的稳定标记。 */
XRT_API uint32 xrtPop3Capability(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint32 Value;
	} arrCapabilities[] = {
		{ "TOP", 3u, XPOP3_CAP_TOP },
		{ "USER", 4u, XPOP3_CAP_USER },
		{ "SASL", 4u, XPOP3_CAP_SASL },
		{ "RESP-CODES", 10u, XPOP3_CAP_RESP_CODES },
		{ "LOGIN-DELAY", 11u, XPOP3_CAP_LOGIN_DELAY },
		{ "PIPELINING", 10u, XPOP3_CAP_PIPELINING },
		{ "EXPIRE", 6u, XPOP3_CAP_EXPIRE },
		{ "UIDL", 4u, XPOP3_CAP_UIDL },
		{ "IMPLEMENTATION", 14u, XPOP3_CAP_IMPLEMENTATION },
		{ "STLS", 4u, XPOP3_CAP_STLS }
	};

	if ( !__xrtMailViewValid(Name) ) {
		return 0;
	}
	for ( size_t i = 0; i < sizeof(arrCapabilities) /
		sizeof(arrCapabilities[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Name,
			__xrtMailView(arrCapabilities[i].Name, arrCapabilities[i].Size)
		) ) {
			return arrCapabilities[i].Value;
		}
	}
	return 0;
}



/* 查找 POP3 SASL 机制名称的稳定标记。 */
XRT_API uint32 xrtPop3SaslMechanism(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint32 Value;
	} arrMechanisms[] = {
		{ "PLAIN", 5u, XPOP3_SASL_PLAIN },
		{ "XOAUTH2", 7u, XPOP3_SASL_XOAUTH2 },
		{ "OAUTHBEARER", 11u, XPOP3_SASL_OAUTHBEARER }
	};

	if ( !__xrtMailViewValid(Name) ) {
		return 0;
	}
	for ( size_t i = 0; i < sizeof(arrMechanisms) /
		sizeof(arrMechanisms[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Name,
			__xrtMailView(arrMechanisms[i].Name, arrMechanisms[i].Size)
		) ) {
			return arrMechanisms[i].Value;
		}
	}
	return 0;
}



/* 验证 POP3 命令名称和参数。 */
static bool __xrtPop3CommandValid(xstrview Verb, xstrview Arguments)
{
	if ( (Verb.Size == 0) || (Verb.Size > 16u) ) {
		return false;
	}
	for ( size_t i = 0; i < Verb.Size; i++ ) {
		unsigned char iByte = (unsigned char)Verb.Data[i];

		if ( !((iByte >= (unsigned char)'A') &&
			(iByte <= (unsigned char)'Z')) ) {
			return false;
		}
	}
	for ( size_t i = 0; i < Arguments.Size; i++ ) {
		unsigned char iByte = (unsigned char)Arguments.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 写出安全 POP3 命令。 */
XRT_API bool xrtPop3CommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;
	size_t iRequired;

	if ( !__xrtMailViewValid(Verb) || !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtPop3CommandValid(Verb, Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid POP3 command"
		);
		return false;
	}
	if ( !__xrtMailSizeAdd(Verb.Size, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > XPOP3_COMMAND_MAX ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"POP3 command exceeds 512 bytes"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Verb.Data,
			Verb.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Arguments.Data,
			Arguments.Size) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( sOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity <= iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Verb.Data, Verb.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Arguments.Data,
			Arguments.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memcpy(sOutput, Verb.Data, Verb.Size);
	if ( iSeparator != 0 ) {
		sOutput[Verb.Size] = ' ';
		memcpy(sOutput + Verb.Size + 1u, Arguments.Data, Arguments.Size);
	}
	sOutput[iRequired - 2u] = '\r';
	sOutput[iRequired - 1u] = '\n';
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 POP3 命令。 */
XRT_API str xrtPop3Command(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtPop3CommandWrite(
		Verb,
		Arguments,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtPop3CommandWrite(
		Verb,
		Arguments,
		sOutput,
		iRequired + 1u,
		&iRequired
	) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xpop3/src/pop3/pop3_client.c */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_CLIENT)




#if defined(XPOP3_FEATURE_POP3_CLIENT)

/* 设置稳定的 POP3 客户端错误。 */
static bool __xrtPop3ClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证客户端仍可交换协议数据。 */
static bool __xrtPop3ClientUsable(const xpop3client* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XPOP3_CLIENT_CLOSED) ||
		(pClient->State == XPOP3_CLIENT_FAILED) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 client is not usable"
		);
	}
	return true;
}



/* 把不可恢复的线路失败记录为终态。 */
bool __xrtPop3ClientFail(xpop3client* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XPOP3_CLIENT_FAILED;
		__xrtMailTransportAbortPreserveError(&pClient->Transport);
	}
	return false;
}



/* 保存并重新解析最后状态行，使公开视图指向稳定缓冲。 */
bool __xrtPop3ClientReplySave(
	xpop3client* pClient,
	xstrview Line,
	xpop3reply* pReply
)
{
	xpop3replyview Parsed;
	xstrview Stable;
	size_t iTextOffset;

	/* 畸形状态行不能覆盖上一条有效回复。 */
	if ( !xrtPop3ReplyParse(Line, &Parsed) ) {
		return false;
	}
	iTextOffset = (size_t)(Parsed.Text.Data - Line.Data);
	if ( !__xrtMailTextSet(&pClient->Reply, Line) ) {
		return false;
	}
	Stable.Data = pClient->Reply.Data;
	Stable.Size = pClient->Reply.Size;
	pReply->Ok = Parsed.Ok;
	pReply->Source = Stable;
	pReply->Text.Data = Stable.Data + iTextOffset;
	pReply->Text.Size = Parsed.Text.Size;
	return true;
}



/* 把 -ERR 转换为保留 LastReply 的命令拒绝错误。 */
static bool __xrtPop3ClientRejected(void)
{
	return __xrtPop3ClientError(XERR_PROTOCOL, "POP3 command was rejected");
}



/* 构建一到两个无符号十进制参数。 */
static xstrview __xrtPop3ClientNumbers(
	char* sOutput,
	uint64 iFirst,
	bool bSecond,
	uint64 iSecond
)
{
	size_t iSize = __xrtMailUint64Write(sOutput, iFirst);

	if ( bSecond ) {
		sOutput[iSize++] = ' ';
		iSize += __xrtMailUint64Write(sOutput + iSize, iSecond);
	}
	sOutput[iSize] = 0;
	return (xstrview) { sOutput, iSize };
}



/* 把 SASL 能力参数合并为已知机制位集。 */
static uint32 __xrtPop3ClientSaslMechanisms(xstrview Parameters)
{
	uint32 iMechanisms = 0;
	size_t iPosition = 0;

	while ( iPosition < Parameters.Size ) {
		size_t iStart;

		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] == ' ') ) {
			iPosition++;
		}
		iStart = iPosition;
		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] != ' ') ) {
			iPosition++;
		}
		if ( iPosition != iStart ) {
			iMechanisms |= xrtPop3SaslMechanism(__xrtMailSlice(
				Parameters,
				iStart,
				iPosition - iStart
			));
		}
	}
	return iMechanisms;
}



/* 读取 CAPA 多行响应并合并已知能力。 */
static bool __xrtPop3ClientCapa(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;
	xstrview Line;
	xmailnext Next;

	pClient->Capabilities = 0;
	pClient->SaslMechanisms = 0;
	if ( !xrtPop3ClientCommand(
		pClient,
		XRT_STR_LITERAL("CAPA"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return true;
	}
	pClient->ReturnState = pClient->State;
	pClient->State = XPOP3_CLIENT_MULTILINE;
	for ( ;; ) {
		xpop3capabilityview Capability;

		Next = xrtPop3ClientNext(
			pClient,
			&Line,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_END ) {
			return true;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!xrtPop3CapabilityParse(Line, &Capability) ) {
			return __xrtPop3ClientFail(pClient);
		}
		pClient->Capabilities |= xrtPop3Capability(Capability.Name);
		if ( __xrtMailAsciiEqualI(
			Capability.Name,
			XRT_STR_LITERAL("SASL")
		) ) {
			pClient->SaslMechanisms |= __xrtPop3ClientSaslMechanisms(
				Capability.Parameters
			);
		}
	}
}



/* 初始化 POP3 客户端配置。 */
XRT_API void xrtPop3ClientConfigInit(xpop3clientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 110u;
	pConfig->ReadCapabilities = true;
}



/* 验证 POP3 客户端配置。 */
XRT_API bool xrtPop3ClientConfigValid(const xpop3clientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailNetConfigValid(&pConfig->Net) ) {
		return false;
	}
	if ( (pConfig->Net.Security == XMAIL_SECURITY_STARTTLS) &&
		!pConfig->ReadCapabilities ) {
		return __xrtPop3ClientError(
			XERR_ARGUMENT,
			"POP3 STLS requires capability discovery"
		);
	}
	return true;
}



/* 建立并协商 POP3 会话。 */
XRT_API xpop3client* xrtPop3ClientOpen(
	const xpop3clientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3client* pClient;
	xpop3reply Reply;

	if ( !xrtPop3ClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (xpop3client*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XPOP3_CLIENT_AUTHORIZATION;
	if ( !__xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	) || !xrtPop3ClientReceive(
		pClient,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( !Reply.Ok ) {
		(void)__xrtPop3ClientRejected();
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->ReadCapabilities && !__xrtPop3ClientCapa(
		pClient,
		iDeadline,
		pCancel
	) ) {
		xrtPop3ClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		#if defined(XPOP3_FEATURE_POP3_CLIENT_TLS)
			if ( (pClient->Capabilities & XPOP3_CAP_STLS) == 0 ) {
				(void)__xrtPop3ClientError(
					XERR_UNSUPPORTED,
					"POP3 server does not support STLS"
				);
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
			memset(&Reply, 0, sizeof(Reply));
			if ( !xrtPop3ClientCommand(
				pClient,
				XRT_STR_LITERAL("STLS"),
				XRT_STR_LITERAL(""),
				&Reply,
				iDeadline,
				pCancel
			) ) {
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
			if ( !Reply.Ok ) {
				(void)__xrtPop3ClientRejected();
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
			if ( !__xrtMailTransportStartTls(
				&pClient->Transport,
				&pConfig->Net,
				iDeadline,
				pCancel
			) || !__xrtPop3ClientCapa(
				pClient,
				iDeadline,
				pCancel
			) ) {
				xrtPop3ClientDestroy(pClient);
				return NULL;
			}
		#else
			(void)__xrtPop3ClientError(
				XERR_UNSUPPORTED,
				"POP3 STLS support is not enabled"
			);
			xrtPop3ClientDestroy(pClient);
			return NULL;
		#endif
	}
	return pClient;
}



/* 返回 POP3 客户端状态。 */
XRT_API xpop3clientstate xrtPop3ClientState(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->State : XPOP3_CLIENT_FAILED;
}



/* 返回 POP3 能力快照。 */
XRT_API uint32 xrtPop3ClientCapabilities(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回 POP3 SASL 机制快照。 */
XRT_API uint32 xrtPop3ClientSaslMechanisms(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->SaslMechanisms : 0;
}



/* 返回 POP3 传输安全级别。 */
XRT_API xmailsecurity xrtPop3ClientSecurity(const xpop3client* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



/* 取得最后 POP3 状态行。 */
XRT_API bool xrtPop3ClientLastReply(
	const xpop3client* pClient,
	xpop3reply* pReply
)
{
	xpop3replyview Parsed;
	xstrview Stable;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		(pClient->Reply.Data == NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Stable.Data = pClient->Reply.Data;
	Stable.Size = pClient->Reply.Size;
	if ( !xrtPop3ReplyParse(Stable, &Parsed) ) {
		return false;
	}
	pReply->Ok = Parsed.Ok;
	pReply->Source = Parsed.Source;
	pReply->Text = Parsed.Text;
	return true;
}



/* 按调用场景的独立上限发送一条 POP3 线路。 */
static bool __xrtPop3ClientSendLine(
	xpop3client* pClient,
	xstrview Line,
	size_t iMaxLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !__xrtMailViewValid(Line) || (iMaxLine < 2u) ||
		(Line.Size > (iMaxLine - 2u)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			return __xrtPop3ClientError(
				XERR_ARGUMENT,
				"POP3 line contains a control separator"
			);
		}
	}
	if ( (Line.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Line.Data,
		Line.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 发送低层 POP3 命令行。 */
XRT_API bool xrtPop3ClientSend(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSendLine(
		pClient,
		Line,
		XPOP3_COMMAND_MAX,
		iDeadline,
		pCancel
	);
}



/* 发送较长的 POP3 SASL 响应行。 */
XRT_API bool xrtPop3ClientAuthLine(
	xpop3client* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State != XPOP3_CLIENT_AUTHORIZATION ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 SASL response requires AUTHORIZATION state"
		);
	}
	return __xrtPop3ClientSendLine(
		pClient,
		Line,
		XPOP3_AUTH_RESPONSE_MAX,
		iDeadline,
		pCancel
	);
}



/* 读取一条未解释的 POP3 线路。 */
XRT_API bool xrtPop3ClientLine(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !xrtMemRangeValid(pLine, sizeof(*pLine)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		pLine,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 读取 POP3 状态行。 */
XRT_API bool xrtPop3ClientReceive(
	xpop3client* pClient,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;

	if ( !xrtMemRangeValid(pReply, sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientLine(
		pClient,
		&Line,
		iDeadline,
		pCancel
	) || !__xrtPop3ClientReplySave(pClient, Line, pReply) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 发送 POP3 命令并读取状态。 */
XRT_API bool xrtPop3ClientCommand(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sCommand[XPOP3_COMMAND_MAX + 1u];
	size_t iSize;

	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_MULTILINE ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is active"
		);
	}
	if ( !xrtMemRangeValid(pReply, sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3CommandWrite(
			Verb,
			Arguments,
			sCommand,
			sizeof(sCommand),
			&iSize
		) ) {
		return false;
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		sCommand,
		iSize,
		iDeadline,
		pCancel
	) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return xrtPop3ClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 开始 POP3 多行响应。 */
XRT_API bool xrtPop3ClientBegin(
	xpop3client* pClient,
	xstrview Verb,
	xstrview Arguments,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !xrtPop3ClientCommand(
		pClient,
		Verb,
		Arguments,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	pClient->ReturnState = pClient->State;
	pClient->State = XPOP3_CLIENT_MULTILINE;
	return true;
}



/* 读取下一条 POP3 多行数据。 */
XRT_API xmailnext xrtPop3ClientNext(
	xpop3client* pClient,
	xstrview* pLine,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;
	xmailnext Next;

	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_MULTILINE) ||
		!xrtMemRangeValid(pLine, sizeof(*pLine)) ) {
		(void)__xrtPop3ClientError(
			XERR_STATE,
			"POP3 multiline response is not active"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		&Line,
		iDeadline,
		pCancel
	) ) {
		(void)__xrtPop3ClientFail(pClient);
		return XMAIL_NEXT_ERROR;
	}
	Next = xrtMailDotLine(Line, pLine);
	if ( Next == XMAIL_NEXT_END ) {
		pClient->State = pClient->ReturnState;
	} else if ( Next == XMAIL_NEXT_ERROR ) {
		(void)__xrtPop3ClientFail(pClient);
	}
	return Next;
}



/* 认证扩展成功后原子进入事务状态。 */
bool __xrtPop3ClientAuthorize(xpop3client* pClient)
{
	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_AUTHORIZATION) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 authorization state is not active"
		);
	}
	pClient->State = XPOP3_CLIENT_TRANSACTION;
	return true;
}



/* 验证命令只能在事务状态执行。 */
static bool __xrtPop3ClientTransaction(const xpop3client* pClient)
{
	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State != XPOP3_CLIENT_TRANSACTION) ) {
		return __xrtPop3ClientError(
			XERR_STATE,
			"POP3 command requires TRANSACTION state"
		);
	}
	return true;
}



/* 执行事务中的无参数单行命令。 */
static bool __xrtPop3ClientSimple(
	xpop3client* pClient,
	xstrview Verb,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	return __xrtPop3ClientTransaction(pClient) &&
		xrtPop3ClientCommand(
			pClient,
			Verb,
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) && (Reply.Ok ? true : __xrtPop3ClientRejected());
}



/* 查询 POP3 STAT。 */
XRT_API bool xrtPop3ClientStat(
	xpop3client* pClient,
	xpop3stat* pStat,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pStat, sizeof(*pStat)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("STAT"),
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	if ( !xrtPop3StatParse(Reply.Source, pStat) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 查询一条 POP3 LIST。 */
XRT_API bool xrtPop3ClientList(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3listview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( (iMessage == 0) || !xrtMemRangeValid(pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("LIST"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	if ( !xrtPop3ListParse(Reply.Text, pItem) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 开始读取全部 POP3 LIST。 */
XRT_API bool xrtPop3ClientListAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientTransaction(pClient) && xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("LIST"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	);
}



/* 查询一条 POP3 UIDL。 */
XRT_API bool xrtPop3ClientUidl(
	xpop3client* pClient,
	uint64 iMessage,
	xpop3uidlview* pItem,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( (iMessage == 0) || !xrtMemRangeValid(pItem, sizeof(*pItem)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("UIDL"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	if ( !xrtPop3UidlParse(Reply.Text, pItem) ) {
		return __xrtPop3ClientFail(pClient);
	}
	return true;
}



/* 开始读取全部 POP3 UIDL。 */
XRT_API bool xrtPop3ClientUidlAll(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientTransaction(pClient) && xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("UIDL"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	);
}



/* 开始读取完整邮件。 */
XRT_API bool xrtPop3ClientRetr(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("RETR"),
		__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
		iDeadline,
		pCancel
	);
}



/* 开始读取邮件字段和有限正文行。 */
XRT_API bool xrtPop3ClientTop(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumbers[42];

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtPop3ClientBegin(
		pClient,
		XRT_STR_LITERAL("TOP"),
		__xrtPop3ClientNumbers(sNumbers, iMessage, true, iLines),
		iDeadline,
		pCancel
	);
}



/* 标记一条 POP3 消息删除。 */
XRT_API bool xrtPop3ClientDelete(
	xpop3client* pClient,
	uint64 iMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sNumber[21];
	xpop3reply Reply;

	if ( !__xrtPop3ClientTransaction(pClient) ) {
		return false;
	}
	if ( iMessage == 0 ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("DELE"),
			__xrtPop3ClientNumbers(sNumber, iMessage, false, 0),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	return Reply.Ok ? true : __xrtPop3ClientRejected();
}



/* 清除 POP3 删除标记。 */
XRT_API bool xrtPop3ClientReset(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSimple(
		pClient,
		XRT_STR_LITERAL("RSET"),
		iDeadline,
		pCancel
	);
}



/* 发送 POP3 NOOP。 */
XRT_API bool xrtPop3ClientNoop(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3ClientSimple(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		iDeadline,
		pCancel
	);
}



/* 发送 POP3 QUIT 并关闭传输。 */
XRT_API bool xrtPop3ClientQuit(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3ClientUsable(pClient) ||
		(pClient->State == XPOP3_CLIENT_MULTILINE) ||
		!xrtPop3ClientCommand(
			pClient,
			XRT_STR_LITERAL("QUIT"),
			XRT_STR_LITERAL(""),
			&Reply,
			iDeadline,
			pCancel
		) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3ClientRejected();
	}
	pClient->State = XPOP3_CLIENT_UPDATE;
	return xrtPop3ClientClose(pClient, iDeadline);
}



/* 正常关闭 POP3 传输。 */
XRT_API bool xrtPop3ClientClose(
	xpop3client* pClient,
	xdeadline iDeadline
)
{
	if ( !__xrtPop3ClientUsable(pClient) ) {
		return false;
	}
	if ( !__xrtMailTransportClose(&pClient->Transport, iDeadline) ) {
		pClient->State = XPOP3_CLIENT_FAILED;
		return false;
	}
	pClient->State = XPOP3_CLIENT_CLOSED;
	return true;
}



/* 无等待异常中止 POP3 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtPop3ClientAbort(xpop3client* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XPOP3_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XPOP3_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XPOP3_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XPOP3_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 POP3 客户端。 */
XRT_API void xrtPop3ClientDestroy(xpop3client* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	__xrtMailTransportDestroy(&pClient->Transport);
	__xrtMailTextDestroy(&pClient->Reply);
	xrtFree(pClient);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xpop3/src/pop3/pop3_auth.c */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_AUTH)




#if defined(XPOP3_FEATURE_POP3_AUTH)

typedef enum __xpop3authnext {
	__XPOP3_AUTH_ERROR = 0,
	__XPOP3_AUTH_CONTINUE,
	__XPOP3_AUTH_OK,
	__XPOP3_AUTH_REJECTED
} __xpop3authnext;



/* 设置稳定的 POP3 认证错误。 */
static bool __xrtPop3AuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 判断线路是否以一个完整、不区分大小写的 POP3 状态词开头。 */
static bool __xrtPop3AuthStatus(xstrview Line, xstrview Status)
{
	return (Line.Size >= Status.Size) && __xrtMailAsciiEqualI(
		__xrtMailSlice(Line, 0, Status.Size),
		Status
	) && ((Line.Size == Status.Size) || (Line.Data[Status.Size] == ' '));
}



/* 读取 SASL continuation 或最终 POP3 状态，并稳定保存最终响应。 */
static __xpop3authnext __xrtPop3AuthNext(
	xpop3client* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Line;
	xpop3reply Reply;

	if ( !xrtPop3ClientLine(pClient, &Line, iDeadline, pCancel) ) {
		return __XPOP3_AUTH_ERROR;
	}
	if ( __xrtPop3AuthStatus(Line, XRT_STR_LITERAL("+OK")) ||
		__xrtPop3AuthStatus(Line, XRT_STR_LITERAL("-ERR")) ) {
		if ( !__xrtPop3ClientReplySave(pClient, Line, &Reply) ) {
			if ( xrtErrorKind(xrtGetError()) != XERR_MEMORY )
				(void)__xrtPop3AuthError(
					XERR_PROTOCOL,
					"invalid POP3 AUTH final response"
				);
			(void)__xrtPop3ClientFail(pClient);
			return __XPOP3_AUTH_ERROR;
		}
		return Reply.Ok ? __XPOP3_AUTH_OK : __XPOP3_AUTH_REJECTED;
	}
	if ( (Line.Size != 0) && (Line.Data[0] == '+') &&
		((Line.Size == 1u) || (Line.Data[1] == ' ')) ) {
		return __XPOP3_AUTH_CONTINUE;
	}
	(void)__xrtPop3AuthError(
		XERR_PROTOCOL,
		"invalid POP3 AUTH continuation"
	);
	(void)__xrtPop3ClientFail(pClient);
	return __XPOP3_AUTH_ERROR;
}



/* 发送并清零一条包含传统 POP3 凭据的命令。 */
static bool __xrtPop3AuthCommand(
	xpop3client* pClient,
	xstrview Prefix,
	xstrview Credential,
	xpop3reply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sLine;
	size_t iSize;
	bool bSuccess;

	if ( !__xrtMailSizeAdd(Prefix.Size, Credential.Size, &iSize) ||
		(iSize > (XPOP3_COMMAND_MAX - 2u)) ) {
		return __xrtPop3AuthError(
			XERR_RANGE,
			"POP3 credential exceeds the command limit"
		);
	}
	sLine = (char*)xrtMalloc(iSize + 1u);
	if ( sLine == NULL ) {
		return false;
	}
	memcpy(sLine, Prefix.Data, Prefix.Size);
	memcpy(sLine + Prefix.Size, Credential.Data, Credential.Size);
	sLine[iSize] = 0;
	bSuccess = xrtPop3ClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sLine, iSize + 1u);
	return bSuccess && xrtPop3ClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 完成 USER/PASS 认证。 */
static bool __xrtPop3AuthUserPass(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3reply Reply;

	if ( !__xrtPop3AuthCommand(
		pClient,
		XRT_STR_LITERAL("USER "),
		pConfig->Username,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 username was rejected"
		);
	}
	if ( !__xrtPop3AuthCommand(
		pClient,
		XRT_STR_LITERAL("PASS "),
		pConfig->Secret,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !Reply.Ok ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 password was rejected"
		);
	}
	return __xrtPop3ClientAuthorize(pClient);
}



/* 返回配置对应的 SASL 机制名称和能力位。 */
static xstrview __xrtPop3AuthMechanism(
	xpop3authmethod Method,
	uint32* pCapability
)
{
	if ( Method == XPOP3_AUTH_PLAIN ) {
		*pCapability = XPOP3_SASL_PLAIN;
		return XRT_STR_LITERAL("PLAIN");
	}
	if ( Method == XPOP3_AUTH_XOAUTH2 ) {
		*pCapability = XPOP3_SASL_XOAUTH2;
		return XRT_STR_LITERAL("XOAUTH2");
	}
	*pCapability = XPOP3_SASL_OAUTHBEARER;
	return XRT_STR_LITERAL("OAUTHBEARER");
}



/* 创建配置机制的一次性 Base64 初始响应。 */
static char* __xrtPop3AuthResponse(
	const xpop3authconfig* pConfig,
	size_t* pEncodedSize
)
{
	if ( pConfig->Method == XPOP3_AUTH_PLAIN ) {
		return __xrtMailAuthPlain(
			pConfig->AuthorizationId,
			pConfig->Username,
			pConfig->Secret,
			pEncodedSize
		);
	}
	if ( pConfig->Method == XPOP3_AUTH_XOAUTH2 ) {
		return __xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			pEncodedSize
		);
	}
	return __xrtMailAuthOauthBearer(
		pConfig->AuthorizationId.Size != 0 ?
			pConfig->AuthorizationId : pConfig->Username,
		pConfig->Secret,
		pEncodedSize
	);
}



/* 发送 AUTH 命令，并在 255 字节预算允许时携带初始响应。 */
static bool __xrtPop3AuthStart(
	xpop3client* pClient,
	xstrview Mechanism,
	xstrview Encoded,
	bool bInitial,
	bool* pInitialUsed,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sLine[XPOP3_AUTH_COMMAND_MAX];
	size_t iSize = 5u + Mechanism.Size;
	bool bUseInitial = bInitial &&
		(Encoded.Size <= ((XPOP3_AUTH_COMMAND_MAX - 2u) - iSize - 1u));
	bool bSuccess;

	memcpy(sLine, "AUTH ", 5u);
	memcpy(sLine + 5u, Mechanism.Data, Mechanism.Size);
	if ( bUseInitial ) {
		sLine[iSize++] = ' ';
		memcpy(sLine + iSize, Encoded.Data, Encoded.Size);
		iSize += Encoded.Size;
	}
	bSuccess = xrtPop3ClientAuthLine(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	xrtSecureZero(sLine, sizeof(sLine));
	*pInitialUsed = bUseInitial;
	return bSuccess;
}



/* 完成 PLAIN 或 bearer 的单响应 SASL 交换。 */
static bool __xrtPop3AuthSasl(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview Mechanism;
	char* sEncoded;
	size_t iEncoded;
	uint32 iCapability;
	__xpop3authnext Next;
	bool bInitialUsed;
	bool bBearer;
	bool bSuccess;

	Mechanism = __xrtPop3AuthMechanism(pConfig->Method, &iCapability);
	if ( (xrtPop3ClientCapabilities(pClient) & XPOP3_CAP_SASL) == 0 ) {
		return __xrtPop3AuthError(
			XERR_UNSUPPORTED,
			"POP3 server did not advertise SASL"
		);
	}
	if ( (xrtPop3ClientSaslMechanisms(pClient) & iCapability) == 0 ) {
		return __xrtPop3AuthError(
			XERR_UNSUPPORTED,
			"POP3 server did not advertise the authentication mechanism"
		);
	}
	sEncoded = __xrtPop3AuthResponse(pConfig, &iEncoded);
	if ( sEncoded == NULL ) {
		return false;
	}
	if ( iEncoded > (XPOP3_AUTH_RESPONSE_MAX - 2u) ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return __xrtPop3AuthError(
			XERR_RANGE,
			"POP3 authentication response exceeds the SASL limit"
		);
	}
	bSuccess = __xrtPop3AuthStart(
		pClient,
		Mechanism,
		(xstrview) { sEncoded, iEncoded },
		pConfig->InitialResponse,
		&bInitialUsed,
		iDeadline,
		pCancel
	);
	Next = bSuccess ? __xrtPop3AuthNext(
		pClient,
		iDeadline,
		pCancel
	) : __XPOP3_AUTH_ERROR;
	if ( (Next == __XPOP3_AUTH_CONTINUE) && !bInitialUsed ) {
		bSuccess = xrtPop3ClientAuthLine(
			pClient,
			(xstrview) { sEncoded, iEncoded },
			iDeadline,
			pCancel
		);
		Next = bSuccess ? __xrtPop3AuthNext(
			pClient,
			iDeadline,
			pCancel
		) : __XPOP3_AUTH_ERROR;
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	if ( Next == __XPOP3_AUTH_ERROR ) {
		return false;
	}
	if ( Next == __XPOP3_AUTH_OK ) {
		return __xrtPop3ClientAuthorize(pClient);
	}
	if ( Next == __XPOP3_AUTH_REJECTED ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 authentication was rejected"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	bSuccess = xrtPop3ClientAuthLine(
		pClient,
		bBearer ? XRT_STR_LITERAL("AQ==") : XRT_STR_LITERAL("*"),
		iDeadline,
		pCancel
	);
	Next = bSuccess ? __xrtPop3AuthNext(
		pClient,
		iDeadline,
		pCancel
	) : __XPOP3_AUTH_ERROR;
	if ( Next == __XPOP3_AUTH_ERROR ) {
		return false;
	}
	if ( (Next == __XPOP3_AUTH_OK) ||
		(Next == __XPOP3_AUTH_CONTINUE) ) {
		(void)__xrtPop3ClientFail(pClient);
		return __xrtPop3AuthError(
			XERR_PROTOCOL,
			"invalid POP3 AUTH completion after cancellation"
		);
	}
	return __xrtPop3AuthError(
		XERR_PERMISSION,
		bBearer ? "POP3 bearer authentication was rejected" :
			"POP3 authentication sent an unexpected extra challenge"
	);
}



/* 初始化 POP3 认证配置。 */
XRT_API void xrtPop3AuthConfigInit(xpop3authconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XPOP3_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 POP3 认证配置。 */
XRT_API bool xrtPop3AuthConfigValid(const xpop3authconfig* pConfig)
{
	bool bBearer;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XPOP3_AUTH_USER_PASS) ||
		(pConfig->Method > XPOP3_AUTH_OAUTHBEARER) ) {
		return __xrtPop3AuthError(
			XERR_ARGUMENT,
			"invalid POP3 authentication method"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XPOP3_AUTH_PLAIN) &&
		 (pConfig->Method != XPOP3_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtPop3AuthError(
			XERR_ARGUMENT,
			"invalid POP3 authentication credentials"
		);
	}
	return true;
}



/* 完成一次 POP3 认证。 */
XRT_API bool xrtPop3ClientAuth(
	xpop3client* pClient,
	const xpop3authconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	bool bBearer;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtPop3AuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtPop3ClientState(pClient) != XPOP3_CLIENT_AUTHORIZATION ) {
		return __xrtPop3AuthError(
			XERR_STATE,
			"POP3 authentication requires AUTHORIZATION state"
		);
	}
	bBearer = (pConfig->Method == XPOP3_AUTH_XOAUTH2) ||
		(pConfig->Method == XPOP3_AUTH_OAUTHBEARER);
	if ( bBearer &&
		(xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 bearer authentication requires TLS"
		);
	}
	if ( (xrtPop3ClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtPop3AuthError(
			XERR_PERMISSION,
			"POP3 credentials require TLS or explicit plaintext opt-in"
		);
	}
	return pConfig->Method == XPOP3_AUTH_USER_PASS ?
		__xrtPop3AuthUserPass(
			pClient,
			pConfig,
			iDeadline,
			pCancel
		) : __xrtPop3AuthSasl(
			pClient,
			pConfig,
			iDeadline,
			pCancel
		);
}



/* 使用传统 USER/PASS 的轻量便利入口。 */
XRT_API bool xrtPop3ClientLogin(
	xpop3client* pClient,
	xstrview Username,
	xstrview Password,
	bool AllowPlaintext,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xpop3authconfig Config;

	xrtPop3AuthConfigInit(&Config);
	Config.Method = XPOP3_AUTH_USER_PASS;
	Config.Username = Username;
	Config.Secret = Password;
	Config.AllowPlaintext = AllowPlaintext;
	return xrtPop3ClientAuth(
		pClient,
		&Config,
		iDeadline,
		pCancel
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xpop3/src/pop3/pop3_message.c */
/* ========================================================================== */

#if defined(XPOP3_FEATURE_POP3_MESSAGE)



#if defined(XPOP3_FEATURE_POP3_MESSAGE)

static const unsigned char __xrtPop3MessageCrlf[] = { '\r', '\n' };



/* 收集 sink 借用本次调用栈上的连续缓冲。 */
typedef struct __xpop3messagesink {
	xbuffer Buffer;
} __xpop3messagesink;



/* 保留主错误并关闭无法安全继续解析的多行会话。 */
static bool __xrtPop3MessageRecover(xpop3client* pClient)
{
	xerror* pPrimaryError = xrtTakeError();
	xerror* pCloseError;
	xpop3clientstate State = xrtPop3ClientState(pClient);

	if ( (State == XPOP3_CLIENT_MULTILINE) ||
		 (State == XPOP3_CLIENT_FAILED) ) {
		(void)xrtPop3ClientAbort(pClient);
	}
	pCloseError = xrtTakeError();
	if ( pPrimaryError != NULL ) {
		xrtErrorFree(pCloseError);
		xrtSetErrorTake(pPrimaryError);
	} else {
		xrtSetErrorTake(pCloseError);
	}
	return false;
}



/* 调用输出 sink，并在无具体错误时补充稳定的 callback 错误。 */
static bool __xrtPop3MessageOutput(
	xmailwriteproc pWrite,
	ptr pUserData,
	xbytesview Data
)
{
	if ( pWrite(Data, pUserData) ) {
		return true;
	}
	if ( xrtGetError() == NULL ) {
		__xrtMailError(
			XERR_IO,
			XMAIL_ERROR_CALLBACK,
			"POP3 message output callback failed"
		);
	}
	return false;
}



/* 共享 RETR/TOP 的有界多行读取与 CRLF 恢复。 */
static bool __xrtPop3MessageWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	bool bTop,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	size_t iWritten = 0;
	xstrview Line;
	xmailnext Next;

	if ( (pWrite == NULL) || !xrtMemRangeValid(
		pWritten,
		pWritten != NULL ? sizeof(*pWritten) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxBytes == 0 ) {
		iMaxBytes = XPOP3_MESSAGE_BYTES_DEFAULT;
	}
	if ( bTop ) {
		if ( !xrtPop3ClientTop(
			pClient,
			iMessage,
			iLines,
			iDeadline,
			pCancel
		) ) {
			return false;
		}
	} else if ( !xrtPop3ClientRetr(
		pClient,
		iMessage,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( ;; ) {
		size_t iNext;

		Next = xrtPop3ClientNext(
			pClient,
			&Line,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_END ) {
			if ( pWritten != NULL ) {
				*pWritten = iWritten;
			}
			return true;
		}
		if ( Next != XMAIL_NEXT_ITEM ) {
			return __xrtPop3MessageRecover(pClient);
		}
		if ( !__xrtMailSizeAdd(Line.Size, 2u, &iNext) ||
			 !__xrtMailSizeAdd(iWritten, iNext, &iNext) ||
			 ((iMaxBytes != SIZE_MAX) && (iNext > iMaxBytes)) ) {
			if ( xrtGetError() == NULL ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"POP3 message exceeds the byte limit"
				);
			}
			return __xrtPop3MessageRecover(pClient);
		}
		if ( ((Line.Size != 0) && !__xrtPop3MessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ (cbytes)Line.Data, Line.Size }
		)) || !__xrtPop3MessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ __xrtPop3MessageCrlf, 2u }
		) ) {
			return __xrtPop3MessageRecover(pClient);
		}
		iWritten = iNext;
	}
}



/* 向连续缓冲追加一个已受外层预算约束的片段。 */
static bool __xrtPop3MessageBufferWrite(xbytesview Data, ptr pUserData)
{
	__xpop3messagesink* pSink = (__xpop3messagesink*)pUserData;

	return xrtBufferAppend(&pSink->Buffer, Data);
}



/* 收集 RETR/TOP 并返回末尾附零的 owned 字节。 */
static bytes __xrtPop3MessageBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	bool bTop,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xpop3messagesink Sink;
	bytes pData;
	size_t iSize;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtBufferInit(&Sink.Buffer) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( !__xrtPop3MessageWrite(
		pClient,
		iMessage,
		iLines,
		bTop,
		iMaxBytes,
		__xrtPop3MessageBufferWrite,
		&Sink,
		&iSize,
		iDeadline,
		pCancel
	) || !xrtBufferAppendByte(&Sink.Buffer, 0) ) {
		xrtBufferUnit(&Sink.Buffer);
		return NULL;
	}
	pData = xrtBufferTake(&Sink.Buffer, NULL, NULL);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return pData;
}



/* 流式读取完整邮件。 */
XRT_API bool xrtPop3ClientRetrWrite(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageWrite(
		pClient,
		iMessage,
		0,
		false,
		iMaxBytes,
		pWrite,
		pUserData,
		pWritten,
		iDeadline,
		pCancel
	);
}



/* 流式读取 TOP 结果。 */
XRT_API bool xrtPop3ClientTopWrite(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageWrite(
		pClient,
		iMessage,
		iLines,
		true,
		iMaxBytes,
		pWrite,
		pUserData,
		pWritten,
		iDeadline,
		pCancel
	);
}



/* 收集完整 RETR 结果。 */
XRT_API bytes xrtPop3ClientRetrBytes(
	xpop3client* pClient,
	uint64 iMessage,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageBytes(
		pClient,
		iMessage,
		0,
		false,
		iMaxBytes,
		pOutputSize,
		iDeadline,
		pCancel
	);
}



/* 收集完整 TOP 结果。 */
XRT_API bytes xrtPop3ClientTopBytes(
	xpop3client* pClient,
	uint64 iMessage,
	uint64 iLines,
	size_t iMaxBytes,
	size_t* pOutputSize,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtPop3MessageBytes(
		pClient,
		iMessage,
		iLines,
		true,
		iMaxBytes,
		pOutputSize,
		iDeadline,
		pCancel
	);
}



/* 收集并解析一棵拥有型 MIME 树。 */
XRT_API bool xrtPop3ClientRetrTree(
	xpop3client* pClient,
	uint64 iMessage,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xmailtreelimits Limits;
	bytes pData;
	size_t iSize;
	bool bResult;

	if ( !xrtMemRangeValid(pTree, sizeof(*pTree)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pLimits != NULL ) {
		if ( !xrtMailTreeLimitsValid(pLimits) ) {
			return false;
		}
		Limits = *pLimits;
	} else {
		xrtMailTreeLimitsInit(&Limits);
	}
	pData = xrtPop3ClientRetrBytes(
		pClient,
		iMessage,
		Limits.MaxSourceBytes,
		&iSize,
		iDeadline,
		pCancel
	);
	if ( pData == NULL ) {
		return false;
	}
	bResult = xrtMailTreeParse(
		(xstrview){ (cstr)pData, iSize },
		&Limits,
		pTree
	);
	xrtFree(pData);
	return bResult;
}

#endif
#endif

#endif
