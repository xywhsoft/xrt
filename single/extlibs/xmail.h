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
#error "xmail requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XMAIL_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XMAIL_IMPLEMENTATION) && \
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
#ifndef XMAIL_SINGLE_HEADER_H
#define XMAIL_SINGLE_HEADER_H
#define XMAIL_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/xmail/include/xmail/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XMAIL_FEATURES_H
#define XMAIL_FEATURES_H

/* xmail 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_XMAIL)
#ifndef XMAIL_FEATURE_MAIL
#define XMAIL_FEATURE_MAIL
#endif
#ifndef XMAIL_MODULE_MAIL_CHARSET
#define XMAIL_MODULE_MAIL_CHARSET
#endif
#ifndef XMAIL_MODULE_MAIL_CODEC
#define XMAIL_MODULE_MAIL_CODEC
#endif
#ifndef XMAIL_MODULE_MAIL_WORD
#define XMAIL_MODULE_MAIL_WORD
#endif
#ifndef XMAIL_MODULE_MAIL_ADDRESS
#define XMAIL_MODULE_MAIL_ADDRESS
#endif
#ifndef XMAIL_MODULE_MAIL_DATE
#define XMAIL_MODULE_MAIL_DATE
#endif
#ifndef XMAIL_MODULE_MAIL_ID
#define XMAIL_MODULE_MAIL_ID
#endif
#ifndef XMAIL_MODULE_MAIL_PARAM
#define XMAIL_MODULE_MAIL_PARAM
#endif
#ifndef XMAIL_MODULE_MAIL_MULTIPART
#define XMAIL_MODULE_MAIL_MULTIPART
#endif
#ifndef XMAIL_MODULE_MAIL_MESSAGE
#define XMAIL_MODULE_MAIL_MESSAGE
#endif
#ifndef XMAIL_MODULE_MAIL_TREE
#define XMAIL_MODULE_MAIL_TREE
#endif
#ifndef XMAIL_MODULE_MAIL_BUILD
#define XMAIL_MODULE_MAIL_BUILD
#endif
#ifndef XMAIL_MODULE_MAIL_COMPOSE
#define XMAIL_MODULE_MAIL_COMPOSE
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#ifndef XMAIL_MODULE_MAIL_NET_DEFLATE
#define XMAIL_MODULE_MAIL_NET_DEFLATE
#endif
#endif

/* mail_net_deflate 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_NET_DEFLATE)
#ifndef XMAIL_FEATURE_MAIL_NET_DEFLATE
#define XMAIL_FEATURE_MAIL_NET_DEFLATE
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#ifndef XRT_MODULE_DEFLATE
#define XRT_MODULE_DEFLATE
#endif
#ifndef XRT_MODULE_INFLATE
#define XRT_MODULE_INFLATE
#endif
#endif

/* mail_net_tls 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_NET_TLS)
#ifndef XMAIL_FEATURE_MAIL_NET_TLS
#define XMAIL_FEATURE_MAIL_NET_TLS
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#ifndef XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#define XRT_MODULE_TLS_STREAM_DIAL_FUTURE
#endif
#ifndef XRT_MODULE_TLS_STREAM_FUTURE
#define XRT_MODULE_TLS_STREAM_FUTURE
#endif
#ifndef XRT_MODULE_TLS_CLIENT_VERIFY
#define XRT_MODULE_TLS_CLIENT_VERIFY
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA256
#define XRT_MODULE_TLS_SCHEDULE_SHA256
#endif
#ifndef XRT_MODULE_TLS_SCHEDULE_SHA384
#define XRT_MODULE_TLS_SCHEDULE_SHA384
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#define XRT_MODULE_TLS_KEY_EXCHANGE_X25519
#endif
#ifndef XRT_MODULE_TLS_KEY_EXCHANGE_P256
#define XRT_MODULE_TLS_KEY_EXCHANGE_P256
#endif
#ifndef XRT_MODULE_TLS_RECORD_AES
#define XRT_MODULE_TLS_RECORD_AES
#endif
#ifndef XRT_MODULE_TLS_RECORD_CHACHA
#define XRT_MODULE_TLS_RECORD_CHACHA
#endif
#endif

/* mail_net 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_NET)
#ifndef XMAIL_FEATURE_MAIL_NET
#define XMAIL_FEATURE_MAIL_NET
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#ifndef XRT_MODULE_NET_TCP_DIAL_SYNC
#define XRT_MODULE_NET_TCP_DIAL_SYNC
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* mail_compose 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_COMPOSE)
#ifndef XMAIL_FEATURE_MAIL_COMPOSE
#define XMAIL_FEATURE_MAIL_COMPOSE
#endif
#ifndef XMAIL_MODULE_MAIL_BUILD
#define XMAIL_MODULE_MAIL_BUILD
#endif
#ifndef XMAIL_MODULE_MAIL_CODEC
#define XMAIL_MODULE_MAIL_CODEC
#endif
#ifndef XMAIL_MODULE_MAIL_DATE
#define XMAIL_MODULE_MAIL_DATE
#endif
#ifndef XMAIL_MODULE_MAIL_ID
#define XMAIL_MODULE_MAIL_ID
#endif
#ifndef XMAIL_MODULE_MAIL_PARAM
#define XMAIL_MODULE_MAIL_PARAM
#endif
#endif

/* mail_build 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_BUILD)
#ifndef XMAIL_FEATURE_MAIL_BUILD
#define XMAIL_FEATURE_MAIL_BUILD
#endif
#ifndef XMAIL_MODULE_MAIL_ADDRESS
#define XMAIL_MODULE_MAIL_ADDRESS
#endif
#ifndef XMAIL_MODULE_MAIL_HEADER
#define XMAIL_MODULE_MAIL_HEADER
#endif
#ifndef XMAIL_MODULE_MAIL_MULTIPART
#define XMAIL_MODULE_MAIL_MULTIPART
#endif
#endif

/* mail_tree 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_TREE)
#ifndef XMAIL_FEATURE_MAIL_TREE
#define XMAIL_FEATURE_MAIL_TREE
#endif
#ifndef XMAIL_MODULE_MAIL_MESSAGE
#define XMAIL_MODULE_MAIL_MESSAGE
#endif
#ifndef XMAIL_MODULE_MAIL_MULTIPART
#define XMAIL_MODULE_MAIL_MULTIPART
#endif
#ifndef XMAIL_MODULE_MAIL_PARAM
#define XMAIL_MODULE_MAIL_PARAM
#endif
#ifndef XMAIL_MODULE_MAIL_CHARSET
#define XMAIL_MODULE_MAIL_CHARSET
#endif
#endif

/* mail_message 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_MESSAGE)
#ifndef XMAIL_FEATURE_MAIL_MESSAGE
#define XMAIL_FEATURE_MAIL_MESSAGE
#endif
#ifndef XMAIL_MODULE_MAIL_CODEC
#define XMAIL_MODULE_MAIL_CODEC
#endif
#ifndef XMAIL_MODULE_MAIL_HEADER
#define XMAIL_MODULE_MAIL_HEADER
#endif
#endif

/* mail_wire 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_WIRE)
#ifndef XMAIL_FEATURE_MAIL_WIRE
#define XMAIL_FEATURE_MAIL_WIRE
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#endif

/* mail_multipart 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_MULTIPART)
#ifndef XMAIL_FEATURE_MAIL_MULTIPART
#define XMAIL_FEATURE_MAIL_MULTIPART
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XMAIL_MODULE_MAIL_HEADER
#define XMAIL_MODULE_MAIL_HEADER
#endif
#endif

/* mail_param 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_PARAM)
#ifndef XMAIL_FEATURE_MAIL_PARAM
#define XMAIL_FEATURE_MAIL_PARAM
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* mail_id 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_ID)
#ifndef XMAIL_FEATURE_MAIL_ID
#define XMAIL_FEATURE_MAIL_ID
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XRT_MODULE_RANDOM_SECURE
#define XRT_MODULE_RANDOM_SECURE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* mail_date 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_DATE)
#ifndef XMAIL_FEATURE_MAIL_DATE
#define XMAIL_FEATURE_MAIL_DATE
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XRT_MODULE_TIME_TEXT
#define XRT_MODULE_TIME_TEXT
#endif
#endif

/* mail_address 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_ADDRESS)
#ifndef XMAIL_FEATURE_MAIL_ADDRESS
#define XMAIL_FEATURE_MAIL_ADDRESS
#endif
#ifndef XMAIL_MODULE_MAIL_WORD
#define XMAIL_MODULE_MAIL_WORD
#endif
#endif

/* mail_word 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_WORD)
#ifndef XMAIL_FEATURE_MAIL_WORD
#define XMAIL_FEATURE_MAIL_WORD
#endif
#ifndef XMAIL_MODULE_MAIL_CODEC
#define XMAIL_MODULE_MAIL_CODEC
#endif
#ifndef XMAIL_MODULE_MAIL_CHARSET
#define XMAIL_MODULE_MAIL_CHARSET
#endif
#endif

/* mail_header 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_HEADER)
#ifndef XMAIL_FEATURE_MAIL_HEADER
#define XMAIL_FEATURE_MAIL_HEADER
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#endif

/* mail_codec 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_CODEC)
#ifndef XMAIL_FEATURE_MAIL_CODEC
#define XMAIL_FEATURE_MAIL_CODEC
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* mail_charset 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_CHARSET)
#ifndef XMAIL_FEATURE_MAIL_CHARSET
#define XMAIL_FEATURE_MAIL_CHARSET
#endif
#ifndef XMAIL_MODULE_MAIL_CORE
#define XMAIL_MODULE_MAIL_CORE
#endif
#ifndef XRT_MODULE_UNICODE
#define XRT_MODULE_UNICODE
#endif
#endif

/* mail_core 及其直接依赖。 */
#if defined(XMAIL_MODULE_ALL) || defined(XMAIL_MODULE_MAIL_CORE)
#ifndef XMAIL_FEATURE_MAIL_CORE
#define XMAIL_FEATURE_MAIL_CORE
#endif
#endif

#endif /* XMAIL_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail.h */
/* ========================================================================== */

#ifndef XRT_MAIL_H
#define XRT_MAIL_H




#if (defined(XMAIL_FEATURE_MAIL_CODEC) || \
	 defined(XMAIL_FEATURE_MAIL_CHARSET) || defined(XMAIL_FEATURE_MAIL_HEADER) || \
	 defined(XMAIL_FEATURE_MAIL_WORD) || defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	 defined(XMAIL_FEATURE_MAIL_DATE) || defined(XMAIL_FEATURE_MAIL_ID) || \
	 defined(XMAIL_FEATURE_MAIL_PARAM) || defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	 defined(XMAIL_FEATURE_MAIL_MESSAGE) || defined(XMAIL_FEATURE_MAIL_TREE) || \
	 defined(XMAIL_FEATURE_MAIL_BUILD) || \
	 defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	 defined(XMAIL_FEATURE_MAIL_WIRE) || \
	 defined(XMAIL_FEATURE_SMTP) || defined(XMAIL_FEATURE_POP3) || \
	 defined(XMAIL_FEATURE_IMAP) || defined(XMAIL_FEATURE_IMAP_DATA) || \
	 defined(XMAIL_FEATURE_IMAP_BODY) || \
	 defined(XMAIL_FEATURE_MAIL_NET) || \
	 defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	 defined(XMAIL_FEATURE_SMTP_CLIENT) || \
	 defined(XMAIL_FEATURE_SMTP_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_SMTP_AUTH) || \
	 defined(XMAIL_FEATURE_SMTP_SUBMIT) || \
	 defined(XMAIL_FEATURE_POP3_CLIENT) || \
	 defined(XMAIL_FEATURE_POP3_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_POP3_AUTH) || \
	 defined(XMAIL_FEATURE_POP3_MESSAGE) || \
	 defined(XMAIL_FEATURE_IMAP_CLIENT) || \
	 defined(XMAIL_FEATURE_IMAP_CLIENT_TLS) || \
	 defined(XMAIL_FEATURE_IMAP_AUTH) || \
	 defined(XMAIL_FEATURE_IMAP_COMMAND) || \
	 defined(XMAIL_FEATURE_IMAP_MESSAGE) || \
	 defined(XMAIL_FEATURE_IMAP_APPEND) || \
	 defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
	 defined(XMAIL_FEATURE_IMAP_COMPRESS)) && \
	!defined(XMAIL_FEATURE_MAIL_CORE)
	#error "xmail content features require XMAIL_FEATURE_MAIL_CORE"
#endif



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 邮件正文编码和字段折叠使用的标准行宽。 */
#define XMAIL_QP_LINE_DEFAULT 76u
#define XMAIL_BASE64_LINE_DEFAULT 76u
#define XMAIL_HEADER_LINE_DEFAULT 78u
#define XMAIL_HEADER_LINE_HARD 998u
#define XMAIL_BOUNDARY_MAX 70u



/* 流式邮件解析统一使用三态结果。 */
typedef enum xmailnext {
	XMAIL_NEXT_ERROR = -1,
	XMAIL_NEXT_END = 0,
	XMAIL_NEXT_ITEM = 1
} xmailnext;



/* 邮件内容流统一使用只借用当前片段的同步 sink。 */
typedef bool (*xmailwriteproc)(xbytesview Data, ptr pUserData);



/* 邮件扩展在 xrt.mail 错误域内使用稳定代码。 */
typedef enum xmailerror {
	XMAIL_ERROR_CONFIG = 1601,
	XMAIL_ERROR_LINE,
	XMAIL_ERROR_HEADER,
	XMAIL_ERROR_ENCODING,
	XMAIL_ERROR_CHARSET,
	XMAIL_ERROR_ADDRESS,
	XMAIL_ERROR_MIME,
	XMAIL_ERROR_PROTOCOL,
	XMAIL_ERROR_LIMIT,
	XMAIL_ERROR_AUTH,
	XMAIL_ERROR_CALLBACK
} xmailerror;



XRT_EXTERN_C_BEGIN



/*
	把 LF、CRLF 和独立 CR 统一写成 CRLF。
	查询模式使用空输出和零容量；实际写入要求容量包含末尾零字节。
*/
XRT_API bool xrtMailCrlfWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 CRLF 规范文本。 */
XRT_API str xrtMailCrlf(xstrview Text, size_t* pOutputSize);



/* 判断文本是否可以直接作为 MIME boundary 参数值使用。 */
XRT_API bool xrtMailBoundaryValid(xstrview Boundary);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_charset.h */
/* ========================================================================== */

#ifndef XRT_MAIL_CHARSET_H
#define XRT_MAIL_CHARSET_H




#if defined(XMAIL_FEATURE_MAIL_CHARSET) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_CHARSET requires XRT Unicode features"
#endif



#if defined(XMAIL_FEATURE_MAIL_CHARSET)

XRT_EXTERN_C_BEGIN



/* 判断字符集名称是否属于内置的小型转换集合。 */
XRT_API bool xrtMailCharsetSupported(xstrview Charset);



/*
	把 UTF-8、ASCII、Latin-1 或 Windows-1252 字节转换成 UTF-8。
	查询模式使用空输出和零容量，实际容量必须包含末尾零字节。
*/
XRT_API bool xrtMailCharsetToUtf8Write(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 UTF-8 文本。 */
XRT_API str xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_codec.h */
/* ========================================================================== */

#ifndef XRT_MAIL_CODEC_H
#define XRT_MAIL_CODEC_H




#if defined(XMAIL_FEATURE_MAIL_CODEC) && !defined(XRT_FEATURE_CODEC_BASE64)
	#error "XMAIL_FEATURE_MAIL_CODEC requires XRT_FEATURE_CODEC_BASE64"
#endif



#if defined(XMAIL_FEATURE_MAIL_CODEC)

/* Quoted-Printable 文本模式会把所有输入换行规范为 CRLF。 */
typedef enum xmailqpflag {
	XMAIL_QP_BINARY = 0,
	XMAIL_QP_TEXT = UINT32_C(0x00000001),
	XMAIL_QP_RELAXED_SOFT_BREAK = UINT32_C(0x00000002)
} xmailqpflag;



XRT_EXTERN_C_BEGIN



/*
	按 RFC 2045 写出 Quoted-Printable；零行宽使用 76。
	文本结果要求输出容量包含末尾零字节。
*/
XRT_API bool xrtMailQpWrite(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 Quoted-Printable 文本。 */
XRT_API str xrtMailQp(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	size_t* pOutputSize
);



/* 严格解码 Quoted-Printable，输出允许与输入从同一地址开始。 */
XRT_API bool xrtMailQpDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节；额外末尾零不计入长度。 */
XRT_API bytes xrtMailQpDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
);



/*
	按 MIME 行宽写出标准 Base64；行宽必须是不超过 76 的四的倍数。
	非空结果以 CRLF 结束，输出容量必须包含末尾零字节。
*/
XRT_API bool xrtMailBase64Write(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 MIME Base64 文本。 */
XRT_API str xrtMailBase64(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
);



/* 忽略 MIME 空白并严格解码 Base64，输出允许与输入同址。 */
XRT_API bool xrtMailBase64DecodeWrite(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码并返回由 xrtFree 释放的字节；额外末尾零不计入长度。 */
XRT_API bytes xrtMailBase64Decode(xstrview Text, size_t* pOutputSize);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_header.h */
/* ========================================================================== */

#ifndef XRT_MAIL_HEADER_H
#define XRT_MAIL_HEADER_H




#if defined(XMAIL_FEATURE_MAIL_HEADER)

/* 字段名称和值均借用原始报文，不要求零结尾。 */
typedef struct xmailheaderview {
	xstrview Name;
	xstrview Value;
} xmailheaderview;



/* 字段游标只保存原报文视图和下一字段位置。 */
typedef struct xmailheadercursor {
	xstrview Block;
	size_t Position;
	bool Done;
} xmailheadercursor;



XRT_EXTERN_C_BEGIN



/* 判断字段名是否只包含 RFC 5322 ftext 字节。 */
XRT_API bool xrtMailHeaderNameValid(xstrview Name);



/* 判断字段值是否只包含安全正文和合法 CRLF 折叠。 */
XRT_API bool xrtMailHeaderValueValid(xstrview Value);



/* 初始化零分配字段游标；Block 可以包含或省略末尾空行。 */
XRT_API bool xrtMailHeaderCursorInit(
	xmailheadercursor* pCursor,
	xstrview Block
);



/* 返回下一个字段的借用名称和原始折叠值。 */
XRT_API xmailnext xrtMailHeaderNext(
	xmailheadercursor* pCursor,
	xmailheaderview* pHeader
);



/* 展开一个字段值中的 CRLF + WSP，输出容量必须包含末尾零字节。 */
XRT_API bool xrtMailHeaderUnfoldWrite(
	xstrview Value,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的展开字段值。 */
XRT_API str xrtMailHeaderUnfold(xstrview Value, size_t* pOutputSize);



/*
	写出 `Name: Value\r\n` 并尽量在空白处折叠。
	零行宽使用 78，任何物理行都不会超过 998 字节。
*/
XRT_API bool xrtMailHeaderWrite(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的完整字段行。 */
XRT_API str xrtMailHeader(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_word.h */
/* ========================================================================== */

#ifndef XRT_MAIL_WORD_H
#define XRT_MAIL_WORD_H




#if defined(XMAIL_FEATURE_MAIL_WORD) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || \
	 !defined(XMAIL_FEATURE_MAIL_CHARSET))
	#error "XMAIL_FEATURE_MAIL_WORD requires mail codec and mail charset"
#endif



#if defined(XMAIL_FEATURE_MAIL_WORD)

/* RFC 2047 编码词支持 Base64 与适用于短文本的 Q 编码。 */
typedef enum xmailwordencoding {
	XMAIL_WORD_BASE64 = 0,
	XMAIL_WORD_Q
} xmailwordencoding;



/* 容错解码保留无法识别或无法解码的原始编码词。 */
typedef enum xmailwordflag {
	XMAIL_WORD_STRICT = 0,
	XMAIL_WORD_RELAXED = UINT32_C(0x00000001)
} xmailwordflag;



/* 编码词视图全部借用输入，Language 是 RFC 2231 可选语言子标签。 */
typedef struct xmailwordview {
	xstrview Source;
	xstrview Charset;
	xstrview Language;
	xstrview Encoded;
	xmailwordencoding Encoding;
} xmailwordview;



XRT_EXTERN_C_BEGIN



/*
	从输入开头读取一个 RFC 2047 编码词及 RFC 2231 语言子标签。
	非编码词返回 XMAIL_NEXT_END，具有编码词前缀但格式错误时返回错误。
*/
XRT_API xmailnext xrtMailWordParse(xstrview Text, xmailwordview* pWord);



/*
	把 UTF-8 字段文本写成 RFC 2047 编码词序列；纯安全 ASCII 原样返回。
	查询模式使用空输出和零容量，实际输出容量必须包含末尾零字节。
*/
XRT_API bool xrtMailWordEncodeWrite(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 RFC 2047 字段文本。 */
XRT_API str xrtMailWordEncode(
	xstrview Text,
	xmailwordencoding Encoding,
	size_t* pOutputSize
);



/*
	把混合普通文本与编码词的字段值解码为 UTF-8。
	相邻编码词之间的线性空白会被忽略，输出允许与输入从同一地址开始。
*/
XRT_API bool xrtMailWordDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 UTF-8 解码文本。 */
XRT_API str xrtMailWordDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_address.h */
/* ========================================================================== */

#ifndef XRT_MAIL_ADDRESS_H
#define XRT_MAIL_ADDRESS_H




#if defined(XMAIL_FEATURE_MAIL_ADDRESS) && !defined(XMAIL_FEATURE_MAIL_WORD)
	#error "XMAIL_FEATURE_MAIL_ADDRESS requires XMAIL_FEATURE_MAIL_WORD"
#endif



#if defined(XMAIL_FEATURE_MAIL_ADDRESS)

/* 注释采用迭代扫描，但仍限制恶意输入的嵌套深度。 */
#define XMAIL_ADDRESS_COMMENT_DEPTH 16u



/* 地址列表游标显式报告组边界，避免丢失列表结构。 */
typedef enum xmailaddresskind {
	XMAIL_ADDRESS_MAILBOX = 1,
	XMAIL_ADDRESS_GROUP_BEGIN,
	XMAIL_ADDRESS_GROUP_END
} xmailaddresskind;



/* 默认只接受 ASCII addr-spec；SMTPUTF8 必须由调用方显式开启。 */
typedef enum xmailaddressflag {
	XMAIL_ADDRESS_DEFAULT = 0,
	XMAIL_ADDRESS_SMTPUTF8 = UINT32_C(0x00000001)
} xmailaddressflag;



/* 所有字段均借用原始列表；Name 保留引号、注释或编码词形式。 */
typedef struct xmailaddressview {
	xmailaddresskind Kind;
	xstrview Source;
	xstrview Name;
	xstrview Address;
	xstrview Local;
	xstrview Domain;
} xmailaddressview;



/* 常用地址列表项借用显示名和 addr-spec，供构建接口直接批量写出。 */
typedef struct xmailaddress {
	xstrview Name;
	xstrview Address;
} xmailaddress;



/* 地址列表游标可跨 group 边界增量推进，不持有堆内存。 */
typedef struct xmailaddresscursor {
	xstrview Text;
	size_t Position;
	uint32 Flags;
	bool InGroup;
	bool Done;
} xmailaddresscursor;



XRT_EXTERN_C_BEGIN



/* 初始化严格地址列表游标。 */
XRT_API bool xrtMailAddressCursorInit(
	xmailaddresscursor* pCursor,
	xstrview Text,
	uint32 iFlags
);



/* 返回下一个 mailbox、group begin 或 group end 借用视图。 */
XRT_API xmailnext xrtMailAddressNext(
	xmailaddresscursor* pCursor,
	xmailaddressview* pAddress
);



/* 验证一个完整 addr-spec，可选返回拆分后的 local-part 与 domain。 */
XRT_API bool xrtMailAddressValid(
	xstrview Address,
	uint32 iFlags,
	xstrview* pLocal,
	xstrview* pDomain
);



/*
	写出常用的 display-name <addr-spec>；空显示名只写 addr-spec。
	非 ASCII 显示名使用指定的 RFC 2047 编码，实际容量必须包含末尾零。
*/
XRT_API bool xrtMailAddressWrite(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的规范 mailbox 文本。 */
XRT_API str xrtMailAddress(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
);



/* 写出逗号和空格分隔的规范 mailbox 列表；空数组写出空文本。 */
XRT_API bool xrtMailAddressListWrite(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的规范 mailbox 列表。 */
XRT_API str xrtMailAddressList(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_date.h */
/* ========================================================================== */

#ifndef XRT_MAIL_DATE_H
#define XRT_MAIL_DATE_H




#if defined(XMAIL_FEATURE_MAIL_DATE) && !defined(XRT_FEATURE_TIME_TEXT)
	#error "XMAIL_FEATURE_MAIL_DATE requires XRT_FEATURE_TIME_TEXT"
#endif



#if defined(XMAIL_FEATURE_MAIL_DATE)

/* 默认严格解析；兼容模式额外接受过时年份和命名时区。 */
typedef enum xmaildateflag {
	XMAIL_DATE_STRICT = 0,
	XMAIL_DATE_RELAXED = UINT32_C(0x00000001)
} xmaildateflag;



XRT_EXTERN_C_BEGIN



/* 按 RFC 5322 规范形式写入邮件日期；UTC 偏移必须精确到分钟。 */
XRT_API bool xrtMailDateWrite(
	xtime iTime,
	int iOffset,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 RFC 5322 规范日期。 */
XRT_API str xrtMailDate(xtime iTime, int iOffset, size_t* pOutputSize);



/* 解析 RFC 5322 日期；星期和秒可以省略，输入必须已展开字段折行。 */
XRT_API bool xrtMailDateParse(
	xstrview Text,
	uint32 iFlags,
	xtime* pTime,
	int* pOffset
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_id.h */
/* ========================================================================== */

#ifndef XRT_MAIL_ID_H
#define XRT_MAIL_ID_H




#if defined(XMAIL_FEATURE_MAIL_ID) && !defined(XRT_FEATURE_RANDOM_SECURE)
	#error "XMAIL_FEATURE_MAIL_ID requires XRT_FEATURE_RANDOM_SECURE"
#endif

#if defined(XMAIL_FEATURE_MAIL_ID) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_ID requires XRT_FEATURE_UNICODE"
#endif



#if defined(XMAIL_FEATURE_MAIL_ID)

/* 默认只接受 ASCII；UTF-8 标识必须由调用方显式开启。 */
typedef enum xmailidflag {
	XMAIL_ID_DEFAULT = 0,
	XMAIL_ID_UTF8 = UINT32_C(0x00000001)
} xmailidflag;



/* Message-ID 视图借用输入，并保留尖括号内左右两部分。 */
typedef struct xmailmessageidview {
	xstrview Source;
	xstrview Left;
	xstrview Right;
} xmailmessageidview;



XRT_EXTERN_C_BEGIN



/* 解析一个完整 Message-ID，不接受过时的 quoted id-left 语法。 */
XRT_API bool xrtMailMessageIdParse(
	xstrview Text,
	uint32 iFlags,
	xmailmessageidview* pMessageId
);



/* 使用安全随机源和给定 id-right 写入一个全新 Message-ID。 */
XRT_API bool xrtMailMessageIdWrite(
	xstrview Right,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的全新 Message-ID。 */
XRT_API str xrtMailMessageId(xstrview Right, size_t* pOutputSize);



/* 使用安全随机源写入不带引号的 MIME boundary。 */
XRT_API bool xrtMailBoundaryWrite(
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的全新 MIME boundary。 */
XRT_API str xrtMailBoundary(size_t* pOutputSize);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_param.h */
/* ========================================================================== */

#ifndef XRT_MAIL_PARAM_H
#define XRT_MAIL_PARAM_H




#if defined(XMAIL_FEATURE_MAIL_PARAM) && !defined(XRT_FEATURE_UNICODE)
	#error "XMAIL_FEATURE_MAIL_PARAM requires XRT_FEATURE_UNICODE"
#endif



#if defined(XMAIL_FEATURE_MAIL_PARAM)

/* 非连续参数使用该值表示没有 section 编号。 */
#define XMAIL_PARAM_SECTION_NONE XRT_NPOS
#define XMAIL_PARAM_SECTIONS_MAX 64u
#define XMAIL_PARAM_SECTION_SIZE 60u



/* MIME type/subtype 和后续参数均借用原字段值。 */
typedef struct xmailmediatypeview {
	xstrview Source;
	xstrview Type;
	xstrview Subtype;
	xstrview Parameters;
} xmailmediatypeview;



/* Content-Disposition 主 token 和后续参数均借用原字段值。 */
typedef struct xmaildispositionview {
	xstrview Source;
	xstrview Type;
	xstrview Parameters;
} xmaildispositionview;



/* 参数视图同时暴露原始形式、基础名称、值和 RFC 2231 section 信息。 */
typedef struct xmailparamview {
	xstrview Source;
	xstrview RawName;
	xstrview Name;
	xstrview RawValue;
	xstrview Value;
	size_t Section;
	bool Extended;
	bool Continued;
	bool Quoted;
} xmailparamview;



/* 参数游标只持有借用文本和下一参数位置。 */
typedef struct xmailparamcursor {
	xstrview Text;
	size_t Position;
	bool Done;
} xmailparamcursor;



/* 合并参数元数据中的字符集和语言视图借用原字段值。 */
typedef struct xmailparaminfo {
	xstrview Charset;
	xstrview Language;
	size_t Sections;
	bool Extended;
	bool Continued;
} xmailparaminfo;



/* AUTO 选择 token、quoted-string 或 UTF-8 扩展参数的最短合法形式。 */
typedef enum xmailparamencoding {
	XMAIL_PARAM_ENCODING_AUTO = 0,
	XMAIL_PARAM_ENCODING_TOKEN,
	XMAIL_PARAM_ENCODING_QUOTED,
	XMAIL_PARAM_ENCODING_UTF8
} xmailparamencoding;



XRT_EXTERN_C_BEGIN



/* 解析并完整验证 Content-Type 字段值。 */
XRT_API bool xrtMailMediaTypeParse(
	xstrview Text,
	xmailmediatypeview* pMediaType
);



/* 解析并完整验证 Content-Disposition 字段值。 */
XRT_API bool xrtMailDispositionParse(
	xstrview Text,
	xmaildispositionview* pDisposition
);



/* 初始化以分号开头或为空的 MIME 参数游标。 */
XRT_API bool xrtMailParamCursorInit(
	xmailparamcursor* pCursor,
	xstrview Parameters
);



/* 返回下一个 MIME 参数及其 RFC 2231 section 描述。 */
XRT_API xmailnext xrtMailParamNext(
	xmailparamcursor* pCursor,
	xmailparamview* pParameter
);



/* 解码单个参数 section 的 quoted-pair 和扩展百分号编码。 */
XRT_API bool xrtMailParamDecodeWrite(
	const xmailparamview* pParameter,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xstrview* pCharset,
	xstrview* pLanguage
);



/* 查找、合并并解码参数；返回 ITEM、END 或 ERROR。 */
XRT_API xmailnext xrtMailParamFindWrite(
	xstrview Parameters,
	xstrview Name,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
);



/* 创建由 xrtFree 释放的合并参数；未找到和错误均返回 NULL。 */
XRT_API str xrtMailParamFind(
	xstrview Parameters,
	xstrview Name,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
);



/* 写出包含分号前缀的 MIME 参数，长值自动拆成 RFC 2231 连续段。 */
XRT_API bool xrtMailParamWrite(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的单个 MIME 参数。 */
XRT_API str xrtMailParam(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_multipart.h */
/* ========================================================================== */

#ifndef XRT_MAIL_MULTIPART_H
#define XRT_MAIL_MULTIPART_H




#if defined(XMAIL_FEATURE_MAIL_MULTIPART) && !defined(XMAIL_FEATURE_MAIL_HEADER)
	#error "XMAIL_FEATURE_MAIL_MULTIPART requires XMAIL_FEATURE_MAIL_HEADER"
#endif



#if defined(XMAIL_FEATURE_MAIL_MULTIPART)

/* 零值使用库默认限制；SIZE_MAX 明确表示不限制 part 数量。 */
#define XMAIL_MULTIPART_PARTS_DEFAULT 1024u



/* multipart 构建标记直接对应第一段、后续段和关闭分隔线。 */
typedef enum xmailmultipartmark {
	XMAIL_MULTIPART_FIRST = 1,
	XMAIL_MULTIPART_NEXT,
	XMAIL_MULTIPART_CLOSE
} xmailmultipartmark;



/* part 视图借用原始正文，不复制字段或正文。 */
typedef struct xmailmultipartview {
	xstrview Source;
	xstrview Headers;
	xstrview Body;
} xmailmultipartview;



/* multipart 游标公开 preamble/epilogue，便于底层调用方保留完整语义。 */
typedef struct xmailmultipartcursor {
	xstrview Source;
	xstrview Boundary;
	xstrview Preamble;
	xstrview Epilogue;
	size_t Position;
	size_t Parts;
	size_t MaxParts;
	bool Closed;
	bool Done;
} xmailmultipartcursor;



XRT_EXTERN_C_BEGIN



/* 初始化严格 CRLF、严格行首 boundary 的零分配 multipart 游标。 */
XRT_API bool xrtMailMultipartCursorInit(
	xmailmultipartcursor* pCursor,
	xstrview Body,
	xstrview Boundary,
	size_t iMaxParts
);



/* 返回下一 part 的字段块和正文；关闭分隔线缺失时返回 ERROR。 */
XRT_API xmailnext xrtMailMultipartNext(
	xmailmultipartcursor* pCursor,
	xmailmultipartview* pPart
);



/* 写入可直接与字段和正文拼接发送的 multipart 分隔片段。 */
XRT_API bool xrtMailMultipartMarkWrite(
	xstrview Boundary,
	xmailmultipartmark Mark,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_wire.h */
/* ========================================================================== */

#ifndef XRT_MAIL_WIRE_H
#define XRT_MAIL_WIRE_H




#if defined(XMAIL_FEATURE_MAIL_WIRE)

/* 零行限制使用 64 KiB；SIZE_MAX 明确表示不限制。 */
#define XMAIL_WIRE_LINE_DEFAULT (64u * 1024u)



/* 增量 dot writer 保留跨片段 CRLF 和行首状态，不持有输入。 */
typedef struct xmaildotwriter {
	bool LineStart;
	bool PendingCr;
	bool Finished;
} xmaildotwriter;



XRT_EXTERN_C_BEGIN



/*
	从增量输入读取一条严格 CRLF 行；ITEM 返回不含 CRLF 的借用视图，
	END 表示数据尚不完整，ERROR 表示裸换行或超限。
*/
XRT_API xmailnext xrtMailLineRead(
	xstrview Data,
	size_t iMaxLine,
	xstrview* pLine,
	size_t* pConsumed
);



/* 对一条已去除 CRLF 的 dot-transparent 行去转义；单点终止行返回 END。 */
XRT_API xmailnext xrtMailDotLine(xstrview Line, xstrview* pData);



/* 初始化可接收任意分块边界的增量 dot writer。 */
XRT_API bool xrtMailDotWriterInit(xmaildotwriter* pWriter);



/* 校验严格 CRLF 并向 sink 写出一个 dot-transparent 输入片段。 */
XRT_API bool xrtMailDotWriterWrite(
	xmaildotwriter* pWriter,
	xbytesview Data,
	xmailwriteproc pWrite,
	ptr pUserData
);



/* 补足最后一行并写出 SMTP/POP3 点终止行。 */
XRT_API bool xrtMailDotWriterFinish(
	xmaildotwriter* pWriter,
	xmailwriteproc pWrite,
	ptr pUserData
);



/*
	按 SMTP/POP3 dot transparency 写出数据；Terminate 会补足 CRLF 并追加
	终止行。输入允许最后一行不完整，但其他换行必须是严格 CRLF。
*/
XRT_API bool xrtMailDotWrite(
	xstrview Data,
	bool Terminate,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 dot-transparent 字节。 */
XRT_API bytes xrtMailDot(
	xstrview Data,
	bool Terminate,
	size_t* pOutputSize
);



/* 解码完整的 dot-transparent 行块；可要求最后一行必须是终止行。 */
XRT_API bool xrtMailDotDecodeWrite(
	xstrview Data,
	bool RequireTerminator,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的去转义字节。 */
XRT_API bytes xrtMailDotDecode(
	xstrview Data,
	bool RequireTerminator,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_message.h */
/* ========================================================================== */

#ifndef XRT_MAIL_MESSAGE_H
#define XRT_MAIL_MESSAGE_H




#if defined(XMAIL_FEATURE_MAIL_MESSAGE) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || !defined(XMAIL_FEATURE_MAIL_HEADER))
	#error "XMAIL_FEATURE_MAIL_MESSAGE requires mail codec and mail header"
#endif



#if defined(XMAIL_FEATURE_MAIL_MESSAGE)

/* 零限制使用适合互联网邮件的保守默认预算。 */
#define XMAIL_MESSAGE_HEADER_BYTES_DEFAULT (256u * 1024u)
#define XMAIL_MESSAGE_HEADERS_DEFAULT 1024u



/* 正文传输编码保留 UNKNOWN，使上层可以决定兼容或拒绝未知扩展。 */
typedef enum xmailtransfer {
	XMAIL_TRANSFER_UNKNOWN = 0,
	XMAIL_TRANSFER_7BIT,
	XMAIL_TRANSFER_8BIT,
	XMAIL_TRANSFER_BINARY,
	XMAIL_TRANSFER_QUOTED_PRINTABLE,
	XMAIL_TRANSFER_BASE64
} xmailtransfer;



/* 消息视图完全借用输入报文；Headers 包含字段后的最后一个 CRLF。 */
typedef struct xmailmessageview {
	xstrview Source;
	xstrview Headers;
	xstrview Body;
	size_t HeaderCount;
} xmailmessageview;



XRT_EXTERN_C_BEGIN



/* 严格解析 RFC 消息的字段块与正文，并在发布结果前验证全部字段。 */
XRT_API bool xrtMailMessageParse(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t iMaxHeaders,
	xmailmessageview* pMessage
);



/* 按 ASCII 大小写不敏感名称查找第 N 个字段，返回三态结果。 */
XRT_API xmailnext xrtMailMessageHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	size_t iOccurrence,
	xmailheaderview* pHeader
);



/* 解析已展开或合法折叠的 Content-Transfer-Encoding 字段值。 */
XRT_API xmailtransfer xrtMailTransferParse(xstrview Value);



/* 返回消息正文的传输编码；字段缺失时按标准返回 7bit。 */
XRT_API bool xrtMailMessageTransfer(
	const xmailmessageview* pMessage,
	xmailtransfer* pTransfer
);



/* 按指定传输编码解码消息正文；原样编码允许同址写入。 */
XRT_API bool xrtMailMessageBodyWrite(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解码正文并返回由 xrtFree 释放的字节；末尾附加零但不计入长度。 */
XRT_API bytes xrtMailMessageBody(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_tree.h */
/* ========================================================================== */

#ifndef XRT_MAIL_TREE_H
#define XRT_MAIL_TREE_H




#if defined(XMAIL_FEATURE_MAIL_TREE) && \
	(!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
	 !defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	 !defined(XMAIL_FEATURE_MAIL_PARAM) || \
	 !defined(XMAIL_FEATURE_MAIL_CHARSET))
	#error "XMAIL_FEATURE_MAIL_TREE requires message, multipart, param and charset"
#endif



#if defined(XMAIL_FEATURE_MAIL_TREE)

/* 零限制使用适合不可信互联网邮件的保守预算。 */
#define XMAIL_TREE_DEPTH_DEFAULT 32u
#define XMAIL_TREE_DEPTH_MAX 128u
#define XMAIL_TREE_PARTS_DEFAULT 4096u
#define XMAIL_TREE_SOURCE_BYTES_DEFAULT (64u * 1024u * 1024u)
#define XMAIL_TREE_DECODED_BYTES_DEFAULT (128u * 1024u * 1024u)



/* 兼容标记必须显式启用，默认解析保持严格。 */
typedef enum xmailtreeflag {
	XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER = UINT32_C(0x00000001),
	XMAIL_TREE_RELAXED_QP = UINT32_C(0x00000002),
	XMAIL_TREE_ALLOW_UNKNOWN_CHARSET = UINT32_C(0x00000004)
} xmailtreeflag;



/* 所有限制作用于整棵树；字段限制作用于每一个 MIME entity。 */
typedef struct xmailtreelimits {
	size_t MaxDepth;
	size_t MaxParts;
	size_t MaxSourceBytes;
	size_t MaxDecodedBytes;
	size_t MaxHeaderBytes;
	size_t MaxHeaders;
	uint32 Flags;
} xmailtreelimits;



typedef struct xmailpart xmailpart;



/*
	MIME part 的全部视图由所属 xmailtree 持有。
	Data 是解码后的叶子正文；未知传输编码获准保留时 Decoded 为 false。
*/
struct xmailpart {
	xmailmessageview Message;
	xmailmediatypeview ContentType;
	xmaildispositionview Disposition;
	xstrview FileName;
	xstrview ContentId;
	xstrview Preamble;
	xstrview Epilogue;
	xbytesview Data;
	xmailpart* Children;
	size_t ChildCount;
	xmailtransfer Transfer;
	bool Attachment;
	bool Inline;
	bool Decoded;
	bool Embedded;
	bool FileNameUtf8;
};



/* Source、Root 及其所有后代统一由 Storage 持有。 */
typedef struct xmailtree {
	xstrview Source;
	xmailpart* Root;
	size_t PartCount;
	size_t DecodedBytes;
	ptr Storage;
} xmailtree;



XRT_EXTERN_C_BEGIN



/* 使用默认预算初始化 MIME 树限制。 */
XRT_API void xrtMailTreeLimitsInit(xmailtreelimits* pLimits);



/* 验证 MIME 树限制；零字段按默认预算解释。 */
XRT_API bool xrtMailTreeLimitsValid(const xmailtreelimits* pLimits);



/*
	复制并解析完整 RFC 消息；成功结果不再依赖输入缓冲。
	重复的 MIME singleton 字段属于语法错误，整树解析将严格失败。
*/
XRT_API bool xrtMailTreeParse(
	xstrview Source,
	const xmailtreelimits* pLimits,
	xmailtree* pTree
);



/* 释放整棵 MIME 树；允许传入 NULL 或已清零结构。 */
XRT_API void xrtMailTreeFree(xmailtree* pTree);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_build.h */
/* ========================================================================== */

#ifndef XRT_MAIL_BUILD_H
#define XRT_MAIL_BUILD_H




#if defined(XMAIL_FEATURE_MAIL_BUILD) && \
	(!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	 !defined(XMAIL_FEATURE_MAIL_HEADER) || \
	 !defined(XMAIL_FEATURE_MAIL_MULTIPART))
	#error "XMAIL_FEATURE_MAIL_BUILD requires address, header and multipart"
#endif



#if defined(XMAIL_FEATURE_MAIL_BUILD)

/* Builder 状态公开，便于栈对象、诊断和无额外查询的快速路径。 */
typedef enum xmailbuilderstate {
	XMAIL_BUILDER_HEADERS = 0,
	XMAIL_BUILDER_BODY,
	XMAIL_BUILDER_CLOSED,
	XMAIL_BUILDER_FAILED
} xmailbuilderstate;



/* Builder 不拥有回调和用户数据，不缓存正文，也不执行隐式网络操作。 */
typedef struct xmailbuilder {
	xmailwriteproc Write;
	ptr UserData;
	size_t Written;
	xmailbuilderstate State;
	unsigned char Tail[2];
	size_t TailSize;
	bool Busy;
} xmailbuilder;



XRT_EXTERN_C_BEGIN



/* 初始化处于字段阶段的同步流式 Builder。 */
XRT_API bool xrtMailBuilderInit(
	xmailbuilder* pBuilder,
	xmailwriteproc pWrite,
	ptr pUserData
);



/* 验证、折叠并写出一个 `Name: Value\r\n` 字段。 */
XRT_API bool xrtMailBuilderHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
);



/* 编码 UTF-8 字段值后写出字段，适用于 Subject 等非结构化字段。 */
XRT_API bool xrtMailBuilderWordHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	xmailwordencoding Encoding,
	size_t iLineSize
);



/* 格式化 mailbox 数组后写出地址字段。 */
XRT_API bool xrtMailBuilderAddressHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t iLineSize
);



/* 零复制写出一个或多个已经完整验证、以 CRLF 结束的字段。 */
XRT_API bool xrtMailBuilderHeaderBlock(
	xmailbuilder* pBuilder,
	xstrview Block
);



/* 写出字段终止空行并进入正文阶段。 */
XRT_API bool xrtMailBuilderHeadersEnd(xmailbuilder* pBuilder);



/* 零复制写出任意正文或已编码 MIME 片段。 */
XRT_API bool xrtMailBuilderBody(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
);



/* 写出可与 part 字段和正文直接拼接的 multipart 分隔片段。 */
XRT_API bool xrtMailBuilderMultipart(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
);



/* 写出 FIRST/NEXT 分隔片段并进入当前 part 的字段阶段。 */
XRT_API bool xrtMailBuilderPartBegin(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
);



/* 关闭 Builder；不自动补换行、boundary 或传输编码。 */
XRT_API bool xrtMailBuilderFinish(xmailbuilder* pBuilder);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_compose.h */
/* ========================================================================== */

#ifndef XRT_MAIL_COMPOSE_H
#define XRT_MAIL_COMPOSE_H




#if defined(XMAIL_FEATURE_MAIL_COMPOSE) && \
	(!defined(XMAIL_FEATURE_MAIL_BUILD) || \
	 !defined(XMAIL_FEATURE_MAIL_CODEC) || \
	 !defined(XMAIL_FEATURE_MAIL_DATE) || \
	 !defined(XMAIL_FEATURE_MAIL_ID) || \
	 !defined(XMAIL_FEATURE_MAIL_PARAM))
	#error "XMAIL_FEATURE_MAIL_COMPOSE requires build, codec, date, id and param"
#endif



#if defined(XMAIL_FEATURE_MAIL_COMPOSE)

/* 附件描述只借用文件名、媒体类型、Content-ID 和原始数据。 */
typedef struct xmailattachment {
	xstrview FileName;
	xstrview MediaType;
	xstrview ContentId;
	xbytesview Data;
	bool Inline;
} xmailattachment;



/*
	高层消息描述全部借用调用方数据；Bcc 只供提交层取得收件人，不写入报文。
	空 Date、MessageId 和 boundary 由库生成，空 MessageIdDomain 从 From 推导。
*/
typedef struct xmailmessage {
	xmailaddress From;
	xmailaddress ReplyTo;
	const xmailaddress* To;
	size_t ToCount;
	const xmailaddress* Cc;
	size_t CcCount;
	const xmailaddress* Bcc;
	size_t BccCount;
	xstrview Subject;
	xstrview Text;
	xstrview Html;
	const xmailattachment* Attachments;
	size_t AttachmentCount;
	const xmailheaderview* Headers;
	size_t HeaderCount;
	xstrview Date;
	xstrview MessageId;
	xstrview MessageIdDomain;
	xstrview MixedBoundary;
	xstrview AlternativeBoundary;
	xstrview RelatedBoundary;
	xmailwordencoding WordEncoding;
	uint32 AddressFlags;
	size_t HeaderLineSize;
} xmailmessage;



XRT_EXTERN_C_BEGIN



/* 初始化 Base64 编码词、默认字段行宽和其余空视图。 */
XRT_API void xrtMailMessageInit(xmailmessage* pMessage);



/* 完整验证消息描述，且不生成随机值、不分配结果或调用 sink。 */
XRT_API bool xrtMailMessageValid(const xmailmessage* pMessage);



/*
	验证后流式构建完整 RFC 消息；回调失败时可能已经提交前缀。
	文本使用 UTF-8 Quoted-Printable，附件按块写出 MIME Base64。
*/
XRT_API bool xrtMailComposeWrite(
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
);



/* 构建由 xrtFree 释放的完整 RFC 消息。 */
XRT_API str xrtMailCompose(
	const xmailmessage* pMessage,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xrt/mail_net.h */
/* ========================================================================== */

#ifndef XRT_MAIL_NET_H
#define XRT_MAIL_NET_H


#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
#endif



#if defined(XMAIL_FEATURE_MAIL_NET) && \
	(!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	 !defined(XRT_FEATURE_NET_TCP_DIAL_SYNC) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XMAIL_FEATURE_MAIL_NET requires mail wire, TCP sync dial and Base64"
#endif

#if defined(XMAIL_FEATURE_MAIL_NET_TLS) && \
	(!defined(XMAIL_FEATURE_MAIL_NET) || \
	 !defined(XRT_FEATURE_TLS_STREAM_DIAL_FUTURE) || \
	 !defined(XRT_FEATURE_TLS_STREAM_FUTURE) || \
	 !defined(XRT_FEATURE_TLS_CLIENT_VERIFY) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE_SHA256) || \
	 !defined(XRT_FEATURE_TLS_SCHEDULE_SHA384) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_X25519) || \
	 !defined(XRT_FEATURE_TLS_KEY_EXCHANGE_P256) || \
	 !defined(XRT_FEATURE_TLS_RECORD_AES) || \
	 !defined(XRT_FEATURE_TLS_RECORD_CHACHA))
	#error "XMAIL_FEATURE_MAIL_NET_TLS requires mail net, TLS Future" \
		" dial/stream, verified client and the standard TLS profile"
#endif



#if defined(XMAIL_FEATURE_MAIL_NET)

#define XMAIL_NET_HOST_MAX 253u
#define XMAIL_NET_READ_CHUNK_DEFAULT 4096u
#define XMAIL_NET_WRITE_CHUNK_DEFAULT 16384u



/* 安全模式明确区分明文、隐式 TLS 和由协议命令触发的 STARTTLS。 */
typedef enum xmailsecurity {
	XMAIL_SECURITY_PLAIN = 0,
	XMAIL_SECURITY_TLS,
	XMAIL_SECURITY_STARTTLS
} xmailsecurity;



/*
	邮件客户端借用 Engine、Resolver、Host 和可选 TLS 共享对象。
	Dial 控制底层 TCP 拨号和流限制。
*/
typedef struct xmailnetconfig {
	xnetengine* Engine;
	xnetresolver* Resolver;
	cstr Host;
	uint16 Port;
	xmailsecurity Security;
	size_t LineLimit;
	size_t ReadChunk;
	size_t WriteChunk;
	xnetdialconfig Dial;
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xtlsclientconfig Tls;
		xtlsstreamconfig TlsStream;
		int64 TlsTimeout;
	#endif
} xmailnetconfig;



XRT_EXTERN_C_BEGIN



/* 初始化有界线路、16 KiB 发送分片和 XRT 默认拨号/TLS 策略。 */
XRT_API void xrtMailNetConfigInit(xmailnetconfig* pConfig);



/* 完整验证主机、端口、安全策略、所有者和缓冲硬边界。 */
XRT_API bool xrtMailNetConfigValid(const xmailnetconfig* pConfig);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/xmail/include/xmail.h */
/* ========================================================================== */

#ifndef XMAIL_H
#define XMAIL_H


#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) && \
	(!defined(XMAIL_FEATURE_MAIL_NET) || \
	 !defined(XRT_FEATURE_DEFLATE) || \
	 !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_MAIL_NET_DEFLATE requires mail net, Deflate and Inflate"
#endif

#if defined(XMAIL_FEATURE_MAIL) && \
	(!defined(XMAIL_FEATURE_MAIL_CODEC) || \
	!defined(XMAIL_FEATURE_MAIL_CHARSET) || \
	!defined(XMAIL_FEATURE_MAIL_HEADER) || \
	!defined(XMAIL_FEATURE_MAIL_WORD) || \
	!defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	!defined(XMAIL_FEATURE_MAIL_DATE) || \
	!defined(XMAIL_FEATURE_MAIL_ID) || \
	!defined(XMAIL_FEATURE_MAIL_PARAM) || \
	!defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	!defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
	!defined(XMAIL_FEATURE_MAIL_TREE) || \
	!defined(XMAIL_FEATURE_MAIL_BUILD) || \
	!defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	!defined(XMAIL_FEATURE_MAIL_WIRE) || \
	!defined(XMAIL_FEATURE_MAIL_NET) || \
	!defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	!defined(XMAIL_FEATURE_MAIL_NET_DEFLATE))
	#error "XMAIL_FEATURE_MAIL requires every public mail capability"
#endif
#if defined(XMAIL_FEATURE_MAIL_CODEC)
#endif

#if defined(XMAIL_FEATURE_MAIL_CHARSET)
#endif

#if defined(XMAIL_FEATURE_MAIL_HEADER)
#endif

#if defined(XMAIL_FEATURE_MAIL_WORD)
#endif

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)
#endif

#if defined(XMAIL_FEATURE_MAIL_DATE)
#endif

#if defined(XMAIL_FEATURE_MAIL_ID)
#endif

#if defined(XMAIL_FEATURE_MAIL_PARAM)
#endif

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)
#endif

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)
#endif

#if defined(XMAIL_FEATURE_MAIL_TREE)
#endif

#if defined(XMAIL_FEATURE_MAIL_BUILD)
#endif

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)
#endif

#if defined(XMAIL_FEATURE_MAIL_WIRE)
#endif

#if defined(XMAIL_FEATURE_MAIL_NET) || defined(XMAIL_FEATURE_MAIL_NET_TLS)
#endif

#endif

#endif

#if defined(XMAIL_IMPLEMENTATION) && !defined(XMAIL_IMPLEMENTATION_ONCE)
#define XMAIL_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/xmail/src/internal/xrt_mail.h */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_CORE) || \
	defined(XMAIL_FEATURE_MAIL_CHARSET) || \
	defined(XMAIL_FEATURE_MAIL_CODEC) || \
	defined(XMAIL_FEATURE_MAIL_HEADER) || \
	defined(XMAIL_FEATURE_MAIL_WORD) || \
	defined(XMAIL_FEATURE_MAIL_ADDRESS) || \
	defined(XMAIL_FEATURE_MAIL_DATE) || \
	defined(XMAIL_FEATURE_MAIL_ID) || \
	defined(XMAIL_FEATURE_MAIL_PARAM) || \
	defined(XMAIL_FEATURE_MAIL_MULTIPART) || \
	defined(XMAIL_FEATURE_MAIL_WIRE) || \
	defined(XMAIL_FEATURE_MAIL_MESSAGE) || \
	defined(XMAIL_FEATURE_MAIL_TREE) || \
	defined(XMAIL_FEATURE_MAIL_BUILD) || \
	defined(XMAIL_FEATURE_MAIL_COMPOSE) || \
	defined(XMAIL_FEATURE_MAIL_NET) || \
	defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
#ifndef XRT_INTERNAL_MAIL_H
#define XRT_INTERNAL_MAIL_H


#if defined(XMAIL_FEATURE_MAIL_CODEC)
#endif

#if defined(XMAIL_FEATURE_MAIL_CHARSET)
#endif

#if defined(XMAIL_FEATURE_MAIL_HEADER)
#endif

#if defined(XMAIL_FEATURE_MAIL_WORD)
#endif

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)
#endif

#if defined(XMAIL_FEATURE_MAIL_DATE)
#endif

#if defined(XMAIL_FEATURE_MAIL_ID)
#endif

#if defined(XMAIL_FEATURE_MAIL_PARAM)
#endif

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)
#endif

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)
#endif

#if defined(XMAIL_FEATURE_MAIL_TREE)
#endif

#if defined(XMAIL_FEATURE_MAIL_BUILD)
#endif

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)
#endif

#if defined(XMAIL_FEATURE_MAIL_WIRE)
#endif

#if defined(XMAIL_FEATURE_MAIL_NET)
#endif

#include <string.h>



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 邮件扩展只通过 XRT 的公开错误入口发布错误。 */
#define __xrtMailSetInvalidArgument() \
	xrtSetErrorInfo(XERR_ARGUMENT, "xrt.mail", 0, "invalid argument")
#define __xrtMailSetRange() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.mail", 0, "value out of range")
#define __xrtMailSetSizeOverflow() \
	xrtSetErrorInfo(XERR_RANGE, "xrt.mail", 0, "size overflow")



#if defined(XMAIL_FEATURE_MAIL_CHARSET)

/* 无错误副作用地查询和转换内置邮件字符集。 */
bool __xrtMailCharsetSupported(xstrview Charset);

bool __xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);

#endif



/* 检查字符串视图的指针和长度组合。 */
static inline bool __xrtMailViewValid(xstrview Text)
{
	return xrtMemRangeValid(Text.Data, Text.Size);
}



/* 从明确地址与长度创建借用字符串视图。 */
static inline xstrview __xrtMailView(const char* sText, size_t iSize)
{
	xstrview Text;

	Text.Data = sText;
	Text.Size = iSize;
	return Text;
}



/* 从有效视图中创建子视图，并避免对空指针执行零偏移运算。 */
static inline xstrview __xrtMailSlice(
	xstrview Text,
	size_t iStart,
	size_t iSize
)
{
	return __xrtMailView(
		Text.Data != NULL ? Text.Data + iStart : NULL,
		iSize
	);
}



/* 返回邮件编码统一使用的大写十六进制字符。 */
static inline char __xrtMailHex(unsigned char iValue)
{
	static const char sDigits[] = "0123456789ABCDEF";

	return sDigits[iValue & 0x0Fu];
}



/* 把一个十六进制字符转换为数值。 */
static inline int __xrtMailHexValue(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0');
	}
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'F') ) {
		return (int)(iByte - (unsigned char)'A') + 10;
	}
	if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'f') ) {
		return (int)(iByte - (unsigned char)'a') + 10;
	}
	return -1;
}



/* 把 ASCII 字节转换为小写，非 ASCII 字节保持不变。 */
static inline unsigned char __xrtMailAsciiLower(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (unsigned char)(iByte + ((unsigned char)'a' - (unsigned char)'A'));
	}
	return iByte;
}



/* 判断两个显式长度文本是否按 ASCII 大小写不敏感规则相等。 */
static inline bool __xrtMailAsciiEqualI(xstrview Left, xstrview Right)
{
	if ( Left.Size != Right.Size ) {
		return false;
	}
	for ( size_t i = 0; i < Left.Size; i++ ) {
		if ( __xrtMailAsciiLower((unsigned char)Left.Data[i]) !=
			 __xrtMailAsciiLower((unsigned char)Right.Data[i]) ) {
			return false;
		}
	}
	return true;
}



/* 判断字节是否属于 RFC 5322 atext。 */
static inline bool __xrtMailAtext(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'!') || (iByte == (unsigned char)'#') ||
		(iByte == (unsigned char)'$') || (iByte == (unsigned char)'%') ||
		(iByte == (unsigned char)'&') || (iByte == (unsigned char)'\'') ||
		(iByte == (unsigned char)'*') || (iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'-') || (iByte == (unsigned char)'/') ||
		(iByte == (unsigned char)'=') || (iByte == (unsigned char)'?') ||
		(iByte == (unsigned char)'^') || (iByte == (unsigned char)'_') ||
		(iByte == (unsigned char)'`') || (iByte == (unsigned char)'{') ||
		(iByte == (unsigned char)'|') || (iByte == (unsigned char)'}') ||
		(iByte == (unsigned char)'~');
}



/* 执行带溢出检查的 size_t 加法。 */
static inline bool __xrtMailSizeAdd(
	size_t iLeft,
	size_t iRight,
	size_t* pResult
)
{
	if ( iRight > (SIZE_MAX - iLeft) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	*pResult = iLeft + iRight;
	return true;
}



/* 把 uint64 写成不带末尾零字节的十进制文本。 */
size_t __xrtMailUint64Write(char* sOutput, uint64 iValue);



/* 设置带稳定邮件错误代码的协议错误。 */
static inline void __xrtMailError(
	xerrkind Kind,
	xmailerror Code,
	cstr sMessage
)
{
	xrtSetErrorInfo(Kind, "xrt.mail", (int32)Code, sMessage);
}

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xmail/src/internal/xrt_mail_net.h */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET) || \
	defined(XMAIL_FEATURE_MAIL_NET_TLS) || \
	defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
#ifndef XRT_INTERNAL_MAIL_NET_H
#define XRT_INTERNAL_MAIL_NET_H


#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
#endif



#if defined(XMAIL_FEATURE_MAIL_NET)

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) && \
	(!defined(XRT_FEATURE_DEFLATE) || !defined(XRT_FEATURE_INFLATE))
	#error "XMAIL_FEATURE_MAIL_NET_DEFLATE requires Deflate and Inflate"
#endif

/* 同步协议客户端共享的传输对象，不向公共 API 隐藏 XRT 网络对象。 */
typedef struct __xmailtransport {
	xnetstream* Tcp;
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xtlsstream* Tls;
	#endif
	bytes Pending;
	size_t PendingSize;
	size_t PendingCapacity;
	size_t PendingConsumed;
	size_t LineLimit;
	size_t ReadChunk;
	size_t WriteChunk;
	xmailsecurity Security;
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		xdeflate* Deflater;
		xinflate* Inflater;
		bytes DeflatePrefix;
		size_t DeflatePrefixSize;
		size_t DeflatePrefixConsumed;
		xnetbytes* DeflateInput;
		size_t DeflateInputConsumed;
	#endif
} __xmailtransport;



/* 协议客户端共享的动态文本只负责所有权，不解释任何协议字段。 */
typedef struct __xmailtext {
	char* Data;
	size_t Size;
	size_t Capacity;
} __xmailtext;



bool __xrtMailTextSet(__xmailtext* pText, xstrview Value);



void __xrtMailTextDestroy(__xmailtext* pText);



bool __xrtMailTransportOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportWrite(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	double iDeadline,
	xcancel* pCancel
);



xnetbytes* __xrtMailTransportRawRecv(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportRawSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportReserve(
	__xmailtransport* pTransport,
	size_t iAppend
);



void __xrtMailTransportConsume(__xmailtransport* pTransport);



bool __xrtMailTransportLine(
	__xmailtransport* pTransport,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportRead(
	__xmailtransport* pTransport,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportClose(
	__xmailtransport* pTransport,
	double iDeadline
);



bool __xrtMailTransportAbort(__xmailtransport* pTransport);



/* 不可恢复的协议故障中止连接，同时保留原始诊断。 */
void __xrtMailTransportAbortPreserveError(__xmailtransport* pTransport);



void __xrtMailTransportDestroy(__xmailtransport* pTransport);



#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
bool __xrtMailTransportDeflateStart(
	__xmailtransport* pTransport,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
);



bool __xrtMailTransportDeflateSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportDeflateFill(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportDeflated(const __xmailtransport* pTransport);



void __xrtMailTransportDeflateDestroy(__xmailtransport* pTransport);
#endif



#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
bool __xrtMailTransportTlsOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportStartTls(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportTlsSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);



xnetbytes* __xrtMailTransportTlsRecv(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
);



bool __xrtMailTransportTlsClose(
	__xmailtransport* pTransport,
	double iDeadline
);



void __xrtMailTransportTlsDestroy(__xmailtransport* pTransport);
#endif

#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/xmail/src/internal/xrt_mail_auth.h */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET)
#ifndef XRT_INTERNAL_MAIL_AUTH_H
#define XRT_INTERNAL_MAIL_AUTH_H





#if defined(XMAIL_FEATURE_MAIL_NET)

bool __xrtMailAuthFieldValid(xstrview Text, bool bRejectSoh);



void __xrtMailAuthFree(char* sText, size_t iSize);



char* __xrtMailAuthEncode(
	const void* pData,
	size_t iSize,
	size_t* pEncodedSize
);



char* __xrtMailAuthPlain(
	xstrview AuthorizationId,
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
);



char* __xrtMailAuthXoauth2(
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
);



char* __xrtMailAuthOauthBearer(
	xstrview AuthorizationId,
	xstrview Secret,
	size_t* pEncodedSize
);

#endif

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_core.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_CORE)



#if defined(XMAIL_FEATURE_MAIL_CORE)

/* 把 uint64 写成不带末尾零字节的十进制文本。 */
size_t __xrtMailUint64Write(char* sOutput, uint64 iValue)
{
	char sReverse[20];
	size_t iSize = 0;

	do {
		sReverse[iSize++] = (char)('0' + (iValue % UINT64_C(10)));
		iValue /= UINT64_C(10);
	} while ( iValue != 0 );
	for ( size_t i = 0; i < iSize; i++ ) {
		sOutput[i] = sReverse[iSize - i - 1u];
	}
	return iSize;
}



/* 计算换行规范化后的精确字节数。 */
static bool __xrtMailCrlfSize(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		size_t iAdd = 1;

		if ( Text.Data[i] == '\r' ) {
			iAdd = 2;
			if ( ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
			}
		} else if ( Text.Data[i] == '\n' ) {
			iAdd = 2;
		}
		if ( !__xrtMailSizeAdd(iRequired, iAdd, &iRequired) ) {
			return false;
		}
	}
	*pOutputSize = iRequired;
	return true;
}



/* 把换行写为唯一的 CRLF 表示。 */
static void __xrtMailCrlfBody(xstrview Text, char* sOutput, size_t iOutputSize)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		char iByte = Text.Data[i];

		if ( iByte == '\r' ) {
			sOutput[iOutput++] = '\r';
			sOutput[iOutput++] = '\n';
			if ( ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
			}
		} else if ( iByte == '\n' ) {
			sOutput[iOutput++] = '\r';
			sOutput[iOutput++] = '\n';
		} else {
			sOutput[iOutput++] = iByte;
		}
	}
	sOutput[iOutputSize] = 0;
}



/* 把任意常见换行规范为 CRLF。 */
XRT_API bool xrtMailCrlfWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		sOutput,
		iCapacity
	)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCrlfSize(Text, &iRequired) ) {
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailCrlfBody(Text, sOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 CRLF 规范文本。 */
XRT_API str xrtMailCrlf(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !__xrtMailViewValid(Text) || !__xrtMailCrlfSize(Text, &iRequired) ) {
		if ( !__xrtMailViewValid(Text) ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtMailCrlfBody(Text, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 判断字节是否属于 MIME bcharsnospace。 */
static bool __xrtMailBoundaryByte(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'\'') || (iByte == (unsigned char)'(') ||
		(iByte == (unsigned char)')') || (iByte == (unsigned char)'+') ||
		(iByte == (unsigned char)'_') || (iByte == (unsigned char)',') ||
		(iByte == (unsigned char)'-') || (iByte == (unsigned char)'.') ||
		(iByte == (unsigned char)'/') || (iByte == (unsigned char)':') ||
		(iByte == (unsigned char)'=') || (iByte == (unsigned char)'?');
}



/* 验证 MIME boundary。 */
XRT_API bool xrtMailBoundaryValid(xstrview Boundary)
{
	if ( !__xrtMailViewValid(Boundary) || (Boundary.Size == 0) ||
		 (Boundary.Size > XMAIL_BOUNDARY_MAX) ||
		 (Boundary.Data[Boundary.Size - 1u] == ' ') ) {
		return false;
	}
	for ( size_t i = 0; i < Boundary.Size; i++ ) {
		if ( (Boundary.Data[i] != ' ') &&
			 !__xrtMailBoundaryByte((unsigned char)Boundary.Data[i]) ) {
			return false;
		}
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_charset.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_CHARSET)



#if defined(XMAIL_FEATURE_MAIL_CHARSET)

/* 内置集合不携带大型码表，保持字符集层可独立裁剪。 */
typedef enum __xmailcharset {
	__XMAIL_CHARSET_UNKNOWN = 0,
	__XMAIL_CHARSET_UTF8,
	__XMAIL_CHARSET_ASCII,
	__XMAIL_CHARSET_LATIN1,
	__XMAIL_CHARSET_WINDOWS_1252
} __xmailcharset;



/* 比较不区分大小写的 ASCII 字符集名称。 */
static bool __xrtMailCharsetEqual(xstrview Text, cstr sValue)
{
	return __xrtMailAsciiEqualI(
		Text,
		__xrtMailView(sValue, strlen(sValue))
	);
}



/* 把字符集别名归一到内置集合。 */
static __xmailcharset __xrtMailCharset(xstrview Charset)
{
	if ( __xrtMailCharsetEqual(Charset, "UTF-8") ||
		 __xrtMailCharsetEqual(Charset, "UTF8") ) {
		return __XMAIL_CHARSET_UTF8;
	}
	if ( __xrtMailCharsetEqual(Charset, "US-ASCII") ||
		 __xrtMailCharsetEqual(Charset, "ASCII") ) {
		return __XMAIL_CHARSET_ASCII;
	}
	if ( __xrtMailCharsetEqual(Charset, "ISO-8859-1") ||
		 __xrtMailCharsetEqual(Charset, "ISO8859-1") ||
		 __xrtMailCharsetEqual(Charset, "LATIN1") ||
		 __xrtMailCharsetEqual(Charset, "LATIN-1") ) {
		return __XMAIL_CHARSET_LATIN1;
	}
	if ( __xrtMailCharsetEqual(Charset, "WINDOWS-1252") ||
		 __xrtMailCharsetEqual(Charset, "CP1252") ) {
		return __XMAIL_CHARSET_WINDOWS_1252;
	}
	return __XMAIL_CHARSET_UNKNOWN;
}



/* 返回 Windows-1252 高位控制区对应的 Unicode 标量。 */
static uint32 __xrtMailCharsetWindows1252(unsigned char iByte)
{
	static const uint16 arrMap[32] = {
		0x20ACu, 0x0000u, 0x201Au, 0x0192u,
		0x201Eu, 0x2026u, 0x2020u, 0x2021u,
		0x02C6u, 0x2030u, 0x0160u, 0x2039u,
		0x0152u, 0x0000u, 0x017Du, 0x0000u,
		0x0000u, 0x2018u, 0x2019u, 0x201Cu,
		0x201Du, 0x2022u, 0x2013u, 0x2014u,
		0x02DCu, 0x2122u, 0x0161u, 0x203Au,
		0x0153u, 0x0000u, 0x017Eu, 0x0178u
	};

	return arrMap[iByte - 0x80u];
}



/* 无错误副作用地查询内置字符集。 */
bool __xrtMailCharsetSupported(xstrview Charset)
{
	return __xrtMailCharset(Charset) != __XMAIL_CHARSET_UNKNOWN;
}



/* 无错误副作用地计量或转换，供协议解析器执行事务式预检。 */
bool __xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	__xmailcharset Encoding = __xrtMailCharset(Charset);
	size_t iOutput = 0;

	if ( Encoding == __XMAIL_CHARSET_UNKNOWN ) {
		return false;
	}
	if ( Encoding == __XMAIL_CHARSET_UTF8 ) {
		xstrview Text = __xrtMailView(
			(const char*)Source.Data,
			Source.Size
		);

		if ( !xrtUtf8Valid(Text, NULL) ||
			 ((sOutput != NULL) && (Source.Size > iCapacity)) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			memcpy(sOutput, Source.Data, Source.Size);
		}
		*pOutputSize = Source.Size;
		return true;
	}
	for ( size_t i = 0; i < Source.Size; i++ ) {
		unsigned char iByte = Source.Data[i];
		uint32 iScalar;
		char arrScalar[4];
		size_t iScalarSize;

		if ( (Encoding == __XMAIL_CHARSET_ASCII) && (iByte >= 0x80u) ) {
			return false;
		}
		if ( (Encoding == __XMAIL_CHARSET_ASCII) || (iByte < 0x80u) ) {
			iScalar = iByte;
		} else if ( (Encoding == __XMAIL_CHARSET_WINDOWS_1252) &&
			 (iByte < 0xA0u) ) {
			iScalar = __xrtMailCharsetWindows1252(iByte);
			if ( iScalar == 0u ) {
				return false;
			}
		} else {
			iScalar = iByte;
		}
		iScalarSize = xrtUtf8Encode(iScalar, arrScalar);
		if ( (iScalarSize == 0) || (iScalarSize > (SIZE_MAX - iOutput)) ) {
			return false;
		}
		if ( (sOutput != NULL) &&
			 (iScalarSize > (iCapacity - iOutput)) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			memcpy(sOutput + iOutput, arrScalar, iScalarSize);
		}
		iOutput += iScalarSize;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 判断字符集名称是否属于内置集合。 */
XRT_API bool xrtMailCharsetSupported(xstrview Charset)
{
	if ( !__xrtMailViewValid(Charset) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailCharsetSupported(Charset);
}



/* 把内置字符集转换成 UTF-8。 */
XRT_API bool xrtMailCharsetToUtf8Write(
	xstrview Charset,
	xbytesview Source,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Charset) ||
		 !xrtMemRangeValid(Source.Data, Source.Size) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), Source.Data, Source.Size
		) || ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCharsetSupported(Charset) ) {
		__xrtMailError(
			XERR_UNSUPPORTED,
			XMAIL_ERROR_CHARSET,
			"unsupported mail character set"
		);
		return false;
	}
	if ( !__xrtMailCharsetToUtf8(
		Charset, Source, NULL, 0, &iRequired
	) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_CHARSET,
			"invalid text for the declared mail character set"
		);
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
	if ( xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Source.Data, Source.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailCharsetToUtf8(
		Charset, Source, sOutput, iRequired, &iRequired
	) ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_CHARSET,
			"measured mail character conversion did not fit"
		);
		return false;
	}
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 UTF-8 转换结果。 */
XRT_API str xrtMailCharsetToUtf8(
	xstrview Charset,
	xbytesview Source,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailCharsetToUtf8Write(
		Charset, Source, NULL, 0, &iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailCharsetToUtf8Write(
		Charset,
		Source,
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
/* source: extlibs/xmail/src/mail/mail_codec.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_CODEC)



#if defined(XMAIL_FEATURE_MAIL_CODEC)

/* 校验 Quoted-Printable 行宽和编码标志。 */
static bool __xrtMailQpEncodeConfig(
	size_t* pLineSize,
	uint32 iFlags
)
{
	if ( iFlags & ~(uint32)XMAIL_QP_TEXT ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid quoted-printable encode flags"
		);
		return false;
	}
	if ( *pLineSize == 0 ) {
		*pLineSize = XMAIL_QP_LINE_DEFAULT;
	}
	if ( (*pLineSize < 4u) || (*pLineSize > XMAIL_QP_LINE_DEFAULT) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"quoted-printable line size must be between 4 and 76"
		);
		return false;
	}
	return true;
}



/* 判断当前位置是否是文本模式下的一次换行。 */
static bool __xrtMailQpNewline(
	const uint8* pData,
	size_t iSize,
	size_t iPosition,
	size_t* pWidth
)
{
	if ( pData[iPosition] == (uint8)'\r' ) {
		*pWidth = (((iPosition + 1u) < iSize) &&
			(pData[iPosition + 1u] == (uint8)'\n')) ? 2u : 1u;
		return true;
	}
	if ( pData[iPosition] == (uint8)'\n' ) {
		*pWidth = 1u;
		return true;
	}
	return false;
}



/* 判断一个字节是否可以在当前位置直接输出。 */
static bool __xrtMailQpLiteral(
	const uint8* pData,
	size_t iSize,
	size_t iPosition,
	bool bText
)
{
	uint8 iByte = pData[iPosition];

	if ( ((iByte >= 33u) && (iByte <= 60u)) ||
		 ((iByte >= 62u) && (iByte <= 126u)) ) {
		return true;
	}
	if ( (iByte != (uint8)' ') && (iByte != (uint8)'\t') ) {
		return false;
	}
	if ( (iPosition + 1u) == iSize ) {
		return false;
	}
	if ( bText ) {
		size_t iWidth;

		if ( __xrtMailQpNewline(pData, iSize, iPosition + 1u, &iWidth) ) {
			return false;
		}
	}
	return true;
}



/* 判断当前令牌后面是否仍有同一逻辑行的数据。 */
static bool __xrtMailQpMoreOnLine(
	const uint8* pData,
	size_t iSize,
	size_t iNext,
	bool bText
)
{
	size_t iWidth;

	if ( iNext >= iSize ) {
		return false;
	}
	return !bText || !__xrtMailQpNewline(pData, iSize, iNext, &iWidth);
}



/* 计算或写出 Quoted-Printable 正文。 */
static bool __xrtMailQpBody(
	const uint8* pData,
	size_t iSize,
	size_t iLineSize,
	bool bText,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;
	size_t iColumn = 0;

	for ( size_t i = 0; i < iSize; ) {
		size_t iNewlineWidth;
		size_t iTokenSize;
		bool bLiteral;
		bool bMore;

		if ( bText && __xrtMailQpNewline(
			pData,
			iSize,
			i,
			&iNewlineWidth
		) ) {
			if ( !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 2u] = '\r';
				sOutput[iOutput - 1u] = '\n';
			}
			i += iNewlineWidth;
			iColumn = 0;
			continue;
		}

		bLiteral = __xrtMailQpLiteral(pData, iSize, i, bText);
		iTokenSize = bLiteral ? 1u : 3u;
		bMore = __xrtMailQpMoreOnLine(pData, iSize, i + 1u, bText);
		if ( (iColumn != 0) &&
			 ((iColumn + iTokenSize + (bMore ? 1u : 0u)) > iLineSize) ) {
			if ( !__xrtMailSizeAdd(iOutput, 3u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 3u] = '=';
				sOutput[iOutput - 2u] = '\r';
				sOutput[iOutput - 1u] = '\n';
			}
			iColumn = 0;
		}
		if ( !__xrtMailSizeAdd(iOutput, iTokenSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			if ( bLiteral ) {
				sOutput[iOutput - 1u] = (char)pData[i];
			} else {
				sOutput[iOutput - 3u] = '=';
				sOutput[iOutput - 2u] = __xrtMailHex(pData[i] >> 4u);
				sOutput[iOutput - 1u] = __xrtMailHex(pData[i]);
			}
		}
		iColumn += iTokenSize;
		i++;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出严格的 Quoted-Printable 文本。 */
XRT_API bool xrtMailQpWrite(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailQpEncodeConfig(&iLineSize, iFlags) ||
		 !__xrtMailQpBody(
			(const uint8*)pData,
			iSize,
			iLineSize,
			(iFlags & (uint32)XMAIL_QP_TEXT) != 0,
			NULL,
			&iRequired
		) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), pData, iSize) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailQpBody(
		(const uint8*)pData,
		iSize,
		iLineSize,
		(iFlags & (uint32)XMAIL_QP_TEXT) != 0,
		sOutput,
		pOutputSize
	);
}



/* 创建独立的 Quoted-Printable 文本。 */
XRT_API str xrtMailQp(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailQpWrite(
		pData,
		iSize,
		iLineSize,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pData,
		iSize
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailQpWrite(
		pData,
		iSize,
		iLineSize,
		iFlags,
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



/* 验证并计算 Quoted-Printable 解码长度。 */
static bool __xrtMailQpDecodedSize(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;

	if ( iFlags & ~(uint32)XMAIL_QP_RELAXED_SOFT_BREAK ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid quoted-printable decode flags"
		);
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] == '=' ) {
			if ( ((i + 2u) < Text.Size) &&
				 (__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) >= 0) &&
				 (__xrtMailHexValue((unsigned char)Text.Data[i + 2u]) >= 0) ) {
				i += 2u;
			} else if ( ((i + 2u) < Text.Size) &&
				 (Text.Data[i + 1u] == '\r') &&
				 (Text.Data[i + 2u] == '\n') ) {
				i += 2u;
				continue;
			} else if ( ((iFlags & (uint32)XMAIL_QP_RELAXED_SOFT_BREAK) != 0) &&
				 ((i + 1u) < Text.Size) && (Text.Data[i + 1u] == '\n') ) {
				i++;
				continue;
			} else {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_ENCODING,
					"invalid quoted-printable escape"
				);
				return false;
			}
		}
		if ( iRequired == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		iRequired++;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 在已验证输入上执行前向 Quoted-Printable 解码。 */
static void __xrtMailQpDecodeBody(
	xstrview Text,
	uint32 iFlags,
	uint8* pOutput
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( Text.Data[i] != '=' ) {
			pOutput[iOutput++] = (uint8)Text.Data[i];
			continue;
		}
		if ( ((i + 2u) < Text.Size) &&
			 (Text.Data[i + 1u] == '\r') &&
			 (Text.Data[i + 2u] == '\n') ) {
			i += 2u;
			continue;
		}
		if ( ((iFlags & (uint32)XMAIL_QP_RELAXED_SOFT_BREAK) != 0) &&
			 (Text.Data[i + 1u] == '\n') ) {
			i++;
			continue;
		}
		pOutput[iOutput++] = (uint8)(
			(__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) << 4) |
			__xrtMailHexValue((unsigned char)Text.Data[i + 2u])
		);
		i += 2u;
	}
}



/* 解码 Quoted-Printable 到调用方缓冲区。 */
XRT_API bool xrtMailQpDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailQpDecodedSize(Text, iFlags, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((pOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pOutput,
		iCapacity
	)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Text.Data, Text.Size) &&
		 (pOutput != (const void*)Text.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iRequired != 0 ) {
		__xrtMailQpDecodeBody(Text, iFlags, (uint8*)pOutput);
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 Quoted-Printable 解码结果。 */
XRT_API bytes xrtMailQpDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailQpDecodeWrite(
		Text,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailQpDecodeWrite(
		Text,
		iFlags,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}



/* 校验 MIME Base64 行宽。 */
static bool __xrtMailBase64Line(size_t* pLineSize)
{
	if ( *pLineSize == 0 ) {
		*pLineSize = XMAIL_BASE64_LINE_DEFAULT;
	}
	if ( (*pLineSize < 4u) ||
		 (*pLineSize > XMAIL_BASE64_LINE_DEFAULT) ||
		 ((*pLineSize % 4u) != 0) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"MIME Base64 line size must be a multiple of four from 4 to 76"
		);
		return false;
	}
	return true;
}



/* 计算 MIME Base64 的总文本长度。 */
static bool __xrtMailBase64Size(
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iChunkSize = (iLineSize / 4u) * 3u;
	size_t iRequired = 0;

	for ( size_t i = 0; i < iSize; ) {
		size_t iChunk = iSize - i;
		size_t iEncoded;

		if ( iChunk > iChunkSize ) {
			iChunk = iChunkSize;
		}
		iEncoded = ((iChunk + 2u) / 3u) * 4u;
		if ( !__xrtMailSizeAdd(iRequired, iEncoded + 2u, &iRequired) ) {
			return false;
		}
		i += iChunk;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 写出逐行 MIME Base64。 */
XRT_API bool xrtMailBase64Write(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iChunkSize;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailBase64Line(&iLineSize) ||
		 !__xrtMailBase64Size(iSize, iLineSize, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), pData, iSize) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iChunkSize = (iLineSize / 4u) * 3u;
	for ( size_t i = 0; i < iSize; ) {
		size_t iChunk = iSize - i;
		size_t iEncoded;

		if ( iChunk > iChunkSize ) {
			iChunk = iChunkSize;
		}
		if ( !xrtBase64Encode(
			(const uint8*)pData + i,
			iChunk,
			sOutput + iOutput,
			iCapacity - iOutput,
			&iEncoded,
			NULL
		) ) {
			return false;
		}
		iOutput += iEncoded;
		sOutput[iOutput++] = '\r';
		sOutput[iOutput++] = '\n';
		i += iChunk;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 创建独立的逐行 MIME Base64 文本。 */
XRT_API str xrtMailBase64(
	const void* pData,
	size_t iSize,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailBase64Write(
		pData,
		iSize,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		pData,
		iSize
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailBase64Write(
		pData,
		iSize,
		iLineSize,
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



/* 使用 XRT 严格 Base64 解码器处理 MIME 空白。 */
XRT_API bool xrtMailBase64DecodeWrite(
	xstrview Text,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xbase64config Config;

	Config.Alphabet = NULL;
	Config.Flags = XBASE64_IGNORE_SPACE;
	return xrtBase64Decode(
		Text.Data,
		Text.Size,
		pOutput,
		iCapacity,
		pOutputSize,
		&Config
	);
}



/* 创建独立的 MIME Base64 解码结果。 */
XRT_API bytes xrtMailBase64Decode(xstrview Text, size_t* pOutputSize)
{
	xbase64config Config;
	size_t iOutputSize;

	Config.Alphabet = NULL;
	Config.Flags = XBASE64_IGNORE_SPACE;
	return xrtBase64DecodeNew(
		Text.Data,
		Text.Size,
		pOutputSize != NULL ? pOutputSize : &iOutputSize,
		&Config
	);
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_header.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_HEADER)



#if defined(XMAIL_FEATURE_MAIL_HEADER)

/* 判断字段名称是否符合 RFC 5322 ftext。 */
XRT_API bool xrtMailHeaderNameValid(xstrview Name)
{
	if ( !__xrtMailViewValid(Name) || (Name.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) || (iByte == (unsigned char)':') ) {
			return false;
		}
	}
	return true;
}



/* 校验字段值并可选返回展开后的精确长度。 */
static bool __xrtMailHeaderValueCheck(
	xstrview Value,
	size_t* pUnfoldedSize
)
{
	size_t iRequired = 0;

	if ( !__xrtMailViewValid(Value) ) {
		return false;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( iByte == (unsigned char)'\r' ) {
			if ( ((i + 2u) >= Value.Size) ||
				 (Value.Data[i + 1u] != '\n') ||
				 ((Value.Data[i + 2u] != ' ') &&
				  (Value.Data[i + 2u] != '\t')) ) {
				return false;
			}
			i += 2u;
			while ( ((i + 1u) < Value.Size) &&
				 ((Value.Data[i + 1u] == ' ') ||
				  (Value.Data[i + 1u] == '\t')) ) {
				i++;
			}
			if ( iRequired == SIZE_MAX ) {
				return false;
			}
			iRequired++;
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
			 (iByte == 127u) ) {
			return false;
		}
		if ( iRequired == SIZE_MAX ) {
			return false;
		}
		iRequired++;
	}
	if ( pUnfoldedSize != NULL ) {
		*pUnfoldedSize = iRequired;
	}
	return true;
}



/* 判断字段值是否安全且折叠语法完整。 */
XRT_API bool xrtMailHeaderValueValid(xstrview Value)
{
	return __xrtMailHeaderValueCheck(Value, NULL);
}



/* 初始化不分配内存的字段游标。 */
XRT_API bool xrtMailHeaderCursorInit(
	xmailheadercursor* pCursor,
	xstrview Block
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Block) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Block.Data, Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Block = Block;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 找到严格 CRLF 物理行的结束位置。 */
static bool __xrtMailHeaderLineEnd(
	xstrview Block,
	size_t iStart,
	size_t* pEnd,
	bool* pCrlf
)
{
	for ( size_t i = iStart; i < Block.Size; i++ ) {
		if ( Block.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail header contains a bare LF"
			);
			return false;
		}
		if ( Block.Data[i] != '\r' ) {
			continue;
		}
		if ( ((i + 1u) >= Block.Size) || (Block.Data[i + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail header contains a bare CR"
			);
			return false;
		}
		*pEnd = i;
		*pCrlf = true;
		return true;
	}
	*pEnd = Block.Size;
	*pCrlf = false;
	return true;
}



/* 发布一个字段解析错误。 */
static xmailnext __xrtMailHeaderParseError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, sMessage);
	return XMAIL_NEXT_ERROR;
}



/* 返回下一个借用字段。 */
XRT_API xmailnext xrtMailHeaderNext(
	xmailheadercursor* pCursor,
	xmailheaderview* pHeader
)
{
	xstrview Block;
	size_t iStart;
	size_t iFirstEnd;
	size_t iValueStart;
	size_t iValueEnd;
	size_t iNext;
	size_t iColon = XRT_NPOS;
	bool bCrlf;
	xmailheaderview Result;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pHeader, sizeof(*pHeader)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Block :
			 __xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Block.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pHeader,
			sizeof(*pHeader)) ||
		 xrtMemRangesOverlap(pHeader, sizeof(*pHeader),
			pCursor->Block.Data, pCursor->Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Block = pCursor->Block;
	iStart = pCursor->Position;
	if ( iStart == Block.Size ) {
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( !__xrtMailHeaderLineEnd(Block, iStart, &iFirstEnd, &bCrlf) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( iFirstEnd == iStart ) {
		pCursor->Position = bCrlf ? iFirstEnd + 2u : iFirstEnd;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( (iFirstEnd - iStart) > XMAIL_HEADER_LINE_HARD ) {
		return __xrtMailHeaderParseError("mail header line exceeds 998 bytes");
	}
	if ( (Block.Data[iStart] == ' ') || (Block.Data[iStart] == '\t') ) {
		return __xrtMailHeaderParseError("mail header starts with a continuation line");
	}
	for ( size_t i = iStart; i < iFirstEnd; i++ ) {
		if ( Block.Data[i] == ':' ) {
			iColon = i;
			break;
		}
	}
	if ( iColon == XRT_NPOS ) {
		return __xrtMailHeaderParseError("mail header field has no colon");
	}
	Result.Name = __xrtMailView(Block.Data + iStart, iColon - iStart);
	if ( !xrtMailHeaderNameValid(Result.Name) ) {
		return __xrtMailHeaderParseError("invalid mail header field name");
	}
	iValueStart = iColon + 1u;
	while ( (iValueStart < iFirstEnd) &&
		 ((Block.Data[iValueStart] == ' ') ||
		  (Block.Data[iValueStart] == '\t')) ) {
		iValueStart++;
	}
	iValueEnd = iFirstEnd;
	iNext = bCrlf ? iFirstEnd + 2u : iFirstEnd;
	while ( bCrlf && (iNext < Block.Size) &&
		 ((Block.Data[iNext] == ' ') || (Block.Data[iNext] == '\t')) ) {
		size_t iEnd;
		bool bNextCrlf;

		if ( !__xrtMailHeaderLineEnd(Block, iNext, &iEnd, &bNextCrlf) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( (iEnd - iNext) > XMAIL_HEADER_LINE_HARD ) {
			return __xrtMailHeaderParseError("mail header line exceeds 998 bytes");
		}
		iValueEnd = iEnd;
		iNext = bNextCrlf ? iEnd + 2u : iEnd;
		bCrlf = bNextCrlf;
	}
	while ( (iValueEnd > iValueStart) &&
		 ((Block.Data[iValueEnd - 1u] == ' ') ||
		  (Block.Data[iValueEnd - 1u] == '\t')) ) {
		iValueEnd--;
	}
	Result.Value = __xrtMailView(
		Block.Data + iValueStart,
		iValueEnd - iValueStart
	);
	if ( !xrtMailHeaderValueValid(Result.Value) ) {
		return __xrtMailHeaderParseError("invalid mail header field value");
	}
	*pHeader = Result;
	pCursor->Position = iNext;
	return XMAIL_NEXT_ITEM;
}



/* 在已验证字段值上写出展开结果。 */
static void __xrtMailHeaderUnfoldBody(
	xstrview Value,
	char* sOutput,
	size_t iOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Value.Size; i++ ) {
		if ( Value.Data[i] != '\r' ) {
			sOutput[iOutput++] = Value.Data[i];
			continue;
		}
		i += 2u;
		while ( ((i + 1u) < Value.Size) &&
			 ((Value.Data[i + 1u] == ' ') ||
			  (Value.Data[i + 1u] == '\t')) ) {
			i++;
		}
		sOutput[iOutput++] = ' ';
	}
	sOutput[iOutputSize] = 0;
}



/* 展开字段值中的规范折叠。 */
XRT_API bool xrtMailHeaderUnfoldWrite(
	xstrview Value,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Value) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailHeaderValueCheck(Value, &iRequired) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"invalid folded mail header value"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Value.Data,
		Value.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Value.Data, Value.Size) &&
		 ((const void*)sOutput != (const void*)Value.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailHeaderUnfoldBody(Value, sOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的展开字段值。 */
XRT_API str xrtMailHeaderUnfold(xstrview Value, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailHeaderUnfoldWrite(
		Value,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Value.Data,
		Value.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	__xrtMailHeaderUnfoldBody(Value, sOutput, iRequired);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 跳过字段值中的普通空白或折叠空白。 */
static size_t __xrtMailHeaderSkipSpace(xstrview Value, size_t iPosition)
{
	while ( iPosition < Value.Size ) {
		if ( (Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t') ) {
			iPosition++;
			continue;
		}
		if ( Value.Data[iPosition] == '\r' ) {
			iPosition += 2u;
			while ( (iPosition < Value.Size) &&
				 ((Value.Data[iPosition] == ' ') ||
				  (Value.Data[iPosition] == '\t')) ) {
				iPosition++;
			}
			continue;
		}
		break;
	}
	return iPosition;
}



/* 返回当前字段单词的结束位置。 */
static size_t __xrtMailHeaderWordEnd(xstrview Value, size_t iPosition)
{
	while ( iPosition < Value.Size ) {
		char iByte = Value.Data[iPosition];

		if ( (iByte == ' ') || (iByte == '\t') || (iByte == '\r') ) {
			break;
		}
		iPosition++;
	}
	return iPosition;
}



/* 计算或写出一个完成折叠的字段行。 */
static bool __xrtMailHeaderBody(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;
	size_t iColumn = Name.Size + 1u;
	size_t iPosition = 0;

	if ( sOutput != NULL ) {
		memcpy(sOutput, Name.Data, Name.Size);
		sOutput[Name.Size] = ':';
	}
	iOutput = Name.Size + 1u;
	while ( true ) {
		size_t iEnd;
		size_t iWordSize;
		size_t iSeparator;

		iPosition = __xrtMailHeaderSkipSpace(Value, iPosition);
		if ( iPosition == Value.Size ) {
			break;
		}
		iEnd = __xrtMailHeaderWordEnd(Value, iPosition);
		iWordSize = iEnd - iPosition;
		if ( iWordSize > (XMAIL_HEADER_LINE_HARD - 1u) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LINE,
				"mail header word exceeds the hard line limit"
			);
			return false;
		}
		iSeparator = 1u;
		if ( ((iColumn + iSeparator + iWordSize) > iLineSize) &&
			 (iColumn != 1u) ) {
			if ( !__xrtMailSizeAdd(iOutput, 3u, &iOutput) ) {
				return false;
			}
			if ( sOutput != NULL ) {
				sOutput[iOutput - 3u] = '\r';
				sOutput[iOutput - 2u] = '\n';
				sOutput[iOutput - 1u] = ' ';
			}
			iColumn = 1u;
			iSeparator = 0;
		}
		if ( (iColumn + iSeparator + iWordSize) > XMAIL_HEADER_LINE_HARD ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LINE,
				"mail header line exceeds 998 bytes"
			);
			return false;
		}
		if ( !__xrtMailSizeAdd(iOutput, iSeparator, &iOutput) ||
			 !__xrtMailSizeAdd(iOutput, iWordSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			if ( iSeparator != 0 ) {
				sOutput[iOutput - iWordSize - 1u] = ' ';
			}
			memcpy(sOutput + iOutput - iWordSize, Value.Data + iPosition,
				iWordSize);
		}
		iColumn += iSeparator + iWordSize;
		iPosition = iEnd;
	}
	if ( !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput - 2u] = '\r';
		sOutput[iOutput - 1u] = '\n';
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出一个完成折叠的字段行。 */
XRT_API bool xrtMailHeaderWrite(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Value) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailHeaderNameValid(Name) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_HEADER,
			"invalid mail header field name"
		);
		return false;
	}
	if ( !__xrtMailHeaderValueCheck(Value, NULL) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_HEADER,
			"invalid mail header field value"
		);
		return false;
	}
	if ( iLineSize == 0 ) {
		iLineSize = XMAIL_HEADER_LINE_DEFAULT;
	}
	if ( (iLineSize < 4u) || (iLineSize > XMAIL_HEADER_LINE_HARD) ||
		 (Name.Size > (XMAIL_HEADER_LINE_HARD - 1u)) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_CONFIG,
			"invalid mail header line size"
		);
		return false;
	}
	if ( !__xrtMailHeaderBody(Name, Value, iLineSize, NULL, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Value.Data, Value.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Name.Data, Name.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Value.Data, Value.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailHeaderBody(
		Name,
		Value,
		iLineSize,
		sOutput,
		pOutputSize
	);
}



/* 创建一个完成折叠的独立字段行。 */
XRT_API str xrtMailHeader(
	xstrview Name,
	xstrview Value,
	size_t iLineSize,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) &&
		 (xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Name.Data,
			Name.Size
		 ) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Value.Data,
			Value.Size
		 )) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
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
/* source: extlibs/xmail/src/mail/mail_word.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_WORD)



#if defined(XMAIL_FEATURE_MAIL_WORD)

#define XMAIL_WORD_LIMIT 75u
#define XMAIL_WORD_PREFIX_SIZE 10u
#define XMAIL_WORD_SUFFIX_SIZE 2u
#define XMAIL_WORD_PAYLOAD_SIZE \
	(XMAIL_WORD_LIMIT - XMAIL_WORD_PREFIX_SIZE - XMAIL_WORD_SUFFIX_SIZE)

/* 最短合法前缀为 =?x?Q?，原始正文和单字节转码容量由协议上限推导。 */
#define __XMAIL_WORD_RAW_MAX (XMAIL_WORD_LIMIT - 8u)
#define __XMAIL_WORD_UTF8_MAX (__XMAIL_WORD_RAW_MAX * 3u)



_Static_assert(__XMAIL_WORD_RAW_MAX >= XMAIL_WORD_PAYLOAD_SIZE,
	"RFC 2047 raw buffer is smaller than the encoder payload");



typedef enum __xmailwordparse {
	__XMAIL_WORD_NONE = 0,
	__XMAIL_WORD_VALID,
	__XMAIL_WORD_INVALID
} __xmailwordparse;



typedef enum __xmailworddecode {
	__XMAIL_WORD_DECODED = 0,
	__XMAIL_WORD_KEEP,
	__XMAIL_WORD_FAILED
} __xmailworddecode;



/* 不发布错误地识别输入开头的编码词。 */
static __xmailwordparse __xrtMailWordParseBody(
	xstrview Text,
	xmailwordview* pWord
)
{
	size_t iCharsetEnd;
	size_t iCharsetNameEnd;
	size_t iEncodedStart;
	size_t iEncodedEnd;
	xmailwordview Word;
	unsigned char iEncoding;

	if ( (Text.Size < 2u) || (Text.Data[0] != '=') || (Text.Data[1] != '?') ) {
		return __XMAIL_WORD_NONE;
	}
	iCharsetEnd = 2u;
	iCharsetNameEnd = SIZE_MAX;
	while ( (iCharsetEnd < Text.Size) && (Text.Data[iCharsetEnd] != '?') ) {
		unsigned char iByte = (unsigned char)Text.Data[iCharsetEnd];

		if ( (iByte < 33u) || (iByte > 126u) ) {
			return __XMAIL_WORD_INVALID;
		}
		if ( (iByte == (unsigned char)'*') &&
			 (iCharsetNameEnd == SIZE_MAX) ) {
			iCharsetNameEnd = iCharsetEnd;
		}
		iCharsetEnd++;
	}
	if ( iCharsetNameEnd == SIZE_MAX ) {
		iCharsetNameEnd = iCharsetEnd;
	}
	if ( (iCharsetNameEnd == 2u) || ((iCharsetEnd + 3u) > Text.Size) ) {
		return __XMAIL_WORD_INVALID;
	}
	iEncoding = __xrtMailAsciiLower(
		(unsigned char)Text.Data[iCharsetEnd + 1u]
	);
	if ( ((iEncoding != (unsigned char)'b') &&
		  (iEncoding != (unsigned char)'q')) ||
		 (Text.Data[iCharsetEnd + 2u] != '?') ) {
		return __XMAIL_WORD_INVALID;
	}
	iEncodedStart = iCharsetEnd + 3u;
	iEncodedEnd = iEncodedStart;
	while ( iEncodedEnd < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iEncodedEnd];

		if ( iByte == (unsigned char)'?' ) {
			break;
		}
		if ( (iByte < 33u) || (iByte > 126u) ) {
			return __XMAIL_WORD_INVALID;
		}
		iEncodedEnd++;
	}
	if ( (iEncodedEnd == iEncodedStart) || ((iEncodedEnd + 1u) >= Text.Size) ||
		 (Text.Data[iEncodedEnd + 1u] != '=') ||
		 ((iEncodedEnd + 2u) > XMAIL_WORD_LIMIT) ) {
		return __XMAIL_WORD_INVALID;
	}
	Word.Source = __xrtMailView(Text.Data, iEncodedEnd + 2u);
	Word.Charset = __xrtMailView(Text.Data + 2u, iCharsetNameEnd - 2u);
	Word.Language = __xrtMailView(
		Text.Data + (iCharsetNameEnd < iCharsetEnd ?
			iCharsetNameEnd + 1u : iCharsetEnd),
		iCharsetNameEnd < iCharsetEnd ?
			iCharsetEnd - iCharsetNameEnd - 1u : 0
	);
	Word.Encoded = __xrtMailView(
		Text.Data + iEncodedStart,
		iEncodedEnd - iEncodedStart
	);
	Word.Encoding = iEncoding == (unsigned char)'b' ?
		XMAIL_WORD_BASE64 : XMAIL_WORD_Q;
	*pWord = Word;
	return __XMAIL_WORD_VALID;
}



/* 从输入开头读取一个编码词。 */
XRT_API xmailnext xrtMailWordParse(xstrview Text, xmailwordview* pWord)
{
	__xmailwordparse Status;
	xmailwordview Word;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pWord, sizeof(*pWord)) ||
		 xrtMemRangesOverlap(pWord, sizeof(*pWord), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Status = __xrtMailWordParseBody(Text, &Word);
	if ( Status == __XMAIL_WORD_NONE ) {
		return XMAIL_NEXT_END;
	}
	if ( Status == __XMAIL_WORD_INVALID ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"invalid RFC 2047 encoded-word"
		);
		return XMAIL_NEXT_ERROR;
	}
	*pWord = Word;
	return XMAIL_NEXT_ITEM;
}



/* 检查文本能否安全进入邮件字段值。 */
static bool __xrtMailWordTextValid(xstrview Text)
{
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail encoded-word input is not valid UTF-8"
		);
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailError(
				XERR_VALUE,
				XMAIL_ERROR_HEADER,
				"mail encoded-word text contains a control byte"
			);
			return false;
		}
	}
	return true;
}



/* 判断纯 ASCII 文本是否仍需避免被误识别为编码词。 */
static bool __xrtMailWordNeedsEncoding(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( ((unsigned char)Text.Data[i] >= 128u) ||
			 ((Text.Data[i] == '=') && ((i + 1u) < Text.Size) &&
			  (Text.Data[i + 1u] == '?')) ) {
			return true;
		}
	}
	return false;
}



/* Q 编码只原样保留在 phrase 与任意 encoded-word 中都安全的字节。 */
static bool __xrtMailWordQSafe(unsigned char iByte)
{
	return ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ||
		(iByte == (unsigned char)'!') || (iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'+') || (iByte == (unsigned char)'-') ||
		(iByte == (unsigned char)'/');
}



/* 返回一个字节的 Q 编码长度。 */
static size_t __xrtMailWordQSize(unsigned char iByte)
{
	return ((iByte == (unsigned char)' ') || __xrtMailWordQSafe(iByte)) ?
		1u : 3u;
}



/* 返回下一个不拆开 UTF-8 标量的编码块。 */
static size_t __xrtMailWordChunk(
	xstrview Text,
	size_t iPosition,
	xmailwordencoding Encoding,
	size_t* pPayloadSize
)
{
	size_t iEnd = iPosition;
	size_t iPayload = 0;

	while ( iEnd < Text.Size ) {
		uint32 iScalar;
		size_t iRead;
		size_t iToken = 0;

		(void)xrtUtf8Decode(
			__xrtMailView(Text.Data + iEnd, Text.Size - iEnd),
			&iScalar,
			&iRead
		);
		(void)iScalar;
		if ( Encoding == XMAIL_WORD_BASE64 ) {
			size_t iBytes = (iEnd + iRead) - iPosition;

			iToken = ((iBytes + 2u) / 3u) * 4u;
			if ( iToken > XMAIL_WORD_PAYLOAD_SIZE ) {
				break;
			}
			iPayload = iToken;
		} else {
			for ( size_t i = 0; i < iRead; i++ ) {
				iToken += __xrtMailWordQSize(
					(unsigned char)Text.Data[iEnd + i]
				);
			}
			if ( iToken > (XMAIL_WORD_PAYLOAD_SIZE - iPayload) ) {
				break;
			}
			iPayload += iToken;
		}
		iEnd += iRead;
	}
	*pPayloadSize = iPayload;
	return iEnd;
}



/* 计算或写出完整编码词序列。 */
static bool __xrtMailWordEncodeBody(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t* pOutputSize
)
{
	static const char arrPrefix[][11] = {
		"=?UTF-8?B?",
		"=?UTF-8?Q?"
	};
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bFirst = true;

	while ( iPosition < Text.Size ) {
		size_t iPayload;
		size_t iEnd = __xrtMailWordChunk(
			Text,
			iPosition,
			Encoding,
			&iPayload
		);
		size_t iWordSize = XMAIL_WORD_PREFIX_SIZE + iPayload +
			XMAIL_WORD_SUFFIX_SIZE;

		if ( !bFirst && !__xrtMailSizeAdd(iOutput, 1u, &iOutput) ) {
			return false;
		}
		if ( !__xrtMailSizeAdd(iOutput, iWordSize, &iOutput) ) {
			return false;
		}
		if ( sOutput != NULL ) {
			size_t iWrite = iOutput - iWordSize;

			if ( !bFirst ) {
				sOutput[iWrite - 1u] = ' ';
			}
			memcpy(sOutput + iWrite, arrPrefix[Encoding],
				XMAIL_WORD_PREFIX_SIZE);
			iWrite += XMAIL_WORD_PREFIX_SIZE;
			if ( Encoding == XMAIL_WORD_BASE64 ) {
				size_t iEncoded;

				(void)xrtBase64Encode(
					Text.Data + iPosition,
					iEnd - iPosition,
					sOutput + iWrite,
					iPayload + 1u,
					&iEncoded,
					NULL
				);
			} else {
				for ( size_t i = iPosition; i < iEnd; i++ ) {
					unsigned char iByte = (unsigned char)Text.Data[i];

					if ( iByte == (unsigned char)' ' ) {
						sOutput[iWrite++] = '_';
					} else if ( __xrtMailWordQSafe(iByte) ) {
						sOutput[iWrite++] = (char)iByte;
					} else {
						sOutput[iWrite++] = '=';
						sOutput[iWrite++] = __xrtMailHex(iByte >> 4u);
						sOutput[iWrite++] = __xrtMailHex(iByte);
					}
				}
			}
			sOutput[iOutput - 2u] = '?';
			sOutput[iOutput - 1u] = '=';
		}
		iPosition = iEnd;
		bFirst = false;
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 写出 RFC 2047 编码词序列。 */
XRT_API bool xrtMailWordEncodeWrite(
	xstrview Text,
	xmailwordencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bool bEncode;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Encoding != XMAIL_WORD_BASE64) && (Encoding != XMAIL_WORD_Q)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailWordTextValid(Text) ) {
		return false;
	}
	bEncode = __xrtMailWordNeedsEncoding(Text);
	if ( bEncode ) {
		if ( !__xrtMailWordEncodeBody(Text, Encoding, NULL, &iRequired) ) {
			return false;
		}
	} else {
		iRequired = Text.Size;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) && ((const void*)sOutput != (const void*)Text.Data || bEncode) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( bEncode ) {
		return __xrtMailWordEncodeBody(
			Text,
			Encoding,
			sOutput,
			pOutputSize
		);
	}
	memmove(sOutput, Text.Data, Text.Size);
	sOutput[Text.Size] = 0;
	*pOutputSize = Text.Size;
	return true;
}



/* 创建独立的 RFC 2047 字段文本。 */
XRT_API str xrtMailWordEncode(
	xstrview Text,
	xmailwordencoding Encoding,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailWordEncodeWrite(
		Text,
		Encoding,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailWordEncodeWrite(
		Text,
		Encoding,
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



/* 返回标准 Base64 字节值。 */
static int __xrtMailWordBase64Value(unsigned char iByte)
{
	if ( (iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z') ) {
		return (int)(iByte - (unsigned char)'A');
	}
	if ( (iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z') ) {
		return (int)(iByte - (unsigned char)'a') + 26;
	}
	if ( (iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9') ) {
		return (int)(iByte - (unsigned char)'0') + 52;
	}
	if ( iByte == (unsigned char)'+' ) {
		return 62;
	}
	return iByte == (unsigned char)'/' ? 63 : -1;
}



/* 在调用 XRT 前无副作用地验证规范 Base64。 */
static bool __xrtMailWordBase64Valid(xstrview Text)
{
	size_t iPadding = 0;
	size_t iData;
	int iLast;

	if ( (Text.Size == 0) || ((Text.Size % 4u) != 0) ) {
		return false;
	}
	while ( (iPadding < Text.Size) &&
		 (Text.Data[Text.Size - iPadding - 1u] == '=') ) {
		iPadding++;
	}
	if ( iPadding > 2u ) {
		return false;
	}
	iData = Text.Size - iPadding;
	if ( (iData == 0) || ((iPadding == 1u) && ((iData % 4u) != 3u)) ||
		 ((iPadding == 2u) && ((iData % 4u) != 2u)) ) {
		return false;
	}
	for ( size_t i = 0; i < iData; i++ ) {
		if ( __xrtMailWordBase64Value((unsigned char)Text.Data[i]) < 0 ) {
			return false;
		}
	}
	for ( size_t i = iData; i < Text.Size; i++ ) {
		if ( Text.Data[i] != '=' ) {
			return false;
		}
	}
	iLast = __xrtMailWordBase64Value((unsigned char)Text.Data[iData - 1u]);
	return ((iPadding != 2u) || ((iLast & 0x0F) == 0)) &&
		((iPadding != 1u) || ((iLast & 0x03) == 0));
}



/* 解码并严格校验 Q 编码正文。 */
static bool __xrtMailWordQDecode(
	xstrview Text,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'_' ) {
			sOutput[iOutput++] = ' ';
			continue;
		}
		if ( iByte != (unsigned char)'=' ) {
			sOutput[iOutput++] = (char)iByte;
			continue;
		}
		if ( ((i + 2u) >= Text.Size) ||
			 (__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) < 0) ||
			 (__xrtMailHexValue((unsigned char)Text.Data[i + 2u]) < 0) ) {
			return false;
		}
		sOutput[iOutput++] = (char)(
			(__xrtMailHexValue((unsigned char)Text.Data[i + 1u]) << 4) |
			__xrtMailHexValue((unsigned char)Text.Data[i + 2u])
		);
		i += 2u;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 校验转码后的 UTF-8 文本可以安全进入邮件字段。 */
static bool __xrtMailWordDecodedValid(xstrview Decoded)
{
	for ( size_t i = 0; i < Decoded.Size; i++ ) {
		unsigned char iByte = (unsigned char)Decoded.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			return false;
		}
	}
	return xrtUtf8Valid(Decoded, NULL);
}



/* 解码单个词；容错模式把任何无法可靠解释的词留给调用方。 */
static __xmailworddecode __xrtMailWordDecodeOne(
	xmailwordview Word,
	uint32 iFlags,
	char arrDecoded[__XMAIL_WORD_UTF8_MAX],
	size_t* pDecodedSize,
	bool bPublishError
)
{
	char arrRaw[__XMAIL_WORD_RAW_MAX];
	size_t iRawSize = 0;
	bool bKnown = __xrtMailCharsetSupported(Word.Charset);
	bool bValid;

	if ( !bKnown ) {
		bValid = false;
	} else if ( Word.Encoding == XMAIL_WORD_BASE64 ) {
		bValid = __xrtMailWordBase64Valid(Word.Encoded);
		if ( bValid ) {
			bValid = xrtBase64Decode(
				Word.Encoded.Data,
				Word.Encoded.Size,
				arrRaw,
				sizeof(arrRaw),
				&iRawSize,
				NULL
			);
		}
	} else {
		bValid = __xrtMailWordQDecode(
			Word.Encoded,
			arrRaw,
			&iRawSize
		);
	}
	if ( bValid ) {
		bValid = __xrtMailCharsetToUtf8(
			Word.Charset,
			(xbytesview) { (cbytes)arrRaw, iRawSize },
			arrDecoded,
			__XMAIL_WORD_UTF8_MAX,
			pDecodedSize
		) && __xrtMailWordDecodedValid(
			__xrtMailView(arrDecoded, *pDecodedSize)
		);
	}
	if ( bValid ) {
		return __XMAIL_WORD_DECODED;
	}
	if ( (iFlags & (uint32)XMAIL_WORD_RELAXED) != 0 ) {
		return __XMAIL_WORD_KEEP;
	}
	if ( bPublishError ) {
		__xrtMailError(
			XERR_PROTOCOL,
			bKnown ? XMAIL_ERROR_ENCODING : XMAIL_ERROR_CHARSET,
			bKnown ? "invalid RFC 2047 encoded text" :
				"unsupported RFC 2047 charset"
		);
	}
	return __XMAIL_WORD_FAILED;
}



/* 校验并累计一个将进入 UTF-8 结果的片段。 */
static bool __xrtMailWordEmit(
	xstrview Text,
	char* sOutput,
	size_t* pPosition
)
{
	if ( !__xrtMailWordTextValid(Text) ||
		 !__xrtMailSizeAdd(*pPosition, Text.Size, pPosition) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		memmove(sOutput + *pPosition - Text.Size, Text.Data, Text.Size);
	}
	return true;
}



/* 返回线性空白结尾，并记录其中是否含合法折叠。 */
static bool __xrtMailWordSpaceEnd(
	xstrview Text,
	size_t iStart,
	size_t* pEnd,
	bool* pFolded
)
{
	size_t i = iStart;
	bool bFolded = false;

	while ( i < Text.Size ) {
		if ( (Text.Data[i] == ' ') || (Text.Data[i] == '\t') ) {
			i++;
			continue;
		}
		if ( Text.Data[i] == '\n' ) {
			return false;
		}
		if ( Text.Data[i] != '\r' ) {
			break;
		}
		if ( ((i + 2u) >= Text.Size) || (Text.Data[i + 1u] != '\n') ||
			 ((Text.Data[i + 2u] != ' ') && (Text.Data[i + 2u] != '\t')) ) {
			return false;
		}
		bFolded = true;
		i += 3u;
	}
	*pEnd = i;
	*pFolded = bFolded;
	return true;
}



/* 判断指定位置是否是可以被实际解码的编码词。 */
static bool __xrtMailWordDecodedAt(
	xstrview Text,
	size_t iPosition,
	uint32 iFlags
)
{
	xmailwordview Word;
	char arrDecoded[__XMAIL_WORD_UTF8_MAX];
	size_t iDecoded = 0;

	if ( __xrtMailWordParseBody(
		__xrtMailView(Text.Data + iPosition, Text.Size - iPosition),
		&Word
	) != __XMAIL_WORD_VALID ) {
		return false;
	}
	return __xrtMailWordDecodeOne(
		Word,
		iFlags,
		arrDecoded,
		&iDecoded,
		false
	) == __XMAIL_WORD_DECODED;
}



/* 验证、计量或写出完整解码字段文本。 */
static bool __xrtMailWordDecodeBody(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bPreviousDecoded = false;

	while ( iPosition < Text.Size ) {
		if ( (Text.Data[iPosition] == ' ') ||
			 (Text.Data[iPosition] == '\t') ||
			 (Text.Data[iPosition] == '\r') ||
			 (Text.Data[iPosition] == '\n') ) {
			size_t iEnd;
			bool bFolded;

			if ( !__xrtMailWordSpaceEnd(
				Text,
				iPosition,
				&iEnd,
				&bFolded
			) ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"invalid folding in RFC 2047 field text"
				);
				return false;
			}
			if ( bPreviousDecoded && (iEnd < Text.Size) &&
				 __xrtMailWordDecodedAt(Text, iEnd, iFlags) ) {
				iPosition = iEnd;
				continue;
			}
			if ( bFolded ) {
				if ( !__xrtMailWordEmit(
					XRT_STR_LITERAL(" "),
					sOutput,
					&iOutput
				) ) {
					return false;
				}
			} else if ( !__xrtMailWordEmit(
				__xrtMailView(Text.Data + iPosition, iEnd - iPosition),
				sOutput,
				&iOutput
			) ) {
				return false;
			}
			iPosition = iEnd;
			bPreviousDecoded = false;
			continue;
		}
		if ( (Text.Data[iPosition] == '=') &&
			 ((iPosition + 1u) < Text.Size) &&
			 (Text.Data[iPosition + 1u] == '?') ) {
			xmailwordview Word;
			char arrDecoded[__XMAIL_WORD_UTF8_MAX];
			size_t iDecoded = 0;
			__xmailwordparse Parse = __xrtMailWordParseBody(
				__xrtMailView(Text.Data + iPosition, Text.Size - iPosition),
				&Word
			);
			__xmailworddecode Decode;

			if ( Parse == __XMAIL_WORD_VALID ) {
				Decode = __xrtMailWordDecodeOne(
					Word,
					iFlags,
					arrDecoded,
					&iDecoded,
					true
				);
				if ( Decode == __XMAIL_WORD_FAILED ) {
					return false;
				}
				if ( Decode == __XMAIL_WORD_DECODED ) {
					if ( !__xrtMailWordEmit(
						__xrtMailView(arrDecoded, iDecoded),
						sOutput,
						&iOutput
					) ) {
						return false;
					}
					bPreviousDecoded = true;
				} else {
					if ( !__xrtMailWordEmit(
						Word.Source,
						sOutput,
						&iOutput
					) ) {
						return false;
					}
					bPreviousDecoded = false;
				}
				iPosition += Word.Source.Size;
				continue;
			}
			if ( (iFlags & (uint32)XMAIL_WORD_RELAXED) == 0 ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_ENCODING,
					"malformed RFC 2047 encoded-word"
				);
				return false;
			}
		}
		{
			size_t iEnd = iPosition + 1u;

			while ( iEnd < Text.Size ) {
				if ( (Text.Data[iEnd] == ' ') || (Text.Data[iEnd] == '\t') ||
					 (Text.Data[iEnd] == '\r') || (Text.Data[iEnd] == '\n') ||
					 ((Text.Data[iEnd] == '=') && ((iEnd + 1u) < Text.Size) &&
					  (Text.Data[iEnd + 1u] == '?')) ) {
					break;
				}
				iEnd++;
			}
			if ( !__xrtMailWordEmit(
				__xrtMailView(Text.Data + iPosition, iEnd - iPosition),
				sOutput,
				&iOutput
			) ) {
				return false;
			}
			iPosition = iEnd;
			bPreviousDecoded = false;
		}
	}
	if ( sOutput != NULL ) {
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码混合普通文本与 RFC 2047 编码词的字段值。 */
XRT_API bool xrtMailWordDecodeWrite(
	xstrview Text,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 (iFlags & ~(uint32)XMAIL_WORD_RELAXED) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailWordDecodeBody(Text, iFlags, NULL, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Text.Data,
		Text.Size
	) && ((const void*)sOutput != (const void*)Text.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailWordDecodeBody(
		Text,
		iFlags,
		sOutput,
		pOutputSize
	);
}



/* 创建独立的 UTF-8 解码文本。 */
XRT_API str xrtMailWordDecode(
	xstrview Text,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailWordDecodeWrite(
		Text,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Text.Data,
		Text.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailWordDecodeWrite(
		Text,
		iFlags,
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
/* source: extlibs/xmail/src/mail/mail_address.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_ADDRESS)



#if defined(XMAIL_FEATURE_MAIL_ADDRESS)

typedef enum __xmailaddressdelimiter {
	__XMAIL_ADDRESS_END = 0,
	__XMAIL_ADDRESS_COMMA,
	__XMAIL_ADDRESS_COLON,
	__XMAIL_ADDRESS_SEMICOLON
} __xmailaddressdelimiter;



typedef enum __xmailaddressname {
	__XMAIL_ADDRESS_NAME_RAW = 0,
	__XMAIL_ADDRESS_NAME_QUOTED,
	__XMAIL_ADDRESS_NAME_WORD
} __xmailaddressname;



/* 发布稳定的地址语法错误。 */
static bool __xrtMailAddressError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_ADDRESS, sMessage);
	return false;
}



/* 消费一段合法折叠空白。 */
static bool __xrtMailAddressFws(
	xstrview Text,
	size_t* pPosition,
	bool* pConsumed
)
{
	size_t i = *pPosition;
	bool bConsumed = false;

	while ( i < Text.Size ) {
		if ( (Text.Data[i] == ' ') || (Text.Data[i] == '\t') ) {
			i++;
			bConsumed = true;
			continue;
		}
		if ( Text.Data[i] == '\n' ) {
			return __xrtMailAddressError("mail address contains a bare LF");
		}
		if ( Text.Data[i] != '\r' ) {
			break;
		}
		if ( ((i + 2u) >= Text.Size) || (Text.Data[i + 1u] != '\n') ||
			 ((Text.Data[i + 2u] != ' ') && (Text.Data[i + 2u] != '\t')) ) {
			return __xrtMailAddressError("mail address contains invalid folding");
		}
		i += 3u;
		bConsumed = true;
	}
	*pPosition = i;
	*pConsumed = bConsumed;
	return true;
}



/* 消费一个支持嵌套和 quoted-pair 的注释。 */
static bool __xrtMailAddressComment(xstrview Text, size_t* pPosition)
{
	size_t i = *pPosition;
	size_t iDepth = 0;

	if ( (i >= Text.Size) || (Text.Data[i] != '(') ) {
		return false;
	}
	while ( i < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[i++];

		if ( iByte == (unsigned char)'(' ) {
			if ( ++iDepth > XMAIL_ADDRESS_COMMENT_DEPTH ) {
				return __xrtMailAddressError(
					"mail address comment nesting is too deep"
				);
			}
			continue;
		}
		if ( iByte == (unsigned char)')' ) {
			if ( --iDepth == 0 ) {
				*pPosition = i;
				return true;
			}
			continue;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( i >= Text.Size ) {
				return __xrtMailAddressError("mail address has a dangling escape");
			}
			iByte = (unsigned char)Text.Data[i++];
			if ( (iByte < 32u) || (iByte == 127u) ) {
				return __xrtMailAddressError("mail address has an invalid escape");
			}
			continue;
		}
		if ( iByte == (unsigned char)'\r' ) {
			size_t iFold = i - 1u;
			bool bConsumed;

			if ( !__xrtMailAddressFws(Text, &iFold, &bConsumed) ) {
				return false;
			}
			i = iFold;
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
			 (iByte == 127u) ) {
			return __xrtMailAddressError("mail address comment has a control byte");
		}
	}
	return __xrtMailAddressError("mail address has an unterminated comment");
}



/* 跳过连续 CFWS。 */
static bool __xrtMailAddressCfws(xstrview Text, size_t* pPosition)
{
	size_t i = *pPosition;

	while ( i < Text.Size ) {
		bool bSpace;

		if ( !__xrtMailAddressFws(Text, &i, &bSpace) ) {
			return false;
		}
		if ( i < Text.Size && (Text.Data[i] == '(') ) {
			if ( !__xrtMailAddressComment(Text, &i) ) {
				return false;
			}
			continue;
		}
		if ( !bSpace ) {
			break;
		}
	}
	*pPosition = i;
	return true;
}



/* 返回一个 quoted-string 结束位置。 */
static bool __xrtMailAddressQuoted(
	xstrview Text,
	size_t iStart,
	bool bUtf8,
	size_t* pEnd
)
{
	for ( size_t i = iStart + 1u; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'"' ) {
			*pEnd = i + 1u;
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			if ( ++i >= Text.Size ) {
				break;
			}
			iByte = (unsigned char)Text.Data[i];
		}
		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)' ') &&
			  (iByte != (unsigned char)'\t')) ||
			 (!bUtf8 && (iByte >= 128u)) ) {
			return __xrtMailAddressError("mail address has invalid quoted text");
		}
	}
	return __xrtMailAddressError("mail address has an unterminated quote");
}



/* 裁掉外层 CFWS，同时保留内部语法的原始切片。 */
static bool __xrtMailAddressTrim(xstrview Text, xstrview* pTrimmed)
{
	size_t i = 0;
	size_t iStart;
	size_t iEnd;
	bool bDomain = false;
	bool bEscape = false;

	if ( !__xrtMailAddressCfws(Text, &i) ) {
		return false;
	}
	iStart = i;
	iEnd = i;
	while ( i < Text.Size ) {
		size_t iSpace = i;
		unsigned char iByte;

		if ( bDomain ) {
			iByte = (unsigned char)Text.Data[i++];
			if ( bEscape ) {
				bEscape = false;
			} else if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
			} else if ( iByte == (unsigned char)']' ) {
				bDomain = false;
			}
			if ( (iByte == 0) || (iByte == 127u) || (iByte < 32u) ) {
				return __xrtMailAddressError(
					"mail domain literal contains a control byte"
				);
			}
			iEnd = i;
			continue;
		}

		if ( !__xrtMailAddressCfws(Text, &iSpace) ) {
			return false;
		}
		if ( iSpace != i ) {
			i = iSpace;
			continue;
		}
		if ( Text.Data[i] == '"' ) {
			if ( !__xrtMailAddressQuoted(Text, i, true, &i) ) {
				return false;
			}
		} else {
			iByte = (unsigned char)Text.Data[i++];

			if ( (iByte == 0) || (iByte == 127u) ||
				 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
				return __xrtMailAddressError("mail address contains a control byte");
			}
			if ( iByte == (unsigned char)'[' ) {
				bDomain = true;
			}
		}
		iEnd = i;
	}
	if ( bDomain || bEscape ) {
		return __xrtMailAddressError("mail address has an unterminated domain literal");
	}
	*pTrimmed = __xrtMailView(Text.Data + iStart, iEnd - iStart);
	return true;
}



/* 扫描当前列表项的顶层分隔符并验证括号结构。 */
static bool __xrtMailAddressDelimiter(
	xstrview Text,
	size_t iStart,
	size_t* pPosition,
	__xmailaddressdelimiter* pDelimiter
)
{
	size_t i = iStart;
	size_t iAngle = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;

	while ( i < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			if ( iByte == (unsigned char)'"' ) {
				bQuoted = false;
			}
			i++;
			continue;
		}
		if ( bDomain ) {
			if ( iByte == (unsigned char)']' ) {
				bDomain = false;
			}
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Text, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'<' ) {
			if ( iAngle != 0 ) {
				return __xrtMailAddressError("mail address has nested angle brackets");
			}
			iAngle = 1u;
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'>' ) {
			if ( iAngle == 0 ) {
				return __xrtMailAddressError("mail address has an unmatched angle bracket");
			}
			iAngle = 0;
			i++;
			continue;
		}
		if ( iAngle == 0 ) {
			if ( iByte == (unsigned char)',' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_COMMA;
				return true;
			}
			if ( iByte == (unsigned char)':' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_COLON;
				return true;
			}
			if ( iByte == (unsigned char)';' ) {
				*pPosition = i;
				*pDelimiter = __XMAIL_ADDRESS_SEMICOLON;
				return true;
			}
		}
		if ( iByte == (unsigned char)'\r' ) {
			bool bConsumed;

			if ( !__xrtMailAddressFws(Text, &i, &bConsumed) ) {
				return false;
			}
			continue;
		}
		if ( (iByte == (unsigned char)'\n') || (iByte == 0) ||
			 ((iByte < 32u) && (iByte != (unsigned char)' ') &&
			  (iByte != (unsigned char)'\t')) || (iByte == 127u) ) {
			return __xrtMailAddressError("mail address contains a control byte");
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain || (iAngle != 0) ) {
		return __xrtMailAddressError("mail address has an unterminated construct");
	}
	*pPosition = Text.Size;
	*pDelimiter = __XMAIL_ADDRESS_END;
	return true;
}



/* 验证显示名 phrase，允许 encoded-word、quoted-string、注释与 UTF-8。 */
static bool __xrtMailAddressPhrase(xstrview Name)
{
	size_t i = 0;

	while ( i < Name.Size ) {
		xmailwordview Word;
		xmailnext Next;

		if ( !__xrtMailAddressCfws(Name, &i) ) {
			return false;
		}
		if ( i == Name.Size ) {
			break;
		}
		if ( Name.Data[i] == '"' ) {
			if ( !__xrtMailAddressQuoted(Name, i, true, &i) ) {
				return false;
			}
			continue;
		}
		if ( (Name.Data[i] == '=') && ((i + 1u) < Name.Size) &&
			 (Name.Data[i + 1u] == '?') ) {
			Next = xrtMailWordParse(
				__xrtMailView(Name.Data + i, Name.Size - i),
				&Word
			);
			if ( Next != XMAIL_NEXT_ITEM ) {
				return false;
			}
			i += Word.Source.Size;
			continue;
		}
		{
			unsigned char iByte = (unsigned char)Name.Data[i];

			if ( (iByte < 128u) && !__xrtMailAtext(iByte) &&
				 (iByte != (unsigned char)'.') ) {
				return __xrtMailAddressError("mail display name has an invalid token");
			}
			i++;
		}
	}
	return true;
}



/* 验证 local-part。 */
static bool __xrtMailAddressLocal(xstrview Local, uint32 iFlags)
{
	bool bSmtpUtf8 = (iFlags & (uint32)XMAIL_ADDRESS_SMTPUTF8) != 0;
	bool bDot = true;

	if ( Local.Size == 0 ) {
		return __xrtMailAddressError("mail address has an empty local-part");
	}
	if ( Local.Data[0] == '"' ) {
		size_t iEnd;

		return __xrtMailAddressQuoted(Local, 0, bSmtpUtf8, &iEnd) &&
			(iEnd == Local.Size);
	}
	for ( size_t i = 0; i < Local.Size; i++ ) {
		unsigned char iByte = (unsigned char)Local.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bDot ) {
				return __xrtMailAddressError("mail local-part has an empty atom");
			}
			bDot = true;
			continue;
		}
		if ( (iByte < 128u) ? !__xrtMailAtext(iByte) : !bSmtpUtf8 ) {
			return __xrtMailAddressError("mail local-part has an invalid byte");
		}
		bDot = false;
	}
	return !bDot || __xrtMailAddressError("mail local-part ends with a dot");
}



/* 验证 domain 或 domain-literal。 */
static bool __xrtMailAddressDomain(xstrview Domain, uint32 iFlags)
{
	bool bSmtpUtf8 = (iFlags & (uint32)XMAIL_ADDRESS_SMTPUTF8) != 0;
	bool bLabelStart = true;
	bool bLastHyphen = false;

	if ( Domain.Size == 0 ) {
		return __xrtMailAddressError("mail address has an empty domain");
	}
	if ( Domain.Data[0] == '[' ) {
		bool bEscape = false;

		if ( (Domain.Size < 3u) || (Domain.Data[Domain.Size - 1u] != ']') ) {
			return __xrtMailAddressError("mail address has an invalid domain literal");
		}
		for ( size_t i = 1u; (i + 1u) < Domain.Size; i++ ) {
			unsigned char iByte = (unsigned char)Domain.Data[i];

			if ( bEscape ) {
				bEscape = false;
			} else if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
				continue;
			} else if ( (iByte == (unsigned char)'[') ||
				 (iByte == (unsigned char)']') ) {
				return __xrtMailAddressError("mail domain literal has an invalid byte");
			}
			if ( (iByte < 33u) || (iByte > 126u) ) {
				return __xrtMailAddressError("mail domain literal has a control byte");
			}
		}
		return !bEscape || __xrtMailAddressError(
			"mail domain literal has a dangling escape"
		);
	}
	for ( size_t i = 0; i < Domain.Size; i++ ) {
		unsigned char iByte = (unsigned char)Domain.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bLabelStart || bLastHyphen ) {
				return __xrtMailAddressError("mail domain has an invalid label");
			}
			bLabelStart = true;
			bLastHyphen = false;
			continue;
		}
		if ( iByte < 128u ) {
			bool bAlphaNumeric =
				((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
				((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
				((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9'));

			if ( !bAlphaNumeric && (iByte != (unsigned char)'-') ) {
				return __xrtMailAddressError("mail domain has an invalid byte");
			}
			if ( bLabelStart && (iByte == (unsigned char)'-') ) {
				return __xrtMailAddressError("mail domain label starts with a hyphen");
			}
			bLastHyphen = iByte == (unsigned char)'-';
		} else {
			if ( !bSmtpUtf8 ) {
				return __xrtMailAddressError("mail domain requires SMTPUTF8");
			}
			bLastHyphen = false;
		}
		bLabelStart = false;
	}
	return (!bLabelStart && !bLastHyphen) || __xrtMailAddressError(
		"mail domain ends with an invalid label"
	);
}



/* 拆分并验证完整 addr-spec。 */
static bool __xrtMailAddressSpec(
	xstrview Address,
	uint32 iFlags,
	xstrview* pAddress,
	xstrview* pLocal,
	xstrview* pDomain
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;
	size_t iAt = XRT_NPOS;
	size_t i = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;

	if ( !__xrtMailAddressTrim(Address, &Trimmed) || (Trimmed.Size == 0) ) {
		return __xrtMailAddressError("mail address is empty");
	}
	while ( i < Trimmed.Size ) {
		unsigned char iByte = (unsigned char)Trimmed.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			bQuoted = iByte != (unsigned char)'"';
			i++;
			continue;
		}
		if ( bDomain ) {
			bDomain = iByte != (unsigned char)']';
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Trimmed, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
		} else if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
		} else if ( iByte == (unsigned char)'@' ) {
			if ( iAt != XRT_NPOS ) {
				return __xrtMailAddressError("mail address has multiple at signs");
			}
			iAt = i;
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain || (iAt == XRT_NPOS) ) {
		return __xrtMailAddressError("mail address has an invalid addr-spec");
	}
	if ( !__xrtMailAddressTrim(
		__xrtMailView(Trimmed.Data, iAt),
		&Local
	) || !__xrtMailAddressTrim(
		__xrtMailView(
			Trimmed.Data + iAt + 1u,
			Trimmed.Size - iAt - 1u
		),
		&Domain
	) || !__xrtMailAddressLocal(Local, iFlags) ||
		 !__xrtMailAddressDomain(Domain, iFlags) ) {
		return false;
	}
	*pAddress = Trimmed;
	*pLocal = Local;
	*pDomain = Domain;
	return true;
}



/* 从一个完整列表项提取 mailbox 视图。 */
static bool __xrtMailAddressMailbox(
	xstrview Segment,
	uint32 iFlags,
	xmailaddressview* pAddress
)
{
	xstrview Source;
	xstrview Name = { NULL, 0 };
	xstrview Address;
	xstrview Local;
	xstrview Domain;
	size_t iLeft = XRT_NPOS;
	size_t iRight = XRT_NPOS;
	size_t i = 0;
	bool bQuoted = false;
	bool bDomain = false;
	bool bEscape = false;
	xmailaddressview Result;

	if ( !__xrtMailAddressTrim(Segment, &Source) || (Source.Size == 0) ) {
		return __xrtMailAddressError("mail address list contains an empty item");
	}
	while ( i < Source.Size ) {
		unsigned char iByte = (unsigned char)Source.Data[i];

		if ( bEscape ) {
			bEscape = false;
			i++;
			continue;
		}
		if ( (bQuoted || bDomain) && (iByte == (unsigned char)'\\') ) {
			bEscape = true;
			i++;
			continue;
		}
		if ( bQuoted ) {
			bQuoted = iByte != (unsigned char)'"';
			i++;
			continue;
		}
		if ( bDomain ) {
			bDomain = iByte != (unsigned char)']';
			i++;
			continue;
		}
		if ( iByte == (unsigned char)'(' ) {
			if ( !__xrtMailAddressComment(Source, &i) ) {
				return false;
			}
			continue;
		}
		if ( iByte == (unsigned char)'"' ) {
			bQuoted = true;
		} else if ( iByte == (unsigned char)'[' ) {
			bDomain = true;
		} else if ( iByte == (unsigned char)'<' ) {
			if ( iLeft != XRT_NPOS ) {
				return __xrtMailAddressError("mailbox has multiple angle addresses");
			}
			iLeft = i;
		} else if ( iByte == (unsigned char)'>' ) {
			if ( (iLeft == XRT_NPOS) || (iRight != XRT_NPOS) ) {
				return __xrtMailAddressError("mailbox has an unmatched angle bracket");
			}
			iRight = i;
		}
		i++;
	}
	if ( bEscape || bQuoted || bDomain ||
		 ((iLeft == XRT_NPOS) != (iRight == XRT_NPOS)) ) {
		return __xrtMailAddressError("mailbox has an unterminated construct");
	}
	if ( iLeft != XRT_NPOS ) {
		xstrview Tail;

		if ( !__xrtMailAddressTrim(
			__xrtMailView(Source.Data, iLeft),
			&Name
		) || !__xrtMailAddressTrim(
			__xrtMailView(
				Source.Data + iLeft + 1u,
				iRight - iLeft - 1u
			),
			&Address
		) || !__xrtMailAddressTrim(
			__xrtMailView(
				Source.Data + iRight + 1u,
				Source.Size - iRight - 1u
			),
			&Tail
		) || (Tail.Size != 0) ) {
			return __xrtMailAddressError("mailbox has text after its angle address");
		}
		if ( (Name.Size != 0) && !__xrtMailAddressPhrase(Name) ) {
			return false;
		}
	} else {
		Address = Source;
	}
	if ( !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Address,
		&Local,
		&Domain
	) ) {
		return false;
	}
	Result.Kind = XMAIL_ADDRESS_MAILBOX;
	Result.Source = Source;
	Result.Name = Name;
	Result.Address = Address;
	Result.Local = Local;
	Result.Domain = Domain;
	*pAddress = Result;
	return true;
}



/* 初始化地址列表游标。 */
XRT_API bool xrtMailAddressCursorInit(
	xmailaddresscursor* pCursor,
	xstrview Text,
	uint32 iFlags
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Text) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Text, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail address list is not valid UTF-8"
		);
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Flags = iFlags;
	pCursor->InGroup = false;
	pCursor->Done = false;
	return true;
}



/* 处理 group 结束符并验证其后的顶层分隔。 */
static bool __xrtMailAddressGroupEnd(
	xmailaddresscursor* pCursor,
	size_t iPosition,
	xmailaddressview* pAddress
)
{
	xmailaddressview Result = { 0 };
	size_t iNext = iPosition + 1u;

	Result.Kind = XMAIL_ADDRESS_GROUP_END;
	Result.Source = __xrtMailView(pCursor->Text.Data + iPosition, 1u);
	if ( !__xrtMailAddressCfws(pCursor->Text, &iNext) ) {
		return false;
	}
	if ( iNext < pCursor->Text.Size ) {
		if ( pCursor->Text.Data[iNext] != ',' ) {
			return __xrtMailAddressError("mail group is not followed by a comma");
		}
		iNext++;
		if ( !__xrtMailAddressCfws(pCursor->Text, &iNext) ) {
			return false;
		}
		if ( iNext == pCursor->Text.Size ) {
			return __xrtMailAddressError("mail address list has a trailing comma");
		}
	}
	pCursor->Position = iNext;
	pCursor->InGroup = false;
	*pAddress = Result;
	return true;
}



/* 返回地址列表的下一个结构项。 */
XRT_API xmailnext xrtMailAddressNext(
	xmailaddresscursor* pCursor,
	xmailaddressview* pAddress
)
{
	xmailaddresscursor Cursor;
	xmailaddressview Result = { 0 };
	size_t iStart;
	size_t iDelimiter;
	__xmailaddressdelimiter Delimiter;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pAddress, sizeof(*pAddress)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Text :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 (pCursor->Flags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pAddress,
			sizeof(*pAddress)) ||
		 xrtMemRangesOverlap(pAddress, sizeof(*pAddress),
			pCursor->Text.Data, pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	if ( !__xrtMailAddressCfws(Cursor.Text, &Cursor.Position) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Cursor.Position == Cursor.Text.Size ) {
		if ( Cursor.InGroup ) {
			(void)__xrtMailAddressError("mail address group has no semicolon");
			return XMAIL_NEXT_ERROR;
		}
		Cursor.Done = true;
		*pCursor = Cursor;
		return XMAIL_NEXT_END;
	}
	if ( Cursor.InGroup && (Cursor.Text.Data[Cursor.Position] == ';') ) {
		if ( !__xrtMailAddressGroupEnd(
			&Cursor,
			Cursor.Position,
			&Result
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	iStart = Cursor.Position;
	if ( !__xrtMailAddressDelimiter(
		Cursor.Text,
		iStart,
		&iDelimiter,
		&Delimiter
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Delimiter == __XMAIL_ADDRESS_COLON ) {
		xstrview Name;

		if ( Cursor.InGroup ) {
			(void)__xrtMailAddressError("nested mail address groups are not allowed");
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailAddressTrim(
			__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
			&Name
		) || (Name.Size == 0) || !__xrtMailAddressPhrase(Name) ) {
			(void)__xrtMailAddressError("mail address group has an invalid name");
			return XMAIL_NEXT_ERROR;
		}
		Result.Kind = XMAIL_ADDRESS_GROUP_BEGIN;
		Result.Source = __xrtMailView(
			Cursor.Text.Data + iStart,
			iDelimiter - iStart + 1u
		);
		Result.Name = Name;
		Cursor.Position = iDelimiter + 1u;
		Cursor.InGroup = true;
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	if ( Delimiter == __XMAIL_ADDRESS_SEMICOLON ) {
		xstrview Item;

		if ( !Cursor.InGroup ) {
			(void)__xrtMailAddressError("mail address list has an unmatched semicolon");
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailAddressTrim(
			__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
			&Item
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( Item.Size == 0 ) {
			if ( !__xrtMailAddressGroupEnd(
				&Cursor,
				iDelimiter,
				&Result
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			*pCursor = Cursor;
			*pAddress = Result;
			return XMAIL_NEXT_ITEM;
		}
		if ( !__xrtMailAddressMailbox(Item, Cursor.Flags, &Result) ) {
			return XMAIL_NEXT_ERROR;
		}
		Cursor.Position = iDelimiter;
		*pCursor = Cursor;
		*pAddress = Result;
		return XMAIL_NEXT_ITEM;
	}
	if ( (Delimiter == __XMAIL_ADDRESS_END) && Cursor.InGroup ) {
		(void)__xrtMailAddressError("mail address group has no semicolon");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailAddressMailbox(
		__xrtMailView(Cursor.Text.Data + iStart, iDelimiter - iStart),
		Cursor.Flags,
		&Result
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Delimiter == __XMAIL_ADDRESS_COMMA ) {
		Cursor.Position = iDelimiter + 1u;
		if ( !__xrtMailAddressCfws(Cursor.Text, &Cursor.Position) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( (Cursor.Position == Cursor.Text.Size) ||
			 (Cursor.Text.Data[Cursor.Position] == ',') ||
			 (Cursor.Text.Data[Cursor.Position] == ';') ) {
			(void)__xrtMailAddressError("mail address list has an empty item");
			return XMAIL_NEXT_ERROR;
		}
	} else {
		Cursor.Position = iDelimiter;
		Cursor.Done = true;
	}
	*pCursor = Cursor;
	*pAddress = Result;
	return XMAIL_NEXT_ITEM;
}



/* 验证并拆分一个 addr-spec。 */
XRT_API bool xrtMailAddressValid(
	xstrview Address,
	uint32 iFlags,
	xstrview* pLocal,
	xstrview* pDomain
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;

	if ( !__xrtMailViewValid(Address) ||
		 !xrtMemRangeValid(pLocal, pLocal != NULL ? sizeof(*pLocal) : 0) ||
		 !xrtMemRangeValid(pDomain, pDomain != NULL ? sizeof(*pDomain) : 0) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ||
		 ((pLocal != NULL) && xrtMemRangesOverlap(
			pLocal, sizeof(*pLocal), Address.Data, Address.Size
		 )) || ((pDomain != NULL) && xrtMemRangesOverlap(
			pDomain, sizeof(*pDomain), Address.Data, Address.Size
		 )) || ((pLocal != NULL) && (pDomain != NULL) &&
			xrtMemRangesOverlap(pLocal, sizeof(*pLocal),
				pDomain, sizeof(*pDomain))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Address, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail address is not valid UTF-8"
		);
		return false;
	}
	if ( !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Trimmed,
		&Local,
		&Domain
	) ) {
		return false;
	}
	if ( pLocal != NULL ) {
		*pLocal = Local;
	}
	if ( pDomain != NULL ) {
		*pDomain = Domain;
	}
	return true;
}



/* 选择显示名的最小合法表示。 */
static bool __xrtMailAddressNameMode(
	xstrview Name,
	__xmailaddressname* pMode,
	size_t* pQuotedSize
)
{
	bool bQuoted = false;
	bool bWord = false;
	size_t iQuoted = 2u;

	if ( !xrtUtf8Valid(Name, NULL) ) {
		__xrtMailError(
			XERR_VALUE,
			XMAIL_ERROR_CHARSET,
			"mail display name is not valid UTF-8"
		);
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		unsigned char iByte = (unsigned char)Name.Data[i];

		if ( (iByte == 0) || (iByte == 127u) || (iByte < 32u) ) {
			return __xrtMailAddressError("mail display name has a control byte");
		}
		if ( (iByte >= 128u) || ((iByte == (unsigned char)'=') &&
			 ((i + 1u) < Name.Size) && (Name.Data[i + 1u] == '?')) ) {
			bWord = true;
		}
		if ( (iByte < 128u) && (iByte != (unsigned char)' ') &&
			 !__xrtMailAtext(iByte) && (iByte != (unsigned char)'.') ) {
			bQuoted = true;
		}
		if ( (iByte == (unsigned char)'"') ||
			 (iByte == (unsigned char)'\\') ) {
			if ( !__xrtMailSizeAdd(iQuoted, 1u, &iQuoted) ) {
				return false;
			}
		}
		if ( !__xrtMailSizeAdd(iQuoted, 1u, &iQuoted) ) {
			return false;
		}
	}
	if ( (Name.Size != 0) && ((Name.Data[0] == ' ') ||
		 (Name.Data[Name.Size - 1u] == ' ')) ) {
		bQuoted = true;
	}
	*pMode = bWord ? __XMAIL_ADDRESS_NAME_WORD :
		(bQuoted ? __XMAIL_ADDRESS_NAME_QUOTED : __XMAIL_ADDRESS_NAME_RAW);
	*pQuotedSize = iQuoted;
	return true;
}



/* 写出规范 mailbox 文本。 */
XRT_API bool xrtMailAddressWrite(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xstrview Trimmed;
	xstrview Local;
	xstrview Domain;
	__xmailaddressname NameMode;
	size_t iQuotedSize;
	size_t iNameSize = 0;
	size_t iAddressSize;
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Address) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Encoding != XMAIL_WORD_BASE64) && (Encoding != XMAIL_WORD_Q)) ||
		 (iFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(Address, NULL) || !__xrtMailAddressSpec(
		Address,
		iFlags,
		&Trimmed,
		&Local,
		&Domain
	) || !__xrtMailAddressNameMode(Name, &NameMode, &iQuotedSize) ) {
		return false;
	}
	if ( Name.Size != 0 ) {
		if ( NameMode == __XMAIL_ADDRESS_NAME_WORD ) {
			if ( !xrtMailWordEncodeWrite(
				Name,
				Encoding,
				NULL,
				0,
				&iNameSize
			) ) {
				return false;
			}
		} else {
			iNameSize = NameMode == __XMAIL_ADDRESS_NAME_QUOTED ?
				iQuotedSize : Name.Size;
		}
	}
	if ( !__xrtMailSizeAdd(Local.Size, 1u, &iAddressSize) ||
		 !__xrtMailSizeAdd(iAddressSize, Domain.Size, &iAddressSize) ) {
		return false;
	}
	iRequired = iAddressSize;
	if ( Name.Size != 0 ) {
		if ( !__xrtMailSizeAdd(iNameSize, 2u, &iRequired) ||
			 !__xrtMailSizeAdd(iRequired, iAddressSize, &iRequired) ||
			 !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ) {
			return false;
		}
	}
	if ( xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Address.Data, Address.Size
	) || ((sOutput != NULL) && xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity
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
	if ( xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		sOutput, iRequired + 1u, Address.Data, Address.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	{
		size_t iOutput = 0;

		if ( Name.Size != 0 ) {
			if ( NameMode == __XMAIL_ADDRESS_NAME_WORD ) {
				(void)xrtMailWordEncodeWrite(
					Name,
					Encoding,
					sOutput,
					iNameSize + 1u,
					&iOutput
				);
			} else if ( NameMode == __XMAIL_ADDRESS_NAME_QUOTED ) {
				sOutput[iOutput++] = '"';
				for ( size_t i = 0; i < Name.Size; i++ ) {
					if ( (Name.Data[i] == '"') || (Name.Data[i] == '\\') ) {
						sOutput[iOutput++] = '\\';
					}
					sOutput[iOutput++] = Name.Data[i];
				}
				sOutput[iOutput++] = '"';
			} else {
				memcpy(sOutput, Name.Data, Name.Size);
				iOutput = Name.Size;
			}
			sOutput[iOutput++] = ' ';
			sOutput[iOutput++] = '<';
		}
		memcpy(sOutput + iOutput, Local.Data, Local.Size);
		iOutput += Local.Size;
		sOutput[iOutput++] = '@';
		memcpy(sOutput + iOutput, Domain.Data, Domain.Size);
		iOutput += Domain.Size;
		if ( Name.Size != 0 ) {
			sOutput[iOutput++] = '>';
		}
		sOutput[iOutput] = 0;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范 mailbox 文本。 */
XRT_API str xrtMailAddress(
	xstrview Name,
	xstrview Address,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailAddressWrite(
		Name,
		Address,
		Encoding,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( (pOutputSize != NULL) && (xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Name.Data, Name.Size
	) || xrtMemRangesOverlap(
		pOutputSize, sizeof(*pOutputSize), Address.Data, Address.Size
	)) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailAddressWrite(
		Name,
		Address,
		Encoding,
		iFlags,
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



/* 验证地址数组、输出范围和所有借用视图都彼此安全。 */
static bool __xrtMailAddressListRanges(
	const xmailaddress* pAddresses,
	size_t iCount,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iArraySize;

	if ( (iCount > (SIZE_MAX / sizeof(*pAddresses))) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iArraySize = iCount * sizeof(*pAddresses);
	if ( !xrtMemRangeValid(pAddresses, iArraySize) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pAddresses,
			iArraySize
		 ) || ((sOutput != NULL) && (xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pAddresses,
			iArraySize
		 ) || xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pOutputSize,
			sizeof(*pOutputSize)
		 ))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 计量全部 mailbox，并在写入前完成视图与别名检查。 */
static bool __xrtMailAddressListMeasure(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	size_t* pRequired
)
{
	size_t iRequired = 0;

	if ( !__xrtMailAddressListRanges(
		pAddresses,
		iCount,
		sOutput,
		iCapacity,
		pOutputSize
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iAddressSize;

		if ( !__xrtMailViewValid(pAddresses[i].Name) ||
			 !__xrtMailViewValid(pAddresses[i].Address) ||
			 xrtMemRangesOverlap(
				pOutputSize,
				sizeof(*pOutputSize),
				pAddresses[i].Name.Data,
				pAddresses[i].Name.Size
			 ) || xrtMemRangesOverlap(
				pOutputSize,
				sizeof(*pOutputSize),
				pAddresses[i].Address.Data,
				pAddresses[i].Address.Size
			 ) || ((sOutput != NULL) && (xrtMemRangesOverlap(
				sOutput,
				iCapacity,
				pAddresses[i].Name.Data,
				pAddresses[i].Name.Size
			 ) || xrtMemRangesOverlap(
				sOutput,
				iCapacity,
				pAddresses[i].Address.Data,
				pAddresses[i].Address.Size
			 ))) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( !xrtMailAddressWrite(
			pAddresses[i].Name,
			pAddresses[i].Address,
			Encoding,
			iFlags,
			NULL,
			0,
			&iAddressSize
		) || ((i != 0) && !__xrtMailSizeAdd(
			iRequired,
			2u,
			&iRequired
		)) || !__xrtMailSizeAdd(
			iRequired,
			iAddressSize,
			&iRequired
		) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 写出逗号分隔的规范 mailbox 列表。 */
XRT_API bool xrtMailAddressListWrite(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iOutput = 0;

	if ( !__xrtMailAddressListMeasure(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		sOutput,
		iCapacity,
		pOutputSize,
		&iRequired
	) ) {
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
	for ( size_t i = 0; i < iCount; i++ ) {
		size_t iAddressSize;

		if ( i != 0 ) {
			sOutput[iOutput++] = ',';
			sOutput[iOutput++] = ' ';
		}
		(void)xrtMailAddressWrite(
			pAddresses[i].Name,
			pAddresses[i].Address,
			Encoding,
			iFlags,
			sOutput + iOutput,
			iCapacity - iOutput,
			&iAddressSize
		);
		iOutput += iAddressSize;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范 mailbox 列表。 */
XRT_API str xrtMailAddressList(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	size_t* pCheckedSize;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	pCheckedSize = pOutputSize != NULL ? pOutputSize : &iRequired;
	if ( !__xrtMailAddressListMeasure(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		NULL,
		0,
		pCheckedSize,
		&iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailAddressListWrite(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
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
/* source: extlibs/xmail/src/mail/mail_date.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_DATE)



#if defined(XMAIL_FEATURE_MAIL_DATE)

/* 规范输出始终使用带星期、秒和数字时区的现代格式。 */
static xstrview __xrtMailDateFormat(void)
{
	static const char sFormat[] = "%a, %d %b %Y %H:%M:%S %z";

	return __xrtMailView(sFormat, sizeof(sFormat) - 1u);
}



/* 根据可选星期和秒选择唯一解析格式，避免探测失败污染错误状态。 */
static xstrview __xrtMailDateParseFormat(bool bWeekday, bool bSecond)
{
	if ( bWeekday ) {
		static const char sWithSecond[] = "%a, %-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%a, %-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
	{
		static const char sWithSecond[] = "%-d %b %Y %H:%M:%S %z";
		static const char sWithoutSecond[] = "%-d %b %Y %H:%M %z";

		return bSecond ?
			__xrtMailView(sWithSecond, sizeof(sWithSecond) - 1u) :
			__xrtMailView(sWithoutSecond, sizeof(sWithoutSecond) - 1u);
	}
}



/* 去掉日期字段外侧的线性空白。 */
static xstrview __xrtMailDateTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 按线性空白拆分兼容日期，注释和折行仍由上层负责处理。 */
static bool __xrtMailDateTokens(
	xstrview Text,
	xstrview arrToken[6],
	size_t* pCount
)
{
	size_t iPosition = 0;
	size_t iCount = 0;

	while ( iPosition < Text.Size ) {
		size_t iStart;

		while ( (iPosition < Text.Size) &&
			 ((Text.Data[iPosition] == ' ') ||
			  (Text.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
		if ( iPosition == Text.Size ) {
			break;
		}
		if ( iCount == 6u ) {
			return false;
		}
		iStart = iPosition;
		while ( (iPosition < Text.Size) &&
			 (Text.Data[iPosition] != ' ') &&
			 (Text.Data[iPosition] != '\t') ) {
			iPosition++;
		}
		arrToken[iCount++] = __xrtMailSlice(
			Text,
			iStart,
			iPosition - iStart
		);
	}
	*pCount = iCount;
	return (iCount == 5u) || (iCount == 6u);
}



/* 解析两位、三位或四位邮件年份并转换成现代四位形式。 */
static bool __xrtMailDateYear(xstrview Text, int* pYear)
{
	int iYear = 0;

	if ( (Text.Size < 2u) || (Text.Size > 4u) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iYear = (iYear * 10) + (int)(Text.Data[i] - '0');
	}
	if ( Text.Size == 2u ) {
		iYear += iYear < 50 ? 2000 : 1900;
	} else if ( Text.Size == 3u ) {
		iYear += 1900;
	}
	if ( (iYear < 1900) || (iYear > 9999) ) {
		return false;
	}
	*pYear = iYear;
	return true;
}



/* 把过时命名时区转换成 RFC 5322 数字偏移。 */
static bool __xrtMailDateZone(xstrview Text, char arrZone[5])
{
	static const struct {
		cstr Name;
		cstr Zone;
	} arrNamed[] = {
		{ "UT", "+0000" }, { "GMT", "+0000" },
		{ "EST", "-0500" }, { "EDT", "-0400" },
		{ "CST", "-0600" }, { "CDT", "-0500" },
		{ "MST", "-0700" }, { "MDT", "-0600" },
		{ "PST", "-0800" }, { "PDT", "-0700" }
	};

	if ( (Text.Size == 5u) &&
		 ((Text.Data[0] == '+') || (Text.Data[0] == '-')) ) {
		for ( size_t i = 1u; i < 5u; i++ ) {
			if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
				return false;
			}
		}
		memcpy(arrZone, Text.Data, 5u);
		return true;
	}
	for ( size_t i = 0; i < (sizeof(arrNamed) / sizeof(arrNamed[0])); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Text,
			__xrtMailView(arrNamed[i].Name, strlen(arrNamed[i].Name))
		) ) {
			memcpy(arrZone, arrNamed[i].Zone, 5u);
			return true;
		}
	}
	if ( Text.Size == 1u ) {
		unsigned char iLetter = __xrtMailAsciiLower(
			(unsigned char)Text.Data[0]
		);
		int iHour;

		if ( iLetter == (unsigned char)'z' ) {
			memcpy(arrZone, "+0000", 5u);
			return true;
		}
		if ( (iLetter >= (unsigned char)'a') &&
			 (iLetter <= (unsigned char)'i') ) {
			iHour = (int)(iLetter - (unsigned char)'a') + 1;
			arrZone[0] = '+';
		} else if ( (iLetter >= (unsigned char)'k') &&
			 (iLetter <= (unsigned char)'m') ) {
			iHour = (int)(iLetter - (unsigned char)'k') + 10;
			arrZone[0] = '+';
		} else if ( (iLetter >= (unsigned char)'n') &&
			 (iLetter <= (unsigned char)'y') ) {
			iHour = (int)(iLetter - (unsigned char)'n') + 1;
			arrZone[0] = '-';
		} else {
			return false;
		}
		arrZone[1] = (char)('0' + (iHour / 10));
		arrZone[2] = (char)('0' + (iHour % 10));
		arrZone[3] = '0';
		arrZone[4] = '0';
		return true;
	}
	return false;
}



/* 向固定规范缓冲追加一个由空格分隔的片段。 */
static bool __xrtMailDateAppend(
	char* sOutput,
	size_t iCapacity,
	size_t* pPosition,
	const char* sText,
	size_t iSize
)
{
	size_t iSeparator = *pPosition != 0 ? 1u : 0u;

	if ( (*pPosition > iCapacity) ||
		 (iSeparator > (iCapacity - *pPosition)) ||
		 (iSize > (iCapacity - *pPosition - iSeparator)) ) {
		return false;
	}
	if ( *pPosition != 0 ) {
		sOutput[(*pPosition)++] = ' ';
	}
	memcpy(sOutput + *pPosition, sText, iSize);
	*pPosition += iSize;
	return true;
}



/* 把兼容输入归一成现代数字时区和四位年份。 */
static bool __xrtMailDateNormalize(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	xstrview* pNormalized
)
{
	xstrview arrToken[6];
	size_t iCount;
	size_t iBase;
	size_t iPosition = 0;
	int iYear;
	char arrYear[4];
	char arrZone[5];

	if ( !__xrtMailDateTokens(Text, arrToken, &iCount) ) {
		return false;
	}
	iBase = iCount == 6u ? 1u : 0u;
	if ( (iBase != 0u) &&
		 ((arrToken[0].Size != 4u) ||
		  (arrToken[0].Data[3] != ',')) ) {
		return false;
	}
	if ( !__xrtMailDateYear(arrToken[iBase + 2u], &iYear) ||
		 !__xrtMailDateZone(arrToken[iBase + 4u], arrZone) ) {
		return false;
	}
	arrYear[0] = (char)('0' + ((iYear / 1000) % 10));
	arrYear[1] = (char)('0' + ((iYear / 100) % 10));
	arrYear[2] = (char)('0' + ((iYear / 10) % 10));
	arrYear[3] = (char)('0' + (iYear % 10));
	if ( ((iBase != 0u) && !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[0].Data, arrToken[0].Size
	)) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase].Data, arrToken[iBase].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase + 1u].Data, arrToken[iBase + 1u].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition, arrYear, sizeof(arrYear)
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition,
		arrToken[iBase + 3u].Data, arrToken[iBase + 3u].Size
	) || !__xrtMailDateAppend(
		sOutput, iCapacity, &iPosition, arrZone, sizeof(arrZone)
	) ) {
		return false;
	}
	*pNormalized = __xrtMailView(sOutput, iPosition);
	return true;
}



/* 校验邮件日期可以无损表达的年份和时区。 */
static bool __xrtMailDateRange(const xdatetime* pDateTime)
{
	if ( (pDateTime->Year < 1900) || (pDateTime->Year > 9999) ||
		 ((pDateTime->Offset % 60) != 0) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_HEADER,
			"mail date is outside the RFC 5322 range"
		);
		return false;
	}
	return true;
}



/* 按规范形式写入邮件日期。 */
XRT_API bool xrtMailDateWrite(
	xtime iTime,
	int iOffset,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xdatetime DateTime;
	size_t iRequired;

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			sOutput,
			iCapacity
		)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtTimeSplitAt(iTime, iOffset, &DateTime) ||
		 !__xrtMailDateRange(&DateTime) ) {
		return false;
	}
	iRequired = xrtTimeWrite(NULL, 0, iTime, iOffset, __xrtMailDateFormat());
	if ( iRequired == XRT_NPOS ) {
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
	if ( xrtTimeWrite(
		sOutput,
		iCapacity,
		iTime,
		iOffset,
		__xrtMailDateFormat()
	) != iRequired ) {
		return false;
	}
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的规范邮件日期。 */
XRT_API str xrtMailDate(xtime iTime, int iOffset, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDateWrite(iTime, iOffset, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDateWrite(
		iTime,
		iOffset,
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



/* 解析 RFC 5322 日期并保留原时区偏移。 */
XRT_API bool xrtMailDateParse(
	xstrview Text,
	uint32 iFlags,
	xtime* pTime,
	int* pOffset
)
{
	char arrNormalized[96];
	xstrview Trimmed;
	xdatetime DateTime;
	xtime iTime;
	bool bWeekday;
	size_t iColonCount = 0;

	if ( !__xrtMailViewValid(Text) ||
		 (iFlags & ~(uint32)XMAIL_DATE_RELAXED) ||
		 !xrtMemRangeValid(pTime, sizeof(*pTime)) ||
		 !xrtMemRangeValid(pOffset, pOffset != NULL ? sizeof(*pOffset) : 0) ||
		 xrtMemRangesOverlap(pTime, sizeof(*pTime), Text.Data, Text.Size) ||
		 ((pOffset != NULL) &&
		  (xrtMemRangesOverlap(pOffset, sizeof(*pOffset), Text.Data, Text.Size) ||
		   xrtMemRangesOverlap(pTime, sizeof(*pTime), pOffset, sizeof(*pOffset)))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Trimmed = __xrtMailDateTrim(Text);
	if ( Trimmed.Size == 0 ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, "mail date is empty");
		return false;
	}
	if ( (iFlags & (uint32)XMAIL_DATE_RELAXED) != 0u ) {
		xstrview Normalized;

		if ( !__xrtMailDateNormalize(
			Trimmed,
			arrNormalized,
			sizeof(arrNormalized),
			&Normalized
		) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_HEADER,
				"obsolete mail date is invalid"
			);
			return false;
		}
		Trimmed = Normalized;
	}
	for ( size_t i = 0; i < Trimmed.Size; i++ ) {
		if ( Trimmed.Data[i] == ':' ) {
			iColonCount++;
		} else if ( (Trimmed.Data[i] == '\r') || (Trimmed.Data[i] == '\n') ||
			 (Trimmed.Data[i] == 0) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_HEADER,
				"mail date contains an invalid byte"
			);
			return false;
		}
	}
	bWeekday = (Trimmed.Size > 3u) && (Trimmed.Data[3] == ',');
	if ( ((iColonCount != 1u) && (iColonCount != 2u)) ||
		 !xrtDateTimeParse(
			Trimmed,
			__xrtMailDateParseFormat(bWeekday, iColonCount == 2u),
			&DateTime
		) || !__xrtMailDateRange(&DateTime) ||
		 !xrtTimeMake(&DateTime, &iTime) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"mail date does not match RFC 5322"
		);
		return false;
	}
	*pTime = iTime;
	if ( pOffset != NULL ) {
		*pOffset = DateTime.Offset;
	}
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_id.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_ID)



#if defined(XMAIL_FEATURE_MAIL_ID)

#define __XMAIL_MESSAGE_ID_RANDOM 16u
#define __XMAIL_BOUNDARY_RANDOM 18u
#define __XMAIL_BOUNDARY_PREFIX "=_xmail_"
#define __XMAIL_BOUNDARY_PREFIX_SIZE 8u



/* 发布标识符语法错误。 */
static bool __xrtMailIdError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_HEADER, sMessage);
	return false;
}



/* 去掉标识符外侧线性空白。 */
static xstrview __xrtMailIdTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 验证 Message-ID 的 dot-atom-text。 */
static bool __xrtMailIdDotAtom(xstrview Text, bool bUtf8)
{
	bool bDot = true;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( iByte == (unsigned char)'.' ) {
			if ( bDot ) {
				return false;
			}
			bDot = true;
			continue;
		}
		if ( (iByte < 128u) ? !__xrtMailAtext(iByte) : !bUtf8 ) {
			return false;
		}
		bDot = false;
	}
	return !bDot;
}



/* 验证 Message-ID 的 no-fold-literal。 */
static bool __xrtMailIdLiteral(xstrview Text, bool bUtf8)
{
	bool bEscape = false;

	if ( (Text.Size < 3u) || (Text.Data[0] != '[') ||
		 (Text.Data[Text.Size - 1u] != ']') ) {
		return false;
	}
	for ( size_t i = 1u; (i + 1u) < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( bEscape ) {
			bEscape = false;
			continue;
		}
		if ( iByte == (unsigned char)'\\' ) {
			bEscape = true;
			continue;
		}
		if ( (iByte == (unsigned char)'[') || (iByte == (unsigned char)']') ||
			 (iByte < 33u) || ((iByte > 126u) && !bUtf8) ) {
			return false;
		}
	}
	return !bEscape;
}



/* 验证 Message-ID 右部。 */
static bool __xrtMailIdRight(xstrview Right, bool bUtf8)
{
	return __xrtMailIdDotAtom(Right, bUtf8) ||
		__xrtMailIdLiteral(Right, bUtf8);
}



/* 解析完整 Message-ID。 */
XRT_API bool xrtMailMessageIdParse(
	xstrview Text,
	uint32 iFlags,
	xmailmessageidview* pMessageId
)
{
	xstrview Source;
	xstrview Inside;
	xstrview Left;
	xstrview Right;
	xmailmessageidview Result;
	size_t iAt = XRT_NPOS;
	bool bUtf8 = (iFlags & (uint32)XMAIL_ID_UTF8) != 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pMessageId, sizeof(*pMessageId)) ||
		 ((iFlags & ~(uint32)XMAIL_ID_UTF8) != 0) ||
		 xrtMemRangesOverlap(pMessageId, sizeof(*pMessageId), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailIdTrim(Text);
	if ( (Source.Size < 5u) || (Source.Data[0] != '<') ||
		 (Source.Data[Source.Size - 1u] != '>') ) {
		return __xrtMailIdError("mail Message-ID requires angle brackets");
	}
	Inside = __xrtMailView(Source.Data + 1u, Source.Size - 2u);
	if ( bUtf8 && !xrtUtf8Valid(Inside, NULL) ) {
		return __xrtMailIdError("mail Message-ID contains invalid UTF-8");
	}
	for ( size_t i = 0; i < Inside.Size; i++ ) {
		if ( Inside.Data[i] == '@' ) {
			iAt = i;
			break;
		}
	}
	if ( iAt == XRT_NPOS ) {
		return __xrtMailIdError("mail Message-ID has no at sign");
	}
	Left = __xrtMailView(Inside.Data, iAt);
	Right = __xrtMailView(Inside.Data + iAt + 1u, Inside.Size - iAt - 1u);
	if ( !__xrtMailIdDotAtom(Left, bUtf8) || !__xrtMailIdRight(Right, bUtf8) ) {
		return __xrtMailIdError("mail Message-ID has an invalid id-left or id-right");
	}
	Result.Source = Source;
	Result.Left = Left;
	Result.Right = Right;
	*pMessageId = Result;
	return true;
}



/* 把随机字节直接写成大写十六进制。 */
static void __xrtMailIdHex(
	const unsigned char* pRandom,
	size_t iRandomSize,
	char* sOutput
)
{
	for ( size_t i = 0; i < iRandomSize; i++ ) {
		sOutput[i * 2u] = __xrtMailHex((unsigned char)(pRandom[i] >> 4u));
		sOutput[(i * 2u) + 1u] = __xrtMailHex(pRandom[i]);
	}
}



/* 使用安全随机源写入 Message-ID。 */
XRT_API bool xrtMailMessageIdWrite(
	xstrview Right,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrRandom[__XMAIL_MESSAGE_ID_RANDOM];
	size_t iRequired;

	if ( !__xrtMailViewValid(Right) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Right.Data, Right.Size) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ||
		   xrtMemRangesOverlap(sOutput, iCapacity, Right.Data, Right.Size))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailIdRight(Right, false) ) {
		return __xrtMailIdError("mail Message-ID id-right is invalid");
	}
	if ( !__xrtMailSizeAdd(Right.Size, 35u, &iRequired) ) {
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
	if ( !xrtSecureRandom(arrRandom, sizeof(arrRandom)) ) {
		return false;
	}
	sOutput[0] = '<';
	__xrtMailIdHex(arrRandom, sizeof(arrRandom), sOutput + 1u);
	sOutput[33u] = '@';
	memcpy(sOutput + 34u, Right.Data, Right.Size);
	sOutput[34u + Right.Size] = '>';
	sOutput[iRequired] = 0;
	memset(arrRandom, 0, sizeof(arrRandom));
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 Message-ID。 */
XRT_API str xrtMailMessageId(xstrview Right, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailMessageIdWrite(Right, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailMessageIdWrite(Right, sOutput, iRequired + 1u, &iRequired) ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return sOutput;
}



/* 使用安全随机源写入 MIME boundary。 */
XRT_API bool xrtMailBoundaryWrite(
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	unsigned char arrRandom[__XMAIL_BOUNDARY_RANDOM];
	size_t iRequired = __XMAIL_BOUNDARY_PREFIX_SIZE +
		(__XMAIL_BOUNDARY_RANDOM * 2u);

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
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
	if ( !xrtSecureRandom(arrRandom, sizeof(arrRandom)) ) {
		return false;
	}
	memcpy(sOutput, __XMAIL_BOUNDARY_PREFIX, __XMAIL_BOUNDARY_PREFIX_SIZE);
	__xrtMailIdHex(
		arrRandom,
		sizeof(arrRandom),
		sOutput + __XMAIL_BOUNDARY_PREFIX_SIZE
	);
	sOutput[iRequired] = 0;
	memset(arrRandom, 0, sizeof(arrRandom));
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的 MIME boundary。 */
XRT_API str xrtMailBoundary(size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailBoundaryWrite(NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailBoundaryWrite(sOutput, iRequired + 1u, &iRequired) ) {
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
/* source: extlibs/xmail/src/mail/mail_param.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_PARAM)



#if defined(XMAIL_FEATURE_MAIL_PARAM)

/* 发布 MIME 参数语法错误。 */
static bool __xrtMailParamError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return false;
}



/* 判断字节是否属于 MIME token。 */
static bool __xrtMailParamTokenByte(unsigned char iByte)
{
	if ( (iByte <= 32u) || (iByte >= 127u) ) {
		return false;
	}
	switch ( iByte ) {
		case '(':
		case ')':
		case '<':
		case '>':
		case '@':
		case ',':
		case ';':
		case ':':
		case '\\':
		case '"':
		case '/':
		case '[':
		case ']':
		case '?':
		case '=':
			return false;
		default:
			return true;
	}
}



/* 去掉字段值外侧的 SP/HTAB。 */
static xstrview __xrtMailParamTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		 ((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		 ((Text.Data[iEnd - 1u] == ' ') || (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 跳过已经展开字段中的 SP/HTAB。 */
static void __xrtMailParamWhitespace(xstrview Text, size_t* pPosition)
{
	while ( (*pPosition < Text.Size) &&
		 ((Text.Data[*pPosition] == ' ') || (Text.Data[*pPosition] == '\t')) ) {
		(*pPosition)++;
	}
}



/* 读取一个非空 MIME token。 */
static bool __xrtMailParamToken(
	xstrview Text,
	size_t* pPosition,
	xstrview* pToken
)
{
	size_t iStart = *pPosition;

	while ( (*pPosition < Text.Size) &&
		 __xrtMailParamTokenByte((unsigned char)Text.Data[*pPosition]) ) {
		(*pPosition)++;
	}
	if ( *pPosition == iStart ) {
		return false;
	}
	*pToken = __xrtMailView(Text.Data + iStart, *pPosition - iStart);
	return true;
}



/* 把十进制 section 后缀转换为 size_t。 */
static bool __xrtMailParamSection(
	xstrview Text,
	size_t iStart,
	size_t iEnd,
	size_t* pSection
)
{
	size_t iValue = 0;

	if ( iStart == iEnd ) {
		return false;
	}
	for ( size_t i = iStart; i < iEnd; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ||
			 (iValue > ((SIZE_MAX - (size_t)(iByte - (unsigned char)'0')) / 10u)) ) {
			return false;
		}
		iValue = (iValue * 10u) + (size_t)(iByte - (unsigned char)'0');
	}
	*pSection = iValue;
	return true;
}



/* 拆分 RFC 2231 的 name*、name*0 和 name*0* 后缀。 */
static bool __xrtMailParamName(
	xstrview RawName,
	xstrview* pName,
	size_t* pSection,
	bool* pExtended,
	bool* pContinued
)
{
	size_t iEnd = RawName.Size;
	size_t iStar = XRT_NPOS;
	bool bExtended = false;
	size_t iSection;

	if ( (iEnd > 0) && (RawName.Data[iEnd - 1u] == '*') ) {
		bExtended = true;
		iEnd--;
	}
	for ( size_t i = iEnd; i > 0; i-- ) {
		if ( RawName.Data[i - 1u] == '*' ) {
			iStar = i - 1u;
			break;
		}
	}
	if ( (iStar != XRT_NPOS) &&
		 __xrtMailParamSection(RawName, iStar + 1u, iEnd, &iSection) ) {
		if ( (iStar == 0) || (iSection >= XMAIL_PARAM_SECTIONS_MAX) ) {
			return false;
		}
		*pName = __xrtMailView(RawName.Data, iStar);
		*pSection = iSection;
		*pExtended = bExtended;
		*pContinued = true;
		return true;
	}
	if ( bExtended ) {
		if ( (iEnd == 0) || (RawName.Data[iEnd - 1u] == '*') ) {
			return false;
		}
		*pName = __xrtMailView(RawName.Data, iEnd);
		*pSection = XMAIL_PARAM_SECTION_NONE;
		*pExtended = true;
		*pContinued = false;
		return true;
	}
	*pName = RawName;
	*pSection = XMAIL_PARAM_SECTION_NONE;
	*pExtended = false;
	*pContinued = false;
	return true;
}



/* 初始化 MIME 参数游标。 */
XRT_API bool xrtMailParamCursorInit(
	xmailparamcursor* pCursor,
	xstrview Parameters
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Parameters) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor),
			Parameters.Data, Parameters.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Parameters;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回下一个 MIME 参数。 */
XRT_API xmailnext xrtMailParamNext(
	xmailparamcursor* pCursor,
	xmailparamview* pParameter
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	size_t iStart;
	size_t iValueStart;
	size_t iValueEnd;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Text :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameter,
			sizeof(*pParameter)) ||
		 xrtMemRangesOverlap(pParameter, sizeof(*pParameter),
			pCursor->Text.Data, pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( Cursor.Position == Cursor.Text.Size ) {
		Cursor.Done = true;
		*pCursor = Cursor;
		return XMAIL_NEXT_END;
	}
	iStart = Cursor.Position;
	if ( Cursor.Text.Data[Cursor.Position++] != ';' ) {
		__xrtMailParamError("MIME parameter requires a semicolon");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( !__xrtMailParamToken(
		Cursor.Text,
		&Cursor.Position,
		&Parameter.RawName
	) ) {
		__xrtMailParamError("MIME parameter has an invalid name");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( (Cursor.Position >= Cursor.Text.Size) ||
		 (Cursor.Text.Data[Cursor.Position++] != '=') ) {
		__xrtMailParamError("MIME parameter has no value");
		return XMAIL_NEXT_ERROR;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	iValueStart = Cursor.Position;
	Parameter.Quoted = (Cursor.Position < Cursor.Text.Size) &&
		(Cursor.Text.Data[Cursor.Position] == '"');
	if ( Parameter.Quoted ) {
		bool bEscape = false;

		Cursor.Position++;
		iValueStart = Cursor.Position;
		while ( Cursor.Position < Cursor.Text.Size ) {
			unsigned char iByte = (unsigned char)Cursor.Text.Data[Cursor.Position++];

			if ( bEscape ) {
				if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
					 (iByte == (unsigned char)'\n') ) {
					__xrtMailParamError("MIME parameter has an invalid quoted-pair");
					return XMAIL_NEXT_ERROR;
				}
				bEscape = false;
				continue;
			}
			if ( iByte == (unsigned char)'\\' ) {
				bEscape = true;
				continue;
			}
			if ( iByte == (unsigned char)'"' ) {
				iValueEnd = Cursor.Position - 1u;
				goto quoted_complete;
			}
			if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
				 (iByte == (unsigned char)'\n') ||
				 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ||
				 (iByte == 127u) ) {
				__xrtMailParamError("MIME parameter has an invalid quoted value");
				return XMAIL_NEXT_ERROR;
			}
		}
		__xrtMailParamError("MIME parameter has an unterminated quoted value");
		return XMAIL_NEXT_ERROR;
	quoted_complete:
		Parameter.RawValue = __xrtMailView(
			Cursor.Text.Data + iValueStart - 1u,
			Cursor.Position - (iValueStart - 1u)
		);
		Parameter.Value = __xrtMailView(
			Cursor.Text.Data + iValueStart,
			iValueEnd - iValueStart
		);
	} else {
		if ( !__xrtMailParamToken(
			Cursor.Text,
			&Cursor.Position,
			&Parameter.Value
		) ) {
			__xrtMailParamError("MIME parameter has an invalid token value");
			return XMAIL_NEXT_ERROR;
		}
		Parameter.RawValue = Parameter.Value;
	}
	__xrtMailParamWhitespace(Cursor.Text, &Cursor.Position);
	if ( (Cursor.Position < Cursor.Text.Size) &&
		 (Cursor.Text.Data[Cursor.Position] != ';') ) {
		__xrtMailParamError("MIME parameter has trailing invalid bytes");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailParamName(
		Parameter.RawName,
		&Parameter.Name,
		&Parameter.Section,
		&Parameter.Extended,
		&Parameter.Continued
	) ) {
		__xrtMailParamError("MIME parameter has an invalid section suffix");
		return XMAIL_NEXT_ERROR;
	}
	Parameter.Source = __xrtMailView(
		Cursor.Text.Data + iStart,
		Cursor.Position - iStart
	);
	*pParameter = Parameter;
	*pCursor = Cursor;
	return XMAIL_NEXT_ITEM;
}



/* 验证参数块可以被游标完整消费。 */
static bool __xrtMailParamValidate(xstrview Parameters)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailnext Next;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return false;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
	}
	return Next == XMAIL_NEXT_END;
}



/* 解析并验证 Content-Type 字段值。 */
XRT_API bool xrtMailMediaTypeParse(
	xstrview Text,
	xmailmediatypeview* pMediaType
)
{
	xstrview Source;
	xmailmediatypeview Result;
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pMediaType, sizeof(*pMediaType)) ||
		 xrtMemRangesOverlap(pMediaType, sizeof(*pMediaType), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailParamTrim(Text);
	if ( !__xrtMailParamToken(Source, &iPosition, &Result.Type) ||
		 (iPosition >= Source.Size) || (Source.Data[iPosition++] != '/') ||
		 !__xrtMailParamToken(Source, &iPosition, &Result.Subtype) ) {
		return __xrtMailParamError("Content-Type requires type/subtype");
	}
	__xrtMailParamWhitespace(Source, &iPosition);
	Result.Source = Source;
	Result.Parameters = __xrtMailView(
		Source.Data + iPosition,
		Source.Size - iPosition
	);
	if ( !__xrtMailParamValidate(Result.Parameters) ) {
		return false;
	}
	*pMediaType = Result;
	return true;
}



/* 解析并验证 Content-Disposition 字段值。 */
XRT_API bool xrtMailDispositionParse(
	xstrview Text,
	xmaildispositionview* pDisposition
)
{
	xstrview Source;
	xmaildispositionview Result;
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(pDisposition, sizeof(*pDisposition)) ||
		 xrtMemRangesOverlap(pDisposition, sizeof(*pDisposition),
			Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Source = __xrtMailParamTrim(Text);
	if ( !__xrtMailParamToken(Source, &iPosition, &Result.Type) ) {
		return __xrtMailParamError("Content-Disposition requires a token");
	}
	__xrtMailParamWhitespace(Source, &iPosition);
	Result.Source = Source;
	Result.Parameters = __xrtMailView(
		Source.Data + iPosition,
		Source.Size - iPosition
	);
	if ( !__xrtMailParamValidate(Result.Parameters) ) {
		return false;
	}
	*pDisposition = Result;
	return true;
}



/* 定位扩展参数第一段的 charset'language' 前缀。 */
static bool __xrtMailParamExtendedPrefix(
	const xmailparamview* pParameter,
	xstrview* pEncoded,
	xstrview* pCharset,
	xstrview* pLanguage
)
{
	size_t iFirst = XRT_NPOS;
	size_t iSecond = XRT_NPOS;

	*pEncoded = pParameter->Value;
	*pCharset = __xrtMailView(NULL, 0);
	*pLanguage = __xrtMailView(NULL, 0);
	if ( !pParameter->Extended ||
		 (pParameter->Continued && (pParameter->Section != 0)) ) {
		return true;
	}
	if ( pParameter->Quoted ) {
		return __xrtMailParamError("extended MIME parameter cannot be quoted");
	}
	for ( size_t i = 0; i < pParameter->Value.Size; i++ ) {
		if ( pParameter->Value.Data[i] == '\'' ) {
			if ( iFirst == XRT_NPOS ) {
				iFirst = i;
			} else {
				iSecond = i;
				break;
			}
		}
	}
	if ( (iFirst == 0) || (iFirst == XRT_NPOS) || (iSecond == XRT_NPOS) ) {
		return __xrtMailParamError(
			"extended MIME parameter has no charset or language prefix"
		);
	}
	*pCharset = __xrtMailView(pParameter->Value.Data, iFirst);
	*pLanguage = __xrtMailView(
		pParameter->Value.Data + iFirst + 1u,
		iSecond - iFirst - 1u
	);
	*pEncoded = __xrtMailView(
		pParameter->Value.Data + iSecond + 1u,
		pParameter->Value.Size - iSecond - 1u
	);
	return true;
}



/* 计算并可选写入一个参数 section。 */
static bool __xrtMailParamDecodeBody(
	const xmailparamview* pParameter,
	xstrview Encoded,
	char* sOutput,
	size_t* pOutputSize
)
{
	size_t iOutput = 0;

	for ( size_t i = 0; i < Encoded.Size; i++ ) {
		unsigned char iByte = (unsigned char)Encoded.Data[i];

		if ( pParameter->Extended && (iByte == (unsigned char)'%') ) {
			int iHigh;
			int iLow;

			if ( (i + 2u) >= Encoded.Size ||
				 ((iHigh = __xrtMailHexValue(
					(unsigned char)Encoded.Data[i + 1u]
				)) < 0) ||
				 ((iLow = __xrtMailHexValue(
					(unsigned char)Encoded.Data[i + 2u]
				)) < 0) ) {
				return __xrtMailParamError(
					"extended MIME parameter has invalid percent encoding"
				);
			}
			iByte = (unsigned char)((iHigh << 4) | iLow);
			i += 2u;
		} else if ( !pParameter->Extended && pParameter->Quoted &&
			 (iByte == (unsigned char)'\\') ) {
			if ( ++i >= Encoded.Size ) {
				return __xrtMailParamError(
					"MIME parameter has a dangling quoted-pair"
				);
			}
			iByte = (unsigned char)Encoded.Data[i];
		}
		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			return __xrtMailParamError(
				"decoded MIME parameter contains a control byte"
			);
		}
		if ( sOutput != NULL ) {
			sOutput[iOutput] = (char)iByte;
		}
		iOutput++;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码单个 MIME 参数 section。 */
XRT_API bool xrtMailParamDecodeWrite(
	const xmailparamview* pParameter,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xstrview* pCharset,
	xstrview* pLanguage
)
{
	xstrview Encoded;
	xstrview Charset;
	xstrview Language;
	size_t iRequired;

	if ( !xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(pCharset, pCharset != NULL ? sizeof(*pCharset) : 0) ||
		 !xrtMemRangeValid(pLanguage, pLanguage != NULL ? sizeof(*pLanguage) : 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailViewValid(pParameter->Source) ||
		 !__xrtMailViewValid(pParameter->RawName) ||
		 !__xrtMailViewValid(pParameter->Name) ||
		 !__xrtMailViewValid(pParameter->RawValue) ||
		 !__xrtMailViewValid(pParameter->Value) ||
		 (pParameter->Name.Size == 0) ||
		 (pParameter->Continued !=
		  (pParameter->Section != XMAIL_PARAM_SECTION_NONE)) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pParameter->Value.Data,
			pParameter->Value.Size
		) || ((pCharset != NULL) &&
		  xrtMemRangesOverlap(pCharset, sizeof(*pCharset),
			pParameter->Value.Data, pParameter->Value.Size)) ||
		 ((pLanguage != NULL) &&
		  xrtMemRangesOverlap(pLanguage, sizeof(*pLanguage),
			pParameter->Value.Data, pParameter->Value.Size)) ||
		 ((pCharset != NULL) && (pLanguage != NULL) &&
		  xrtMemRangesOverlap(pCharset, sizeof(*pCharset),
			pLanguage, sizeof(*pLanguage))) ||
		 ((pCharset != NULL) && xrtMemRangesOverlap(
			pCharset, sizeof(*pCharset), pOutputSize, sizeof(*pOutputSize))) ||
		 ((pLanguage != NULL) && xrtMemRangesOverlap(
			pLanguage, sizeof(*pLanguage), pOutputSize, sizeof(*pOutputSize))) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			sOutput, iCapacity) ||
		   ((pCharset != NULL) && xrtMemRangesOverlap(
			pCharset, sizeof(*pCharset), sOutput, iCapacity)) ||
		   ((pLanguage != NULL) && xrtMemRangesOverlap(
			pLanguage, sizeof(*pLanguage), sOutput, iCapacity)))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailParamExtendedPrefix(
		pParameter,
		&Encoded,
		&Charset,
		&Language
	) || !__xrtMailParamDecodeBody(
		pParameter,
		Encoded,
		NULL,
		&iRequired
	) ) {
		return false;
	}
	if ( sOutput != NULL ) {
		if ( iCapacity <= iRequired ) {
			*pOutputSize = iRequired;
			__xrtMailSetRange();
			return false;
		}
		if ( xrtMemRangesOverlap(
			sOutput,
			iRequired + 1u,
			pParameter->Value.Data,
			pParameter->Value.Size
		) && (sOutput != pParameter->Value.Data) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( !__xrtMailParamDecodeBody(
			pParameter,
			Encoded,
			sOutput,
			&iRequired
		) ) {
			return false;
		}
		sOutput[iRequired] = 0;
	}
	*pOutputSize = iRequired;
	if ( pCharset != NULL ) {
		*pCharset = Charset;
	}
	if ( pLanguage != NULL ) {
		*pLanguage = Language;
	}
	return true;
}



/* 查找一种参数表示并校验重复和 section 连续性。 */
static xmailnext __xrtMailParamSelect(
	xstrview Parameters,
	xstrview Name,
	xmailparamview* pSingle,
	bool* pContinued,
	size_t* pSections
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailparamview Plain;
	xmailparamview Extended;
	xmailnext Next;
	size_t iContinuedCount = 0;
	size_t iMaxSection = 0;
	bool bPlain = false;
	bool bExtended = false;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return XMAIL_NEXT_ERROR;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailAsciiEqualI(Parameter.Name, Name) ) {
			continue;
		}
		if ( Parameter.Continued ) {
			iContinuedCount++;
			if ( Parameter.Section > iMaxSection ) {
				iMaxSection = Parameter.Section;
			}
		} else if ( Parameter.Extended ) {
			if ( bExtended ) {
				__xrtMailParamError("MIME parameter has duplicate extended values");
				return XMAIL_NEXT_ERROR;
			}
			Extended = Parameter;
			bExtended = true;
		} else {
			if ( bPlain ) {
				__xrtMailParamError("MIME parameter has duplicate values");
				return XMAIL_NEXT_ERROR;
			}
			Plain = Parameter;
			bPlain = true;
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( iContinuedCount != 0 ) {
		if ( (iMaxSection + 1u) != iContinuedCount ) {
			__xrtMailParamError("MIME parameter sections are missing or duplicated");
			return XMAIL_NEXT_ERROR;
		}
		*pContinued = true;
		*pSections = iContinuedCount;
		return XMAIL_NEXT_ITEM;
	}
	if ( bExtended || bPlain ) {
		*pSingle = bExtended ? Extended : Plain;
		*pContinued = false;
		*pSections = 1u;
		return XMAIL_NEXT_ITEM;
	}
	return XMAIL_NEXT_END;
}



/* 在连续表示中查找唯一 section。 */
static bool __xrtMailParamFindSection(
	xstrview Parameters,
	xstrview Name,
	size_t iSection,
	xmailparamview* pParameter
)
{
	xmailparamcursor Cursor;
	xmailparamview Parameter;
	xmailnext Next;
	bool bFound = false;

	if ( !xrtMailParamCursorInit(&Cursor, Parameters) ) {
		return false;
	}
	while ( (Next = xrtMailParamNext(&Cursor, &Parameter)) == XMAIL_NEXT_ITEM ) {
		if ( Parameter.Continued && (Parameter.Section == iSection) &&
			 __xrtMailAsciiEqualI(Parameter.Name, Name) ) {
			if ( bFound ) {
				return __xrtMailParamError("MIME parameter section is duplicated");
			}
			*pParameter = Parameter;
			bFound = true;
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	return bFound || __xrtMailParamError("MIME parameter section is missing");
}



/* 查找、合并并解码 MIME 参数。 */
XRT_API xmailnext xrtMailParamFindWrite(
	xstrview Parameters,
	xstrview Name,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
)
{
	xmailparamview Parameter;
	xmailparaminfo Info;
	xmailnext Next;
	size_t iSections;
	size_t iRequired = 0;
	bool bContinued;

	if ( !__xrtMailViewValid(Parameters) || !__xrtMailViewValid(Name) ||
		 (Name.Size == 0) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 !xrtMemRangeValid(pInfo, pInfo != NULL ? sizeof(*pInfo) : 0) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Parameters.Data, Parameters.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Name.Data, Name.Size) ||
		 ((pInfo != NULL) &&
		  (xrtMemRangesOverlap(pInfo, sizeof(*pInfo),
			Parameters.Data, Parameters.Size) ||
		   xrtMemRangesOverlap(pInfo, sizeof(*pInfo), Name.Data, Name.Size) ||
		   xrtMemRangesOverlap(pInfo, sizeof(*pInfo),
			pOutputSize, sizeof(*pOutputSize)))) ||
		 ((sOutput != NULL) && xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			Parameters.Data,
			Parameters.Size
		)) || ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(sOutput, iCapacity, Name.Data, Name.Size) ||
		   xrtMemRangesOverlap(sOutput, iCapacity,
			pOutputSize, sizeof(*pOutputSize)) ||
		   ((pInfo != NULL) && xrtMemRangesOverlap(
			sOutput, iCapacity, pInfo, sizeof(*pInfo))))) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		if ( !__xrtMailParamTokenByte((unsigned char)Name.Data[i]) ||
			 (Name.Data[i] == '*') ) {
			__xrtMailSetInvalidArgument();
			return XMAIL_NEXT_ERROR;
		}
	}
	Next = __xrtMailParamSelect(
		Parameters,
		Name,
		&Parameter,
		&bContinued,
		&iSections
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	memset(&Info, 0, sizeof(Info));
	Info.Sections = iSections;
	Info.Continued = bContinued;
	for ( size_t i = 0; i < iSections; i++ ) {
		xstrview Charset;
		xstrview Language;
		size_t iPartSize;

		if ( bContinued && !__xrtMailParamFindSection(
			Parameters,
			Name,
			i,
			&Parameter
		) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( !xrtMailParamDecodeWrite(
			&Parameter,
			NULL,
			0,
			&iPartSize,
			&Charset,
			&Language
		) || !__xrtMailSizeAdd(iRequired, iPartSize, &iRequired) ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( i == 0 ) {
			Info.Charset = Charset;
			Info.Language = Language;
		}
		Info.Extended = Info.Extended || Parameter.Extended;
	}
	if ( sOutput != NULL ) {
		size_t iOutput = 0;

		if ( iCapacity <= iRequired ) {
			*pOutputSize = iRequired;
			__xrtMailSetRange();
			return XMAIL_NEXT_ERROR;
		}
		for ( size_t i = 0; i < iSections; i++ ) {
			size_t iPartSize;

			if ( bContinued && !__xrtMailParamFindSection(
				Parameters,
				Name,
				i,
				&Parameter
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			if ( !xrtMailParamDecodeWrite(
				&Parameter,
				sOutput + iOutput,
				iCapacity - iOutput,
				&iPartSize,
				NULL,
				NULL
			) ) {
				return XMAIL_NEXT_ERROR;
			}
			iOutput += iPartSize;
		}
		sOutput[iRequired] = 0;
	}
	*pOutputSize = iRequired;
	if ( pInfo != NULL ) {
		*pInfo = Info;
	}
	return XMAIL_NEXT_ITEM;
}



/* 创建独立的合并 MIME 参数。 */
XRT_API str xrtMailParamFind(
	xstrview Parameters,
	xstrview Name,
	size_t* pOutputSize,
	xmailparaminfo* pInfo
)
{
	xmailparaminfo Info;
	xmailnext Next;
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMemRangeValid(pInfo, pInfo != NULL ? sizeof(*pInfo) : 0) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	Next = xrtMailParamFindWrite(
		Parameters,
		Name,
		NULL,
		0,
		&iRequired,
		&Info
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( xrtMailParamFindWrite(
		Parameters,
		Name,
		sOutput,
		iRequired + 1u,
		&iRequired,
		&Info
	) != XMAIL_NEXT_ITEM ) {
		xrtFree(sOutput);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	if ( pInfo != NULL ) {
		*pInfo = Info;
	}
	return sOutput;
}



/* 判断字节能否直接出现在 RFC 2231 扩展参数值中。 */
static bool __xrtMailParamAttributeByte(unsigned char iByte)
{
	if ( ((iByte >= (unsigned char)'a') && (iByte <= (unsigned char)'z')) ||
		 ((iByte >= (unsigned char)'A') && (iByte <= (unsigned char)'Z')) ||
		 ((iByte >= (unsigned char)'0') && (iByte <= (unsigned char)'9')) ) {
		return true;
	}
	switch ( iByte ) {
		case '!':
		case '#':
		case '$':
		case '&':
		case '+':
		case '-':
		case '.':
		case '^':
		case '_':
		case '`':
		case '|':
		case '~':
			return true;
		default:
			return false;
	}
}



/* 验证参数名和值，选择编码并精确计量。 */
static bool __xrtMailParamWriteMeasure(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	xmailparamencoding* pSelected,
	size_t* pValueSize,
	size_t* pRequired
)
{
	bool bAscii = true;
	bool bToken = Value.Size != 0;
	size_t iValueSize = 0;
	size_t iRequired;

	if ( !__xrtMailViewValid(Name) || !__xrtMailViewValid(Value) ||
		 (Name.Size == 0) ||
		 (Encoding < XMAIL_PARAM_ENCODING_AUTO) ||
		 (Encoding > XMAIL_PARAM_ENCODING_UTF8) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Name.Size; i++ ) {
		if ( !__xrtMailParamTokenByte((unsigned char)Name.Data[i]) ||
			 (Name.Data[i] == '*') ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (iByte == 0) || (iByte == 127u) ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		bAscii = bAscii && (iByte < 128u);
		bToken = bToken && __xrtMailParamTokenByte(iByte);
	}
	if ( !bAscii && !xrtUtf8Valid(Value, NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Encoding == XMAIL_PARAM_ENCODING_AUTO ) {
		Encoding = bToken ? XMAIL_PARAM_ENCODING_TOKEN :
			(bAscii ? XMAIL_PARAM_ENCODING_QUOTED :
			 XMAIL_PARAM_ENCODING_UTF8);
	} else if ( ((Encoding == XMAIL_PARAM_ENCODING_TOKEN) && !bToken) ||
		 ((Encoding == XMAIL_PARAM_ENCODING_QUOTED) && !bAscii) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}

	if ( Encoding == XMAIL_PARAM_ENCODING_TOKEN ) {
		iValueSize = Value.Size;
	} else if ( Encoding == XMAIL_PARAM_ENCODING_QUOTED ) {
		iValueSize = 2u;
		for ( size_t i = 0; i < Value.Size; i++ ) {
			if ( ((Value.Data[i] == '\"') || (Value.Data[i] == '\\')) &&
				 !__xrtMailSizeAdd(iValueSize, 1u, &iValueSize) ) {
				return false;
			}
			if ( !__xrtMailSizeAdd(iValueSize, 1u, &iValueSize) ) {
				return false;
			}
		}
	} else {
		iValueSize = 8u;
		for ( size_t i = 0; i < Value.Size; i++ ) {
			size_t iByteSize = __xrtMailParamAttributeByte(
				(unsigned char)Value.Data[i]
			) ? 1u : 3u;

			if ( !__xrtMailSizeAdd(iValueSize, iByteSize, &iValueSize) ) {
				return false;
			}
		}
	}
	if ( !__xrtMailSizeAdd(2u, Name.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iValueSize, &iRequired) ) {
		return false;
	}
	*pSelected = Encoding;
	*pValueSize = iValueSize -
		(Encoding == XMAIL_PARAM_ENCODING_QUOTED ? 2u :
		 (Encoding == XMAIL_PARAM_ENCODING_UTF8 ? 7u : 0u));
	*pRequired = iRequired;
	return true;
}



/* 返回连续段编号的十进制位数。 */
static size_t __xrtMailParamDigits(size_t iValue)
{
	size_t iDigits = 1u;

	while ( iValue >= 10u ) {
		iValue /= 10u;
		iDigits++;
	}
	return iDigits;
}



/* 返回一个源字节编码后的参数值长度。 */
static size_t __xrtMailParamEncodedByte(
	xmailparamencoding Encoding,
	unsigned char iByte
)
{
	if ( Encoding == XMAIL_PARAM_ENCODING_QUOTED ) {
		return ((iByte == (unsigned char)'\"') ||
			(iByte == (unsigned char)'\\')) ? 2u : 1u;
	}
	if ( Encoding == XMAIL_PARAM_ENCODING_UTF8 ) {
		return __xrtMailParamAttributeByte(iByte) ? 1u : 3u;
	}
	return 1u;
}



/* 返回一个源字符占用的字节数和编码后长度。 */
static void __xrtMailParamEncodedUnit(
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iPosition,
	size_t* pSourceSize,
	size_t* pEncodedSize
)
{
	unsigned char iByte = (unsigned char)Value.Data[iPosition];
	size_t iSourceSize = 1u;
	size_t iEncodedSize = __xrtMailParamEncodedByte(Encoding, iByte);

	if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) && (iByte >= 128u) ) {
		iSourceSize = (iByte < 224u) ? 2u : ((iByte < 240u) ? 3u : 4u);
		iEncodedSize = iSourceSize * 3u;
	}
	*pSourceSize = iSourceSize;
	*pEncodedSize = iEncodedSize;
}



/* 在不拆开转义单元或 UTF-8 字符的前提下取得下一个连续段。 */
static void __xrtMailParamChunk(
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iPosition,
	size_t* pEnd,
	size_t* pEncodedSize
)
{
	size_t iSize = 0;
	size_t iEnd = iPosition;

	while ( iEnd < Value.Size ) {
		size_t iSourceSize;
		size_t iUnitSize;

		__xrtMailParamEncodedUnit(
			Value,
			Encoding,
			iEnd,
			&iSourceSize,
			&iUnitSize
		);
		if ( (iSize != 0) && ((iSize + iUnitSize) > XMAIL_PARAM_SECTION_SIZE) ) {
			break;
		}
		iSize += iUnitSize;
		iEnd += iSourceSize;
	}
	*pEnd = iEnd;
	*pEncodedSize = iSize;
}



/* 精确计量自动连续分段后的参数文本。 */
static bool __xrtMailParamSectionsMeasure(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t iValueSize,
	size_t iSingleSize,
	size_t* pRequired,
	size_t* pSections
)
{
	size_t iRequired = 0;
	size_t iPosition = 0;
	size_t iSection = 0;

	if ( iValueSize <= XMAIL_PARAM_SECTION_SIZE ) {
		*pRequired = iSingleSize;
		*pSections = 1u;
		return true;
	}
	while ( iPosition < Value.Size ) {
		size_t iEnd;
		size_t iEncodedSize;
		size_t iPartSize;

		if ( iSection >= XMAIL_PARAM_SECTIONS_MAX ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"MIME parameter requires too many continuation sections"
			);
			return false;
		}
		__xrtMailParamChunk(Value, Encoding, iPosition, &iEnd, &iEncodedSize);
		iPartSize = 2u;
		if ( !__xrtMailSizeAdd(iPartSize, Name.Size, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, 1u, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, __xrtMailParamDigits(iSection),
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize,
				Encoding == XMAIL_PARAM_ENCODING_UTF8 ? 1u : 0u,
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, 1u, &iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize,
				Encoding == XMAIL_PARAM_ENCODING_QUOTED ? 2u :
				((Encoding == XMAIL_PARAM_ENCODING_UTF8) && (iSection == 0) ?
				 7u : 0u),
				&iPartSize) ||
			 !__xrtMailSizeAdd(iPartSize, iEncodedSize, &iPartSize) ||
			 !__xrtMailSizeAdd(iRequired, iPartSize, &iRequired) ) {
			return false;
		}
		iPosition = iEnd;
		iSection++;
	}
	*pRequired = iRequired;
	*pSections = iSection;
	return true;
}



/* 写出一个十进制连续段编号。 */
static size_t __xrtMailParamWriteDigits(char* sOutput, size_t iValue)
{
	char arrDigits[32];
	size_t iDigits = 0;

	do {
		arrDigits[iDigits++] = (char)('0' + (iValue % 10u));
		iValue /= 10u;
	} while ( iValue != 0 );
	for ( size_t i = 0; i < iDigits; i++ ) {
		sOutput[i] = arrDigits[iDigits - i - 1u];
	}
	return iDigits;
}



/* 写出一段 token、quoted-string 或扩展参数值。 */
static size_t __xrtMailParamWriteValue(
	char* sOutput,
	xstrview Value,
	xmailparamencoding Encoding,
	bool bFirst,
	bool bQuoted
)
{
	static const char arrHex[] = "0123456789ABCDEF";
	size_t iOutput = 0;

	if ( bQuoted ) {
		sOutput[iOutput++] = '\"';
	} else if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) && bFirst ) {
		memcpy(sOutput + iOutput, "UTF-8''", 7u);
		iOutput += 7u;
	}
	for ( size_t i = 0; i < Value.Size; i++ ) {
		unsigned char iByte = (unsigned char)Value.Data[i];

		if ( (Encoding == XMAIL_PARAM_ENCODING_QUOTED) &&
			 ((iByte == (unsigned char)'\"') ||
			  (iByte == (unsigned char)'\\')) ) {
			sOutput[iOutput++] = '\\';
			sOutput[iOutput++] = (char)iByte;
		} else if ( (Encoding == XMAIL_PARAM_ENCODING_UTF8) &&
			 !__xrtMailParamAttributeByte(iByte) ) {
			sOutput[iOutput++] = '%';
			sOutput[iOutput++] = arrHex[iByte >> 4];
			sOutput[iOutput++] = arrHex[iByte & 15u];
		} else {
			sOutput[iOutput++] = (char)iByte;
		}
	}
	if ( bQuoted ) {
		sOutput[iOutput++] = '\"';
	}
	return iOutput;
}



/* 写出包含分号前缀的 MIME 参数，长值自动使用连续段。 */
XRT_API bool xrtMailParamWrite(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	xmailparamencoding Selected;
	size_t iValueSize;
	size_t iRequired;
	size_t iSections;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Name.Data,
			Name.Size
		 ) || xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			Value.Data,
			Value.Size
		 ) || ((sOutput != NULL) && xrtMemRangesOverlap(
			sOutput,
			iCapacity,
			pOutputSize,
			sizeof(*pOutputSize)
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailParamWriteMeasure(
		Name,
		Value,
		Encoding,
		&Selected,
		&iValueSize,
		&iRequired
	) || !__xrtMailParamSectionsMeasure(
		Name,
		Value,
		Selected,
		iValueSize,
		iRequired,
		&iRequired,
		&iSections
	) ) {
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
	if ( xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Name.Data,
		Name.Size
	) || xrtMemRangesOverlap(
		sOutput,
		iRequired + 1u,
		Value.Data,
		Value.Size
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}

	if ( iSections == 1u ) {
		sOutput[iOutput++] = ';';
		sOutput[iOutput++] = ' ';
		memcpy(sOutput + iOutput, Name.Data, Name.Size);
		iOutput += Name.Size;
		if ( Selected == XMAIL_PARAM_ENCODING_UTF8 ) {
			sOutput[iOutput++] = '*';
		}
		sOutput[iOutput++] = '=';
		iOutput += __xrtMailParamWriteValue(
			sOutput + iOutput,
			Value,
			Selected,
			true,
			Selected == XMAIL_PARAM_ENCODING_QUOTED
		);
	} else {
		size_t iPosition = 0;

		for ( size_t iSection = 0; iSection < iSections; iSection++ ) {
			size_t iEnd;
			size_t iEncodedSize;

			__xrtMailParamChunk(Value, Selected, iPosition,
				&iEnd, &iEncodedSize);
			(void)iEncodedSize;
			sOutput[iOutput++] = ';';
			sOutput[iOutput++] = ' ';
			memcpy(sOutput + iOutput, Name.Data, Name.Size);
			iOutput += Name.Size;
			sOutput[iOutput++] = '*';
			iOutput += __xrtMailParamWriteDigits(
				sOutput + iOutput,
				iSection
			);
			if ( Selected == XMAIL_PARAM_ENCODING_UTF8 ) {
				sOutput[iOutput++] = '*';
			}
			sOutput[iOutput++] = '=';
			iOutput += __xrtMailParamWriteValue(
				sOutput + iOutput,
				__xrtMailView(Value.Data + iPosition, iEnd - iPosition),
				Selected,
				iSection == 0,
				Selected == XMAIL_PARAM_ENCODING_QUOTED
			);
			iPosition = iEnd;
		}
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 创建独立的单个 MIME 参数。 */
XRT_API str xrtMailParam(
	xstrview Name,
	xstrview Value,
	xmailparamencoding Encoding,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( (pOutputSize != NULL) && (xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Name.Data,
		Name.Size
	) || xrtMemRangesOverlap(
		pOutputSize,
		sizeof(*pOutputSize),
		Value.Data,
		Value.Size
	)) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !xrtMailParamWrite(
		Name,
		Value,
		Encoding,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailParamWrite(
		Name,
		Value,
		Encoding,
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
/* source: extlibs/xmail/src/mail/mail_multipart.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_MULTIPART)



#if defined(XMAIL_FEATURE_MAIL_MULTIPART)

/* 发布 multipart 结构错误。 */
static xmailnext __xrtMailMultipartError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return XMAIL_NEXT_ERROR;
}



/* 判断位置是否位于 MIME 物理行首。 */
static bool __xrtMailMultipartLineStart(xstrview Body, size_t iPosition)
{
	return (iPosition == 0) ||
		((iPosition >= 2u) && (Body.Data[iPosition - 2u] == '\r') &&
		 (Body.Data[iPosition - 1u] == '\n'));
}



/* 验证一个候选位置并返回 delimiter 行后的第一个字节。 */
static bool __xrtMailMultipartDelimiterAt(
	xstrview Body,
	xstrview Boundary,
	size_t iPosition,
	size_t* pAfter,
	bool* pClose
)
{
	size_t i = iPosition;
	bool bClose = false;

	if ( !__xrtMailMultipartLineStart(Body, iPosition) ||
		 ((Body.Size - iPosition) < (Boundary.Size + 2u)) ||
		 (Body.Data[i++] != '-') || (Body.Data[i++] != '-') ||
		 (memcmp(Body.Data + i, Boundary.Data, Boundary.Size) != 0) ) {
		return false;
	}
	i += Boundary.Size;
	if ( ((i + 1u) < Body.Size) && (Body.Data[i] == '-') &&
		 (Body.Data[i + 1u] == '-') ) {
		bClose = true;
		i += 2u;
	}
	while ( (i < Body.Size) &&
		 ((Body.Data[i] == ' ') || (Body.Data[i] == '\t')) ) {
		i++;
	}
	if ( i == Body.Size ) {
		*pAfter = i;
		*pClose = bClose;
		return true;
	}
	if ( ((i + 1u) >= Body.Size) || (Body.Data[i] != '\r') ||
		 (Body.Data[i + 1u] != '\n') ) {
		return false;
	}
	*pAfter = i + 2u;
	*pClose = bClose;
	return true;
}



/* 从指定位置寻找下一条严格 boundary delimiter 行。 */
static bool __xrtMailMultipartDelimiter(
	xstrview Body,
	xstrview Boundary,
	size_t iStart,
	size_t* pPosition,
	size_t* pAfter,
	bool* pClose
)
{
	if ( (Boundary.Size + 2u) > Body.Size ) {
		return false;
	}
	for ( size_t i = iStart; (i + Boundary.Size + 2u) <= Body.Size; i++ ) {
		if ( (Body.Data[i] == '-') && (Body.Data[i + 1u] == '-') &&
			 __xrtMailMultipartDelimiterAt(
				Body,
				Boundary,
				i,
				pAfter,
				pClose
			) ) {
			*pPosition = i;
			return true;
		}
	}
	return false;
}



/* 初始化零分配 multipart 游标并定位首条分隔线。 */
XRT_API bool xrtMailMultipartCursorInit(
	xmailmultipartcursor* pCursor,
	xstrview Body,
	xstrview Boundary,
	size_t iMaxParts
)
{
	xmailmultipartcursor Cursor;
	size_t iBoundary;
	size_t iAfter;
	size_t iPreambleEnd;
	bool bClose;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Body) || !__xrtMailViewValid(Boundary) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Body.Data, Body.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor),
			Boundary.Data, Boundary.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBoundaryValid(Boundary) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart boundary is invalid");
		return false;
	}
	if ( !__xrtMailMultipartDelimiter(
		Body,
		Boundary,
		0,
		&iBoundary,
		&iAfter,
		&bClose
	) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart body has no opening boundary");
		return false;
	}
	iPreambleEnd = (iBoundary >= 2u) ? iBoundary - 2u : iBoundary;
	memset(&Cursor, 0, sizeof(Cursor));
	Cursor.Source = Body;
	Cursor.Boundary = Boundary;
	Cursor.Preamble = __xrtMailView(Body.Data, iPreambleEnd);
	Cursor.Position = iAfter;
	Cursor.MaxParts = iMaxParts == 0 ? XMAIL_MULTIPART_PARTS_DEFAULT : iMaxParts;
	Cursor.Closed = bClose;
	Cursor.Done = bClose;
	if ( bClose ) {
		Cursor.Epilogue = __xrtMailView(
			Body.Data + iAfter,
			Body.Size - iAfter
		);
	}
	*pCursor = Cursor;
	return true;
}



/* 验证一个 part 的字段块，并保持 header 游标为唯一语法实现。 */
static bool __xrtMailMultipartHeaders(xstrview Headers)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;

	if ( Headers.Size == 0 ) {
		return true;
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, Headers) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
	}
	return Next == XMAIL_NEXT_END;
}



/* 把一段 MIME entity 拆成字段块和正文。 */
static bool __xrtMailMultipartPart(
	xstrview Source,
	xmailmultipartview* pPart
)
{
	xmailmultipartview Part;
	size_t iSeparator = XRT_NPOS;
	size_t iBodyStart;

	Part.Source = Source;
	if ( Source.Size == 0 ) {
		Part.Headers = __xrtMailView(Source.Data, 0);
		Part.Body = __xrtMailView(Source.Data, 0);
		*pPart = Part;
		return true;
	}
	if ( (Source.Size >= 2u) && (Source.Data[0] == '\r') &&
		 (Source.Data[1] == '\n') ) {
		iSeparator = 0;
		iBodyStart = 2u;
	} else {
		for ( size_t i = 0; (i + 3u) < Source.Size; i++ ) {
			if ( (Source.Data[i] == '\r') && (Source.Data[i + 1u] == '\n') &&
				 (Source.Data[i + 2u] == '\r') &&
				 (Source.Data[i + 3u] == '\n') ) {
				iSeparator = i;
				iBodyStart = i + 4u;
				break;
			}
		}
	}
	if ( iSeparator == XRT_NPOS ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart part has no header/body separator");
		return false;
	}
	Part.Headers = __xrtMailView(Source.Data, iSeparator);
	Part.Body = __xrtMailView(
		Source.Data + iBodyStart,
		Source.Size - iBodyStart
	);
	if ( !__xrtMailMultipartHeaders(Part.Headers) ) {
		return false;
	}
	*pPart = Part;
	return true;
}



/* 返回下一 multipart part。 */
XRT_API xmailnext xrtMailMultipartNext(
	xmailmultipartcursor* pCursor,
	xmailmultipartview* pPart
)
{
	xmailmultipartcursor Cursor;
	xmailmultipartview Part;
	size_t iBoundary;
	size_t iAfter;
	size_t iSourceEnd;
	bool bClose;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pPart, sizeof(*pPart)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Source :
			__xrtMailView(NULL, 0)) ||
		 !__xrtMailViewValid(pCursor != NULL ? pCursor->Boundary :
			__xrtMailView(NULL, 0)) ||
		 (pCursor->Position > pCursor->Source.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pPart, sizeof(*pPart)) ||
		 xrtMemRangesOverlap(pPart, sizeof(*pPart),
			pCursor->Source.Data, pCursor->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	Cursor = *pCursor;
	if ( (Cursor.MaxParts != SIZE_MAX) && (Cursor.Parts >= Cursor.MaxParts) ) {
		__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT,
			"multipart part limit exceeded");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailMultipartDelimiter(
		Cursor.Source,
		Cursor.Boundary,
		Cursor.Position,
		&iBoundary,
		&iAfter,
		&bClose
	) ) {
		return __xrtMailMultipartError(
			"multipart body has no closing boundary"
		);
	}
	if ( (iBoundary < 2u) || (iBoundary < Cursor.Position) ) {
		return __xrtMailMultipartError("multipart boundary position is invalid");
	}
	iSourceEnd = iBoundary - 2u;
	if ( iSourceEnd < Cursor.Position ) {
		return __xrtMailMultipartError("multipart part framing is invalid");
	}
	if ( !__xrtMailMultipartPart(
		__xrtMailView(
			Cursor.Source.Data + Cursor.Position,
			iSourceEnd - Cursor.Position
		),
		&Part
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	Cursor.Parts++;
	Cursor.Position = iAfter;
	Cursor.Closed = bClose;
	Cursor.Done = bClose;
	if ( bClose ) {
		Cursor.Epilogue = __xrtMailView(
			Cursor.Source.Data + iAfter,
			Cursor.Source.Size - iAfter
		);
	}
	*pPart = Part;
	*pCursor = Cursor;
	return XMAIL_NEXT_ITEM;
}



/* 写入 multipart 分隔片段。 */
XRT_API bool xrtMailMultipartMarkWrite(
	xstrview Boundary,
	xmailmultipartmark Mark,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iPrefix;
	size_t iSuffix;
	size_t iRequired;

	if ( !__xrtMailViewValid(Boundary) || !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 ((Mark != XMAIL_MULTIPART_FIRST) &&
		  (Mark != XMAIL_MULTIPART_NEXT) &&
		  (Mark != XMAIL_MULTIPART_CLOSE)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			Boundary.Data, Boundary.Size) ||
		 ((sOutput != NULL) &&
		  (xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ||
		   xrtMemRangesOverlap(sOutput, iCapacity, Boundary.Data, Boundary.Size))) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBoundaryValid(Boundary) ) {
		__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME,
			"multipart boundary is invalid");
		return false;
	}
	iPrefix = Mark == XMAIL_MULTIPART_FIRST ? 2u : 4u;
	iSuffix = Mark == XMAIL_MULTIPART_CLOSE ? 4u : 2u;
	if ( !__xrtMailSizeAdd(iPrefix, Boundary.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iSuffix, &iRequired) ) {
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
	if ( Mark == XMAIL_MULTIPART_FIRST ) {
		memcpy(sOutput, "--", 2u);
	} else {
		memcpy(sOutput, "\r\n--", 4u);
	}
	memcpy(sOutput + iPrefix, Boundary.Data, Boundary.Size);
	if ( Mark == XMAIL_MULTIPART_CLOSE ) {
		memcpy(sOutput + iPrefix + Boundary.Size, "--\r\n", 4u);
	} else {
		memcpy(sOutput + iPrefix + Boundary.Size, "\r\n", 2u);
	}
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/transport/mail_wire.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_WIRE)



#if defined(XMAIL_FEATURE_MAIL_WIRE)

/* 从增量输入探测一条严格 CRLF 线路。 */
XRT_API xmailnext xrtMailLineRead(
	xstrview Data,
	size_t iMaxLine,
	xstrview* pLine,
	size_t* pConsumed
)
{
	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pLine, sizeof(*pLine)) ||
		 !xrtMemRangeValid(pConsumed, sizeof(*pConsumed)) ||
		 xrtMemRangesOverlap(pLine, sizeof(*pLine), pConsumed,
			sizeof(*pConsumed)) ||
		 xrtMemRangesOverlap(pLine, sizeof(*pLine), Data.Data, Data.Size) ||
		 xrtMemRangesOverlap(pConsumed, sizeof(*pConsumed), Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( iMaxLine == 0 ) {
		iMaxLine = XMAIL_WIRE_LINE_DEFAULT;
	}
	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( Data.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail wire contains a bare LF"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( Data.Data[i] != '\r' ) {
			continue;
		}
		if ( (i + 1u) == Data.Size ) {
			if ( i > iMaxLine ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"mail wire line exceeds the byte limit"
				);
				return XMAIL_NEXT_ERROR;
			}
			return XMAIL_NEXT_END;
		}
		if ( Data.Data[i + 1u] != '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail wire contains a bare CR"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( i > iMaxLine ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail wire line exceeds the byte limit"
			);
			return XMAIL_NEXT_ERROR;
		}
		*pLine = __xrtMailSlice(Data, 0, i);
		*pConsumed = i + 2u;
		return XMAIL_NEXT_ITEM;
	}
	if ( Data.Size > iMaxLine ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"mail wire line exceeds the byte limit"
		);
		return XMAIL_NEXT_ERROR;
	}
	return XMAIL_NEXT_END;
}



/* 去除一条线路的 dot transparency 前缀。 */
XRT_API xmailnext xrtMailDotLine(xstrview Line, xstrview* pData)
{
	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pData, sizeof(*pData)) ||
		 xrtMemRangesOverlap(pData, sizeof(*pData), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Line.Size == 1u) && (Line.Data[0] == '.') ) {
		return XMAIL_NEXT_END;
	}
	*pData = (Line.Size != 0) && (Line.Data[0] == '.') ?
		__xrtMailSlice(Line, 1u, Line.Size - 1u) : Line;
	return XMAIL_NEXT_ITEM;
}



/* 将一个非空片段交给增量 sink，并保留原始回调错误。 */
static bool __xrtMailDotEmit(
	xmailwriteproc pWrite,
	ptr pUserData,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;

	if ( iSize == 0 ) {
		return true;
	}
	Data.Data = (const unsigned char*)pData;
	Data.Size = iSize;
	if ( pWrite(Data, pUserData) ) {
		return true;
	}
	if ( xrtGetError() == NULL ) {
		__xrtMailError(
			XERR_IO,
			XMAIL_ERROR_CALLBACK,
			"mail dot writer sink failed"
		);
	}
	return false;
}



/* 在发布任何输出前校验当前片段的 CRLF 结构。 */
static bool __xrtMailDotWriterValidate(
	const xmaildotwriter* pWriter,
	xbytesview Data
)
{
	size_t iPosition = 0;

	if ( pWriter->PendingCr ) {
		if ( Data.Size == 0 ) {
			return true;
		}
		if ( Data.Data[0] != (unsigned char)'\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot stream contains a bare CR"
			);
			return false;
		}
		iPosition = 1u;
	}
	while ( iPosition < Data.Size ) {
		unsigned char iByte = Data.Data[iPosition];

		if ( iByte == (unsigned char)'\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot stream contains a bare LF"
			);
			return false;
		}
		if ( iByte == (unsigned char)'\r' ) {
			if ( (iPosition + 1u) == Data.Size ) {
				return true;
			}
			if ( Data.Data[iPosition + 1u] != (unsigned char)'\n' ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot stream contains a bare CR"
				);
				return false;
			}
			iPosition += 2u;
			continue;
		}
		iPosition++;
	}
	return true;
}



/* 初始化增量 dot writer。 */
XRT_API bool xrtMailDotWriterInit(xmaildotwriter* pWriter)
{
	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memset(pWriter, 0, sizeof(*pWriter));
	pWriter->LineStart = true;
	return true;
}



/* 写出一个增量 dot-transparent 片段。 */
XRT_API bool xrtMailDotWriterWrite(
	xmaildotwriter* pWriter,
	xbytesview Data,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	xmaildotwriter Writer;
	size_t iPosition = 0;
	size_t iStart = 0;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		 !xrtMemRangeValid(Data.Data, Data.Size) || (pWrite == NULL) ||
		 pWriter->Finished ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotWriterValidate(pWriter, Data) ) {
		return false;
	}
	Writer = *pWriter;
	if ( Writer.PendingCr && (Data.Size != 0) ) {
		if ( !__xrtMailDotEmit(pWrite, pUserData, "\r\n", 2u) ) {
			pWriter->Finished = true;
			return false;
		}
		Writer.PendingCr = false;
		Writer.LineStart = true;
		iPosition = 1u;
		iStart = 1u;
	}
	while ( iPosition < Data.Size ) {
		unsigned char iByte = Data.Data[iPosition];

		if ( Writer.LineStart && (iByte == (unsigned char)'.') ) {
			if ( !__xrtMailDotEmit(
				pWrite,
				pUserData,
				Data.Data + iStart,
				iPosition - iStart
			) || !__xrtMailDotEmit(pWrite, pUserData, ".", 1u) ) {
				pWriter->Finished = true;
				return false;
			}
			iStart = iPosition;
		}
		if ( iByte == (unsigned char)'\r' ) {
			if ( (iPosition + 1u) == Data.Size ) {
				if ( !__xrtMailDotEmit(
					pWrite,
					pUserData,
					Data.Data + iStart,
					iPosition - iStart
				) ) {
					pWriter->Finished = true;
					return false;
				}
				Writer.PendingCr = true;
				iStart = Data.Size;
				break;
			}
			iPosition++;
			Writer.LineStart = true;
		} else {
			Writer.LineStart = false;
		}
		iPosition++;
	}
	if ( !__xrtMailDotEmit(
		pWrite,
		pUserData,
		Data.Data != NULL ? Data.Data + iStart : NULL,
		Data.Size - iStart
	) ) {
		pWriter->Finished = true;
		return false;
	}
	*pWriter = Writer;
	return true;
}



/* 完成增量 dot stream。 */
XRT_API bool xrtMailDotWriterFinish(
	xmaildotwriter* pWriter,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	cstr sTerminator;
	size_t iSize;

	if ( !xrtMemRangeValid(pWriter, sizeof(*pWriter)) ||
		 (pWrite == NULL) || pWriter->Finished ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pWriter->PendingCr ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_LINE,
			"mail dot stream ends with a bare CR"
		);
		return false;
	}
	sTerminator = pWriter->LineStart ? ".\r\n" : "\r\n.\r\n";
	iSize = pWriter->LineStart ? 3u : 5u;
	if ( !__xrtMailDotEmit(pWrite, pUserData, sTerminator, iSize) ) {
		pWriter->Finished = true;
		return false;
	}
	pWriter->Finished = true;
	return true;
}



/* 验证 dot 编码输入并计算额外前导点数量。 */
static bool __xrtMailDotMeasure(
	xstrview Data,
	bool Terminate,
	size_t* pRequired
)
{
	size_t iRequired = Data.Size;
	bool bLineStart = true;
	bool bEndsCrlf = false;

	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( bLineStart && (Data.Data[i] == '.') ) {
			if ( !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ) {
				return false;
			}
		}
		bLineStart = false;
		if ( Data.Data[i] == '\n' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot input contains a bare LF"
			);
			return false;
		}
		if ( Data.Data[i] != '\r' ) {
			continue;
		}
		if ( ((i + 1u) >= Data.Size) || (Data.Data[i + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail dot input contains a bare CR"
			);
			return false;
		}
		i++;
		bLineStart = true;
		bEndsCrlf = (i + 1u) == Data.Size;
	}
	if ( Terminate ) {
		if ( (Data.Size != 0) && !bEndsCrlf &&
			 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
			return false;
		}
		if ( !__xrtMailSizeAdd(iRequired, 3u, &iRequired) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 执行已经测量过的 dot 编码。 */
static void __xrtMailDotBody(
	xstrview Data,
	bool Terminate,
	bytes pOutput,
	size_t iRequired
)
{
	size_t iOutput = 0;
	bool bLineStart = true;
	bool bEndsCrlf = false;

	for ( size_t i = 0; i < Data.Size; i++ ) {
		if ( bLineStart && (Data.Data[i] == '.') ) {
			pOutput[iOutput++] = (uint8)'.';
		}
		pOutput[iOutput++] = (uint8)Data.Data[i];
		bLineStart = false;
		if ( Data.Data[i] == '\r' ) {
			pOutput[iOutput++] = (uint8)Data.Data[++i];
			bLineStart = true;
			bEndsCrlf = (i + 1u) == Data.Size;
		}
	}
	if ( Terminate && (Data.Size != 0) && !bEndsCrlf ) {
		pOutput[iOutput++] = (uint8)'\r';
		pOutput[iOutput++] = (uint8)'\n';
	}
	if ( Terminate ) {
		pOutput[iOutput++] = (uint8)'.';
		pOutput[iOutput++] = (uint8)'\r';
		pOutput[iOutput++] = (uint8)'\n';
	}
	(void)iRequired;
}



/* 写出 dot-transparent 数据。 */
XRT_API bool xrtMailDotWrite(
	xstrview Data,
	bool Terminate,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Data.Data,
			Data.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotMeasure(Data, Terminate, &iRequired) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailDotBody(Data, Terminate, (bytes)pOutput, iRequired);
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 dot-transparent 数据。 */
XRT_API bytes xrtMailDot(
	xstrview Data,
	bool Terminate,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDotWrite(Data, Terminate, NULL, 0, &iRequired) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDotWrite(
		Data,
		Terminate,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}



/* 计算或写出完整 dot-transparent 行块。 */
static bool __xrtMailDotDecodeBody(
	xstrview Data,
	bool RequireTerminator,
	bytes pOutput,
	size_t* pOutputSize
)
{
	size_t iPosition = 0;
	size_t iOutput = 0;
	bool bTerminated = false;

	while ( iPosition < Data.Size ) {
		xstrview Remaining = __xrtMailSlice(
			Data,
			iPosition,
			Data.Size - iPosition
		);
		xstrview Line;
		xstrview Plain;
		size_t iConsumed;
		xmailnext Next = xrtMailLineRead(
			Remaining,
			SIZE_MAX,
			&Line,
			&iConsumed
		);

		if ( Next != XMAIL_NEXT_ITEM ) {
			if ( Next == XMAIL_NEXT_END ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot block ends with an incomplete line"
				);
			}
			return false;
		}
		Next = xrtMailDotLine(Line, &Plain);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		iPosition += iConsumed;
		if ( Next == XMAIL_NEXT_END ) {
			if ( iPosition != Data.Size ) {
				__xrtMailError(
					XERR_PROTOCOL,
					XMAIL_ERROR_LINE,
					"mail dot block contains data after the terminator"
				);
				return false;
			}
			bTerminated = true;
			break;
		}
		if ( !__xrtMailSizeAdd(iOutput, Plain.Size, &iOutput) ||
			 !__xrtMailSizeAdd(iOutput, 2u, &iOutput) ) {
			return false;
		}
		if ( pOutput != NULL ) {
			memcpy(pOutput + iOutput - Plain.Size - 2u, Plain.Data, Plain.Size);
			pOutput[iOutput - 2u] = (uint8)'\r';
			pOutput[iOutput - 1u] = (uint8)'\n';
		}
	}
	if ( RequireTerminator && !bTerminated ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_LINE,
			"mail dot block has no terminator"
		);
		return false;
	}
	*pOutputSize = iOutput;
	return true;
}



/* 解码完整 dot-transparent 行块。 */
XRT_API bool xrtMailDotDecodeWrite(
	xstrview Data,
	bool RequireTerminator,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Data) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Data.Data,
			Data.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailDotDecodeBody(
		Data,
		RequireTerminator,
		NULL,
		&iRequired
	) ) {
		return false;
	}
	if ( pOutput == NULL ) {
		*pOutputSize = iRequired;
		return true;
	}
	if ( iCapacity < iRequired ) {
		*pOutputSize = iRequired;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, iRequired, Data.Data, Data.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailDotDecodeBody(
		Data,
		RequireTerminator,
		(bytes)pOutput,
		pOutputSize
	);
}



/* 分配并解码完整 dot-transparent 行块。 */
XRT_API bytes xrtMailDotDecode(
	xstrview Data,
	bool RequireTerminator,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailDotDecodeWrite(
		Data,
		RequireTerminator,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailDotDecodeWrite(
		Data,
		RequireTerminator,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_message.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_MESSAGE)



#if defined(XMAIL_FEATURE_MAIL_MESSAGE)

/* 检查消息视图的公开字段是否都具有合法内存范围。 */
static bool __xrtMailMessageViewValid(const xmailmessageview* pMessage)
{
	return xrtMemRangeValid(pMessage, pMessage != NULL ? sizeof(*pMessage) : 0) &&
		(pMessage != NULL) &&
		__xrtMailViewValid(pMessage->Source) &&
		__xrtMailViewValid(pMessage->Headers) &&
		__xrtMailViewValid(pMessage->Body);
}



/* 查找严格 CRLF 空行，并拒绝字段区中的裸换行。 */
static bool __xrtMailMessageSplit(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t* pHeaderSize,
	size_t* pBodyStart
)
{
	size_t iLineStart = 0;

	while ( iLineStart < Source.Size ) {
		size_t iLineEnd = iLineStart;

		while ( (iLineEnd < Source.Size) &&
			(Source.Data[iLineEnd] != '\r') &&
			(Source.Data[iLineEnd] != '\n') ) {
			iLineEnd++;
		}
		if ( (iLineEnd >= Source.Size) ||
			(Source.Data[iLineEnd] != '\r') ||
			((iLineEnd + 1u) >= Source.Size) ||
			(Source.Data[iLineEnd + 1u] != '\n') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_LINE,
				"mail message headers require a CRLF terminator"
			);
			return false;
		}
		if ( iLineEnd > iMaxHeaderBytes ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail message headers exceed the byte limit"
			);
			return false;
		}
		if ( iLineEnd == iLineStart ) {
			*pHeaderSize = iLineStart;
			*pBodyStart = iLineEnd + 2u;
			return true;
		}
		iLineStart = iLineEnd + 2u;
	}
	__xrtMailError(
		XERR_PROTOCOL,
		XMAIL_ERROR_HEADER,
		"mail message has no header separator"
	);
	return false;
}



/* 严格解析消息字段块与正文。 */
XRT_API bool xrtMailMessageParse(
	xstrview Source,
	size_t iMaxHeaderBytes,
	size_t iMaxHeaders,
	xmailmessageview* pMessage
)
{
	xmailmessageview Result;
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iHeaderSize;
	size_t iBodyStart;
	size_t iHeaderCount = 0;

	if ( !__xrtMailViewValid(Source) ||
		 !xrtMemRangeValid(pMessage, sizeof(*pMessage)) ||
		 xrtMemRangesOverlap(pMessage, sizeof(*pMessage), Source.Data, Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxHeaderBytes == 0 ) {
		iMaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	}
	if ( iMaxHeaders == 0 ) {
		iMaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
	}
	if ( !__xrtMailMessageSplit(
		Source,
		iMaxHeaderBytes,
		&iHeaderSize,
		&iBodyStart
	) ) {
		return false;
	}
	Result.Source = Source;
	Result.Headers = __xrtMailSlice(Source, 0, iHeaderSize);
	Result.Body = __xrtMailSlice(Source, iBodyStart, Source.Size - iBodyStart);
	if ( !xrtMailHeaderCursorInit(&Cursor, Result.Headers) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		if ( iHeaderCount == iMaxHeaders ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail message exceeds the header count limit"
			);
			return false;
		}
		iHeaderCount++;
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	Result.HeaderCount = iHeaderCount;
	*pMessage = Result;
	return true;
}



/* 查找指定序号的消息字段。 */
XRT_API xmailnext xrtMailMessageHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	size_t iOccurrence,
	xmailheaderview* pHeader
)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iFound = 0;

	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !__xrtMailViewValid(Name) ||
		 !xrtMemRangeValid(pHeader, sizeof(*pHeader)) ||
		 !xrtMailHeaderNameValid(Name) ||
		 xrtMemRangesOverlap(pHeader, sizeof(*pHeader),
			pMessage->Source.Data, pMessage->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, pMessage->Headers) ) {
		return XMAIL_NEXT_ERROR;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailAsciiEqualI(Header.Name, Name) ) {
			continue;
		}
		if ( iFound == iOccurrence ) {
			*pHeader = Header;
			return XMAIL_NEXT_ITEM;
		}
		iFound++;
	}
	return Next;
}



/* 跳过字段值首尾允许出现的空白和折叠空白。 */
static bool __xrtMailTransferSkipFws(xstrview Value, size_t* pPosition)
{
	size_t iPosition = *pPosition;

	while ( iPosition < Value.Size ) {
		if ( (Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t') ) {
			iPosition++;
			continue;
		}
		if ( Value.Data[iPosition] != '\r' ) {
			break;
		}
		if ( ((iPosition + 2u) >= Value.Size) ||
			 (Value.Data[iPosition + 1u] != '\n') ||
			 ((Value.Data[iPosition + 2u] != ' ') &&
			  (Value.Data[iPosition + 2u] != '\t')) ) {
			return false;
		}
		iPosition += 3u;
		while ( (iPosition < Value.Size) &&
			((Value.Data[iPosition] == ' ') ||
			 (Value.Data[iPosition] == '\t')) ) {
			iPosition++;
		}
	}
	*pPosition = iPosition;
	return true;
}



/* 解析正文传输编码 token。 */
XRT_API xmailtransfer xrtMailTransferParse(xstrview Value)
{
	xstrview Token;
	size_t iPosition = 0;
	size_t iStart;

	if ( !__xrtMailViewValid(Value) ||
		 !__xrtMailTransferSkipFws(Value, &iPosition) ) {
		return XMAIL_TRANSFER_UNKNOWN;
	}
	iStart = iPosition;
	while ( (iPosition < Value.Size) &&
		 (Value.Data[iPosition] != ' ') &&
		 (Value.Data[iPosition] != '\t') &&
		 (Value.Data[iPosition] != '\r') ) {
		if ( Value.Data[iPosition] == '\n' ) {
			return XMAIL_TRANSFER_UNKNOWN;
		}
		iPosition++;
	}
	Token = __xrtMailSlice(Value, iStart, iPosition - iStart);
	if ( !__xrtMailTransferSkipFws(Value, &iPosition) ||
		 (iPosition != Value.Size) || (Token.Size == 0) ) {
		return XMAIL_TRANSFER_UNKNOWN;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("7bit")) ) {
		return XMAIL_TRANSFER_7BIT;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("8bit")) ) {
		return XMAIL_TRANSFER_8BIT;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("binary")) ) {
		return XMAIL_TRANSFER_BINARY;
	}
	if ( __xrtMailAsciiEqualI(
		Token,
		XRT_STR_LITERAL("quoted-printable")
	) ) {
		return XMAIL_TRANSFER_QUOTED_PRINTABLE;
	}
	if ( __xrtMailAsciiEqualI(Token, XRT_STR_LITERAL("base64")) ) {
		return XMAIL_TRANSFER_BASE64;
	}
	return XMAIL_TRANSFER_UNKNOWN;
}



/* 读取并验证消息的唯一传输编码字段。 */
XRT_API bool xrtMailMessageTransfer(
	const xmailmessageview* pMessage,
	xmailtransfer* pTransfer
)
{
	xmailheaderview Header;
	xmailnext Next;
	xmailtransfer Transfer;

	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !xrtMemRangeValid(pTransfer, sizeof(*pTransfer)) ||
		 xrtMemRangesOverlap(pTransfer, sizeof(*pTransfer),
			pMessage->Source.Data, pMessage->Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Next = xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		0,
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		*pTransfer = XMAIL_TRANSFER_7BIT;
		return true;
	}
	if ( xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		1,
		&Header
	) != XMAIL_NEXT_END ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"mail message has duplicate transfer encoding fields"
		);
		return false;
	}
	Next = xrtMailMessageHeader(
		pMessage,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		0,
		&Header
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return false;
	}
	Transfer = xrtMailTransferParse(Header.Value);
	if ( Transfer == XMAIL_TRANSFER_UNKNOWN ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_ENCODING,
			"mail message uses an unsupported transfer encoding"
		);
		return false;
	}
	*pTransfer = Transfer;
	return true;
}



/* 复制无需转换的正文，保持查询和容量失败的事务语义。 */
static bool __xrtMailMessageBodyCopy(
	xstrview Body,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	if ( pOutput == NULL ) {
		*pOutputSize = Body.Size;
		return true;
	}
	if ( iCapacity < Body.Size ) {
		*pOutputSize = Body.Size;
		__xrtMailSetRange();
		return false;
	}
	if ( xrtMemRangesOverlap(pOutput, Body.Size, Body.Data, Body.Size) &&
		 (pOutput != (const void*)Body.Data) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memmove(pOutput, Body.Data, Body.Size);
	*pOutputSize = Body.Size;
	return true;
}



/* 按调用方选择的传输编码解码正文。 */
XRT_API bool xrtMailMessageBodyWrite(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	void* pOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	if ( !__xrtMailMessageViewValid(pMessage) ||
		 !xrtMemRangeValid(pOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			pMessage->Source.Data, pMessage->Source.Size) ||
		 ((pOutput != NULL) && xrtMemRangesOverlap(
			pOutputSize,
			sizeof(*pOutputSize),
			pOutput,
			iCapacity
		 )) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	switch ( Transfer ) {
		case XMAIL_TRANSFER_7BIT:
		case XMAIL_TRANSFER_8BIT:
		case XMAIL_TRANSFER_BINARY:
			return __xrtMailMessageBodyCopy(
				pMessage->Body,
				pOutput,
				iCapacity,
				pOutputSize
			);
		case XMAIL_TRANSFER_QUOTED_PRINTABLE:
			return xrtMailQpDecodeWrite(
				pMessage->Body,
				iFlags,
				pOutput,
				iCapacity,
				pOutputSize
			);
		case XMAIL_TRANSFER_BASE64:
			if ( iFlags != 0 ) {
				__xrtMailSetInvalidArgument();
				return false;
			}
			return xrtMailBase64DecodeWrite(
				pMessage->Body,
				pOutput,
				iCapacity,
				pOutputSize
			);
		default:
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_ENCODING,
				"unknown mail transfer encoding"
			);
			return false;
	}
}



/* 分配并解码消息正文。 */
XRT_API bytes xrtMailMessageBody(
	const xmailmessageview* pMessage,
	xmailtransfer Transfer,
	uint32 iFlags,
	size_t* pOutputSize
)
{
	size_t iRequired;
	bytes pOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtMailMessageBodyWrite(
		pMessage,
		Transfer,
		iFlags,
		NULL,
		0,
		&iRequired
	) ) {
		return NULL;
	}
	if ( iRequired == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return NULL;
	}
	pOutput = (bytes)xrtMalloc(iRequired + 1u);
	if ( pOutput == NULL ) {
		return NULL;
	}
	if ( !xrtMailMessageBodyWrite(
		pMessage,
		Transfer,
		iFlags,
		pOutput,
		iRequired,
		&iRequired
	) ) {
		xrtFree(pOutput);
		return NULL;
	}
	pOutput[iRequired] = 0;
	if ( pOutputSize != NULL ) {
		*pOutputSize = iRequired;
	}
	return pOutput;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_tree.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_TREE)



#if defined(XMAIL_FEATURE_MAIL_TREE)

#define __XMAIL_TREE_BLOCK_SIZE 4096u
#define __XMAIL_TREE_FLAGS \
	(XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER | XMAIL_TREE_RELAXED_QP | \
	 XMAIL_TREE_ALLOW_UNKNOWN_CHARSET)



/* 为不提供 max_align_t 的 C11 编译器描述普通对象最大对齐。 */
typedef union __xmailtreealign {
	void* Pointer;
	void (*Function)(void);
	long double Float;
	uint64 Integer;
} __xmailtreealign;



/* arena 块保证 Data 具有通用对象所需的对齐。 */
typedef struct __xmailtreeblock {
	struct __xmailtreeblock* Next;
	size_t Used;
	size_t Capacity;
	__xmailtreealign Align;
	unsigned char Data[];
} __xmailtreeblock;



/* 解析上下文集中维护预算和统一释放的 arena。 */
typedef struct __xmailtreecontext {
	xmailtree Tree;
	xmailtreelimits Limits;
	__xmailtreeblock* Blocks;
} __xmailtreecontext;



/* 发布 MIME 树协议错误。 */
static bool __xrtMailTreeError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_MIME, sMessage);
	return false;
}



/* 发布 MIME 树预算错误。 */
static bool __xrtMailTreeLimit(cstr sMessage)
{
	__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT, sMessage);
	return false;
}



/* 释放解析上下文的全部 arena 块。 */
static void __xrtMailTreeBlocksFree(__xmailtreeblock* pBlock)
{
	while ( pBlock != NULL ) {
		__xmailtreeblock* pNext = pBlock->Next;

		xrtFree(pBlock);
		pBlock = pNext;
	}
}



/* 从不移动的 arena 中分配通用对齐内存。 */
static void* __xrtMailTreeAlloc(
	__xmailtreecontext* pContext,
	size_t iSize
)
{
	const size_t iAlign = _Alignof(__xmailtreealign);
	__xmailtreeblock* pBlock = pContext->Blocks;
	size_t iPosition;
	size_t iCapacity;
	size_t iTotal;

	if ( iSize == 0 ) {
		return NULL;
	}
	if ( pBlock != NULL ) {
		iPosition = (pBlock->Used + iAlign - 1u) & ~(iAlign - 1u);
		if ( (iPosition <= pBlock->Capacity) &&
			(iSize <= (pBlock->Capacity - iPosition)) ) {
			void* pData = pBlock->Data + iPosition;

			pBlock->Used = iPosition + iSize;
			return pData;
		}
	}
	iCapacity = iSize > __XMAIL_TREE_BLOCK_SIZE ?
		iSize : __XMAIL_TREE_BLOCK_SIZE;
	if ( !__xrtMailSizeAdd(
		offsetof(__xmailtreeblock, Data),
		iCapacity,
		&iTotal
	) ) {
		return NULL;
	}
	pBlock = (__xmailtreeblock*)xrtMalloc(iTotal);
	if ( pBlock == NULL ) {
		return NULL;
	}
	pBlock->Next = pContext->Blocks;
	pBlock->Used = iSize;
	pBlock->Capacity = iCapacity;
	pContext->Blocks = pBlock;
	return pBlock->Data;
}



/* 把借用文本复制到 arena，并附加不计入视图的零字节。 */
static bool __xrtMailTreeCopy(
	__xmailtreecontext* pContext,
	xstrview Source,
	xstrview* pCopy
)
{
	char* sCopy;
	size_t iSize;

	if ( !__xrtMailSizeAdd(Source.Size, 1u, &iSize) ) {
		return false;
	}
	sCopy = (char*)__xrtMailTreeAlloc(pContext, iSize);
	if ( sCopy == NULL ) {
		return false;
	}
	if ( Source.Size != 0 ) {
		memcpy(sCopy, Source.Data, Source.Size);
	}
	sCopy[Source.Size] = 0;
	*pCopy = __xrtMailView(sCopy, Source.Size);
	return true;
}



/* 按名称取得唯一字段，不存在时返回 END。 */
static xmailnext __xrtMailTreeHeader(
	const xmailmessageview* pMessage,
	xstrview Name,
	xmailheaderview* pHeader
)
{
	xmailheaderview Duplicate;
	xmailnext Next = xrtMailMessageHeader(pMessage, Name, 0, pHeader);

	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	Next = xrtMailMessageHeader(pMessage, Name, 1u, &Duplicate);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_HEADER,
			"MIME entity has a duplicate singleton field"
		);
		return XMAIL_NEXT_ERROR;
	}
	return XMAIL_NEXT_ITEM;
}



/* 展开字段值并把结果存入 arena。 */
static bool __xrtMailTreeUnfold(
	__xmailtreecontext* pContext,
	xstrview Value,
	xstrview* pUnfolded
)
{
	char* sValue;
	size_t iSize;
	size_t iCapacity;

	if ( !xrtMailHeaderUnfoldWrite(Value, NULL, 0, &iSize) ||
		 !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return false;
	}
	sValue = (char*)__xrtMailTreeAlloc(pContext, iCapacity);
	if ( sValue == NULL ) {
		return false;
	}
	if ( !xrtMailHeaderUnfoldWrite(
		Value,
		sValue,
		iCapacity,
		&iSize
	) ) {
		return false;
	}
	*pUnfolded = __xrtMailView(sValue, iSize);
	return true;
}



/* 去掉字段值两端普通空白。 */
static xstrview __xrtMailTreeTrim(xstrview Text)
{
	size_t iStart = 0;
	size_t iEnd = Text.Size;

	while ( (iStart < iEnd) &&
		((Text.Data[iStart] == ' ') || (Text.Data[iStart] == '\t')) ) {
		iStart++;
	}
	while ( (iEnd > iStart) &&
		((Text.Data[iEnd - 1u] == ' ') ||
		 (Text.Data[iEnd - 1u] == '\t')) ) {
		iEnd--;
	}
	return __xrtMailSlice(Text, iStart, iEnd - iStart);
}



/* 解析可选 Content-Type；缺失时使用调用方指定的标准默认值。 */
static bool __xrtMailTreeContentType(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	bool bDigestDefault
)
{
	xmailheaderview Header;
	xmailnext Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Type"),
		&Header
	);

	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		if ( bDigestDefault ) {
			pPart->ContentType.Source = XRT_STR_LITERAL("message/rfc822");
			pPart->ContentType.Type = XRT_STR_LITERAL("message");
			pPart->ContentType.Subtype = XRT_STR_LITERAL("rfc822");
		} else {
			pPart->ContentType.Source = XRT_STR_LITERAL("text/plain");
			pPart->ContentType.Type = XRT_STR_LITERAL("text");
			pPart->ContentType.Subtype = XRT_STR_LITERAL("plain");
		}
		pPart->ContentType.Parameters = __xrtMailView(NULL, 0);
		return true;
	}
	if ( !__xrtMailTreeUnfold(
		pContext,
		Header.Value,
		&pPart->ContentType.Source
	) ) {
		return false;
	}
	return xrtMailMediaTypeParse(
		pPart->ContentType.Source,
		&pPart->ContentType
	);
}



/* MIME 参数始终拒绝控制字符，已转换文本还必须是严格 UTF-8。 */
static bool __xrtMailTreeParameterTextValid(xstrview Text, bool bUtf8)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return __xrtMailTreeError("MIME parameter contains a control byte");
		}
	}
	if ( bUtf8 ) {
		size_t iPosition = 0;

		while ( iPosition < Text.Size ) {
			uint32 iScalar;
			size_t iRead;

			if ( xrtUtf8Decode(
				__xrtMailView(Text.Data + iPosition, Text.Size - iPosition),
				&iScalar,
				&iRead
			) != XUTF_OK ) {
				return __xrtMailTreeError("MIME parameter is not valid UTF-8");
			}
			if ( (iScalar >= 0x80u) && (iScalar <= 0x9Fu) ) {
				return __xrtMailTreeError("MIME parameter contains a control character");
			}
			iPosition += iRead;
		}
	}
	return true;
}



/* 从参数块查找值、执行声明的字符集转换并存入 arena。 */
static xmailnext __xrtMailTreeParam(
	__xmailtreecontext* pContext,
	xstrview Parameters,
	xstrview Name,
	xstrview* pValue,
	bool* pUtf8
)
{
	xmailparaminfo Info;
	xmailnext Next;
	char* sRaw;
	size_t iSize;
	size_t iCapacity;

	Next = xrtMailParamFindWrite(
		Parameters,
		Name,
		NULL,
		0,
		&iSize,
		&Info
	);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( Info.Charset.Size == 0 ) {
		char* sValue = (char*)__xrtMailTreeAlloc(pContext, iCapacity);

		if ( sValue == NULL ) {
			return XMAIL_NEXT_ERROR;
		}
		if ( xrtMailParamFindWrite(
			Parameters,
			Name,
			sValue,
			iCapacity,
			&iSize,
			&Info
		) != XMAIL_NEXT_ITEM ) {
			return XMAIL_NEXT_ERROR;
		}
		*pValue = __xrtMailView(sValue, iSize);
		*pUtf8 = true;
		return __xrtMailTreeParameterTextValid(*pValue, true) ?
			XMAIL_NEXT_ITEM : XMAIL_NEXT_ERROR;
	}
	sRaw = (char*)xrtMalloc(iCapacity);
	if ( sRaw == NULL ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( xrtMailParamFindWrite(
		Parameters,
		Name,
		sRaw,
		iCapacity,
		&iSize,
		&Info
	) != XMAIL_NEXT_ITEM ) {
		xrtFree(sRaw);
		return XMAIL_NEXT_ERROR;
	}
	if ( __xrtMailCharsetSupported(Info.Charset) ) {
		xbytesview Raw = { (cbytes)sRaw, iSize };
		size_t iUtf8Size;
		char* sUtf8;

		if ( !__xrtMailCharsetToUtf8(
			Info.Charset, Raw, NULL, 0, &iUtf8Size
		) ) {
			xrtFree(sRaw);
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_CHARSET,
				"invalid MIME parameter character set data"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailSizeAdd(iUtf8Size, 1u, &iCapacity) ) {
			xrtFree(sRaw);
			return XMAIL_NEXT_ERROR;
		}
		sUtf8 = (char*)__xrtMailTreeAlloc(pContext, iCapacity);
		if ( sUtf8 == NULL ) {
			xrtFree(sRaw);
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtMailCharsetToUtf8(
			Info.Charset, Raw, sUtf8, iUtf8Size, &iUtf8Size
		) ) {
			xrtFree(sRaw);
			__xrtMailError(
				XERR_STATE,
				XMAIL_ERROR_CHARSET,
				"measured MIME parameter conversion did not fit"
			);
			return XMAIL_NEXT_ERROR;
		}
		sUtf8[iUtf8Size] = 0;
		*pValue = __xrtMailView(sUtf8, iUtf8Size);
		*pUtf8 = true;
		xrtFree(sRaw);
		return __xrtMailTreeParameterTextValid(*pValue, true) ?
			XMAIL_NEXT_ITEM : XMAIL_NEXT_ERROR;
	}
	if ( (pContext->Limits.Flags &
		 (uint32)XMAIL_TREE_ALLOW_UNKNOWN_CHARSET) == 0u ) {
		xrtFree(sRaw);
		__xrtMailError(
			XERR_UNSUPPORTED,
			XMAIL_ERROR_CHARSET,
			"unsupported MIME parameter character set"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtMailTreeCopy(
		pContext,
		__xrtMailView(sRaw, iSize),
		pValue
	) ) {
		xrtFree(sRaw);
		return XMAIL_NEXT_ERROR;
	}
	xrtFree(sRaw);
	*pUtf8 = false;
	if ( !__xrtMailTreeParameterTextValid(*pValue, false) ) {
		return XMAIL_NEXT_ERROR;
	}
	return XMAIL_NEXT_ITEM;
}



/* 解析可选 Content-Disposition、文件名和 Content-ID。 */
static bool __xrtMailTreeMetadata(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	xmailheaderview Header;
	xmailnext Next;

	pPart->FileNameUtf8 = true;

	Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Disposition"),
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailTreeUnfold(
			pContext,
			Header.Value,
			&pPart->Disposition.Source
		) || !xrtMailDispositionParse(
			pPart->Disposition.Source,
			&pPart->Disposition
		) ) {
			return false;
		}
		Next = __xrtMailTreeParam(
			pContext,
			pPart->Disposition.Parameters,
			XRT_STR_LITERAL("filename"),
			&pPart->FileName,
			&pPart->FileNameUtf8
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		pPart->Inline = __xrtMailAsciiEqualI(
			pPart->Disposition.Type,
			XRT_STR_LITERAL("inline")
		);
		pPart->Attachment = __xrtMailAsciiEqualI(
			pPart->Disposition.Type,
			XRT_STR_LITERAL("attachment")
		);
	}
	if ( pPart->FileName.Size == 0 ) {
		Next = __xrtMailTreeParam(
			pContext,
			pPart->ContentType.Parameters,
			XRT_STR_LITERAL("name"),
			&pPart->FileName,
			&pPart->FileNameUtf8
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
	}
	pPart->Attachment = pPart->Attachment || (pPart->FileName.Size != 0);

	Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-ID"),
		&Header
	);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_ITEM ) {
		if ( !__xrtMailTreeUnfold(
			pContext,
			Header.Value,
			&pPart->ContentId
		) ) {
			return false;
		}
		pPart->ContentId = __xrtMailTreeTrim(pPart->ContentId);
		if ( (pPart->ContentId.Size >= 2u) &&
			(pPart->ContentId.Data[0] == '<') &&
			(pPart->ContentId.Data[pPart->ContentId.Size - 1u] == '>') ) {
			pPart->ContentId = __xrtMailSlice(
				pPart->ContentId,
				1u,
				pPart->ContentId.Size - 2u
			);
		}
	}
	return true;
}



/* 解析唯一 Content-Transfer-Encoding 字段。 */
static bool __xrtMailTreeTransfer(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	xmailheaderview Header;
	xmailnext Next = __xrtMailTreeHeader(
		&pPart->Message,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		&Header
	);

	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		pPart->Transfer = XMAIL_TRANSFER_7BIT;
		return true;
	}
	pPart->Transfer = xrtMailTransferParse(Header.Value);
	if ( pPart->Transfer != XMAIL_TRANSFER_UNKNOWN ) {
		return true;
	}
	if ( (pContext->Limits.Flags &
		XMAIL_TREE_ALLOW_UNKNOWN_TRANSFER) != 0 ) {
		return true;
	}
	return __xrtMailTreeError(
		"MIME entity uses an unsupported transfer encoding"
	);
}



/* 按传输编码返回正文，并将转换结果计入全树预算。 */
static bool __xrtMailTreeBody(
	__xmailtreecontext* pContext,
	xmailpart* pPart
)
{
	uint32 iFlags = 0;
	unsigned char* pData;
	size_t iSize;
	size_t iCapacity;
	size_t iTotal;

	if ( (pPart->Transfer == XMAIL_TRANSFER_7BIT) ||
		 (pPart->Transfer == XMAIL_TRANSFER_8BIT) ||
		 (pPart->Transfer == XMAIL_TRANSFER_BINARY) ||
		 (pPart->Transfer == XMAIL_TRANSFER_UNKNOWN) ) {
		pPart->Data.Data = (const unsigned char*)pPart->Message.Body.Data;
		pPart->Data.Size = pPart->Message.Body.Size;
		pPart->Decoded = pPart->Transfer != XMAIL_TRANSFER_UNKNOWN;
		return true;
	}
	if ( (pPart->Transfer == XMAIL_TRANSFER_QUOTED_PRINTABLE) &&
		 ((pContext->Limits.Flags & XMAIL_TREE_RELAXED_QP) != 0) ) {
		iFlags = XMAIL_QP_RELAXED_SOFT_BREAK;
	}
	if ( !xrtMailMessageBodyWrite(
		&pPart->Message,
		pPart->Transfer,
		iFlags,
		NULL,
		0,
		&iSize
	) || !__xrtMailSizeAdd(
		pContext->Tree.DecodedBytes,
		iSize,
		&iTotal
	) ) {
		return false;
	}
	if ( (pContext->Limits.MaxDecodedBytes != SIZE_MAX) &&
		 (iTotal > pContext->Limits.MaxDecodedBytes) ) {
		return __xrtMailTreeLimit(
			"MIME tree exceeds the decoded byte limit"
		);
	}
	if ( !__xrtMailSizeAdd(iSize, 1u, &iCapacity) ) {
		return false;
	}
	pData = (unsigned char*)__xrtMailTreeAlloc(pContext, iCapacity);
	if ( pData == NULL ) {
		return false;
	}
	if ( !xrtMailMessageBodyWrite(
		&pPart->Message,
		pPart->Transfer,
		iFlags,
		pData,
		iSize,
		&iSize
	) ) {
		return false;
	}
	pData[iSize] = 0;
	pPart->Data.Data = pData;
	pPart->Data.Size = iSize;
	pPart->Decoded = true;
	pContext->Tree.DecodedBytes = iTotal;
	return true;
}



static bool __xrtMailTreeEntity(
	__xmailtreecontext* pContext,
	xstrview Source,
	size_t iDepth,
	bool bDigestDefault,
	xmailpart* pPart
);



/* 解析 multipart 子项，先计数再一次性分配连续节点。 */
static bool __xrtMailTreeMultipart(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	size_t iDepth
)
{
	xstrview Boundary;
	xstrview Body = __xrtMailView(
		(const char*)pPart->Data.Data,
		pPart->Data.Size
	);
	xmailmultipartcursor Cursor;
	xmailmultipartview View;
	xmailnext Next;
	size_t iCount = 0;
	size_t iBytes;
	bool bBoundaryUtf8;
	bool bDigest = __xrtMailAsciiEqualI(
		pPart->ContentType.Subtype,
		XRT_STR_LITERAL("digest")
	);

	Next = __xrtMailTreeParam(
		pContext,
		pPart->ContentType.Parameters,
		XRT_STR_LITERAL("boundary"),
		&Boundary,
		&bBoundaryUtf8
	);
	(void)bBoundaryUtf8;
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( Next == XMAIL_NEXT_END ) {
		return __xrtMailTreeError("multipart entity has no boundary parameter");
	}
	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		Body,
		Boundary,
		pContext->Limits.MaxParts
	) ) {
		return false;
	}
	while ( (Next = xrtMailMultipartNext(&Cursor, &View)) == XMAIL_NEXT_ITEM ) {
		iCount++;
		if ( (pContext->Limits.MaxParts != SIZE_MAX) &&
			(iCount > (pContext->Limits.MaxParts -
			 pContext->Tree.PartCount)) ) {
			return __xrtMailTreeLimit("MIME tree exceeds the part limit");
		}
	}
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	pPart->Preamble = Cursor.Preamble;
	pPart->Epilogue = Cursor.Epilogue;
	if ( iCount == 0 ) {
		return true;
	}
	if ( (iCount > (SIZE_MAX / sizeof(xmailpart))) ||
		 !__xrtMailSizeAdd(0, iCount * sizeof(xmailpart), &iBytes) ) {
		return false;
	}
	pPart->Children = (xmailpart*)__xrtMailTreeAlloc(pContext, iBytes);
	if ( pPart->Children == NULL ) {
		return false;
	}
	memset(pPart->Children, 0, iBytes);
	pPart->ChildCount = iCount;
	if ( !xrtMailMultipartCursorInit(
		&Cursor,
		Body,
		Boundary,
		iCount
	) ) {
		return false;
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( (xrtMailMultipartNext(&Cursor, &View) != XMAIL_NEXT_ITEM) ||
			 !__xrtMailTreeEntity(
				pContext,
				View.Source,
				iDepth + 1u,
				bDigest,
				&pPart->Children[i]
			 ) ) {
			return false;
		}
	}
	return xrtMailMultipartNext(&Cursor, &View) == XMAIL_NEXT_END;
}



/* 把 message/rfc822 解码正文作为唯一子消息继续解析。 */
static bool __xrtMailTreeEmbedded(
	__xmailtreecontext* pContext,
	xmailpart* pPart,
	size_t iDepth
)
{
	xstrview Source = __xrtMailView(
		(const char*)pPart->Data.Data,
		pPart->Data.Size
	);

	pPart->Children = (xmailpart*)__xrtMailTreeAlloc(
		pContext,
		sizeof(xmailpart)
	);
	if ( pPart->Children == NULL ) {
		return false;
	}
	memset(pPart->Children, 0, sizeof(xmailpart));
	pPart->ChildCount = 1u;
	pPart->Embedded = true;
	return __xrtMailTreeEntity(
		pContext,
		Source,
		iDepth + 1u,
		false,
		pPart->Children
	);
}



/* 解析一个 MIME entity，并递归进入 multipart 或 message/rfc822。 */
static bool __xrtMailTreeEntity(
	__xmailtreecontext* pContext,
	xstrview Source,
	size_t iDepth,
	bool bDigestDefault,
	xmailpart* pPart
)
{
	if ( iDepth > pContext->Limits.MaxDepth ) {
		return __xrtMailTreeLimit("MIME tree exceeds the depth limit");
	}
	if ( (pContext->Limits.MaxParts != SIZE_MAX) &&
		 (pContext->Tree.PartCount >= pContext->Limits.MaxParts) ) {
		return __xrtMailTreeLimit("MIME tree exceeds the part limit");
	}
	pContext->Tree.PartCount++;
	if ( !xrtMailMessageParse(
		Source,
		pContext->Limits.MaxHeaderBytes,
		pContext->Limits.MaxHeaders,
		&pPart->Message
	) || !__xrtMailTreeContentType(
		pContext,
		pPart,
		bDigestDefault
	) || !__xrtMailTreeMetadata(
		pContext,
		pPart
	) || !__xrtMailTreeTransfer(
		pContext,
		pPart
	) || !__xrtMailTreeBody(
		pContext,
		pPart
	) ) {
		return false;
	}
	if ( __xrtMailAsciiEqualI(
		pPart->ContentType.Type,
		XRT_STR_LITERAL("multipart")
	) && pPart->Decoded ) {
		return __xrtMailTreeMultipart(pContext, pPart, iDepth);
	}
	if ( __xrtMailAsciiEqualI(
		pPart->ContentType.Type,
		XRT_STR_LITERAL("message")
	) && __xrtMailAsciiEqualI(
		pPart->ContentType.Subtype,
		XRT_STR_LITERAL("rfc822")
	) && pPart->Decoded ) {
		return __xrtMailTreeEmbedded(pContext, pPart, iDepth);
	}
	return true;
}



/* 使用默认预算初始化 MIME 树限制。 */
XRT_API void xrtMailTreeLimitsInit(xmailtreelimits* pLimits)
{
	if ( pLimits == NULL ) {
		return;
	}
	memset(pLimits, 0, sizeof(*pLimits));
	pLimits->MaxDepth = XMAIL_TREE_DEPTH_DEFAULT;
	pLimits->MaxParts = XMAIL_TREE_PARTS_DEFAULT;
	pLimits->MaxSourceBytes = XMAIL_TREE_SOURCE_BYTES_DEFAULT;
	pLimits->MaxDecodedBytes = XMAIL_TREE_DECODED_BYTES_DEFAULT;
	pLimits->MaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	pLimits->MaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
}



/* 验证并展开 MIME 树的零值默认预算。 */
static bool __xrtMailTreeLimits(
	const xmailtreelimits* pInput,
	xmailtreelimits* pLimits
)
{
	if ( pInput != NULL ) {
		if ( !xrtMemRangeValid(pInput, sizeof(*pInput)) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		*pLimits = *pInput;
	} else {
		xrtMailTreeLimitsInit(pLimits);
	}
	if ( pLimits->MaxDepth == 0 ) {
		pLimits->MaxDepth = XMAIL_TREE_DEPTH_DEFAULT;
	}
	if ( pLimits->MaxParts == 0 ) {
		pLimits->MaxParts = XMAIL_TREE_PARTS_DEFAULT;
	}
	if ( pLimits->MaxSourceBytes == 0 ) {
		pLimits->MaxSourceBytes = XMAIL_TREE_SOURCE_BYTES_DEFAULT;
	}
	if ( pLimits->MaxDecodedBytes == 0 ) {
		pLimits->MaxDecodedBytes = XMAIL_TREE_DECODED_BYTES_DEFAULT;
	}
	if ( pLimits->MaxHeaderBytes == 0 ) {
		pLimits->MaxHeaderBytes = XMAIL_MESSAGE_HEADER_BYTES_DEFAULT;
	}
	if ( pLimits->MaxHeaders == 0 ) {
		pLimits->MaxHeaders = XMAIL_MESSAGE_HEADERS_DEFAULT;
	}
	if ( (pLimits->MaxDepth > XMAIL_TREE_DEPTH_MAX) ||
		 ((pLimits->Flags & ~__XMAIL_TREE_FLAGS) != 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证 MIME 树限制。 */
XRT_API bool xrtMailTreeLimitsValid(const xmailtreelimits* pLimits)
{
	xmailtreelimits Normalized;

	if ( pLimits == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtMailTreeLimits(pLimits, &Normalized);
}



/* 复制并解析完整 RFC 消息。 */
XRT_API bool xrtMailTreeParse(
	xstrview Source,
	const xmailtreelimits* pLimits,
	xmailtree* pTree
)
{
	__xmailtreecontext Context;
	xmailtreelimits Limits;
	xstrview OwnedSource;

	if ( !__xrtMailViewValid(Source) ||
		 !xrtMemRangeValid(pTree, sizeof(*pTree)) ||
		 xrtMemRangesOverlap(pTree, sizeof(*pTree), Source.Data, Source.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailTreeLimits(pLimits, &Limits) ) {
		return false;
	}
	if ( (Limits.MaxSourceBytes != SIZE_MAX) &&
		 (Source.Size > Limits.MaxSourceBytes) ) {
		return __xrtMailTreeLimit("MIME tree exceeds the source byte limit");
	}
	memset(&Context, 0, sizeof(Context));
	Context.Limits = Limits;
	if ( !__xrtMailTreeCopy(&Context, Source, &OwnedSource) ) {
		goto fail;
	}
	Context.Tree.Source = OwnedSource;
	Context.Tree.Root = (xmailpart*)__xrtMailTreeAlloc(
		&Context,
		sizeof(xmailpart)
	);
	if ( Context.Tree.Root == NULL ) {
		goto fail;
	}
	memset(Context.Tree.Root, 0, sizeof(xmailpart));
	if ( !__xrtMailTreeEntity(
		&Context,
		OwnedSource,
		1u,
		false,
		Context.Tree.Root
	) ) {
		goto fail;
	}
	Context.Tree.Storage = Context.Blocks;
	*pTree = Context.Tree;
	return true;

fail:
	__xrtMailTreeBlocksFree(Context.Blocks);
	return false;
}



/* 释放整棵 MIME 树。 */
XRT_API void xrtMailTreeFree(xmailtree* pTree)
{
	if ( pTree == NULL ) {
		return;
	}
	__xrtMailTreeBlocksFree((__xmailtreeblock*)pTree->Storage);
	memset(pTree, 0, sizeof(*pTree));
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_build.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_BUILD)



#if defined(XMAIL_FEATURE_MAIL_BUILD)

#define __XMAIL_BUILD_STACK 1024u



/* 验证 Builder 对象和公开状态没有被调用方破坏。 */
static bool __xrtMailBuilderValid(const xmailbuilder* pBuilder)
{
	if ( !xrtMemRangeValid(pBuilder, sizeof(*pBuilder)) ||
		 (pBuilder->Write == NULL) ||
		 (pBuilder->State < XMAIL_BUILDER_HEADERS) ||
		 (pBuilder->State > XMAIL_BUILDER_FAILED) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 要求 Builder 位于指定阶段且当前未从输出回调重入。 */
static bool __xrtMailBuilderState(
	const xmailbuilder* pBuilder,
	xmailbuilderstate State
)
{
	if ( !__xrtMailBuilderValid(pBuilder) ) {
		return false;
	}
	if ( pBuilder->Busy ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_CALLBACK,
			"mail builder cannot be reentered from its output callback"
		);
		return false;
	}
	if ( pBuilder->State != State ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_PROTOCOL,
			"mail builder operation is invalid in the current state"
		);
		return false;
	}
	return true;
}



/* 同步提交一段借用输出，并把回调失败固化为终止状态。 */
static bool __xrtMailBuilderEmit(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
)
{
	xbytesview Data;
	bool bResult;

	if ( !xrtMemRangeValid(pData, iSize) ||
		 xrtMemRangesOverlap(pBuilder, sizeof(*pBuilder), pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( iSize > (SIZE_MAX - pBuilder->Written) ) {
		pBuilder->State = XMAIL_BUILDER_FAILED;
		__xrtMailSetSizeOverflow();
		return false;
	}
	Data.Data = (cbytes)pData;
	Data.Size = iSize;
	xrtClearError();
	pBuilder->Busy = true;
	bResult = pBuilder->Write(Data, pBuilder->UserData);
	pBuilder->Busy = false;
	if ( !bResult ) {
		pBuilder->State = XMAIL_BUILDER_FAILED;
		if ( xrtGetError() == NULL ) {
			__xrtMailError(
				XERR_IO,
				XMAIL_ERROR_CALLBACK,
				"mail builder output callback failed"
			);
		}
		return false;
	}
	if ( iSize >= 2u ) {
		pBuilder->Tail[0] = Data.Data[iSize - 2u];
		pBuilder->Tail[1] = Data.Data[iSize - 1u];
		pBuilder->TailSize = 2u;
	} else if ( pBuilder->TailSize == 0 ) {
		pBuilder->Tail[0] = Data.Data[0];
		pBuilder->TailSize = 1u;
	} else {
		pBuilder->Tail[0] = pBuilder->Tail[pBuilder->TailSize - 1u];
		pBuilder->Tail[1] = Data.Data[0];
		pBuilder->TailSize = 2u;
	}
	pBuilder->Written += iSize;
	return true;
}



/* 通过查询、栈缓冲和按需堆缓冲调用文本写入原语。 */
static bool __xrtMailBuilderHeaderValue(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
)
{
	char arrStack[__XMAIL_BUILD_STACK];
	char* sOutput = arrStack;
	size_t iRequired;
	bool bResult;

	if ( !xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		NULL,
		0,
		&iRequired
	) ) {
		return false;
	}
	if ( iRequired >= sizeof(arrStack) ) {
		if ( iRequired == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		sOutput = (char*)xrtMalloc(iRequired + 1u);
		if ( sOutput == NULL ) {
			return false;
		}
	}
	bResult = xrtMailHeaderWrite(
		Name,
		Value,
		iLineSize,
		sOutput,
		iRequired + 1u,
		&iRequired
	) && __xrtMailBuilderEmit(pBuilder, sOutput, iRequired);
	if ( sOutput != arrStack ) {
		xrtFree(sOutput);
	}
	return bResult;
}



/* 初始化流式邮件 Builder。 */
XRT_API bool xrtMailBuilderInit(
	xmailbuilder* pBuilder,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	if ( !xrtMemRangeValid(pBuilder, sizeof(*pBuilder)) ||
		 (pWrite == NULL) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pBuilder->Write = pWrite;
	pBuilder->UserData = pUserData;
	pBuilder->Written = 0;
	pBuilder->State = XMAIL_BUILDER_HEADERS;
	pBuilder->Tail[0] = 0;
	pBuilder->Tail[1] = 0;
	pBuilder->TailSize = 0;
	pBuilder->Busy = false;
	return true;
}



/* 验证、折叠并写出一个字段。 */
XRT_API bool xrtMailBuilderHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	size_t iLineSize
)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	return __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		Value,
		iLineSize
	);
}



/* 编码 UTF-8 字段值后写出字段。 */
XRT_API bool xrtMailBuilderWordHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	xstrview Value,
	xmailwordencoding Encoding,
	size_t iLineSize
)
{
	str sValue;
	size_t iValueSize;
	bool bResult;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	sValue = xrtMailWordEncode(Value, Encoding, &iValueSize);
	if ( sValue == NULL ) {
		return false;
	}
	bResult = __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		(xstrview){ sValue, iValueSize },
		iLineSize
	);
	xrtFree(sValue);
	return bResult;
}



/* 格式化地址列表后写出字段。 */
XRT_API bool xrtMailBuilderAddressHeader(
	xmailbuilder* pBuilder,
	xstrview Name,
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags,
	size_t iLineSize
)
{
	str sValue;
	size_t iValueSize;
	bool bResult;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	sValue = xrtMailAddressList(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		&iValueSize
	);
	if ( sValue == NULL ) {
		return false;
	}
	bResult = __xrtMailBuilderHeaderValue(
		pBuilder,
		Name,
		(xstrview){ sValue, iValueSize },
		iLineSize
	);
	xrtFree(sValue);
	return bResult;
}



/* 零复制写出已经完整验证的字段块。 */
XRT_API bool xrtMailBuilderHeaderBlock(
	xmailbuilder* pBuilder,
	xstrview Block
)
{
	xmailheadercursor Cursor;
	xmailheaderview Header;
	xmailnext Next;
	size_t iCount = 0;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	if ( !__xrtMailViewValid(Block) || (Block.Size < 2u) ||
		 (Block.Data[Block.Size - 2u] != '\r') ||
		 (Block.Data[Block.Size - 1u] != '\n') ||
		 xrtMemRangesOverlap(pBuilder, sizeof(*pBuilder), Block.Data, Block.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; (i + 3u) < Block.Size; i++ ) {
		if ( (Block.Data[i] == '\r') && (Block.Data[i + 1u] == '\n') &&
			 (Block.Data[i + 2u] == '\r') &&
			 (Block.Data[i + 3u] == '\n') ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	if ( !xrtMailHeaderCursorInit(&Cursor, Block) ) {
		return false;
	}
	while ( (Next = xrtMailHeaderNext(&Cursor, &Header)) == XMAIL_NEXT_ITEM ) {
		iCount++;
	}
	if ( (Next != XMAIL_NEXT_END) || (iCount == 0) ) {
		return false;
	}
	return __xrtMailBuilderEmit(pBuilder, Block.Data, Block.Size);
}



/* 结束字段并进入正文阶段。 */
XRT_API bool xrtMailBuilderHeadersEnd(xmailbuilder* pBuilder)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_HEADERS) ) {
		return false;
	}
	if ( !__xrtMailBuilderEmit(pBuilder, "\r\n", 2u) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_BODY;
	return true;
}



/* 零复制写出正文或已编码 MIME 片段。 */
XRT_API bool xrtMailBuilderBody(
	xmailbuilder* pBuilder,
	const void* pData,
	size_t iSize
)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	return __xrtMailBuilderEmit(pBuilder, pData, iSize);
}



/* 写出 multipart 分隔片段。 */
XRT_API bool xrtMailBuilderMultipart(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
)
{
	char arrOutput[XMAIL_BOUNDARY_MAX + 9u];
	char* sOutput = arrOutput;
	size_t iSize;
	bool bAtLine;

	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	bAtLine = (pBuilder->TailSize == 2u) &&
		(pBuilder->Tail[0] == (unsigned char)'\r') &&
		(pBuilder->Tail[1] == (unsigned char)'\n');
	if ( !xrtMailMultipartMarkWrite(
		Boundary,
		((Mark == XMAIL_MULTIPART_NEXT) && bAtLine) ?
			XMAIL_MULTIPART_FIRST : Mark,
		arrOutput,
		sizeof(arrOutput),
		&iSize
	) ) {
		return false;
	}
	if ( (Mark == XMAIL_MULTIPART_CLOSE) && bAtLine ) {
		sOutput += 2u;
		iSize -= 2u;
	}
	return __xrtMailBuilderEmit(pBuilder, sOutput, iSize);
}



/* 开始 multipart 的一个新 part，并重新进入字段阶段。 */
XRT_API bool xrtMailBuilderPartBegin(
	xmailbuilder* pBuilder,
	xstrview Boundary,
	xmailmultipartmark Mark
)
{
	if ( (Mark != XMAIL_MULTIPART_FIRST) &&
		 (Mark != XMAIL_MULTIPART_NEXT) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtMailBuilderMultipart(pBuilder, Boundary, Mark) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_HEADERS;
	return true;
}



/* 关闭 Builder，不猜测调用方的正文或 multipart 结构。 */
XRT_API bool xrtMailBuilderFinish(xmailbuilder* pBuilder)
{
	if ( !__xrtMailBuilderState(pBuilder, XMAIL_BUILDER_BODY) ) {
		return false;
	}
	pBuilder->State = XMAIL_BUILDER_CLOSED;
	return true;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/mail/mail_compose.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_COMPOSE)



#if defined(XMAIL_FEATURE_MAIL_COMPOSE)

#define __XMAIL_COMPOSE_STACK 1024u
#define __XMAIL_COMPOSE_BASE64_INPUT \
	((XMAIL_BASE64_LINE_DEFAULT / 4u) * 3u * 48u)
#define __XMAIL_COMPOSE_BASE64_OUTPUT 4096u



typedef struct __xmailcomposecontext {
	const xmailmessage* Message;
	xmailbuilder Builder;
	xstrview Date;
	xstrview MessageId;
	xstrview Mixed;
	xstrview Alternative;
	xstrview Related;
	xstrview Text;
	xstrview Html;
	str OwnedDate;
	str OwnedMessageId;
	str OwnedMixed;
	str OwnedAlternative;
	str OwnedRelated;
	str OwnedText;
	str OwnedHtml;
	bool HasText;
	bool HasHtml;
	bool HasInline;
	bool HasRootAttachment;
} __xmailcomposecontext;



typedef struct __xmailcomposebuffer {
	str Data;
	size_t Size;
	size_t Capacity;
} __xmailcomposebuffer;



/* 按字节比较两个借用视图。 */
static bool __xrtMailComposeViewEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) || (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 验证借用数组范围且避免 count 乘法回绕。 */
static bool __xrtMailComposeArray(
	const void* pArray,
	size_t iCount,
	size_t iElementSize
)
{
	if ( (iElementSize == 0) || (iCount > (SIZE_MAX / iElementSize)) ||
		 !xrtMemRangeValid(pArray, iCount * iElementSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 验证地址数组的完整格式化路径。 */
static bool __xrtMailComposeAddresses(
	const xmailaddress* pAddresses,
	size_t iCount,
	xmailwordencoding Encoding,
	uint32 iFlags
)
{
	size_t iSize;

	return __xrtMailComposeArray(
		pAddresses,
		iCount,
		sizeof(*pAddresses)
	) && xrtMailAddressListWrite(
		pAddresses,
		iCount,
		Encoding,
		iFlags,
		NULL,
		0,
		&iSize
	);
}



/* 判断自定义字段是否会覆盖 Compose 管理的报文字段。 */
static bool __xrtMailComposeManagedHeader(xstrview Name)
{
	static const char* const arrNames[] = {
		"date",
		"message-id",
		"from",
		"to",
		"cc",
		"bcc",
		"reply-to",
		"subject",
		"mime-version",
		"content-type",
		"content-transfer-encoding",
		"content-disposition",
		"content-id"
	};

	for ( size_t i = 0; i < (sizeof(arrNames) / sizeof(arrNames[0])); i++ ) {
		if ( __xrtMailAsciiEqualI(Name, __xrtMailView(
			arrNames[i],
			strlen(arrNames[i])
		)) ) {
			return true;
		}
	}
	return false;
}



/* Content-ID 由 Compose 补尖括号，因此输入只允许安全 ASCII 正文。 */
static bool __xrtMailComposeContentId(xstrview ContentId)
{
	for ( size_t i = 0; i < ContentId.Size; i++ ) {
		unsigned char iByte = (unsigned char)ContentId.Data[i];

		if ( (iByte < 33u) || (iByte > 126u) ||
			 (iByte == (unsigned char)'<') ||
			 (iByte == (unsigned char)'>') ) {
			return false;
		}
	}
	return true;
}



/* 检查长度结果是否与消息中的任一借用范围重叠。 */
static bool __xrtMailComposeSizeOverlap(
	const xmailmessage* pMessage,
	const size_t* pSize
)
{
	if ( pSize == NULL ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pSize, sizeof(*pSize), pMessage, sizeof(*pMessage)) ||
		 xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->To,
			pMessage->ToCount * sizeof(*pMessage->To)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Cc,
			pMessage->CcCount * sizeof(*pMessage->Cc)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Bcc,
			pMessage->BccCount * sizeof(*pMessage->Bcc)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Attachments,
			pMessage->AttachmentCount * sizeof(*pMessage->Attachments)
		 ) || xrtMemRangesOverlap(
			pSize,
			sizeof(*pSize),
			pMessage->Headers,
			pMessage->HeaderCount * sizeof(*pMessage->Headers)
		 ) ) {
		return true;
	}
	{
		xstrview arrViews[13];

		arrViews[0] = pMessage->From.Name;
		arrViews[1] = pMessage->From.Address;
		arrViews[2] = pMessage->ReplyTo.Name;
		arrViews[3] = pMessage->ReplyTo.Address;
		arrViews[4] = pMessage->Subject;
		arrViews[5] = pMessage->Text;
		arrViews[6] = pMessage->Html;
		arrViews[7] = pMessage->Date;
		arrViews[8] = pMessage->MessageId;
		arrViews[9] = pMessage->MessageIdDomain;
		arrViews[10] = pMessage->MixedBoundary;
		arrViews[11] = pMessage->AlternativeBoundary;
		arrViews[12] = pMessage->RelatedBoundary;

		for ( size_t i = 0; i < (sizeof(arrViews) / sizeof(arrViews[0])); i++ ) {
			if ( xrtMemRangesOverlap(
				pSize,
				sizeof(*pSize),
				arrViews[i].Data,
				arrViews[i].Size
			) ) {
				return true;
			}
		}
	}
	for ( size_t i = 0; i < pMessage->ToCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->To[i].Name.Data, pMessage->To[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->To[i].Address.Data, pMessage->To[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->CcCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Cc[i].Name.Data, pMessage->Cc[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Cc[i].Address.Data, pMessage->Cc[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->BccCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Bcc[i].Name.Data, pMessage->Bcc[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Bcc[i].Address.Data, pMessage->Bcc[i].Address.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment = &pMessage->Attachments[i];

		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->FileName.Data, pAttachment->FileName.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->MediaType.Data, pAttachment->MediaType.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->ContentId.Data, pAttachment->ContentId.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pAttachment->Data.Data, pAttachment->Data.Size) ) {
			return true;
		}
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		if ( xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Headers[i].Name.Data, pMessage->Headers[i].Name.Size) ||
			 xrtMemRangesOverlap(pSize, sizeof(*pSize),
			pMessage->Headers[i].Value.Data, pMessage->Headers[i].Value.Size) ) {
			return true;
		}
	}
	return false;
}



/* 在第一次输出前完整验证消息描述和所有借用输入。 */
static bool __xrtMailComposeValid(
	const xmailmessage* pMessage,
	const size_t* pSize
)
{
	xmailmessageidview MessageId;
	xmailmediatypeview MediaType;
	xstrview Local;
	xstrview Domain;
	xtime iDate;
	int iOffset;
	size_t iSize;

	if ( !xrtMemRangeValid(pMessage, sizeof(*pMessage)) ||
		 !xrtMemRangeValid(pSize, pSize != NULL ? sizeof(*pSize) : 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailComposeArray(pMessage->To, pMessage->ToCount,
		sizeof(*pMessage->To)) ||
		 !__xrtMailComposeArray(pMessage->Cc, pMessage->CcCount,
		sizeof(*pMessage->Cc)) ||
		 !__xrtMailComposeArray(pMessage->Bcc, pMessage->BccCount,
		sizeof(*pMessage->Bcc)) ||
		 !__xrtMailComposeArray(pMessage->Attachments,
		pMessage->AttachmentCount, sizeof(*pMessage->Attachments)) ||
		 !__xrtMailComposeArray(pMessage->Headers, pMessage->HeaderCount,
		sizeof(*pMessage->Headers)) ) {
		return false;
	}
	if ( !__xrtMailViewValid(pMessage->Subject) ||
		 !__xrtMailViewValid(pMessage->Text) ||
		 !__xrtMailViewValid(pMessage->Html) ||
		 !__xrtMailViewValid(pMessage->Date) ||
		 !__xrtMailViewValid(pMessage->MessageId) ||
		 !__xrtMailViewValid(pMessage->MessageIdDomain) ||
		 !__xrtMailViewValid(pMessage->MixedBoundary) ||
		 !__xrtMailViewValid(pMessage->AlternativeBoundary) ||
		 !__xrtMailViewValid(pMessage->RelatedBoundary) ||
		 (pMessage->From.Address.Size == 0) ||
		 ((pMessage->ToCount == 0) && (pMessage->CcCount == 0) &&
		  (pMessage->BccCount == 0)) ||
		 ((pMessage->ReplyTo.Address.Size == 0) &&
		  (pMessage->ReplyTo.Name.Size != 0)) ||
		 ((pMessage->WordEncoding != XMAIL_WORD_BASE64) &&
		  (pMessage->WordEncoding != XMAIL_WORD_Q)) ||
		 ((pMessage->AddressFlags & ~(uint32)XMAIL_ADDRESS_SMTPUTF8) != 0) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtUtf8Valid(pMessage->Subject, NULL) ||
		 !xrtUtf8Valid(pMessage->Text, NULL) ||
		 !xrtUtf8Valid(pMessage->Html, NULL) ||
		 !__xrtMailComposeAddresses(&pMessage->From, 1u,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->To, pMessage->ToCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->Cc, pMessage->CcCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 !__xrtMailComposeAddresses(pMessage->Bcc, pMessage->BccCount,
			pMessage->WordEncoding, pMessage->AddressFlags) ||
		 ((pMessage->ReplyTo.Address.Size != 0) &&
		  !__xrtMailComposeAddresses(&pMessage->ReplyTo, 1u,
			pMessage->WordEncoding, pMessage->AddressFlags)) ||
		 !xrtMailWordEncodeWrite(pMessage->Subject,
			pMessage->WordEncoding, NULL, 0, &iSize) ||
		 !xrtMailHeaderWrite(XRT_STR_LITERAL("X"), XRT_STR_LITERAL("x"),
			pMessage->HeaderLineSize, NULL, 0, &iSize) ) {
		return false;
	}
	if ( (pMessage->Date.Size != 0) && !xrtMailDateParse(
		pMessage->Date,
		XMAIL_DATE_STRICT,
		&iDate,
		&iOffset
	) ) {
		return false;
	}
	if ( (pMessage->MessageId.Size != 0) && !xrtMailMessageIdParse(
		pMessage->MessageId,
		XMAIL_ID_DEFAULT,
		&MessageId
	) ) {
		return false;
	}
	if ( pMessage->MessageId.Size == 0 ) {
		if ( pMessage->MessageIdDomain.Size != 0 ) {
			Domain = pMessage->MessageIdDomain;
		} else if ( !xrtMailAddressValid(
			pMessage->From.Address,
			pMessage->AddressFlags,
			&Local,
			&Domain
		) ) {
			return false;
		}
		if ( !xrtMailMessageIdWrite(Domain, NULL, 0, &iSize) ) {
			return false;
		}
	}
	if ( ((pMessage->MixedBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->MixedBoundary)) ||
		 ((pMessage->AlternativeBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->AlternativeBoundary)) ||
		 ((pMessage->RelatedBoundary.Size != 0) &&
		 !xrtMailBoundaryValid(pMessage->RelatedBoundary)) ) {
		return false;
	}
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment = &pMessage->Attachments[i];
		xstrview Type = pAttachment->MediaType.Size != 0 ?
			pAttachment->MediaType : XRT_STR_LITERAL("application/octet-stream");

		if ( !__xrtMailViewValid(pAttachment->FileName) ||
			 !__xrtMailViewValid(pAttachment->MediaType) ||
			 !__xrtMailViewValid(pAttachment->ContentId) ||
			 !xrtMemRangeValid(pAttachment->Data.Data, pAttachment->Data.Size) ||
			 !xrtUtf8Valid(pAttachment->FileName, NULL) ||
			 !xrtMailMediaTypeParse(Type, &MediaType) ||
			 !__xrtMailComposeContentId(pAttachment->ContentId) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
		if ( (pAttachment->FileName.Size != 0) && !xrtMailParamWrite(
			XRT_STR_LITERAL("filename"),
			pAttachment->FileName,
			XMAIL_PARAM_ENCODING_AUTO,
			NULL,
			0,
			&iSize
		) ) {
			return false;
		}
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		const xmailheaderview* pHeader = &pMessage->Headers[i];

		if ( !__xrtMailViewValid(pHeader->Name) ||
			 !__xrtMailViewValid(pHeader->Value) ||
			 __xrtMailComposeManagedHeader(pHeader->Name) ||
			 !xrtMailHeaderWrite(pHeader->Name, pHeader->Value,
				pMessage->HeaderLineSize, NULL, 0, &iSize) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}
	}
	if ( __xrtMailComposeSizeOverlap(pMessage, pSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return true;
}



/* 释放 Compose 在首次输出前准备的全部 owned 文本。 */
static void __xrtMailComposeFree(__xmailcomposecontext* pContext)
{
	xrtFree(pContext->OwnedDate);
	xrtFree(pContext->OwnedMessageId);
	xrtFree(pContext->OwnedMixed);
	xrtFree(pContext->OwnedAlternative);
	xrtFree(pContext->OwnedRelated);
	xrtFree(pContext->OwnedText);
	xrtFree(pContext->OwnedHtml);
	memset(pContext, 0, sizeof(*pContext));
}



/* 取得调用方 boundary，或在真正需要该层时生成一个。 */
static bool __xrtMailComposeBoundary(
	xstrview Configured,
	bool bNeeded,
	xstrview* pBoundary,
	str* pOwned
)
{
	size_t iSize;

	if ( !bNeeded ) {
		*pBoundary = __xrtMailView(NULL, 0);
		return true;
	}
	if ( Configured.Size != 0 ) {
		*pBoundary = Configured;
		return true;
	}
	*pOwned = xrtMailBoundary(&iSize);
	if ( *pOwned == NULL ) {
		return false;
	}
	*pBoundary = __xrtMailView(*pOwned, iSize);
	return true;
}



/* 生成所有随机值并预编码文本，避免语法错误发生在首个 sink 回调之后。 */
static bool __xrtMailComposePrepare(
	__xmailcomposecontext* pContext,
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData
)
{
	xstrview Local;
	xstrview Domain;
	size_t iSize;

	memset(pContext, 0, sizeof(*pContext));
	pContext->Message = pMessage;
	pContext->HasText = pMessage->Text.Size != 0;
	pContext->HasHtml = pMessage->Html.Size != 0;
	for ( size_t i = 0; i < pMessage->AttachmentCount; i++ ) {
		if ( pMessage->Attachments[i].Inline && pContext->HasHtml ) {
			pContext->HasInline = true;
		} else {
			pContext->HasRootAttachment = true;
		}
	}
	if ( pMessage->Date.Size != 0 ) {
		pContext->Date = pMessage->Date;
	} else {
		pContext->OwnedDate = xrtMailDate(xrtNow(), 0, &iSize);
		if ( pContext->OwnedDate == NULL ) {
			goto fail;
		}
		pContext->Date = __xrtMailView(pContext->OwnedDate, iSize);
	}
	if ( pMessage->MessageId.Size != 0 ) {
		pContext->MessageId = pMessage->MessageId;
	} else {
		if ( pMessage->MessageIdDomain.Size != 0 ) {
			Domain = pMessage->MessageIdDomain;
		} else if ( !xrtMailAddressValid(
			pMessage->From.Address,
			pMessage->AddressFlags,
			&Local,
			&Domain
		) ) {
			goto fail;
		}
		pContext->OwnedMessageId = xrtMailMessageId(Domain, &iSize);
		if ( pContext->OwnedMessageId == NULL ) {
			goto fail;
		}
		pContext->MessageId = __xrtMailView(
			pContext->OwnedMessageId,
			iSize
		);
	}
	if ( !__xrtMailComposeBoundary(
		pMessage->MixedBoundary,
		pContext->HasRootAttachment,
		&pContext->Mixed,
		&pContext->OwnedMixed
	) || !__xrtMailComposeBoundary(
		pMessage->AlternativeBoundary,
		pContext->HasText && pContext->HasHtml,
		&pContext->Alternative,
		&pContext->OwnedAlternative
	) || !__xrtMailComposeBoundary(
		pMessage->RelatedBoundary,
		pContext->HasInline,
		&pContext->Related,
		&pContext->OwnedRelated
	) ) {
		goto fail;
	}
	if ( ((pContext->Mixed.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Mixed, pContext->Alternative)) ||
		 ((pContext->Mixed.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Mixed, pContext->Related)) ||
		 ((pContext->Alternative.Size != 0) &&
		 __xrtMailComposeViewEqual(pContext->Alternative, pContext->Related)) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_MIME,
			"nested MIME boundaries must be distinct"
		);
		goto fail;
	}
	if ( pContext->HasText ) {
		pContext->OwnedText = xrtMailQp(
			pMessage->Text.Data,
			pMessage->Text.Size,
			0,
			XMAIL_QP_TEXT,
			&iSize
		);
		if ( pContext->OwnedText == NULL ) {
			goto fail;
		}
		pContext->Text = __xrtMailView(pContext->OwnedText, iSize);
	}
	if ( pContext->HasHtml ) {
		pContext->OwnedHtml = xrtMailQp(
			pMessage->Html.Data,
			pMessage->Html.Size,
			0,
			XMAIL_QP_TEXT,
			&iSize
		);
		if ( pContext->OwnedHtml == NULL ) {
			goto fail;
		}
		pContext->Html = __xrtMailView(pContext->OwnedHtml, iSize);
	}
	if ( !xrtMailBuilderInit(&pContext->Builder, pWrite, pUserData) ) {
		goto fail;
	}
	return true;

fail:
	__xrtMailComposeFree(pContext);
	return false;
}



/* 写出带一个 MIME 参数的字段值，常见路径只使用栈缓冲。 */
static bool __xrtMailComposeParamHeader(
	xmailbuilder* pBuilder,
	xstrview Header,
	xstrview Base,
	xstrview ParamName,
	xstrview ParamValue,
	xmailparamencoding Encoding,
	size_t iLineSize
)
{
	char arrStack[__XMAIL_COMPOSE_STACK];
	char* sValue = arrStack;
	size_t iParamSize;
	size_t iValueSize;
	bool bResult;

	if ( ParamValue.Size == 0 ) {
		return xrtMailBuilderHeader(pBuilder, Header, Base, iLineSize);
	}
	if ( !xrtMailParamWrite(
		ParamName,
		ParamValue,
		Encoding,
		NULL,
		0,
		&iParamSize
	) || !__xrtMailSizeAdd(Base.Size, iParamSize, &iValueSize) ) {
		return false;
	}
	if ( iValueSize == SIZE_MAX ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	if ( iValueSize >= sizeof(arrStack) ) {
		sValue = (char*)xrtMalloc(iValueSize + 1u);
		if ( sValue == NULL ) {
			return false;
		}
	}
	memcpy(sValue, Base.Data, Base.Size);
	bResult = xrtMailParamWrite(
		ParamName,
		ParamValue,
		Encoding,
		sValue + Base.Size,
		iParamSize + 1u,
		&iParamSize
	) && xrtMailBuilderHeader(
		pBuilder,
		Header,
		__xrtMailView(sValue, iValueSize),
		iLineSize
	);
	if ( sValue != arrStack ) {
		xrtFree(sValue);
	}
	return bResult;
}



/* 写出一个 UTF-8 文本 entity。 */
static bool __xrtMailComposeText(
	__xmailcomposecontext* pContext,
	xstrview MediaType,
	xstrview Encoded
)
{
	return __xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		MediaType,
		XRT_STR_LITERAL("charset"),
		XRT_STR_LITERAL("UTF-8"),
		XMAIL_PARAM_ENCODING_TOKEN,
		pContext->Message->HeaderLineSize
	) && xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		XRT_STR_LITERAL("quoted-printable"),
		pContext->Message->HeaderLineSize
	) && xrtMailBuilderHeadersEnd(&pContext->Builder) &&
		xrtMailBuilderBody(&pContext->Builder, Encoded.Data, Encoded.Size);
}



/* 按固定小块写出附件 Base64，不创建按附件大小增长的临时结果。 */
static bool __xrtMailComposeBase64(
	xmailbuilder* pBuilder,
	xbytesview Data
)
{
	char arrOutput[__XMAIL_COMPOSE_BASE64_OUTPUT + 1u];

	for ( size_t i = 0; i < Data.Size; ) {
		size_t iChunk = Data.Size - i;
		size_t iOutputSize;

		if ( iChunk > __XMAIL_COMPOSE_BASE64_INPUT ) {
			iChunk = __XMAIL_COMPOSE_BASE64_INPUT;
		}
		if ( !xrtMailBase64Write(
			Data.Data + i,
			iChunk,
			XMAIL_BASE64_LINE_DEFAULT,
			arrOutput,
			sizeof(arrOutput),
			&iOutputSize
		) || !xrtMailBuilderBody(pBuilder, arrOutput, iOutputSize) ) {
			return false;
		}
		i += iChunk;
	}
	return true;
}



/* 写出一个内联或普通附件 entity。 */
static bool __xrtMailComposeAttachment(
	__xmailcomposecontext* pContext,
	const xmailattachment* pAttachment
)
{
	xstrview MediaType = pAttachment->MediaType.Size != 0 ?
		pAttachment->MediaType : XRT_STR_LITERAL("application/octet-stream");
	xstrview Disposition = pAttachment->Inline ?
		XRT_STR_LITERAL("inline") : XRT_STR_LITERAL("attachment");
	char arrContentId[__XMAIL_COMPOSE_STACK];
	char* sContentId = arrContentId;
	size_t iContentIdSize;
	bool bResult;

	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		MediaType,
		XRT_STR_LITERAL("name"),
		pAttachment->FileName,
		XMAIL_PARAM_ENCODING_AUTO,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Transfer-Encoding"),
		XRT_STR_LITERAL("base64"),
		pContext->Message->HeaderLineSize
	) || !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Disposition"),
		Disposition,
		XRT_STR_LITERAL("filename"),
		pAttachment->FileName,
		XMAIL_PARAM_ENCODING_AUTO,
		pContext->Message->HeaderLineSize
	) ) {
		return false;
	}
	if ( pAttachment->ContentId.Size != 0 ) {
		if ( !__xrtMailSizeAdd(pAttachment->ContentId.Size, 2u,
			&iContentIdSize) ) {
			return false;
		}
		if ( iContentIdSize == SIZE_MAX ) {
			__xrtMailSetSizeOverflow();
			return false;
		}
		if ( iContentIdSize >= sizeof(arrContentId) ) {
			sContentId = (char*)xrtMalloc(iContentIdSize + 1u);
			if ( sContentId == NULL ) {
				return false;
			}
		}
		sContentId[0] = '<';
		memcpy(sContentId + 1u, pAttachment->ContentId.Data,
			pAttachment->ContentId.Size);
		sContentId[iContentIdSize - 1u] = '>';
		sContentId[iContentIdSize] = 0;
		bResult = xrtMailBuilderHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Content-ID"),
			__xrtMailView(sContentId, iContentIdSize),
			pContext->Message->HeaderLineSize
		);
		if ( sContentId != arrContentId ) {
			xrtFree(sContentId);
		}
		if ( !bResult ) {
			return false;
		}
	}
	return xrtMailBuilderHeadersEnd(&pContext->Builder) &&
		__xrtMailComposeBase64(&pContext->Builder, pAttachment->Data);
}



/* 写出 HTML 与全部内联资源组成的 related entity。 */
static bool __xrtMailComposeRelated(__xmailcomposecontext* pContext)
{
	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("multipart/related"),
		XRT_STR_LITERAL("boundary"),
		pContext->Related,
		XMAIL_PARAM_ENCODING_QUOTED,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
		 !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Related,
			XMAIL_MULTIPART_FIRST
		 ) || !__xrtMailComposeText(
			pContext,
			XRT_STR_LITERAL("text/html"),
			pContext->Html
		 ) ) {
		return false;
	}
	for ( size_t i = 0; i < pContext->Message->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment =
			&pContext->Message->Attachments[i];

		if ( !pAttachment->Inline ) {
			continue;
		}
		if ( !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Related,
			XMAIL_MULTIPART_NEXT
		) || !__xrtMailComposeAttachment(pContext, pAttachment) ) {
			return false;
		}
	}
	return xrtMailBuilderMultipart(
		&pContext->Builder,
		pContext->Related,
		XMAIL_MULTIPART_CLOSE
	);
}



/* 写出正文核心：plain、html、related 或 alternative。 */
static bool __xrtMailComposeCore(__xmailcomposecontext* pContext)
{
	if ( pContext->HasText && pContext->HasHtml ) {
		if ( !__xrtMailComposeParamHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Content-Type"),
			XRT_STR_LITERAL("multipart/alternative"),
			XRT_STR_LITERAL("boundary"),
			pContext->Alternative,
			XMAIL_PARAM_ENCODING_QUOTED,
			pContext->Message->HeaderLineSize
		) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
			 !xrtMailBuilderPartBegin(
				&pContext->Builder,
				pContext->Alternative,
				XMAIL_MULTIPART_FIRST
			 ) || !__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/plain"),
				pContext->Text
			 ) || !xrtMailBuilderPartBegin(
				&pContext->Builder,
				pContext->Alternative,
				XMAIL_MULTIPART_NEXT
			 ) ) {
			return false;
		}
		if ( pContext->HasInline ?
			!__xrtMailComposeRelated(pContext) :
			!__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/html"),
				pContext->Html
			) ) {
			return false;
		}
		return xrtMailBuilderMultipart(
			&pContext->Builder,
			pContext->Alternative,
			XMAIL_MULTIPART_CLOSE
		);
	}
	if ( pContext->HasHtml ) {
		return pContext->HasInline ? __xrtMailComposeRelated(pContext) :
			__xrtMailComposeText(
				pContext,
				XRT_STR_LITERAL("text/html"),
				pContext->Html
			);
	}
	return __xrtMailComposeText(
		pContext,
		XRT_STR_LITERAL("text/plain"),
		pContext->Text
	);
}



/* 写出可选 mixed 根和全部非 related 附件。 */
static bool __xrtMailComposeRoot(__xmailcomposecontext* pContext)
{
	if ( !pContext->HasRootAttachment ) {
		return __xrtMailComposeCore(pContext);
	}
	if ( !__xrtMailComposeParamHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Content-Type"),
		XRT_STR_LITERAL("multipart/mixed"),
		XRT_STR_LITERAL("boundary"),
		pContext->Mixed,
		XMAIL_PARAM_ENCODING_QUOTED,
		pContext->Message->HeaderLineSize
	) || !xrtMailBuilderHeadersEnd(&pContext->Builder) ||
		 !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Mixed,
			XMAIL_MULTIPART_FIRST
		 ) || !__xrtMailComposeCore(pContext) ) {
		return false;
	}
	for ( size_t i = 0; i < pContext->Message->AttachmentCount; i++ ) {
		const xmailattachment* pAttachment =
			&pContext->Message->Attachments[i];

		if ( pAttachment->Inline && pContext->HasHtml ) {
			continue;
		}
		if ( !xrtMailBuilderPartBegin(
			&pContext->Builder,
			pContext->Mixed,
			XMAIL_MULTIPART_NEXT
		) || !__xrtMailComposeAttachment(pContext, pAttachment) ) {
			return false;
		}
	}
	return xrtMailBuilderMultipart(
		&pContext->Builder,
		pContext->Mixed,
		XMAIL_MULTIPART_CLOSE
	);
}



/* 写出消息级字段后进入 MIME entity 树。 */
static bool __xrtMailComposeMessage(__xmailcomposecontext* pContext)
{
	const xmailmessage* pMessage = pContext->Message;

	if ( !xrtMailBuilderHeader(&pContext->Builder,
		XRT_STR_LITERAL("Date"), pContext->Date,
		pMessage->HeaderLineSize) ||
		 !xrtMailBuilderHeader(&pContext->Builder,
		XRT_STR_LITERAL("Message-ID"), pContext->MessageId,
		pMessage->HeaderLineSize) ||
		 !xrtMailBuilderAddressHeader(&pContext->Builder,
		XRT_STR_LITERAL("From"), &pMessage->From, 1u,
		pMessage->WordEncoding, pMessage->AddressFlags,
		pMessage->HeaderLineSize) ) {
		return false;
	}
	if ( (pMessage->ToCount != 0) && !xrtMailBuilderAddressHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("To"),
		pMessage->To,
		pMessage->ToCount,
		pMessage->WordEncoding,
		pMessage->AddressFlags,
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	if ( (pMessage->CcCount != 0) && !xrtMailBuilderAddressHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Cc"),
		pMessage->Cc,
		pMessage->CcCount,
		pMessage->WordEncoding,
		pMessage->AddressFlags,
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	if ( (pMessage->ReplyTo.Address.Size != 0) &&
		 !xrtMailBuilderAddressHeader(
			&pContext->Builder,
			XRT_STR_LITERAL("Reply-To"),
			&pMessage->ReplyTo,
			1u,
			pMessage->WordEncoding,
			pMessage->AddressFlags,
			pMessage->HeaderLineSize
		 ) ) {
		return false;
	}
	if ( !xrtMailBuilderWordHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("Subject"),
		pMessage->Subject,
		pMessage->WordEncoding,
		pMessage->HeaderLineSize
	) || !xrtMailBuilderHeader(
		&pContext->Builder,
		XRT_STR_LITERAL("MIME-Version"),
		XRT_STR_LITERAL("1.0"),
		pMessage->HeaderLineSize
	) ) {
		return false;
	}
	for ( size_t i = 0; i < pMessage->HeaderCount; i++ ) {
		if ( !xrtMailBuilderHeader(
			&pContext->Builder,
			pMessage->Headers[i].Name,
			pMessage->Headers[i].Value,
			pMessage->HeaderLineSize
		) ) {
			return false;
		}
	}
	return __xrtMailComposeRoot(pContext) &&
		xrtMailBuilderFinish(&pContext->Builder);
}



/* 扩展 owned 输出缓冲并复制同步片段。 */
static bool __xrtMailComposeBufferWrite(xbytesview Data, ptr pUserData)
{
	__xmailcomposebuffer* pBuffer = (__xmailcomposebuffer*)pUserData;
	size_t iRequired;
	size_t iCapacity;
	str sData;

	if ( (pBuffer->Size == SIZE_MAX) ||
		 (Data.Size > (SIZE_MAX - pBuffer->Size - 1u)) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	iRequired = pBuffer->Size + Data.Size + 1u;
	if ( iRequired > pBuffer->Capacity ) {
		iCapacity = pBuffer->Capacity != 0 ? pBuffer->Capacity : 4096u;
		while ( iCapacity < iRequired ) {
			if ( iCapacity > (SIZE_MAX / 2u) ) {
				iCapacity = iRequired;
				break;
			}
			iCapacity *= 2u;
		}
		sData = pBuffer->Data != NULL ?
			(str)xrtRealloc(pBuffer->Data, iCapacity) :
			(str)xrtMalloc(iCapacity);
		if ( sData == NULL ) {
			return false;
		}
		pBuffer->Data = sData;
		pBuffer->Capacity = iCapacity;
	}
	memcpy(pBuffer->Data + pBuffer->Size, Data.Data, Data.Size);
	pBuffer->Size += Data.Size;
	pBuffer->Data[pBuffer->Size] = 0;
	return true;
}



/* 初始化高层消息描述。 */
XRT_API void xrtMailMessageInit(xmailmessage* pMessage)
{
	if ( pMessage == NULL ) {
		return;
	}
	memset(pMessage, 0, sizeof(*pMessage));
	pMessage->WordEncoding = XMAIL_WORD_BASE64;
}



/* 完整验证高层消息描述。 */
XRT_API bool xrtMailMessageValid(const xmailmessage* pMessage)
{
	return __xrtMailComposeValid(pMessage, NULL);
}



/* 流式构建完整 RFC 消息。 */
XRT_API bool xrtMailComposeWrite(
	const xmailmessage* pMessage,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten
)
{
	__xmailcomposecontext Context;
	size_t iWritten;

	if ( pWrite == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtMailComposeValid(pMessage, pWritten) ||
		 !__xrtMailComposePrepare(&Context, pMessage, pWrite, pUserData) ) {
		return false;
	}
	if ( !__xrtMailComposeMessage(&Context) ) {
		__xrtMailComposeFree(&Context);
		return false;
	}
	iWritten = Context.Builder.Written;
	__xrtMailComposeFree(&Context);
	if ( pWritten != NULL ) {
		*pWritten = iWritten;
	}
	return true;
}



/* 构建独立的完整 RFC 消息。 */
XRT_API str xrtMailCompose(
	const xmailmessage* pMessage,
	size_t* pOutputSize
)
{
	__xmailcomposebuffer Buffer;
	size_t iSize;

	if ( !__xrtMailComposeValid(pMessage, pOutputSize) ) {
		return NULL;
	}
	memset(&Buffer, 0, sizeof(Buffer));
	if ( !xrtMailComposeWrite(
		pMessage,
		__xrtMailComposeBufferWrite,
		&Buffer,
		&iSize
	) ) {
		xrtFree(Buffer.Data);
		return NULL;
	}
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return Buffer.Data;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/transport/mail_net.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET)



#if defined(XMAIL_FEATURE_MAIL_NET)

/* 保存一段末尾补零的动态文本，并复用已有容量。 */
bool __xrtMailTextSet(__xmailtext* pText, xstrview Value)
{
	char* sText;

	if ( !xrtMemRangeValid(pText, sizeof(*pText)) ||
		!__xrtMailViewValid(Value) || (Value.Size >= SIZE_MAX) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Value.Size + 1u > pText->Capacity ) {
		size_t iCapacity = pText->Capacity != 0 ? pText->Capacity : 64u;

		while ( iCapacity < Value.Size + 1u ) {
			size_t iNext = iCapacity <= (SIZE_MAX / 2u) ?
				iCapacity * 2u : Value.Size + 1u;

			if ( iNext <= iCapacity ) {
				iCapacity = Value.Size + 1u;
				break;
			}
			iCapacity = iNext;
		}
		sText = (char*)xrtRealloc(pText->Data, iCapacity);
		if ( sText == NULL ) {
			return false;
		}
		pText->Data = sText;
		pText->Capacity = iCapacity;
	}
	if ( Value.Size != 0 ) {
		memcpy(pText->Data, Value.Data, Value.Size);
	}
	pText->Data[Value.Size] = 0;
	pText->Size = Value.Size;
	return true;
}



/* 释放协议客户端共享的动态文本。 */
void __xrtMailTextDestroy(__xmailtext* pText)
{
	if ( pText == NULL ) {
		return;
	}
	xrtFree(pText->Data);
	memset(pText, 0, sizeof(*pText));
}




/* 返回有界零结尾主机名长度。 */
static bool __xrtMailNetHostSize(cstr sHost, size_t* pSize)
{
	if ( !xrtMemRangeValid(sHost, 1u) ) {
		return false;
	}
	for ( size_t i = 0; i <= XMAIL_NET_HOST_MAX; i++ ) {
		if ( sHost[i] == 0 ) {
			*pSize = i;
			return i != 0;
		}
	}
	return false;
}



/* 初始化邮件网络配置。 */
XRT_API void xrtMailNetConfigInit(xmailnetconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Security = XMAIL_SECURITY_PLAIN;
	pConfig->LineLimit = XMAIL_WIRE_LINE_DEFAULT;
	pConfig->ReadChunk = XMAIL_NET_READ_CHUNK_DEFAULT;
	pConfig->WriteChunk = XMAIL_NET_WRITE_CHUNK_DEFAULT;
	xrtNetDialConfigInit(&pConfig->Dial);
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		xrtTlsClientConfigInit(&pConfig->Tls);
		xrtTlsStreamConfigInit(&pConfig->TlsStream);
		pConfig->TlsTimeout = pConfig->Dial.Timeout;
	#endif
}



/* 验证邮件网络配置。 */
XRT_API bool xrtMailNetConfigValid(const xmailnetconfig* pConfig)
{
	size_t iHostSize;
	size_t iLineLimit;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		(pConfig->Engine == NULL) || (pConfig->Resolver == NULL) ||
		!__xrtMailNetHostSize(pConfig->Host, &iHostSize) ||
		(pConfig->Port == 0) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid mail network owner, host or port"
		);
		return false;
	}
	(void)iHostSize;
	if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
		(pConfig->Security != XMAIL_SECURITY_TLS) &&
		(pConfig->Security != XMAIL_SECURITY_STARTTLS) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_CONFIG,
			"invalid mail network security mode"
		);
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
			(pConfig->Tls.Verifier == NULL) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_CONFIG,
				"verified TLS requires a mail network verifier"
			);
			return false;
		}
	#else
		if ( pConfig->Security != XMAIL_SECURITY_PLAIN ) {
			__xrtMailError(
				XERR_UNSUPPORTED,
				XMAIL_ERROR_CONFIG,
				"mail TLS support is not enabled"
			);
			return false;
		}
	#endif
	iLineLimit = pConfig->LineLimit != 0 ?
		pConfig->LineLimit : XMAIL_WIRE_LINE_DEFAULT;
	if ( (pConfig->ReadChunk == 0) || (pConfig->WriteChunk == 0) ||
		(pConfig->ReadChunk > (SIZE_MAX - 2u)) ||
		(iLineLimit > (SIZE_MAX - pConfig->ReadChunk - 2u)) ||
		(pConfig->WriteChunk > pConfig->Dial.Stream.WriteLimit) ||
		!xrtNetDialConfigValid(&pConfig->Dial) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"invalid mail network buffering or dial limit"
		);
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( (pConfig->Security != XMAIL_SECURITY_PLAIN) &&
			(pConfig->WriteChunk > pConfig->TlsStream.AsyncBytesLimit) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"mail TLS write chunk exceeds the asynchronous byte limit"
			);
			return false;
		}
	#endif
	return true;
}



/* 打开明文或隐式 TLS 传输；STARTTLS 首先建立明文连接。 */
bool __xrtMailTransportOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMailNetConfigValid(pConfig) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pConfig, sizeof(*pConfig)) ) {
		return false;
	}
	memset(pTransport, 0, sizeof(*pTransport));
	pTransport->LineLimit = pConfig->LineLimit != 0 ?
		pConfig->LineLimit : XMAIL_WIRE_LINE_DEFAULT;
	pTransport->ReadChunk = pConfig->ReadChunk;
	pTransport->WriteChunk = pConfig->WriteChunk;
	pTransport->Security = pConfig->Security;
	/* 已失效的调用不能先向 Engine 提交拨号任务。 */
	if ( __xrtWaitExpired(iDeadline) ) {
		__xrtMailError(XERR_TIMEOUT, XMAIL_ERROR_PROTOCOL,
			"mail connection timed out before dialing");
		return false;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtMailError(XERR_CANCELLED, XMAIL_ERROR_PROTOCOL,
			"mail connection was cancelled before dialing");
		return false;
	}
	if ( pConfig->Security == XMAIL_SECURITY_TLS ) {
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			return __xrtMailTransportTlsOpen(
				pTransport,
				pConfig,
				iDeadline,
				pCancel
			);
		#else
			return false;
		#endif
	} else {
		pTransport->Tcp = __xrtNetConnect(
			pConfig->Engine,
			pConfig->Resolver,
			pConfig->Host,
			pConfig->Port,
			&pConfig->Dial,
			NULL,
			NULL,
			iDeadline,
			pCancel
		);
		return pTransport->Tcp != NULL;
	}
}



/* 绕过可选内容编码，有界发送完整底层字节序列。 */
bool __xrtMailTransportRawSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	const unsigned char* pBytes = (const unsigned char*)pData;
	size_t iOffset = 0;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pData, iSize) ||
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			((pTransport->Tcp == NULL) && (pTransport->Tls == NULL))
		#else
			(pTransport->Tcp == NULL)
		#endif
		) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	while ( iOffset < iSize ) {
		size_t iChunk = iSize - iOffset;

		/* 无需等待写就绪时也必须遵守取消和截止时间。 */
		if ( __xrtWaitExpired(iDeadline) ) {
			__xrtMailError(XERR_TIMEOUT, XMAIL_ERROR_PROTOCOL,
				"mail send timed out");
			return false;
		}
		if ( xrtCancelRequested(pCancel) ) {
			__xrtMailError(XERR_CANCELLED, XMAIL_ERROR_PROTOCOL,
				"mail send was cancelled");
			return false;
		}
		if ( iChunk > pTransport->WriteChunk ) {
			iChunk = pTransport->WriteChunk;
		}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			if ( !__xrtMailTransportTlsSend(
				pTransport,
				pBytes + iOffset,
				iChunk,
				iDeadline,
				pCancel
			) ) {
				return false;
			}
		} else {
		#endif
			xnetresult Result;

			for ( ;; ) {
				/* 写队列唤醒后再次检查，避免失效请求继续提交字节。 */
				if ( __xrtWaitExpired(iDeadline) ) {
					__xrtMailError(XERR_TIMEOUT, XMAIL_ERROR_PROTOCOL,
						"mail send timed out");
					return false;
				}
				if ( xrtCancelRequested(pCancel) ) {
					__xrtMailError(XERR_CANCELLED, XMAIL_ERROR_PROTOCOL,
						"mail send was cancelled");
					return false;
				}
				Result = xrtNetStreamSend(
					pTransport->Tcp,
					pBytes + iOffset,
					iChunk
				);
				if ( Result == XNET_RESULT_OK ) {
					break;
				}
				if ( (Result != XNET_RESULT_AGAIN) ||
					!__xrtNetStreamWait(
						pTransport->Tcp,
						XNET_STREAM_WAIT_WRITE,
						iDeadline,
						pCancel
					) ) {
					return false;
				}
			}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		}
		#endif
		iOffset += iChunk;
	}
	return true;
}



/* 写入可选压缩层，并由调用方决定是否建立同步刷新边界。 */
bool __xrtMailTransportWrite(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	double iDeadline,
	xcancel* pCancel
)
{
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		if ( __xrtMailTransportDeflated(pTransport) ) {
			return __xrtMailTransportDeflateSend(
				pTransport,
				pData,
				iSize,
				bFlush,
				iDeadline,
				pCancel
			);
		}
	#else
		(void)bFlush;
	#endif
	return __xrtMailTransportRawSend(
		pTransport,
		pData,
		iSize,
		iDeadline,
		pCancel
	);
}



/* 发送完整协议片段，并保证压缩数据对端可立即消费。 */
bool __xrtMailTransportSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtMailTransportWrite(
		pTransport,
		pData,
		iSize,
		true,
		iDeadline,
		pCancel
	);
}



/* 绕过可选内容解码，取得一块拥有型传输字节。 */
xnetbytes* __xrtMailTransportRawRecv(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
)
{
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			return __xrtMailTransportTlsRecv(
				pTransport,
				iDeadline,
				pCancel
			);
		}
	#endif
	return __xrtNetStreamRecv(
			pTransport->Tcp,
			pTransport->ReadChunk,
			iDeadline,
			pCancel
		);
}



/* 在读取下一条线路前提交上一条借用线路的消费。 */
void __xrtMailTransportConsume(__xmailtransport* pTransport)
{
	if ( pTransport->PendingConsumed == 0 ) {
		return;
	}
	pTransport->PendingSize -= pTransport->PendingConsumed;
	if ( pTransport->PendingSize != 0 ) {
		memmove(
			pTransport->Pending,
			pTransport->Pending + pTransport->PendingConsumed,
			pTransport->PendingSize
		);
	}
	pTransport->PendingConsumed = 0;
}



/* 为下一块接收数据扩展动态线路缓冲。 */
bool __xrtMailTransportReserve(
	__xmailtransport* pTransport,
	size_t iAppend
)
{
	size_t iRequired;
	size_t iCapacity;
	bytes pPending;

	if ( iAppend > (SIZE_MAX - pTransport->PendingSize) ) {
		__xrtMailSetSizeOverflow();
		return false;
	}
	iRequired = pTransport->PendingSize + iAppend;
	if ( iRequired <= pTransport->PendingCapacity ) {
		return true;
	}
	iCapacity = pTransport->PendingCapacity != 0 ?
		pTransport->PendingCapacity : pTransport->ReadChunk;
	while ( iCapacity < iRequired ) {
		size_t iNext = iCapacity <= (SIZE_MAX / 2u) ?
			iCapacity * 2u : SIZE_MAX;

		if ( iNext <= iCapacity ) {
			iCapacity = iRequired;
			break;
		}
		iCapacity = iNext;
	}
	pPending = (bytes)xrtRealloc(pTransport->Pending, iCapacity);
	if ( pPending == NULL ) {
		return false;
	}
	pTransport->Pending = pPending;
	pTransport->PendingCapacity = iCapacity;
	return true;
}



/* 增量读取一条严格 CRLF 线路，返回视图在下一次 Line 调用前有效。 */
bool __xrtMailTransportLine(
	__xmailtransport* pTransport,
	xstrview* pLine,
	double iDeadline,
	xcancel* pCancel
)
{
	size_t iConsumed;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pLine, sizeof(*pLine)) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pLine, sizeof(*pLine)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	for ( ;; ) {
		xstrview Data = __xrtMailView(
			(const char*)pTransport->Pending,
			pTransport->PendingSize
		);
		xmailnext Next = xrtMailLineRead(
			Data,
			pTransport->LineLimit,
			pLine,
			&iConsumed
		);

		if ( Next == XMAIL_NEXT_ITEM ) {
			pTransport->PendingConsumed = iConsumed;
			return true;
		}
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		{
			#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
				if ( __xrtMailTransportDeflated(pTransport) ) {
					if ( !__xrtMailTransportDeflateFill(
						pTransport,
						iDeadline,
						pCancel
					) ) {
						return false;
					}
					continue;
				}
			#endif
			xnetbytes* pReceived = __xrtMailTransportRawRecv(
				pTransport,
				iDeadline,
				pCancel
			);
			xbytesview Received;

			if ( pReceived == NULL ) {
				return false;
			}
			Received = xrtNetBytesView(pReceived);
			if ( (Received.Size == 0) ||
				!__xrtMailTransportReserve(pTransport, Received.Size) ) {
				xrtNetBytesDestroy(pReceived);
				if ( Received.Size == 0 ) {
					__xrtMailError(
						XERR_CLOSED,
						XMAIL_ERROR_PROTOCOL,
						"mail transport closed before a complete line"
					);
				}
				return false;
			}
			memcpy(
				pTransport->Pending + pTransport->PendingSize,
				Received.Data,
				Received.Size
			);
			pTransport->PendingSize += Received.Size;
			xrtNetBytesDestroy(pReceived);
		}
	}
}



/* 优先消费线路解析后的缓存，再把网络块直接复制到调用方缓冲区。 */
bool __xrtMailTransportRead(
	__xmailtransport* pTransport,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
)
{
	bytes pOutput = (bytes)pBuffer;
	size_t iCopy;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMemRangeValid(pBuffer, iCapacity) || (iCapacity == 0) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pBuffer, iCapacity) ||
		xrtMemRangesOverlap(pTransport, sizeof(*pTransport),
			pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pRead, sizeof(*pRead), pBuffer, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	if ( pTransport->PendingSize != 0 ) {
		iCopy = pTransport->PendingSize < iCapacity ?
			pTransport->PendingSize : iCapacity;
		memcpy(pOutput, pTransport->Pending, iCopy);
		pTransport->PendingConsumed = iCopy;
		*pRead = iCopy;
		return true;
	}
	{
		#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
			if ( __xrtMailTransportDeflated(pTransport) ) {
				while ( pTransport->PendingSize == 0 ) {
					if ( !__xrtMailTransportDeflateFill(
						pTransport,
						iDeadline,
						pCancel
					) ) {
						return false;
					}
				}
				iCopy = pTransport->PendingSize < iCapacity ?
					pTransport->PendingSize : iCapacity;
				memcpy(pOutput, pTransport->Pending, iCopy);
				pTransport->PendingConsumed = iCopy;
				*pRead = iCopy;
				return true;
			}
		#endif
		xnetbytes* pReceived = __xrtMailTransportRawRecv(
			pTransport,
			iDeadline,
			pCancel
		);
		xbytesview Received;

		if ( pReceived == NULL ) {
			return false;
		}
		Received = xrtNetBytesView(pReceived);
		if ( Received.Size == 0 ) {
			xrtNetBytesDestroy(pReceived);
			__xrtMailError(
				XERR_CLOSED,
				XMAIL_ERROR_PROTOCOL,
				"mail transport closed before the requested bytes"
			);
			return false;
		}
		iCopy = Received.Size < iCapacity ? Received.Size : iCapacity;
		memcpy(pOutput, Received.Data, iCopy);
		if ( Received.Size > iCopy ) {
			size_t iRemain = Received.Size - iCopy;

			if ( !__xrtMailTransportReserve(pTransport, iRemain) ) {
				xrtNetBytesDestroy(pReceived);
				return false;
			}
			memcpy(
				pTransport->Pending,
				(const unsigned char*)Received.Data + iCopy,
				iRemain
			);
			pTransport->PendingSize = iRemain;
		}
		xrtNetBytesDestroy(pReceived);
	}
	*pRead = iCopy;
	return true;
}



/* 正常关闭传输；任一关闭失败均中止连接并保留原始错误。 */
bool __xrtMailTransportClose(
	__xmailtransport* pTransport,
	double iDeadline
)
{
	bool bSuccess = true;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			bSuccess = __xrtMailTransportTlsClose(pTransport, iDeadline);
		} else
	#endif
	if ( pTransport->Tcp != NULL ) {
		bSuccess = xrtNetStreamClose(pTransport->Tcp) &&
			__xrtNetStreamWait(
				pTransport->Tcp,
				XNET_STREAM_WAIT_CLOSE,
				iDeadline,
				NULL
			);
	}
	if ( !bSuccess ) {
		__xrtMailTransportAbortPreserveError(pTransport);
	}
	return bSuccess;
}



/* 立即异常中止活动连接，保留传输对象中的配置和诊断快照。 */
bool __xrtMailTransportAbort(__xmailtransport* pTransport)
{
	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			return xrtTlsStreamAbort(pTransport->Tls);
		}
	#endif
	if ( pTransport->Tcp != NULL ) {
		return xrtNetStreamAbort(pTransport->Tcp);
	}
	return true;
}



/* 终止失败会话上的待发异步字节，不让清理错误覆盖真正的故障。 */
void __xrtMailTransportAbortPreserveError(__xmailtransport* pTransport)
{
	xerror* pPrimary = xrtTakeError();
	xerror* pAbort;

	(void)__xrtMailTransportAbort(pTransport);
	pAbort = xrtTakeError();
	if ( pPrimary != NULL ) {
		xrtSetError(pPrimary);
	} else if ( pAbort != NULL ) {
		xrtSetError(pAbort);
	}
	xrtErrorFree(pPrimary);
	xrtErrorFree(pAbort);
}



/* 释放传输及动态线路缓冲；未关闭连接会先异常中止。 */
void __xrtMailTransportDestroy(__xmailtransport* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)
		__xrtMailTransportDeflateDestroy(pTransport);
	#endif
	#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
		if ( pTransport->Tls != NULL ) {
			__xrtMailTransportTlsDestroy(pTransport);
		} else
	#endif
	if ( pTransport->Tcp != NULL ) {
		if ( xrtNetStreamState(pTransport->Tcp) != XNET_STREAM_CLOSED ) {
			(void)xrtNetStreamAbort(pTransport->Tcp);
		}
		xrtNetStreamDestroy(pTransport->Tcp);
	}
	xrtFree(pTransport->Pending);
	memset(pTransport, 0, sizeof(*pTransport));
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/transport/mail_auth.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET)



#if defined(XMAIL_FEATURE_MAIL_NET)

/* 验证认证字段的地址和机制专属分隔符。 */
bool __xrtMailAuthFieldValid(xstrview Text, bool bRejectSoh)
{
	if ( !__xrtMailViewValid(Text) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ||
			(bRejectSoh && (iByte == 1u)) ) {
			return false;
		}
	}
	return true;
}



/* 清零并释放凭据或编码后的临时缓冲。 */
void __xrtMailAuthFree(char* sText, size_t iSize)
{
	if ( sText == NULL ) {
		return;
	}
	xrtSecureZero(sText, iSize);
	xrtFree(sText);
}



/* 创建规范 Base64 凭据文本。 */
char* __xrtMailAuthEncode(
	const void* pData,
	size_t iSize,
	size_t* pEncodedSize
)
{
	char* sEncoded;
	size_t iEncoded;

	if ( !xrtMemRangeValid(pData, iSize) ||
		!xrtMemRangeValid(pEncodedSize, sizeof(*pEncodedSize)) ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !xrtBase64Encode(
		pData,
		iSize,
		NULL,
		0,
		&iEncoded,
		NULL
	) || (iEncoded >= SIZE_MAX) ) {
		return NULL;
	}
	sEncoded = (char*)xrtMalloc(iEncoded + 1u);
	if ( sEncoded == NULL ) {
		return NULL;
	}
	if ( !xrtBase64Encode(
		pData,
		iSize,
		sEncoded,
		iEncoded + 1u,
		&iEncoded,
		NULL
	) ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return NULL;
	}
	*pEncodedSize = iEncoded;
	return sEncoded;
}



/* 创建 SASL PLAIN 的 Base64 初始响应。 */
char* __xrtMailAuthPlain(
	xstrview AuthorizationId,
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	char* sPlain;
	char* sEncoded;
	size_t iPlain;
	size_t iOffset = 0;

	if ( !__xrtMailSizeAdd(AuthorizationId.Size, Username.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	if ( AuthorizationId.Size != 0 ) {
		memcpy(sPlain, AuthorizationId.Data, AuthorizationId.Size);
		iOffset = AuthorizationId.Size;
	}
	sPlain[iOffset++] = 0;
	memcpy(sPlain + iOffset, Username.Data, Username.Size);
	iOffset += Username.Size;
	sPlain[iOffset++] = 0;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}



/* 创建 XOAUTH2 bearer 的 Base64 初始响应。 */
char* __xrtMailAuthXoauth2(
	xstrview Username,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	static const char sUser[] = "user=";
	static const char sBearer[] = "\x01" "auth=Bearer ";
	char* sPlain;
	char* sEncoded;
	size_t iPlain = sizeof(sUser) - 1u;
	size_t iOffset = 0;

	if ( !__xrtMailSizeAdd(iPlain, Username.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, sizeof(sBearer) - 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	memcpy(sPlain + iOffset, sUser, sizeof(sUser) - 1u);
	iOffset += sizeof(sUser) - 1u;
	memcpy(sPlain + iOffset, Username.Data, Username.Size);
	iOffset += Username.Size;
	memcpy(sPlain + iOffset, sBearer, sizeof(sBearer) - 1u);
	iOffset += sizeof(sBearer) - 1u;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	iOffset += Secret.Size;
	sPlain[iOffset++] = 1;
	sPlain[iOffset] = 1;
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}



/* 计算并写出 GS2 authzid，其中逗号和等号使用规范转义。 */
static bool __xrtMailAuthGs2Id(
	xstrview AuthorizationId,
	char* sOutput,
	size_t* pSize
)
{
	size_t iRequired = 0;
	size_t iOffset = 0;

	for ( size_t i = 0; i < AuthorizationId.Size; i++ ) {
		size_t iByteSize = (AuthorizationId.Data[i] == ',') ||
			(AuthorizationId.Data[i] == '=') ? 3u : 1u;

		if ( !__xrtMailSizeAdd(iRequired, iByteSize, &iRequired) ) {
			return false;
		}
	}
	if ( sOutput != NULL ) {
		for ( size_t i = 0; i < AuthorizationId.Size; i++ ) {
			if ( AuthorizationId.Data[i] == ',' ) {
				memcpy(sOutput + iOffset, "=2C", 3u);
				iOffset += 3u;
			} else if ( AuthorizationId.Data[i] == '=' ) {
				memcpy(sOutput + iOffset, "=3D", 3u);
				iOffset += 3u;
			} else {
				sOutput[iOffset++] = AuthorizationId.Data[i];
			}
		}
	}
	*pSize = iRequired;
	return true;
}



/* 创建 RFC 7628 OAUTHBEARER 的 Base64 初始响应。 */
char* __xrtMailAuthOauthBearer(
	xstrview AuthorizationId,
	xstrview Secret,
	size_t* pEncodedSize
)
{
	static const char sHeader[] = "n,a=";
	static const char sBearer[] = "\x01" "auth=Bearer ";
	char* sPlain;
	char* sEncoded;
	size_t iAuthzid;
	size_t iPlain = sizeof(sHeader) - 1u;
	size_t iOffset = 0;

	if ( !__xrtMailAuthGs2Id(AuthorizationId, NULL, &iAuthzid) ||
		!__xrtMailSizeAdd(iPlain, iAuthzid, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, sizeof(sBearer) - 1u, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, Secret.Size, &iPlain) ||
		!__xrtMailSizeAdd(iPlain, 2u, &iPlain) ) {
		return NULL;
	}
	sPlain = (char*)xrtMalloc(iPlain);
	if ( sPlain == NULL ) {
		return NULL;
	}
	memcpy(sPlain + iOffset, sHeader, sizeof(sHeader) - 1u);
	iOffset += sizeof(sHeader) - 1u;
	if ( !__xrtMailAuthGs2Id(
		AuthorizationId,
		sPlain + iOffset,
		&iAuthzid
	) ) {
		__xrtMailAuthFree(sPlain, iPlain);
		return NULL;
	}
	iOffset += iAuthzid;
	sPlain[iOffset++] = ',';
	memcpy(sPlain + iOffset, sBearer, sizeof(sBearer) - 1u);
	iOffset += sizeof(sBearer) - 1u;
	memcpy(sPlain + iOffset, Secret.Data, Secret.Size);
	iOffset += Secret.Size;
	sPlain[iOffset++] = 1;
	sPlain[iOffset] = 1;
	sEncoded = __xrtMailAuthEncode(sPlain, iPlain, pEncodedSize);
	__xrtMailAuthFree(sPlain, iPlain);
	return sEncoded;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/transport/mail_net_tls.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET_TLS)



#if defined(XMAIL_FEATURE_MAIL_NET_TLS)

/* 排队期间由 Worker 持有；接管开始后调用线程必须等到交接完成。 */
typedef struct __xmailtlsupgrade {
	xatomic32 Gate; /* 0: queued, 1: running, 2: cancelled before start */
	xnetstream* Tcp;
	const xtlsclientconfig* Client;
	const xtlsstreamconfig* Stream;
	xtlsstream* Tls;
	xpromise* Promise;
} __xmailtlsupgrade;

/* 判断拨号主机是否是无需 SNI 的数字 IP。 */
static bool __xrtMailNetTlsHostIsIp(cstr sHost)
{
	xnetaddr Address;
	xerror* pSaved = xrtTakeError();
	bool bIp = xrtNetAddrParse(&Address, sHost, 0);
	xerror* pProbe = xrtTakeError();

	if ( pSaved != NULL ) {
		xrtSetError(pSaved);
	} else {
		xrtClearError();
	}
	xrtErrorFree(pSaved);
	xrtErrorFree(pProbe);
	return bIp;
}



/* 等待 TLS Future；只有关闭时的接收允许认证 EOF 的 CLOSED 终态。 */
static bool __xrtMailNetTlsFutureResult(
	xfuture* pFuture,
	double iDeadline,
	xcancel* pCancel,
	bool bAllowClosed
)
{
	xwaitresult Wait = __xrtFutureWaitUntilCancel(
		pFuture,
		iDeadline,
		pCancel
	);
	xfuturestate State;

	if ( Wait != XWAIT_OK ) {
		(void)xrtFutureCancel(pFuture);
		if ( Wait == XWAIT_TIMEOUT ) {
			__xrtMailError(
				XERR_TIMEOUT,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation timed out"
			);
		} else if ( Wait == XWAIT_CANCELLED ) {
			__xrtMailError(
				XERR_CANCELLED,
				XMAIL_ERROR_PROTOCOL,
				"mail TLS operation was cancelled"
			);
		}
		return false;
	}
	State = xrtFutureState(pFuture);
	if ( (State == XFUTURE_RESOLVED) ||
		(bAllowClosed && (State == XFUTURE_CLOSED)) ) {
		return true;
	}
	if ( State == XFUTURE_FAILED ) {
		xrtSetError(xrtFutureError(pFuture));
	} else if ( State == XFUTURE_CANCELLED ) {
		__xrtMailError(
			XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation was cancelled"
		);
	} else {
		__xrtMailError(
			XERR_CLOSED,
			XMAIL_ERROR_PROTOCOL,
			"mail TLS operation closed without a result"
		);
	}
	return false;
}



/* 一般 TLS 操作必须交付成功结果，不能把 EOF 当作成功。 */
static bool __xrtMailNetTlsFuture(
	xfuture* pFuture,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtMailNetTlsFutureResult(
		pFuture,
		iDeadline,
		pCancel,
		false
	);
}



/* 为 TLS 拨号补齐默认验证名称，并避免向数字 IP 发送 SNI。 */
static void __xrtMailNetTlsClient(
	const xmailnetconfig* pConfig,
	xtlsclientconfig* pTls
)
{
	xstrview Host;

	*pTls = pConfig->Tls;
	Host.Data = pConfig->Host;
	Host.Size = strlen(pConfig->Host);
	if ( pTls->VerifyName.Size == 0 ) {
		pTls->VerifyName = Host;
	}
	if ( (pTls->ServerName.Size == 0) &&
		!__xrtMailNetTlsHostIsIp(pConfig->Host) ) {
		pTls->ServerName = Host;
	}
}



/* 在 TCP 所属 Worker 上原子接管传输引用。 */
static void __xrtMailNetTlsUpgradeTask(
	xnetworker* pWorker,
	ptr pData
)
{
	__xmailtlsupgrade* pUpgrade = (__xmailtlsupgrade*)pData;
	xpromise* pPromise = pUpgrade->Promise;
	uint32 iExpected = 0;

	(void)pWorker;
	if ( !xrtAtomic32CompareExchange(
		&pUpgrade->Gate, &iExpected, 1,
		XMEMORY_ACQ_REL, XMEMORY_ACQUIRE
	) ) {
		/* 调用方已返回；不得再触碰其 TCP 或借用的 TLS 配置。 */
		xrtPromiseDestroy(pPromise);
		xrtFree(pUpgrade);
		return;
	}
	if ( xrtTlsStreamClient(
		pUpgrade->Tcp,
		pUpgrade->Client,
		pUpgrade->Stream,
		NULL,
		NULL,
		&pUpgrade->Tls
	) ) {
		(void)xrtPromiseResolve(pPromise, NULL);
	} else if ( xrtGetError() != NULL ) {
		(void)xrtPromiseReject(pPromise, xrtGetError());
	} else {
		(void)xrtPromiseClose(pPromise);
	}
	/* Resolve/Reject 可立即唤醒调用方；此后不再访问 pUpgrade。 */
	xrtPromiseDestroy(pPromise);
}



/* 完成隐式 TLS 拨号。 */
bool __xrtMailTransportTlsOpen(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	xtlsclientconfig Tls;
	xtlsdialconfig Dial;
	xfuture* pFuture;

	__xrtMailNetTlsClient(pConfig, &Tls);
	xrtTlsDialConfigInit(&Dial);
	Dial.Transport = pConfig->Dial;
	Dial.Stream = pConfig->TlsStream;
	Dial.Timeout = pConfig->TlsTimeout;
	Dial.ServerNameFromHost = false;
	pFuture = xrtTlsDialAsync(
		pConfig->Engine,
		pConfig->Resolver,
		pConfig->Host,
		pConfig->Port,
		&Tls,
		&Dial,
		NULL,
		NULL
	);
	if ( pFuture == NULL ) {
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return false;
	}
	pTransport->Tls = xrtTlsStreamRef(
		(xtlsstream*)xrtFutureValue(pFuture)
	);
	xrtFutureDestroy(pFuture);
	return pTransport->Tls != NULL;
}



/* 把已经完成协议协商的明文 TCP Stream 接管为 TLS Stream。 */
bool __xrtMailTransportStartTls(
	__xmailtransport* pTransport,
	const xmailnetconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	__xmailtlsupgrade* pUpgrade;
	xtlsclientconfig Client;
	xnetworker* pWorker;
	xfuture* pFuture;
	xfuture* pOpen;
	xwaitresult Wait;
	uint32 iExpected;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtMailNetConfigValid(pConfig) ||
		(pConfig->Security != XMAIL_SECURITY_STARTTLS) ||
		(pTransport->Tcp == NULL) || (pTransport->Tls != NULL) ||
		(pTransport->PendingConsumed > pTransport->PendingSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pTransport->PendingSize - pTransport->PendingConsumed) != 0 ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"STARTTLS response left unconsumed plaintext"
		);
		return false;
	}
	if ( __xrtWaitExpired(iDeadline) || xrtCancelRequested(pCancel) ) {
		__xrtMailError(
			__xrtWaitExpired(iDeadline) ? XERR_TIMEOUT : XERR_CANCELLED,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS was not started"
		);
		return false;
	}
	pWorker = xrtNetStreamWorker(pTransport->Tcp);
	if ( (pWorker == NULL) || xrtNetWorkerIsCurrent(pWorker) ) {
		__xrtMailError(
			XERR_STATE,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS cannot block its transport worker"
		);
		return false;
	}
	__xrtMailNetTlsClient(pConfig, &Client);
	pUpgrade = (__xmailtlsupgrade*)xrtCalloc(1, sizeof(*pUpgrade));
	if ( pUpgrade == NULL ) {
		return false;
	}
	xrtAtomic32Init(&pUpgrade->Gate, 0);
	pUpgrade->Tcp = pTransport->Tcp;
	pUpgrade->Client = &Client;
	pUpgrade->Stream = &pConfig->TlsStream;
	pUpgrade->Promise = xrtPromiseCreate(&pFuture, NULL);
	if ( pUpgrade->Promise == NULL ) {
		xrtFree(pUpgrade);
		return false;
	}
	if ( !xrtNetEnginePost(
		xrtNetWorkerEngine(pWorker),
		xrtNetWorkerIndex(pWorker),
		__xrtMailNetTlsUpgradeTask,
		pUpgrade
	) ) {
		xrtPromiseDestroy(pUpgrade->Promise);
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		return false;
	}
	Wait = __xrtFutureWaitUntilCancel(pFuture, iDeadline, pCancel);
	if ( Wait != XWAIT_OK ) {
		iExpected = 0;
		if ( xrtAtomic32CompareExchange(
			&pUpgrade->Gate, &iExpected, 2,
			XMEMORY_ACQ_REL, XMEMORY_ACQUIRE
		) ) {
			/* Worker 尚未开始接管，TCP 仍由调用方持有。 */
			xrtFutureDestroy(pFuture);
			__xrtMailError(
				Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT :
					Wait == XWAIT_CANCELLED ? XERR_CANCELLED : XERR_IO,
				XMAIL_ERROR_PROTOCOL,
				"mail STARTTLS worker handoff interrupted"
			);
			return false;
		}
		/* Worker 已开始：先收回所有权，避免 TLS/TCP 引用悬空。 */
		(void)xrtFutureWait(pFuture);
		if ( pUpgrade->Tls != NULL ) {
			pTransport->Tcp = NULL;
			(void)xrtTlsStreamAbort(pUpgrade->Tls);
			xrtTlsStreamDestroy(pUpgrade->Tls);
		}
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		__xrtMailError(
			Wait == XWAIT_TIMEOUT ? XERR_TIMEOUT :
				Wait == XWAIT_CANCELLED ? XERR_CANCELLED : XERR_IO,
			XMAIL_ERROR_PROTOCOL,
			"mail STARTTLS worker handoff interrupted"
		);
		return false;
	}
	if ( (Wait != XWAIT_OK) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ||
		(pUpgrade->Tls == NULL) ) {
		if ( xrtFutureState(pFuture) == XFUTURE_FAILED ) {
			xrtSetError(xrtFutureError(pFuture));
		} else if ( Wait != XWAIT_OK ) {
			__xrtMailError(
				XERR_IO,
				XMAIL_ERROR_PROTOCOL,
				"mail STARTTLS worker handoff failed"
			);
		}
		xrtFutureDestroy(pFuture);
		xrtFree(pUpgrade);
		return false;
	}
	xrtFutureDestroy(pFuture);
	pTransport->Tcp = NULL;
	pTransport->Tls = pUpgrade->Tls;
	xrtFree(pUpgrade);
	pTransport->PendingSize = 0;
	pTransport->PendingConsumed = 0;
	pOpen = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_OPEN
	);
	if ( pOpen == NULL ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	if ( !__xrtMailNetTlsFuture(pOpen, iDeadline, pCancel) ) {
		xrtFutureDestroy(pOpen);
		(void)xrtTlsStreamAbort(pTransport->Tls);
		xrtTlsStreamDestroy(pTransport->Tls);
		pTransport->Tls = NULL;
		return false;
	}
	xrtFutureDestroy(pOpen);
	pTransport->Security = XMAIL_SECURITY_TLS;
	return true;
}



/* 发送一个完整 TLS 明文分片。 */
bool __xrtMailTransportTlsSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamSendAsync(
		pTransport->Tls,
		pData,
		iSize
	);
	bool bSuccess;

	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, pCancel);
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 取得一块拥有型 TLS 明文。 */
xnetbytes* __xrtMailTransportTlsRecv(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
)
{
	xfuture* pFuture = xrtTlsStreamRecvAsync(
		pTransport->Tls,
		pTransport->ReadChunk
	);
	xnetbytes* pBytes;

	if ( pFuture == NULL ) {
		return NULL;
	}
	if ( !__xrtMailNetTlsFuture(pFuture, iDeadline, pCancel) ) {
		xrtFutureDestroy(pFuture);
		return NULL;
	}
	pBytes = xrtNetBytesRef((xnetbytes*)xrtFutureValue(pFuture));
	xrtFutureDestroy(pFuture);
	return pBytes;
}



/* 按有界块消费剩余 TLS 明文，使读取背压不会挡住 close_notify。 */
static bool __xrtMailTransportTlsDrainClose(
	__xmailtransport* pTransport,
	double iDeadline
)
{
	for ( ;; ) {
		xfuture* pFuture = xrtTlsStreamRecvAsync(
			pTransport->Tls,
			pTransport->ReadChunk
		);
		bool bSuccess;
		bool bEnd;

		if ( pFuture == NULL ) {
			return false;
		}
		bSuccess = __xrtMailNetTlsFutureResult(
			pFuture,
			iDeadline,
			NULL,
			true
		);
		bEnd = xrtFutureState(pFuture) == XFUTURE_CLOSED;
		xrtFutureDestroy(pFuture);
		if ( !bSuccess || bEnd ) {
			return bSuccess;
		}
	}
}



/* 请求认证关闭，消费协议结束后的残留明文并等待 Stream 关闭终态。 */
bool __xrtMailTransportTlsClose(
	__xmailtransport* pTransport,
	double iDeadline
)
{
	xfuture* pFuture;
	bool bSuccess;

	if ( !xrtTlsStreamClose(pTransport->Tls) ||
		!__xrtMailTransportTlsDrainClose(pTransport, iDeadline) ) {
		return false;
	}
	pFuture = xrtTlsStreamWaitAsync(
		pTransport->Tls,
		XTLS_STREAM_WAIT_CLOSE
	);
	if ( pFuture == NULL ) {
		return false;
	}
	bSuccess = __xrtMailNetTlsFuture(pFuture, iDeadline, NULL);
	xrtFutureDestroy(pFuture);
	return bSuccess;
}



/* 异常中止并释放 TLS Stream 引用。 */
void __xrtMailTransportTlsDestroy(__xmailtransport* pTransport)
{
	if ( (xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_CLOSED) &&
		(xrtTlsStreamState(pTransport->Tls) != XTLS_STREAM_FAILED) ) {
		(void)xrtTlsStreamAbort(pTransport->Tls);
	}
	xrtTlsStreamDestroy(pTransport->Tls);
	pTransport->Tls = NULL;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/xmail/src/transport/mail_net_deflate.c */
/* ========================================================================== */

#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)



#if defined(XMAIL_FEATURE_MAIL_NET_DEFLATE)

#define __XMAIL_DEFLATE_INPUT_CHUNK 256u
#define __XMAIL_DEFLATE_WRITE_CHUNK 16384u



/* Deflate 输出回调同步写入原始 TCP 或 TLS 传输。 */
typedef struct __xmaildeflatesend {
	__xmailtransport* Transport;
	double Deadline;
	xcancel* Cancel;
} __xmaildeflatesend;

typedef struct __xmailinflateread {
	__xmailtransport* Transport;
	double Deadline;
	xcancel* Cancel;
} __xmailinflateread;

/* 编解码器可能只缓存数据；请求检查不能依赖网络回调。 */
static bool __xrtMailDeflateRequestActive(double Deadline, xcancel* pCancel)
{
	if ( __xrtWaitExpired(Deadline) ) {
		__xrtMailError(XERR_TIMEOUT, XMAIL_ERROR_PROTOCOL,
			"compressed mail operation timed out");
		return false;
	}
	if ( xrtCancelRequested(pCancel) ) {
		__xrtMailError(XERR_CANCELLED, XMAIL_ERROR_PROTOCOL,
			"compressed mail operation was cancelled");
		return false;
	}
	return true;
}



/* 直接发送编码器产生的一块压缩数据。 */
static bool __xrtMailTransportDeflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xmaildeflatesend* pSend = (__xmaildeflatesend*)pData;

	return __xrtMailTransportRawSend(
		pSend->Transport,
		Data.Data,
		Data.Size,
		pSend->Deadline,
		pSend->Cancel
	);
}



/* 把 inflater 的一块明文追加到共享读取缓冲。 */
static bool __xrtMailTransportInflateOutput(
	xbytesview Data,
	ptr pData
)
{
	__xmailinflateread* pRead = (__xmailinflateread*)pData;
	__xmailtransport* pTransport = pRead->Transport;

	if ( !__xrtMailDeflateRequestActive(pRead->Deadline, pRead->Cancel) ) {
		return false;
	}

	if ( Data.Size == 0 ) {
		return true;
	}
	if ( !__xrtMailTransportReserve(pTransport, Data.Size) ) {
		return false;
	}
	memcpy(
		pTransport->Pending + pTransport->PendingSize,
		Data.Data,
		Data.Size
	);
	pTransport->PendingSize += Data.Size;
	return true;
}



/* 返回传输是否已经切换到 IMAP raw DEFLATE。 */
bool __xrtMailTransportDeflated(const __xmailtransport* pTransport)
{
	return (pTransport != NULL) && (pTransport->Deflater != NULL) &&
		(pTransport->Inflater != NULL);
}



/* 在 COMPRESS 的明文 OK 后原子安装双向 raw DEFLATE。 */
bool __xrtMailTransportDeflateStart(
	__xmailtransport* pTransport,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
)
{
	xdeflate* pEncoder;
	xinflate* pDecoder;

	if ( !xrtMemRangeValid(pTransport, sizeof(*pTransport)) ||
		!xrtDeflateConfigValid(pDeflate) ||
		!xrtInflateConfigValid(pInflate) ||
		__xrtMailTransportDeflated(pTransport) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pEncoder = xrtDeflateCreate(pDeflate);
	if ( pEncoder == NULL ) {
		return false;
	}
	pDecoder = xrtInflateCreate(pInflate);
	if ( pDecoder == NULL ) {
		xrtDeflateDestroy(pEncoder);
		return false;
	}
	__xrtMailTransportConsume(pTransport);
	pTransport->DeflatePrefix = pTransport->Pending;
	pTransport->DeflatePrefixSize = pTransport->PendingSize;
	pTransport->DeflatePrefixConsumed = 0;
	pTransport->Pending = NULL;
	pTransport->PendingSize = 0;
	pTransport->PendingCapacity = 0;
	pTransport->PendingConsumed = 0;
	pTransport->Deflater = pEncoder;
	pTransport->Inflater = pDecoder;
	return true;
}



/* 压缩一块协议数据，并按命令边界选择同步刷新。 */
bool __xrtMailTransportDeflateSend(
	__xmailtransport* pTransport,
	const void* pData,
	size_t iSize,
	bool bFlush,
	double iDeadline,
	xcancel* pCancel
)
{
	__xmaildeflatesend Send;
	const unsigned char* pBytes = (const unsigned char*)pData;
	size_t iOffset = 0;

	if ( !__xrtMailTransportDeflated(pTransport) ||
		!xrtMemRangeValid(pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Send.Transport = pTransport;
	Send.Deadline = iDeadline;
	Send.Cancel = pCancel;
	for ( ;; ) {
		size_t iChunk = iSize - iOffset;
		if ( !__xrtMailDeflateRequestActive(iDeadline, pCancel) ) return false;
		if ( iChunk > __XMAIL_DEFLATE_WRITE_CHUNK )
			iChunk = __XMAIL_DEFLATE_WRITE_CHUNK;
		bool bLast = iChunk == iSize - iOffset;
		if ( !xrtDeflateWrite(
			pTransport->Deflater,
			(xbytesview) { pBytes != NULL ? pBytes + iOffset : NULL, iChunk },
			bFlush && bLast ? XDEFLATE_FLUSH_SYNC : XDEFLATE_FLUSH_NONE,
			__xrtMailTransportDeflateOutput,
			&Send
		) ) return false;
		iOffset += iChunk;
		if ( bLast ) break;
	}
	return __xrtMailDeflateRequestActive(iDeadline, pCancel);
}



/* 释放已经消费完的预读前缀或网络块。 */
static void __xrtMailTransportDeflateInputConsume(
	__xmailtransport* pTransport
)
{
	if ( pTransport->DeflatePrefixConsumed ==
		pTransport->DeflatePrefixSize ) {
		xrtFree(pTransport->DeflatePrefix);
		pTransport->DeflatePrefix = NULL;
		pTransport->DeflatePrefixSize = 0;
		pTransport->DeflatePrefixConsumed = 0;
	}
	if ( pTransport->DeflateInput != NULL ) {
		xbytesview Input = xrtNetBytesView(pTransport->DeflateInput);

		if ( pTransport->DeflateInputConsumed == Input.Size ) {
			xrtNetBytesDestroy(pTransport->DeflateInput);
			pTransport->DeflateInput = NULL;
			pTransport->DeflateInputConsumed = 0;
		}
	}
}



/* 取得下一小块压缩输入，避免高压缩比正文一次性膨胀。 */
static bool __xrtMailTransportDeflateInput(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel,
	xbytesview* pInput
)
{
		xbytesview Input;
	size_t iRemain;
	size_t iChunk;

	__xrtMailTransportDeflateInputConsume(pTransport);
	if ( pTransport->DeflatePrefix != NULL ) {
		Input.Data = pTransport->DeflatePrefix +
			pTransport->DeflatePrefixConsumed;
		Input.Size = pTransport->DeflatePrefixSize -
			pTransport->DeflatePrefixConsumed;
	} else {
		if ( pTransport->DeflateInput == NULL ) {
			pTransport->DeflateInput = __xrtMailTransportRawRecv(
				pTransport,
				iDeadline,
				pCancel
			);
			if ( pTransport->DeflateInput == NULL ) {
				return false;
			}
		}
		Input = xrtNetBytesView(pTransport->DeflateInput);
		if ( (Input.Size == 0) ||
			(pTransport->DeflateInputConsumed >= Input.Size) ) {
			__xrtMailError(
				XERR_CLOSED,
				XMAIL_ERROR_PROTOCOL,
				"compressed mail transport returned no input"
			);
			return false;
		}
		Input.Data += pTransport->DeflateInputConsumed;
		Input.Size -= pTransport->DeflateInputConsumed;
	}
	iRemain = Input.Size;
	iChunk = iRemain < __XMAIL_DEFLATE_INPUT_CHUNK ?
		iRemain : __XMAIL_DEFLATE_INPUT_CHUNK;
	pInput->Data = Input.Data;
	pInput->Size = iChunk;
	return true;
}



/* 提交刚才交给 inflater 的压缩输入长度。 */
static void __xrtMailTransportDeflateAdvance(
	__xmailtransport* pTransport,
	size_t iSize
)
{
	if ( pTransport->DeflatePrefix != NULL ) {
		pTransport->DeflatePrefixConsumed += iSize;
	} else {
		pTransport->DeflateInputConsumed += iSize;
	}
}



/* 推进压缩输入，直到至少产生一块可消费明文。 */
bool __xrtMailTransportDeflateFill(
	__xmailtransport* pTransport,
	double iDeadline,
	xcancel* pCancel
)
{
	__xmailinflateread Read = { pTransport, iDeadline, pCancel };

	if ( !__xrtMailTransportDeflated(pTransport) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( ;; ) {
		xbytesview Input;
		size_t iBefore = pTransport->PendingSize;

		if ( !__xrtMailDeflateRequestActive(iDeadline, pCancel) ||
			!__xrtMailTransportDeflateInput(
			pTransport,
			iDeadline,
			pCancel,
			&Input
		) || !xrtInflateWrite(
			pTransport->Inflater,
			Input,
			false,
			__xrtMailTransportInflateOutput,
			&Read
		) ) {
			return false;
		}
		__xrtMailTransportDeflateAdvance(pTransport, Input.Size);
		if ( pTransport->PendingSize != iBefore ) {
			return __xrtMailDeflateRequestActive(iDeadline, pCancel);
		}
	}
}



/* 释放压缩状态和尚未消费的原始输入。 */
void __xrtMailTransportDeflateDestroy(__xmailtransport* pTransport)
{
	if ( pTransport == NULL ) {
		return;
	}
	xrtDeflateDestroy(pTransport->Deflater);
	xrtInflateDestroy(pTransport->Inflater);
	xrtFree(pTransport->DeflatePrefix);
	xrtNetBytesDestroy(pTransport->DeflateInput);
	pTransport->Deflater = NULL;
	pTransport->Inflater = NULL;
	pTransport->DeflatePrefix = NULL;
	pTransport->DeflatePrefixSize = 0;
	pTransport->DeflatePrefixConsumed = 0;
	pTransport->DeflateInput = NULL;
	pTransport->DeflateInputConsumed = 0;
}

#endif
#endif

#endif
