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
#error "xsmtp requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XSMTP_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XSMTP_IMPLEMENTATION) && \
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
#ifndef XSMTP_SINGLE_HEADER_H
#define XSMTP_SINGLE_HEADER_H
#define XSMTP_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xsmtp/include/xsmtp/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XSMTP_FEATURES_H
#define XSMTP_FEATURES_H

/* smtp_auth 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_AUTH)
#ifndef XSMTP_FEATURE_SMTP_AUTH
#define XSMTP_FEATURE_SMTP_AUTH
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* smtp_submit 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_SUBMIT)
#ifndef XSMTP_FEATURE_SMTP_SUBMIT
#define XSMTP_FEATURE_SMTP_SUBMIT
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_COMPOSE
#define XMAIL_MODULE_MAIL_COMPOSE
#endif
#endif

/* smtp_client_tls 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_CLIENT_TLS)
#ifndef XSMTP_FEATURE_SMTP_CLIENT_TLS
#define XSMTP_FEATURE_SMTP_CLIENT_TLS
#endif
#ifndef XSMTP_MODULE_SMTP_CLIENT
#define XSMTP_MODULE_SMTP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* smtp_client 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP_CLIENT)
#ifndef XSMTP_FEATURE_SMTP_CLIENT
#define XSMTP_FEATURE_SMTP_CLIENT
#endif
#ifndef XSMTP_MODULE_SMTP
#define XSMTP_MODULE_SMTP
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* smtp 及其直接依赖。 */
#if defined(XSMTP_MODULE_ALL) || defined(XSMTP_MODULE_SMTP)
#ifndef XSMTP_FEATURE_SMTP
#define XSMTP_FEATURE_SMTP
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XSMTP_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xsmtp/include/xrt/smtp.h */
/* ========================================================================== */

#ifndef XRT_SMTP_H
#define XRT_SMTP_H




#if defined(XSMTP_FEATURE_SMTP) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XSMTP_FEATURE_SMTP requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XSMTP_FEATURE_SMTP)

#define XSMTP_REPLY_LINES_DEFAULT 100u
#define XSMTP_COMMAND_MAX 512u
#define XSMTP_AUTH_RESPONSE_MAX 12288u

#define XSMTP_CAP_PIPELINING UINT64_C(0x00000001)
#define XSMTP_CAP_SIZE UINT64_C(0x00000002)
#define XSMTP_CAP_STARTTLS UINT64_C(0x00000004)
#define XSMTP_CAP_AUTH_PLAIN UINT64_C(0x00000008)
#define XSMTP_CAP_AUTH_LOGIN UINT64_C(0x00000010)
#define XSMTP_CAP_AUTH_XOAUTH2 UINT64_C(0x00000020)
#define XSMTP_CAP_8BITMIME UINT64_C(0x00000040)
#define XSMTP_CAP_SMTPUTF8 UINT64_C(0x00000080)
#define XSMTP_CAP_DSN UINT64_C(0x00000100)
#define XSMTP_CAP_CHUNKING UINT64_C(0x00000200)
#define XSMTP_CAP_AUTH_OAUTHBEARER UINT64_C(0x00000400)
#define XSMTP_CAP_BINARYMIME UINT64_C(0x00000800)
#define XSMTP_CAP_ENHANCED_STATUS UINT64_C(0x00001000)



/* 单条 SMTP 响应行借用输入；Continued 为真表示后续仍有同码行。 */
typedef struct xsmtpreplyline {
	xstrview Source;
	xstrview Text;
	int Code;
	bool Continued;
} xsmtpreplyline;



/* 响应状态机验证多行响应代码、数量和唯一终止行。 */
typedef struct xsmtpreplyparser {
	int Code;
	size_t Lines;
	size_t MaxLines;
	bool Started;
	bool Done;
} xsmtpreplyparser;



/* EHLO 能力名称与参数均借用响应文本。 */
typedef struct xsmtpcapabilityview {
	xstrview Source;
	xstrview Name;
	xstrview Parameters;
} xsmtpcapabilityview;



XRT_EXTERN_C_BEGIN



/* 验证不含尖括号的 SMTP reverse-path 或 forward-path。 */
XRT_API bool xrtSmtpPathValid(xstrview Path, bool AllowEmpty);



/* 解析一条 `ddd-Text` 或 `ddd Text` SMTP 响应行。 */
XRT_API bool xrtSmtpReplyLineParse(
	xstrview Line,
	xsmtpreplyline* pReply
);



/* 初始化多行 SMTP 响应验证器；零限制使用默认值。 */
XRT_API bool xrtSmtpReplyParserInit(
	xsmtpreplyparser* pParser,
	size_t iMaxLines
);



/* 接受并发布一条响应行；解析器完成后不再接受输入。 */
XRT_API bool xrtSmtpReplyRead(
	xsmtpreplyparser* pParser,
	xstrview Line,
	xsmtpreplyline* pReply
);



/* 把 EHLO 文本拆成能力名称和可选参数。 */
XRT_API bool xrtSmtpCapabilityParse(
	xstrview Text,
	xsmtpcapabilityview* pCapability
);



/* 返回已知能力名称对应的位；未知扩展返回零。 */
XRT_API uint64 xrtSmtpCapability(xstrview Name);



/* 把一条能力及其参数合并到位集和 SIZE 上限。 */
XRT_API bool xrtSmtpCapabilityAdd(
	const xsmtpcapabilityview* pCapability,
	uint64* pCapabilities,
	uint64* pSizeLimit
);



/* 安全写出 `Verb [Arguments]\r\n`，拒绝控制字符和命令注入。 */
XRT_API bool xrtSmtpCommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 SMTP 命令行。 */
XRT_API str xrtSmtpCommand(
	xstrview Verb,
	xstrview Arguments,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xsmtp/include/xrt/smtp_client.h */
/* ========================================================================== */

#ifndef XRT_SMTP_CLIENT_H
#define XRT_SMTP_CLIENT_H




#if defined(XSMTP_FEATURE_SMTP_CLIENT) && \
	(!defined(XSMTP_FEATURE_SMTP) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XSMTP_FEATURE_SMTP_CLIENT requires SMTP and mail net"
#endif

#if defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) && \
	(!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XSMTP_FEATURE_SMTP_CLIENT_TLS requires SMTP client and mail net TLS"
#endif



#if defined(XSMTP_FEATURE_SMTP_CLIENT)

#define XSMTP_HELLO_MAX 255u



typedef struct xsmtpclient xsmtpclient;



/* SMTP 会话状态明确区分事务阶段和不可恢复的传输失败。 */
typedef enum xsmtpclientstate {
	XSMTP_CLIENT_READY = 0,
	XSMTP_CLIENT_MAIL,
	XSMTP_CLIENT_RECIPIENT,
	XSMTP_CLIENT_DATA,
	XSMTP_CLIENT_CHUNK,
	XSMTP_CLIENT_CLOSED,
	XSMTP_CLIENT_FAILED
} xsmtpclientstate;



/* 客户端借用网络配置和 EHLO 名称，只在 Open 调用期间读取。 */
typedef struct xsmtpclientconfig {
	xmailnetconfig Net;
	xstrview Hello;
	size_t ReplyLines;
	bool HeloFallback;
} xsmtpclientconfig;



/* 最终响应行摘要借用 Client，下一次 Receive 或销毁 Client 后失效。 */
typedef struct xsmtpreply {
	int Code;
	size_t Lines;
	xstrview Text;
} xsmtpreply;



XRT_EXTERN_C_BEGIN



/* 初始化明文 25 端口、localhost EHLO 和有界响应配置。 */
XRT_API void xrtSmtpClientConfigInit(xsmtpclientconfig* pConfig);



/* 验证网络所有者、EHLO 名称和响应行上限。 */
XRT_API bool xrtSmtpClientConfigValid(const xsmtpclientconfig* pConfig);



/* 建立连接，验证 220 banner 并完成 EHLO/可选 HELO 和 STARTTLS。 */
XRT_API xsmtpclient* xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 返回当前事务或终态。 */
XRT_API xsmtpclientstate xrtSmtpClientState(const xsmtpclient* pClient);



/* 返回最近一次 EHLO 得到的已知能力位集。 */
XRT_API uint64 xrtSmtpClientCapabilities(const xsmtpclient* pClient);



/* 返回服务器声明的 SIZE；零表示未声明或未给出上限。 */
XRT_API uint64 xrtSmtpClientSizeLimit(const xsmtpclient* pClient);



/* 返回当前会话实际使用的明文或 TLS 传输安全级别。 */
XRT_API xmailsecurity xrtSmtpClientSecurity(const xsmtpclient* pClient);



/* 返回当前会话是否已成功完成 SMTP AUTH。 */
XRT_API bool xrtSmtpClientAuthenticated(const xsmtpclient* pClient);



/* 取得最近一条完整响应的稳定借用摘要。 */
XRT_API bool xrtSmtpClientLastReply(
	const xsmtpclient* pClient,
	xsmtpreply* pReply
);



/* 发送一条不含 CRLF 的低层 SMTP 行。 */
XRT_API bool xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送预编码的 SASL 响应行；允许认证协议要求的扩展行长。 */
XRT_API bool xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 读取并验证一条完整单行或多行响应。 */
XRT_API bool xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 构建并发送命令，然后读取完整响应。 */
XRT_API bool xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 开始新 envelope；空 ReversePath 表示标准空反向路径。 */
XRT_API bool xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 增加一个收件人；接受 250、251 和 252 响应。 */
XRT_API bool xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 进入 DATA 模式并验证 354 响应。 */
XRT_API bool xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送任意分块边界的消息片段，并跨片段执行 dot transparency。 */
XRT_API bool xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 补足 DATA 终止行、读取最终响应并回到 READY。 */
XRT_API bool xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 直接流式发送完整消息视图，执行 dot transparency 而不复制整份报文。 */
XRT_API bool xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 声明一个 BDAT 块并直接发送命令头；随后必须精确写入 iChunkSize 个字节。 */
XRT_API bool xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 写入当前 BDAT 块的任意片段；不扫描、不转义且不追加线路分隔符。 */
XRT_API bool xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证块长度、读取 250 响应，并进入下一块或结束当前 envelope。 */
XRT_API bool xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 零副本发送一个连续 BDAT 块；服务器拒绝后必须 RSET 才能继续。 */
XRT_API bool xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 取消当前 envelope 并回到 READY。 */
XRT_API bool xrtSmtpClientReset(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 验证连接仍可交换命令。 */
XRT_API bool xrtSmtpClientNoop(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 发送 QUIT、验证 221 并认证关闭传输。 */
XRT_API bool xrtSmtpClientQuit(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 不发送 QUIT，直接正常关闭传输。 */
XRT_API bool xrtSmtpClientClose(
	xsmtpclient* pClient,
	xdeadline iDeadline
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtSmtpClientAbort(xsmtpclient* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtSmtpClientDestroy(xsmtpclient* pClient);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xsmtp/include/xrt/smtp_submit.h */
/* ========================================================================== */

#ifndef XRT_SMTP_SUBMIT_H
#define XRT_SMTP_SUBMIT_H




#if defined(XSMTP_FEATURE_SMTP_SUBMIT) && \
	(!defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	 !defined(XSMTP_FEATURE_SMTP_CLIENT))
	#error "XSMTP_FEATURE_SMTP_SUBMIT requires mail compose and SMTP client"
#endif



#if defined(XSMTP_FEATURE_SMTP_SUBMIT)

/* 高级 envelope 允许每个收件人携带独立 ESMTP 参数。 */
typedef struct xsmtprecipient {
	xstrview Address;
	xstrview Parameters;
} xsmtprecipient;



/* Envelope 与消息字段彼此独立，全部只在提交调用期间借用。 */
typedef struct xsmtpenvelope {
	xstrview ReversePath;
	xstrview MailParameters;
	const xsmtprecipient* Recipients;
	size_t RecipientCount;
} xsmtpenvelope;



XRT_EXTERN_C_BEGIN



/* 使用独立 envelope 流式构建并提交消息，不创建整封临时报文。 */
XRT_API bool xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



/* 从消息 From、To、Cc、Bcc 自动建立 envelope 并流式提交。 */
XRT_API bool xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xsmtp/include/xrt/smtp_auth.h */
/* ========================================================================== */

#ifndef XRT_SMTP_AUTH_H
#define XRT_SMTP_AUTH_H




#if defined(XSMTP_FEATURE_SMTP_AUTH) && \
	(!defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XSMTP_FEATURE_SMTP_AUTH requires SMTP client and Base64"
#endif



#if defined(XSMTP_FEATURE_SMTP_AUTH)

/* 内置认证覆盖 SMTP 常用的口令和 OAuth2 bearer 机制。 */
typedef enum xsmtpauthmethod {
	XSMTP_AUTH_PLAIN = 0,
	XSMTP_AUTH_LOGIN,
	XSMTP_AUTH_XOAUTH2,
	XSMTP_AUTH_OAUTHBEARER
} xsmtpauthmethod;



/* 凭据视图只在 Auth 调用期间借用，调用结束后不会保存在 Client。 */
typedef struct xsmtpauthconfig {
	xsmtpauthmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} xsmtpauthconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、初始响应开启、明文凭据发送关闭的安全默认值。 */
XRT_API void xrtSmtpAuthConfigInit(xsmtpauthconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtSmtpAuthConfigValid(const xsmtpauthconfig* pConfig);



/* 在 READY 会话上完成一次认证；服务器拒绝后仍允许调用方选择其他机制。 */
XRT_API bool xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xsmtp/include/xsmtp.h */
/* ========================================================================== */

#ifndef XSMTP_H
#define XSMTP_H


#if defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	defined(XSMTP_FEATURE_SMTP_CLIENT_TLS)
#endif

#if defined(XSMTP_FEATURE_SMTP_AUTH)
#endif

#if defined(XSMTP_FEATURE_SMTP_SUBMIT)
#endif

#endif

#endif

#if defined(XSMTP_IMPLEMENTATION) && !defined(XSMTP_IMPLEMENTATION_ONCE)
#define XSMTP_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xsmtp/src/internal/xrt_mail.h */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP) || \
	defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	defined(XSMTP_FEATURE_SMTP_CLIENT_TLS) || \
	defined(XSMTP_FEATURE_SMTP_SUBMIT) || \
	defined(XSMTP_FEATURE_SMTP_AUTH)
#ifndef XSMTP_INTERNAL_XRT_MAIL_H_BRIDGE_H
#define XSMTP_INTERNAL_XRT_MAIL_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xsmtp/src/internal/xrt_mail_net.h */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	defined(XSMTP_FEATURE_SMTP_CLIENT_TLS)
#ifndef XSMTP_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H
#define XSMTP_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xsmtp/src/internal/xrt_smtp_client.h */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_CLIENT) || \
	defined(XSMTP_FEATURE_SMTP_AUTH)
#ifndef XRT_INTERNAL_SMTP_CLIENT_H
#define XRT_INTERNAL_SMTP_CLIENT_H




#if defined(XSMTP_FEATURE_SMTP_CLIENT)

/* 同步客户端只保存会话状态、能力和最后响应，不复制配置。 */
struct xsmtpclient {
	__xmailtransport Transport;
	xsmtpclientstate State;
	uint64 Capabilities;
	uint64 SizeLimit;
	__xmailtext Reply;
	size_t ReplyLines;
	size_t ReplyLineLimit;
	int ReplyCode;
	xmaildotwriter DataWriter;
	size_t ChunkRemaining;
	bool ChunkLast;
	bool ChunkActive;
	bool ChunkRejected;
	bool Authenticated;
};

/* 认证层成功完成 AUTH 后只通过该内部边界更新会话状态。 */
void __xrtSmtpClientAuthComplete(xsmtpclient* pClient);

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xsmtp/src/internal/xrt_mail_auth.h */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_AUTH)
#ifndef XSMTP_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H
#define XSMTP_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* source: extlibs/xsmtp/src/smtp/smtp.c */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP)



#if defined(XSMTP_FEATURE_SMTP)

/* 验证 envelope 路径没有分隔符或控制字符。 */
XRT_API bool xrtSmtpPathValid(xstrview Path, bool AllowEmpty)
{
	if ( !__xrtMailViewValid(Path) || (!AllowEmpty && (Path.Size == 0)) ) {
		return false;
	}
	for ( size_t i = 0; i < Path.Size; i++ ) {
		unsigned char iByte = (unsigned char)Path.Data[i];

		if ( (iByte <= 32u) || (iByte == 127u) ||
			(iByte == (unsigned char)'<') ||
			(iByte == (unsigned char)'>') ) {
			return false;
		}
	}
	return true;
}



/* 返回 ASCII 字节是否可以用于 SMTP 命令名称。 */
static bool __xrtSmtpVerbByte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'-');
}



/* 解析固定三位 SMTP 响应码。 */
static bool __xrtSmtpReplyCode(xstrview Line, int* pCode)
{
	int iCode = 0;

	if ( Line.Size < 4u ) {
		return false;
	}
	for ( size_t i = 0; i < 3u; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			return false;
		}
		iCode = (iCode * 10) + (int)(iByte - (unsigned char)'0');
	}
	if ( (iCode < 100) || (iCode > 599) ) {
		return false;
	}
	*pCode = iCode;
	return true;
}



/* 解析一条 SMTP 响应行。 */
XRT_API bool xrtSmtpReplyLineParse(
	xstrview Line,
	xsmtpreplyline* pReply
)
{
	xsmtpreplyline Reply;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 xrtMemRangesOverlap(pReply, sizeof(*pReply), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtSmtpReplyCode(Line, &Reply.Code) ||
		 ((Line.Data[3] != ' ') && (Line.Data[3] != '-')) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP reply line"
		);
		return false;
	}
	for ( size_t i = 4u; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"SMTP reply contains a control character"
			);
			return false;
		}
	}
	Reply.Source = Line;
	Reply.Text = __xrtMailSlice(Line, 4u, Line.Size - 4u);
	Reply.Continued = Line.Data[3] == '-';
	*pReply = Reply;
	return true;
}



/* 初始化多行 SMTP 响应验证器。 */
XRT_API bool xrtSmtpReplyParserInit(
	xsmtpreplyparser* pParser,
	size_t iMaxLines
)
{
	if ( !xrtMemRangeValid(pParser, sizeof(*pParser)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxLines == 0 ) {
		iMaxLines = XSMTP_REPLY_LINES_DEFAULT;
	}
	pParser->Code = 0;
	pParser->Lines = 0;
	pParser->MaxLines = iMaxLines;
	pParser->Started = false;
	pParser->Done = false;
	return true;
}



/* 接受一条属于当前响应的 SMTP 行。 */
XRT_API bool xrtSmtpReplyRead(
	xsmtpreplyparser* pParser,
	xstrview Line,
	xsmtpreplyline* pReply
)
{
	xsmtpreplyline Reply;

	if ( !xrtMemRangeValid(pParser, sizeof(*pParser)) ||
		 !xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		 (pParser == NULL) || pParser->Done ||
		 (pParser->Lines > pParser->MaxLines) ||
		 xrtMemRangesOverlap(pParser, sizeof(*pParser), pReply,
			sizeof(*pReply)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtSmtpReplyLineParse(Line, &Reply) ) {
		return false;
	}
	if ( pParser->Lines == pParser->MaxLines ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"SMTP reply exceeds the line limit"
		);
		return false;
	}
	if ( pParser->Started && (Reply.Code != pParser->Code) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"SMTP multiline reply changed its status code"
		);
		return false;
	}
	if ( !pParser->Started ) {
		pParser->Code = Reply.Code;
		pParser->Started = true;
	}
	pParser->Lines++;
	pParser->Done = !Reply.Continued;
	*pReply = Reply;
	return true;
}



/* 解析 EHLO 能力名称和参数。 */
XRT_API bool xrtSmtpCapabilityParse(
	xstrview Text,
	xsmtpcapabilityview* pCapability
)
{
	xsmtpcapabilityview Capability;
	size_t iPosition = 0;
	size_t iParameters;
	size_t iEqual = XRT_NPOS;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pCapability, sizeof(*pCapability)) ||
		 xrtMemRangesOverlap(pCapability, sizeof(*pCapability), Text.Data,
			Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( (iPosition < Text.Size) &&
		 (Text.Data[iPosition] != ' ') &&
		 (Text.Data[iPosition] != '\t') ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];

		if ( iByte == (unsigned char)'=' ) {
			if ( iEqual != XRT_NPOS ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_PROTOCOL,
					"invalid SMTP capability name"
				);
				return false;
			}
			iEqual = iPosition++;
			continue;
		}
		if ( !__xrtSmtpVerbByte(iByte) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP capability name"
			);
			return false;
		}
		iPosition++;
	}
	if ( iPosition == 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"empty SMTP capability name"
		);
		return false;
	}
	if ( iEqual != XRT_NPOS ) {
		if ( (iEqual != 4u) || ((iEqual + 1u) >= iPosition) ||
			 !__xrtMailAsciiEqualI(
				__xrtMailSlice(Text, 0, iEqual),
				XRT_STR_LITERAL("AUTH")
			 )) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP AUTH capability"
			);
			return false;
		}
		iParameters = iEqual + 1u;
	} else {
		iParameters = iPosition;
		while ( (iParameters < Text.Size) &&
			 ((Text.Data[iParameters] == ' ') ||
			  (Text.Data[iParameters] == '\t')) ) {
			iParameters++;
		}
	}
	for ( size_t i = iParameters; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"SMTP capability contains a control character"
			);
			return false;
		}
	}
	Capability.Source = Text;
	Capability.Name = __xrtMailSlice(
		Text,
		0,
		iEqual != XRT_NPOS ? iEqual : iPosition
	);
	Capability.Parameters = __xrtMailSlice(
		Text,
		iParameters,
		Text.Size - iParameters
	);
	*pCapability = Capability;
	return true;
}



/* 查找 SMTP 能力名称的稳定标记。 */
XRT_API uint64 xrtSmtpCapability(xstrview Name)
{
	static const struct {
		cstr Name;
		size_t Size;
		uint64 Value;
	} arrCapabilities[] = {
		{ "PIPELINING", 10u, XSMTP_CAP_PIPELINING },
		{ "SIZE", 4u, XSMTP_CAP_SIZE },
		{ "STARTTLS", 8u, XSMTP_CAP_STARTTLS },
		{ "8BITMIME", 8u, XSMTP_CAP_8BITMIME },
		{ "SMTPUTF8", 8u, XSMTP_CAP_SMTPUTF8 },
		{ "DSN", 3u, XSMTP_CAP_DSN },
		{ "CHUNKING", 8u, XSMTP_CAP_CHUNKING },
		{ "BINARYMIME", 10u, XSMTP_CAP_BINARYMIME },
		{ "ENHANCEDSTATUSCODES", 19u, XSMTP_CAP_ENHANCED_STATUS }
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



/* 解析无符号十进制 SIZE 参数。 */
static bool __xrtSmtpSize(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ||
			 (iValue > ((UINT64_MAX - (uint64)(iByte - (unsigned char)'0')) /
			  UINT64_C(10))) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
	}
	*pValue = iValue;
	return true;
}



/* 把 AUTH 参数中的机制合并到能力位集。 */
static bool __xrtSmtpAuthAdd(xstrview Parameters, uint64* pCapabilities)
{
	size_t iPosition = 0;

	while ( iPosition < Parameters.Size ) {
		size_t iStart;
		xstrview Mechanism;

		while ( (iPosition < Parameters.Size) &&
			((Parameters.Data[iPosition] == ' ') ||
			 (Parameters.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
		iStart = iPosition;
		while ( (iPosition < Parameters.Size) &&
			(Parameters.Data[iPosition] != ' ') &&
			(Parameters.Data[iPosition] != '\t') ) {
			iPosition++;
		}
		Mechanism = __xrtMailSlice(Parameters, iStart, iPosition - iStart);
		if ( __xrtMailAsciiEqualI(Mechanism, XRT_STR_LITERAL("PLAIN")) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_PLAIN;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("LOGIN")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_LOGIN;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("XOAUTH2")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_XOAUTH2;
		} else if ( __xrtMailAsciiEqualI(
			Mechanism,
			XRT_STR_LITERAL("OAUTHBEARER")
		) ) {
			*pCapabilities |= XSMTP_CAP_AUTH_OAUTHBEARER;
		}
	}
	return true;
}



/* 合并 EHLO 能力。 */
XRT_API bool xrtSmtpCapabilityAdd(
	const xsmtpcapabilityview* pCapability,
	uint64* pCapabilities,
	uint64* pSizeLimit
)
{
	uint64 iCapabilities;
	uint64 iSizeLimit;
	uint64 iCapability;

	if ( !xrtMemRangeValid(
		pCapability,
		pCapability != NULL ? sizeof(*pCapability) : 0
	) || (pCapability == NULL) ||
		 !__xrtMailViewValid(pCapability->Name) ||
		 !__xrtMailViewValid(pCapability->Parameters) ||
		 !xrtMemRangeValid(pCapabilities, sizeof(*pCapabilities)) ||
		 !xrtMemRangeValid(pSizeLimit, sizeof(*pSizeLimit)) ||
		 xrtMemRangesOverlap(pCapabilities, sizeof(*pCapabilities),
			pSizeLimit, sizeof(*pSizeLimit)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iCapabilities = *pCapabilities;
	iSizeLimit = *pSizeLimit;
	iCapability = xrtSmtpCapability(pCapability->Name);
	if ( __xrtMailAsciiEqualI(pCapability->Name, XRT_STR_LITERAL("AUTH")) ) {
		if ( !__xrtSmtpAuthAdd(pCapability->Parameters, &iCapabilities) ) {
			return false;
		}
	} else if ( iCapability == XSMTP_CAP_SIZE ) {
		if ( (pCapability->Parameters.Size != 0) &&
			 !__xrtSmtpSize(pCapability->Parameters, &iSizeLimit) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid SMTP SIZE capability"
			);
			return false;
		}
		iCapabilities |= XSMTP_CAP_SIZE;
	} else {
		iCapabilities |= iCapability;
	}
	*pCapabilities = iCapabilities;
	*pSizeLimit = iSizeLimit;
	return true;
}



/* 验证 SMTP 命令名称与参数。 */
static bool __xrtSmtpCommandValid(xstrview Verb, xstrview Arguments)
{
	if ( (Verb.Size == 0) || (Verb.Size > 32u) ) {
		return false;
	}
	for ( size_t i = 0; i < Verb.Size; i++ ) {
		if ( !__xrtSmtpVerbByte((unsigned char)Verb.Data[i]) ) {
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



/* 写出经过控制字符检查的 SMTP 命令。 */
XRT_API bool xrtSmtpCommandWrite(
	xstrview Verb,
	xstrview Arguments,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;

	if ( !__xrtMailViewValid(Verb) || !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtSmtpCommandValid(Verb, Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP command"
		);
		return false;
	}
	if ( !__xrtMailSizeAdd(Verb.Size, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > XSMTP_COMMAND_MAX ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"SMTP command exceeds 512 bytes"
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



/* 分配并写出 SMTP 命令。 */
XRT_API str xrtSmtpCommand(
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
	) || !xrtSmtpCommandWrite(
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
	if ( !xrtSmtpCommandWrite(
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
/* source: extlibs/xsmtp/src/smtp/smtp_client.c */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_CLIENT)




#if defined(XSMTP_FEATURE_SMTP_CLIENT)





/* 判断一个借用视图是否具有有效地址范围。 */
static bool __xrtSmtpClientViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 创建 SMTP 客户端错误。 */
static void __xrtSmtpClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
}



/* 验证客户端仍然可以交换协议数据。 */
static bool __xrtSmtpClientUsable(const xsmtpclient* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XSMTP_CLIENT_CLOSED) ||
		(pClient->State == XSMTP_CLIENT_FAILED) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP client is not usable");
		return false;
	}
	return true;
}



/* 普通命令不能插入 DATA、未完成的 BDAT 或已拒绝的 BDAT 事务。 */
static bool __xrtSmtpClientCommandMode(
	const xsmtpclient* pClient,
	bool bReset
)
{
	if ( !__xrtSmtpClientUsable(pClient) ) {
		return false;
	}
	if ( (pClient->State == XSMTP_CLIENT_DATA) ||
		pClient->ChunkActive ||
		(pClient->ChunkRejected && !bReset) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP transaction does not accept a command"
		);
		return false;
	}
	return true;
}



/* 将不可恢复的线路失败记录为会话终态。 */
static bool __xrtSmtpClientFailed(xsmtpclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XSMTP_CLIENT_FAILED;
		__xrtMailTransportAbortPreserveError(&pClient->Transport);
	}
	return false;
}



/* 保存最后一条响应文本，借用视图稳定到下一次响应。 */
static bool __xrtSmtpClientReplySave(
	xsmtpclient* pClient,
	xstrview Text,
	int iCode,
	size_t iLines
)
{
	if ( !__xrtMailTextSet(&pClient->Reply, Text) ) {
		return false;
	}
	pClient->ReplyCode = iCode;
	pClient->ReplyLines = iLines;
	return true;
}



/* 把一条 EHLO 扩展合并到客户端能力快照。 */
static bool __xrtSmtpClientCapability(
	xsmtpclient* pClient,
	xstrview Text
)
{
	xsmtpcapabilityview Capability;

	return xrtSmtpCapabilityParse(Text, &Capability) &&
		xrtSmtpCapabilityAdd(
			&Capability,
			&pClient->Capabilities,
			&pClient->SizeLimit
		);
}



/* 读取一条响应，并可解析 EHLO 首行之后的扩展。 */
static bool __xrtSmtpClientReceiveMode(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	bool bCapabilities,
	size_t iReplyLines,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreplyparser Parser;
	xsmtpreplyline Line;
	xstrview Text;

	if ( !__xrtSmtpClientUsable(pClient) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		!xrtSmtpReplyParserInit(&Parser, iReplyLines) ) {
		return false;
	}
	for ( ;; ) {
		if ( !__xrtMailTransportLine(
			&pClient->Transport,
			&Text,
			iDeadline,
			pCancel
		) || !xrtSmtpReplyRead(&Parser, Text, &Line) ) {
			return __xrtSmtpClientFailed(pClient);
		}
		if ( bCapabilities && (Parser.Lines > 1u) &&
			(Line.Code != 421) &&
			!__xrtSmtpClientCapability(pClient, Line.Text) ) {
			return __xrtSmtpClientFailed(pClient);
		}
		if ( Parser.Done ) {
			break;
		}
	}
	if ( !__xrtSmtpClientReplySave(
		pClient,
		Line.Text,
		Line.Code,
		Parser.Lines
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	pReply->Code = pClient->ReplyCode;
	pReply->Lines = pClient->ReplyLines;
	pReply->Text.Data = pClient->Reply.Data;
	pReply->Text.Size = pClient->Reply.Size;
	if ( pReply->Code == 421 ) {
		__xrtSmtpClientError(
			XERR_CLOSED,
			"SMTP server is closing the transmission channel"
		);
		return __xrtSmtpClientFailed(pClient);
	}
	return true;
}



/* 设置意外服务器状态错误，同时保留 LastReply 供诊断。 */
static bool __xrtSmtpClientUnexpected(void)
{
	__xrtSmtpClientError(XERR_PROTOCOL, "unexpected SMTP reply code");
	return false;
}



/* 发送 EHLO，并在明确不支持时按配置回退 HELO。 */
static bool __xrtSmtpClientHello(
	xsmtpclient* pClient,
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;
	char sCommand[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;

	pClient->Capabilities = 0;
	pClient->SizeLimit = 0;
	if ( !xrtSmtpCommandWrite(
		XRT_STR_LITERAL("EHLO"),
		pConfig->Hello,
		sCommand,
		sizeof(sCommand),
		&iSize
	) || !__xrtMailTransportSend(
		&pClient->Transport,
		sCommand,
		iSize,
		iDeadline,
		pCancel
	) || !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		true,
		pConfig->ReplyLines,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code == 250 ) {
		return true;
	}
	if ( !pConfig->HeloFallback ||
		((Reply.Code != 500) && (Reply.Code != 502) &&
		 (Reply.Code != 504)) ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->Capabilities = 0;
	pClient->SizeLimit = 0;
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("HELO"),
		pConfig->Hello,
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	return Reply.Code == 250 ? true : __xrtSmtpClientUnexpected();
}



/* 在栈上构建 FROM/TO 路径参数。 */
static bool __xrtSmtpClientPathArguments(
	cstr sPrefix,
	xstrview Path,
	xstrview Parameters,
	bool bAllowEmpty,
	char* sOutput,
	size_t iCapacity,
	size_t* pSize
)
{
	size_t iPrefix = strlen(sPrefix);
	size_t iRequired = iPrefix;

	if ( !xrtSmtpPathValid(Path, bAllowEmpty) ||
		!__xrtSmtpClientViewValid(Parameters) ||
		!__xrtMailSizeAdd(iRequired, Path.Size, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		((Parameters.Size != 0) &&
		 (!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		  !__xrtMailSizeAdd(iRequired, Parameters.Size, &iRequired))) ||
		(iRequired >= iCapacity) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP envelope path or parameters"
		);
		return false;
	}
	memcpy(sOutput, sPrefix, iPrefix);
	if ( Path.Size != 0 ) {
		memcpy(sOutput + iPrefix, Path.Data, Path.Size);
	}
	sOutput[iPrefix + Path.Size] = '>';
	if ( Parameters.Size != 0 ) {
		sOutput[iPrefix + Path.Size + 1u] = ' ';
		memcpy(
			sOutput + iPrefix + Path.Size + 2u,
			Parameters.Data,
			Parameters.Size
		);
	}
	sOutput[iRequired] = 0;
	*pSize = iRequired;
	return true;
}



/* 把 dot writer 输出直接提交到当前 SMTP 传输。 */
typedef struct __xsmtpclientdatasink {
	xsmtpclient* Client;
	xdeadline Deadline;
	xcancel* Cancel;
} __xsmtpclientdatasink;



/* 同步发送一个已经完成 dot transparency 的 DATA 片段。 */
static bool __xrtSmtpClientDataSink(xbytesview Data, ptr pUserData)
{
	__xsmtpclientdatasink* pSink = (__xsmtpclientdatasink*)pUserData;

	return __xrtMailTransportSend(
		&pSink->Client->Transport,
		Data.Data,
		Data.Size,
		pSink->Deadline,
		pSink->Cancel
	);
}



/* 初始化 SMTP 客户端配置。 */
XRT_API void xrtSmtpClientConfigInit(xsmtpclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 25u;
	pConfig->Hello = (xstrview)XRT_STR_LITERAL("localhost");
	pConfig->ReplyLines = XSMTP_REPLY_LINES_DEFAULT;
	pConfig->HeloFallback = true;
}



/* 验证 SMTP 客户端配置。 */
XRT_API bool xrtSmtpClientConfigValid(const xsmtpclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailNetConfigValid(&pConfig->Net) ) {
		return false;
	}
	if ( !__xrtSmtpClientViewValid(pConfig->Hello) ||
		(pConfig->Hello.Size == 0) ||
		(pConfig->Hello.Size > XSMTP_HELLO_MAX) ||
		(pConfig->ReplyLines == 0) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid SMTP client configuration"
		);
		return false;
	}
	for ( size_t i = 0; i < pConfig->Hello.Size; i++ ) {
		unsigned char iByte = (unsigned char)pConfig->Hello.Data[i];

		if ( (iByte <= 32u) || (iByte >= 127u) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_CONFIG,
				"invalid SMTP EHLO name"
			);
			return false;
		}
	}
	return true;
}



/* 建立并协商 SMTP 会话。 */
XRT_API xsmtpclient* xrtSmtpClientOpen(
	const xsmtpclientconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpclient* pClient;
	xsmtpreply Reply;
	bool bOpened;
	bool bReceived;

	if ( !xrtSmtpClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (xsmtpclient*)xrtCalloc(1, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XSMTP_CLIENT_READY;
	pClient->ReplyLineLimit = pConfig->ReplyLines;
	memset(&Reply, 0, sizeof(Reply));
	bOpened = __xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	);
	bReceived = bOpened && __xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	);
	if ( !bOpened || !bReceived ) {
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( Reply.Code != 220 ) {
		(void)__xrtSmtpClientUnexpected();
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( !__xrtSmtpClientHello(
		pClient,
		pConfig,
		iDeadline,
		pCancel
	) ) {
		xrtSmtpClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		#if defined(XSMTP_FEATURE_SMTP_CLIENT_TLS)
			if ( (pClient->Capabilities & XSMTP_CAP_STARTTLS) == 0 ) {
				__xrtSmtpClientError(
					XERR_UNSUPPORTED,
					"SMTP server does not support STARTTLS"
				);
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
			memset(&Reply, 0, sizeof(Reply));
			if ( !xrtSmtpClientCommand(
				pClient,
				XRT_STR_LITERAL("STARTTLS"),
				XRT_STR_LITERAL(""),
				&Reply,
				iDeadline,
				pCancel
			) ) {
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
			if ( Reply.Code != 220 ) {
				(void)__xrtSmtpClientUnexpected();
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
			if ( !__xrtMailTransportStartTls(
					&pClient->Transport,
					&pConfig->Net,
					iDeadline,
					pCancel
				) || !__xrtSmtpClientHello(
					pClient,
					pConfig,
					iDeadline,
					pCancel
				) ) {
				xrtSmtpClientDestroy(pClient);
				return NULL;
			}
		#else
			__xrtSmtpClientError(
				XERR_UNSUPPORTED,
				"SMTP STARTTLS support is not enabled"
			);
			xrtSmtpClientDestroy(pClient);
			return NULL;
		#endif
	}
	pClient->State = XSMTP_CLIENT_READY;
	return pClient;
}



/* 返回 SMTP 客户端状态。 */
XRT_API xsmtpclientstate xrtSmtpClientState(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->State : XSMTP_CLIENT_FAILED;
}



/* 返回能力快照。 */
XRT_API uint64 xrtSmtpClientCapabilities(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回 SIZE 快照。 */
XRT_API uint64 xrtSmtpClientSizeLimit(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->SizeLimit : 0;
}



/* 返回当前 SMTP 传输安全级别。 */
XRT_API xmailsecurity xrtSmtpClientSecurity(const xsmtpclient* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



/* 返回当前会话的 SMTP AUTH 完成状态。 */
XRT_API bool xrtSmtpClientAuthenticated(const xsmtpclient* pClient)
{
	return (pClient != NULL) && pClient->Authenticated;
}



/* 由认证层在服务器接受凭据后记录不可逆的会话状态。 */
void __xrtSmtpClientAuthComplete(xsmtpclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->Authenticated = true;
	}
}



/* 取得最后响应。 */
XRT_API bool xrtSmtpClientLastReply(
	const xsmtpclient* pClient,
	xsmtpreply* pReply
)
{
	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		(pClient->ReplyCode == 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pReply->Code = pClient->ReplyCode;
	pReply->Lines = pClient->ReplyLines;
	pReply->Text.Data = pClient->Reply.Data;
	pReply->Text.Size = pClient->Reply.Size;
	return true;
}



/* 验证并发送一条具有调用方指定上限的 SMTP 行。 */
static bool __xrtSmtpClientSendLine(
	xsmtpclient* pClient,
	xstrview Line,
	size_t iLimit,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientCommandMode(pClient, false) ) {
		return false;
	}
	if ( !__xrtSmtpClientViewValid(Line) ||
		(Line.Size > iLimit) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Line.Size; i++ ) {
		unsigned char iByte = (unsigned char)Line.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			__xrtSmtpClientError(
				XERR_ARGUMENT,
				"SMTP line contains a control separator"
			);
			return false;
		}
	}
	if ( (Line.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Line.Data,
		Line.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	return true;
}



/* 发送受普通命令长度约束的低层 SMTP 行。 */
XRT_API bool xrtSmtpClientSend(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtSmtpClientSendLine(
		pClient,
		Line,
		XSMTP_COMMAND_MAX - 2u,
		iDeadline,
		pCancel
	);
}



/* 发送具有独立长度上限的 SASL continuation 响应。 */
XRT_API bool xrtSmtpClientAuthLine(
	xsmtpclient* pClient,
	xstrview Line,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	return __xrtSmtpClientSendLine(
		pClient,
		Line,
		XSMTP_AUTH_RESPONSE_MAX,
		iDeadline,
		pCancel
	);
}



/* 读取完整 SMTP 响应。 */
XRT_API bool xrtSmtpClientReceive(
	xsmtpclient* pClient,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientCommandMode(pClient, false) ) {
		return false;
	}
	return __xrtSmtpClientReceiveMode(
		pClient,
		pReply,
		false,
		pClient != NULL ? pClient->ReplyLineLimit : 0,
		iDeadline,
		pCancel
	);
}



/* 发送命令并读取响应。 */
XRT_API bool xrtSmtpClientCommand(
	xsmtpclient* pClient,
	xstrview Verb,
	xstrview Arguments,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sCommand[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	bool bReset;

	if ( !__xrtSmtpClientViewValid(Verb) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	bReset = __xrtMailAsciiEqualI(Verb, XRT_STR_LITERAL("RSET"));
	if ( !__xrtSmtpClientCommandMode(pClient, bReset) ||
		!xrtMemRangeValid(pReply, sizeof(*pReply)) ||
		!xrtSmtpCommandWrite(
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
		return __xrtSmtpClientFailed(pClient);
	}
	return __xrtSmtpClientReceiveMode(
		pClient,
		pReply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	);
}



/* 开始 SMTP envelope。 */
XRT_API bool xrtSmtpClientMail(
	xsmtpclient* pClient,
	xstrview ReversePath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sArguments[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_READY) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP MAIL requires READY state");
		return false;
	}
	if ( !__xrtSmtpClientPathArguments(
		"FROM:<",
		ReversePath,
		Parameters,
		true,
		sArguments,
		sizeof(sArguments),
		&iSize
	) || !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("MAIL"),
		(xstrview) { sArguments, iSize },
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_MAIL;
	return true;
}



/* 增加 SMTP envelope 收件人。 */
XRT_API bool xrtSmtpClientRcpt(
	xsmtpclient* pClient,
	xstrview ForwardPath,
	xstrview Parameters,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sArguments[XSMTP_COMMAND_MAX + 1u];
	size_t iSize;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		((pClient->State != XSMTP_CLIENT_MAIL) &&
		 (pClient->State != XSMTP_CLIENT_RECIPIENT)) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP RCPT requires an active envelope"
		);
		return false;
	}
	if ( !__xrtSmtpClientPathArguments(
		"TO:<",
		ForwardPath,
		Parameters,
		false,
		sArguments,
		sizeof(sArguments),
		&iSize
	) || !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("RCPT"),
		(xstrview) { sArguments, iSize },
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (Reply.Code != 250) && (Reply.Code != 251) &&
		(Reply.Code != 252) ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_RECIPIENT;
	return true;
}



/* 进入 SMTP DATA 模式。 */
XRT_API bool xrtSmtpClientDataBegin(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_RECIPIENT) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP DATA requires at least one accepted recipient"
		);
		return false;
	}
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("DATA"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 354 ) {
		return __xrtSmtpClientUnexpected();
	}
	if ( !xrtMailDotWriterInit(&pClient->DataWriter) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	pClient->State = XSMTP_CLIENT_DATA;
	return true;
}



/* 发送一个 SMTP DATA 消息片段。 */
XRT_API bool xrtSmtpClientDataWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpclientdatasink Sink;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_DATA) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP DATA write requires DATA state");
		return false;
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailDotWriterWrite(
		&pClient->DataWriter,
		Data,
		__xrtSmtpClientDataSink,
		&Sink
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	return true;
}



/* 完成 SMTP DATA 并读取最终响应。 */
XRT_API bool xrtSmtpClientDataEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpclientdatasink Sink;
	xsmtpreply Reply;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_DATA) ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP DATA end requires DATA state");
		return false;
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailDotWriterFinish(
		&pClient->DataWriter,
		__xrtSmtpClientDataSink,
		&Sink
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	if ( !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	pClient->State = XSMTP_CLIENT_READY;
	return Reply.Code == 250 ? true : __xrtSmtpClientUnexpected();
}



/* 发送一份已经连续存放的 SMTP DATA。 */
XRT_API bool xrtSmtpClientData(
	xsmtpclient* pClient,
	xstrview Message,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xbytesview Data;
	size_t iEncoded;

	if ( !xrtMailDotWrite(Message, true, NULL, 0, &iEncoded) ) {
		return false;
	}
	(void)iEncoded;
	Data.Data = (const unsigned char*)Message.Data;
	Data.Size = Message.Size;
	return xrtSmtpClientDataBegin(pClient, iDeadline, pCancel) &&
		xrtSmtpClientDataWrite(pClient, Data, iDeadline, pCancel) &&
		xrtSmtpClientDataEnd(pClient, iDeadline, pCancel);
}



/* 开始一个精确计数的 SMTP BDAT 块。 */
XRT_API bool xrtSmtpClientBdatBegin(
	xsmtpclient* pClient,
	size_t iChunkSize,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sLine[5u + (sizeof(size_t) * 3u) + 5u];
	size_t iSize;

	if ( !__xrtSmtpClientCommandMode(pClient, false) ||
		((pClient->State != XSMTP_CLIENT_RECIPIENT) &&
		 (pClient->State != XSMTP_CLIENT_CHUNK)) ) {
		__xrtSmtpClientError(
			XERR_STATE,
			"SMTP BDAT requires an accepted recipient"
		);
		return false;
	}
	if ( (pClient->Capabilities & XSMTP_CAP_CHUNKING) == 0 ) {
		__xrtSmtpClientError(
			XERR_UNSUPPORTED,
			"SMTP server does not advertise CHUNKING"
		);
		return false;
	}
	memcpy(sLine, "BDAT ", 5u);
	iSize = 5u + __xrtMailUint64Write(
		sLine + 5u,
		(uint64)iChunkSize
	);
	if ( Last ) {
		memcpy(sLine + iSize, " LAST", 5u);
		iSize += 5u;
	}
	if ( !xrtSmtpClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	pClient->State = XSMTP_CLIENT_CHUNK;
	pClient->ChunkRemaining = iChunkSize;
	pClient->ChunkLast = Last;
	pClient->ChunkActive = true;
	return true;
}



/* 发送当前 BDAT 块的一段原始字节。 */
XRT_API bool xrtSmtpClientBdatWrite(
	xsmtpclient* pClient,
	xbytesview Data,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_CHUNK) ||
		!pClient->ChunkActive ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT write requires a block");
		return false;
	}
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Data.Size > pClient->ChunkRemaining ) {
		__xrtSmtpClientError(XERR_RANGE, "SMTP BDAT data exceeds chunk size");
		return false;
	}
	if ( (Data.Size != 0) && !__xrtMailTransportSend(
		&pClient->Transport,
		Data.Data,
		Data.Size,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpClientFailed(pClient);
	}
	pClient->ChunkRemaining -= Data.Size;
	return true;
}



/* 完成当前 BDAT 块并读取服务器确认。 */
XRT_API bool xrtSmtpClientBdatEnd(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;
	bool bLast;

	if ( !__xrtSmtpClientUsable(pClient) ||
		(pClient->State != XSMTP_CLIENT_CHUNK) ||
		!pClient->ChunkActive ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT end requires a block");
		return false;
	}
	if ( pClient->ChunkRemaining != 0 ) {
		__xrtSmtpClientError(XERR_STATE, "SMTP BDAT block is incomplete");
		return false;
	}
	bLast = pClient->ChunkLast;
	pClient->ChunkActive = false;
	pClient->ChunkLast = false;
	if ( !__xrtSmtpClientReceiveMode(
		pClient,
		&Reply,
		false,
		pClient->ReplyLineLimit,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		pClient->ChunkRejected = true;
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = bLast ? XSMTP_CLIENT_READY : XSMTP_CLIENT_CHUNK;
	return true;
}



/* 发送一个连续存放的 SMTP BDAT 块。 */
XRT_API bool xrtSmtpClientBdat(
	xsmtpclient* pClient,
	xbytesview Data,
	bool Last,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtSmtpClientBdatBegin(
		pClient,
		Data.Size,
		Last,
		iDeadline,
		pCancel
	) && xrtSmtpClientBdatWrite(
		pClient,
		Data,
		iDeadline,
		pCancel
	) && xrtSmtpClientBdatEnd(pClient, iDeadline, pCancel);
}



/* 重置 SMTP envelope。 */
XRT_API bool xrtSmtpClientReset(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("RSET"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 250 ) {
		return __xrtSmtpClientUnexpected();
	}
	pClient->State = XSMTP_CLIENT_READY;
	pClient->ChunkRemaining = 0;
	pClient->ChunkLast = false;
	pClient->ChunkActive = false;
	pClient->ChunkRejected = false;
	return true;
}



/* 发送 SMTP NOOP。 */
XRT_API bool xrtSmtpClientNoop(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	return xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) && (Reply.Code == 250 ? true : __xrtSmtpClientUnexpected());
}



/* 发送 QUIT 并关闭传输。 */
XRT_API bool xrtSmtpClientQuit(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpreply Reply;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("QUIT"),
		XRT_STR_LITERAL(""),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 221 ) {
		return __xrtSmtpClientUnexpected();
	}
	return xrtSmtpClientClose(pClient, iDeadline);
}



/* 正常关闭 SMTP 传输。 */
XRT_API bool xrtSmtpClientClose(
	xsmtpclient* pClient,
	xdeadline iDeadline
)
{
	if ( !__xrtSmtpClientUsable(pClient) ) {
		return false;
	}
	if ( !__xrtMailTransportClose(&pClient->Transport, iDeadline) ) {
		pClient->State = XSMTP_CLIENT_FAILED;
		return false;
	}
	pClient->State = XSMTP_CLIENT_CLOSED;
	return true;
}



/* 无等待异常中止 SMTP 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtSmtpClientAbort(xsmtpclient* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XSMTP_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XSMTP_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XSMTP_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XSMTP_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 SMTP 客户端。 */
XRT_API void xrtSmtpClientDestroy(xsmtpclient* pClient)
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
/* source: extlibs/xsmtp/src/smtp/smtp_submit.c */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_SUBMIT)



#if defined(XSMTP_FEATURE_SMTP_SUBMIT)

/* Compose sink 只借用客户端与本次统一等待上下文。 */
typedef struct __xsmtpsubmitsink {
	xsmtpclient* Client;
	xdeadline Deadline;
	xcancel* Cancel;
} __xsmtpsubmitsink;



/* 验证 ESMTP 参数文本不含线路控制字符。 */
static bool __xrtSmtpSubmitParameters(xstrview Parameters)
{
	if ( !__xrtMailViewValid(Parameters) ) {
		return false;
	}
	for ( size_t i = 0; i < Parameters.Size; i++ ) {
		unsigned char iByte = (unsigned char)Parameters.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 验证路径与参数能够装入单条 SMTP 命令。 */
static bool __xrtSmtpSubmitPath(
	xstrview Path,
	xstrview Parameters,
	size_t iPrefix,
	bool bAllowEmpty
)
{
	size_t iArguments;
	size_t iCommand;

	if ( !xrtSmtpPathValid(Path, bAllowEmpty) ||
		 !__xrtSmtpSubmitParameters(Parameters) ||
		 !__xrtMailSizeAdd(iPrefix, Path.Size, &iArguments) ||
		 !__xrtMailSizeAdd(iArguments, 1u, &iArguments) ||
		 ((Parameters.Size != 0) &&
		  (!__xrtMailSizeAdd(iArguments, 1u, &iArguments) ||
		   !__xrtMailSizeAdd(iArguments, Parameters.Size, &iArguments))) ||
		 !__xrtMailSizeAdd(7u, iArguments, &iCommand) ||
		 (iCommand > XSMTP_COMMAND_MAX) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid SMTP submit envelope path or parameters"
		);
		return false;
	}
	return true;
}



/* 失败后恢复 envelope；DATA 已开始时关闭连接以保证半封消息不会提交。 */
static bool __xrtSmtpSubmitRecover(
	xsmtpclient* pClient,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xsmtpclientstate State = xrtSmtpClientState(pClient);
	xerror* pPrimaryError = xrtTakeError();
	xerror* pRecoveryError;

	if ( (State == XSMTP_CLIENT_MAIL) ||
		 (State == XSMTP_CLIENT_RECIPIENT) ) {
		if ( !xrtSmtpClientReset(pClient, iDeadline, pCancel) ) {
			(void)xrtSmtpClientAbort(pClient);
		}
	} else if ( (State == XSMTP_CLIENT_DATA) ||
		 (State == XSMTP_CLIENT_FAILED) ) {
		(void)xrtSmtpClientAbort(pClient);
	}
	pRecoveryError = xrtTakeError();
	if ( pPrimaryError != NULL ) {
		xrtErrorFree(pRecoveryError);
		xrtSetErrorTake(pPrimaryError);
	} else {
		xrtSetErrorTake(pRecoveryError);
	}
	return false;
}



/* 把 Compose 片段直接送入 SMTP 增量 DATA。 */
static bool __xrtSmtpSubmitWrite(xbytesview Data, ptr pUserData)
{
	__xsmtpsubmitsink* pSink = (__xsmtpsubmitsink*)pUserData;

	return xrtSmtpClientDataWrite(
		pSink->Client,
		Data,
		pSink->Deadline,
		pSink->Cancel
	);
}



/* 在 envelope 已建立后流式提交消息内容。 */
static bool __xrtSmtpSubmitData(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	__xsmtpsubmitsink Sink;
	size_t iWritten;

	if ( !xrtSmtpClientDataBegin(pClient, iDeadline, pCancel) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	Sink.Client = pClient;
	Sink.Deadline = iDeadline;
	Sink.Cancel = pCancel;
	if ( !xrtMailComposeWrite(
		pMessage,
		__xrtSmtpSubmitWrite,
		&Sink,
		&iWritten
	) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	(void)iWritten;
	return xrtSmtpClientDataEnd(pClient, iDeadline, pCancel);
}



/* 验证 envelope 本体和收件人数组范围，避免计数字节乘法溢出。 */
static bool __xrtSmtpSubmitEnvelopeValid(const xsmtpenvelope* pEnvelope)
{
	size_t iRecipientBytes;

	if ( !xrtMemRangeValid(pEnvelope, sizeof(*pEnvelope)) ||
		 (pEnvelope->RecipientCount == 0) ||
		 (pEnvelope->RecipientCount >
		  (SIZE_MAX / sizeof(*pEnvelope->Recipients))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iRecipientBytes = pEnvelope->RecipientCount *
		sizeof(*pEnvelope->Recipients);
	if ( !xrtMemRangeValid(pEnvelope->Recipients, iRecipientBytes) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 使用独立 envelope 流式提交消息。 */
XRT_API bool xrtSmtpSubmitEnvelope(
	xsmtpclient* pClient,
	const xsmtpenvelope* pEnvelope,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtSmtpSubmitEnvelopeValid(pEnvelope) ||
		 (xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY) ||
		 !xrtMailMessageValid(pMessage) ||
		 !__xrtSmtpSubmitPath(
			pEnvelope->ReversePath,
			pEnvelope->MailParameters,
			6u,
			true
		 ) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return false;
	}
	for ( size_t i = 0; i < pEnvelope->RecipientCount; i++ ) {
		if ( !__xrtSmtpSubmitPath(
			pEnvelope->Recipients[i].Address,
			pEnvelope->Recipients[i].Parameters,
			4u,
			false
		) ) {
			return false;
		}
	}
	if ( !xrtSmtpClientMail(
		pClient,
		pEnvelope->ReversePath,
		pEnvelope->MailParameters,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( size_t i = 0; i < pEnvelope->RecipientCount; i++ ) {
		if ( !xrtSmtpClientRcpt(
			pClient,
			pEnvelope->Recipients[i].Address,
			pEnvelope->Recipients[i].Parameters,
			iDeadline,
			pCancel
		) ) {
			return __xrtSmtpSubmitRecover(
				pClient,
				iDeadline,
				pCancel
			);
		}
	}
	return __xrtSmtpSubmitData(pClient, pMessage, iDeadline, pCancel);
}



/* 发送消息描述中的一个地址数组。 */
static bool __xrtSmtpSubmitAddresses(
	xsmtpclient* pClient,
	const xmailaddress* pAddresses,
	size_t iCount,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !xrtSmtpClientRcpt(
			pClient,
			pAddresses[i].Address,
			XRT_STR_LITERAL(""),
			iDeadline,
			pCancel
		) ) {
			return false;
		}
	}
	return true;
}



/* 从消息 From、To、Cc、Bcc 自动建立 envelope 并提交。 */
XRT_API bool xrtSmtpSubmit(
	xsmtpclient* pClient,
	const xmailmessage* pMessage,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	if ( (xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY) ||
		 !xrtMailMessageValid(pMessage) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return false;
	}
	if ( !xrtSmtpClientMail(
		pClient,
		pMessage->From.Address,
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->To,
		pMessage->ToCount,
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->Cc,
		pMessage->CcCount,
		iDeadline,
		pCancel
	) || !__xrtSmtpSubmitAddresses(
		pClient,
		pMessage->Bcc,
		pMessage->BccCount,
		iDeadline,
		pCancel
	) ) {
		return __xrtSmtpSubmitRecover(
			pClient,
			iDeadline,
			pCancel
		);
	}
	return __xrtSmtpSubmitData(pClient, pMessage, iDeadline, pCancel);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xsmtp/src/smtp/smtp_auth.c */
/* ========================================================================== */

#if defined(XSMTP_FEATURE_SMTP_AUTH)




#if defined(XSMTP_FEATURE_SMTP_AUTH)

/* 判断认证字段是否具有有效的借用地址范围。 */
/* 设置稳定的 SMTP 认证错误。 */
static bool __xrtSmtpAuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 发送一行敏感文本并在发送返回后立即清零临时副本。 */
static bool __xrtSmtpAuthSend(
	xsmtpclient* pClient,
	xstrview Prefix,
	xstrview Encoded,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sLine;
	size_t iSize;
	bool bSuccess;

	if ( !__xrtMailSizeAdd(Prefix.Size, Encoded.Size, &iSize) ||
		(iSize > (Prefix.Size != 0 ?
		 (XSMTP_COMMAND_MAX - 2u) : XSMTP_AUTH_RESPONSE_MAX)) ) {
		return __xrtSmtpAuthError(
			XERR_RANGE,
			"SMTP authentication response exceeds the command limit"
		);
	}
	sLine = (char*)xrtMalloc(iSize + 1u);
	if ( sLine == NULL ) {
		return false;
	}
	if ( Prefix.Size != 0 ) {
		memcpy(sLine, Prefix.Data, Prefix.Size);
	}
	if ( Encoded.Size != 0 ) {
		memcpy(sLine + Prefix.Size, Encoded.Data, Encoded.Size);
	}
	sLine[iSize] = 0;
	bSuccess = Prefix.Size != 0 ? xrtSmtpClientSend(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	) : xrtSmtpClientAuthLine(
		pClient,
		(xstrview) { sLine, iSize },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sLine, iSize + 1u);
	return bSuccess && xrtSmtpClientReceive(
		pClient,
		pReply,
		iDeadline,
		pCancel
	);
}



/* 把认证终态响应转换为稳定错误。 */
static bool __xrtSmtpAuthResult(const xsmtpreply* pReply)
{
	if ( pReply->Code == 235 ) {
		return true;
	}
	return __xrtSmtpAuthError(
		((pReply->Code >= 400) && (pReply->Code <= 599)) ?
			XERR_PERMISSION : XERR_PROTOCOL,
		"SMTP authentication was rejected"
	);
}



/* 按配置和 SMTP 行长限制决定初始响应或单独 challenge。 */
static bool __xrtSmtpAuthExchange(
	xsmtpclient* pClient,
	xstrview Mechanism,
	char* sEncoded,
	size_t iEncoded,
	bool bInitial,
	xsmtpreply* pReply,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char sPrefix[32];
	size_t iPrefix;

	if ( iEncoded > XSMTP_AUTH_RESPONSE_MAX ) {
		return __xrtSmtpAuthError(
			XERR_RANGE,
			"SMTP authentication response exceeds the SASL limit"
		);
	}
	if ( bInitial &&
		(Mechanism.Size <= (XSMTP_COMMAND_MAX - 8u)) &&
		(iEncoded <= ((XSMTP_COMMAND_MAX - 8u) - Mechanism.Size)) ) {
		iPrefix = 5u + Mechanism.Size + 1u;
		if ( iPrefix > sizeof(sPrefix) ) {
			return __xrtSmtpAuthError(
				XERR_INTERNAL,
				"SMTP authentication mechanism prefix overflow"
			);
		}
		memcpy(sPrefix, "AUTH ", 5u);
		memcpy(sPrefix + 5u, Mechanism.Data, Mechanism.Size);
		sPrefix[iPrefix - 1u] = ' ';
		return __xrtSmtpAuthSend(
			pClient,
			(xstrview) { sPrefix, iPrefix },
			(xstrview) { sEncoded, iEncoded },
			pReply,
			iDeadline,
			pCancel
		);
	}
	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("AUTH"),
		Mechanism,
		pReply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( pReply->Code != 334 ) {
		return true;
	}
	return __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sEncoded, iEncoded },
		pReply,
		iDeadline,
		pCancel
	);
}



/* 执行 SASL PLAIN 认证。 */
static bool __xrtSmtpAuthPlain(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sEncoded;
	size_t iEncoded;
	xsmtpreply Reply;
	bool bSuccess;

	sEncoded = __xrtMailAuthPlain(
		pConfig->AuthorizationId,
		pConfig->Username,
		pConfig->Secret,
		&iEncoded
	);
	if ( sEncoded == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthExchange(
		pClient,
		XRT_STR_LITERAL("PLAIN"),
		sEncoded,
		iEncoded,
		pConfig->InitialResponse,
		&Reply,
		iDeadline,
		pCancel
	);
	if ( bSuccess && (Reply.Code == 334) ) {
		bSuccess = __xrtSmtpAuthSend(
			pClient,
			XRT_STR_LITERAL(""),
			(xstrview) { sEncoded, iEncoded },
			&Reply,
			iDeadline,
			pCancel
		);
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 执行传统 AUTH LOGIN challenge 交换。 */
static bool __xrtSmtpAuthLogin(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	char* sUsername;
	char* sSecret;
	size_t iUsername;
	size_t iSecret;
	xsmtpreply Reply;
	bool bSuccess;

	if ( !xrtSmtpClientCommand(
		pClient,
		XRT_STR_LITERAL("AUTH"),
		XRT_STR_LITERAL("LOGIN"),
		&Reply,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( Reply.Code != 334 ) {
		return __xrtSmtpAuthResult(&Reply);
	}
	sUsername = __xrtMailAuthEncode(
		pConfig->Username.Data,
		pConfig->Username.Size,
		&iUsername
	);
	if ( sUsername == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sUsername, iUsername },
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sUsername, iUsername + 1u);
	if ( !bSuccess || (Reply.Code != 334) ) {
		return bSuccess ? __xrtSmtpAuthResult(&Reply) : false;
	}
	sSecret = __xrtMailAuthEncode(
		pConfig->Secret.Data,
		pConfig->Secret.Size,
		&iSecret
	);
	if ( sSecret == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthSend(
		pClient,
		XRT_STR_LITERAL(""),
		(xstrview) { sSecret, iSecret },
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sSecret, iSecret + 1u);
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 执行 XOAUTH2 或 OAUTHBEARER 初始响应交换。 */
static bool __xrtSmtpAuthBearer(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	xstrview AuthorizationId = pConfig->AuthorizationId.Size != 0 ?
		pConfig->AuthorizationId : pConfig->Username;
	xstrview Mechanism = pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		XRT_STR_LITERAL("XOAUTH2") : XRT_STR_LITERAL("OAUTHBEARER");
	char* sEncoded;
	size_t iEncoded;
	xsmtpreply Reply;
	bool bSuccess;

	sEncoded = pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		__xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		) : __xrtMailAuthOauthBearer(
			AuthorizationId,
			pConfig->Secret,
			&iEncoded
		);
	if ( sEncoded == NULL ) {
		return false;
	}
	bSuccess = __xrtSmtpAuthExchange(
		pClient,
		Mechanism,
		sEncoded,
		iEncoded,
		pConfig->InitialResponse,
		&Reply,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	if ( bSuccess && (Reply.Code == 334) ) {
		bSuccess = xrtSmtpClientSend(
			pClient,
			pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
				XRT_STR_LITERAL("") : XRT_STR_LITERAL("AQ=="),
			iDeadline,
			pCancel
		) && xrtSmtpClientReceive(
			pClient,
			&Reply,
			iDeadline,
			pCancel
		);
	}
	return bSuccess && __xrtSmtpAuthResult(&Reply);
}



/* 初始化 SMTP 认证配置。 */
XRT_API void xrtSmtpAuthConfigInit(xsmtpauthconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XSMTP_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 SMTP 认证配置。 */
XRT_API bool xrtSmtpAuthConfigValid(const xsmtpauthconfig* pConfig)
{
	bool bBearer;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XSMTP_AUTH_PLAIN) ||
		(pConfig->Method > XSMTP_AUTH_OAUTHBEARER) ) {
		return __xrtSmtpAuthError(
			XERR_ARGUMENT,
			"invalid SMTP authentication method"
		);
	}
	bBearer = (pConfig->Method == XSMTP_AUTH_XOAUTH2) ||
		(pConfig->Method == XSMTP_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XSMTP_AUTH_PLAIN) &&
		 (pConfig->Method != XSMTP_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtSmtpAuthError(
			XERR_ARGUMENT,
			"invalid SMTP authentication credentials"
		);
	}
	return true;
}



/* 完成一次 SMTP 认证。 */
XRT_API bool xrtSmtpClientAuth(
	xsmtpclient* pClient,
	const xsmtpauthconfig* pConfig,
	xdeadline iDeadline,
	xcancel* pCancel
)
{
	uint64 iCapability;
	bool bSuccess;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtSmtpAuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtSmtpClientState(pClient) != XSMTP_CLIENT_READY ) {
		return __xrtSmtpAuthError(
			XERR_STATE,
			"SMTP authentication requires READY state"
		);
	}
	if ( xrtSmtpClientAuthenticated(pClient) ) {
		return __xrtSmtpAuthError(
			XERR_STATE,
			"SMTP session is already authenticated"
		);
	}
	if ( (pConfig->Method == XSMTP_AUTH_OAUTHBEARER) &&
		(xrtSmtpClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtSmtpAuthError(
			XERR_PERMISSION,
			"SMTP OAUTHBEARER requires TLS"
		);
	}
	if ( (xrtSmtpClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtSmtpAuthError(
			XERR_PERMISSION,
			"SMTP credentials require TLS or explicit plaintext opt-in"
		);
	}
	iCapability = pConfig->Method == XSMTP_AUTH_PLAIN ?
		XSMTP_CAP_AUTH_PLAIN :
		(pConfig->Method == XSMTP_AUTH_LOGIN ? XSMTP_CAP_AUTH_LOGIN :
		 (pConfig->Method == XSMTP_AUTH_XOAUTH2 ?
		  XSMTP_CAP_AUTH_XOAUTH2 : XSMTP_CAP_AUTH_OAUTHBEARER));
	if ( (xrtSmtpClientCapabilities(pClient) & iCapability) == 0 ) {
		return __xrtSmtpAuthError(
			XERR_UNSUPPORTED,
			"SMTP server did not advertise the authentication mechanism"
		);
	}
	if ( pConfig->Method == XSMTP_AUTH_PLAIN ) {
		bSuccess = __xrtSmtpAuthPlain(pClient, pConfig, iDeadline, pCancel);
	} else if ( pConfig->Method == XSMTP_AUTH_LOGIN ) {
		bSuccess = __xrtSmtpAuthLogin(pClient, pConfig, iDeadline, pCancel);
	} else {
		bSuccess = __xrtSmtpAuthBearer(pClient, pConfig, iDeadline, pCancel);
	}
	if ( bSuccess ) {
		__xrtSmtpClientAuthComplete(pClient);
	}
	return bSuccess;
}

#endif
#endif

#endif
