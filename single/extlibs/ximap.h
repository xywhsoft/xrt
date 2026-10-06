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
#error "ximap requires XRT; include xrt.h or xrt_decl.h first"
#endif
#if defined(XIMAP_IMPLEMENTATION) && defined(_MSC_VER) && \
	!defined(_CRT_SECURE_NO_WARNINGS)
	#define _CRT_SECURE_NO_WARNINGS
#endif
#if defined(XIMAP_IMPLEMENTATION) && \
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
#ifndef XIMAP_SINGLE_HEADER_H
#define XIMAP_SINGLE_HEADER_H
#define XIMAP_SINGLE_HEADER 1


/* ========================================================================== */
/* feature selection: extlibs/ximap/include/ximap/features.h */
/* ========================================================================== */

/* 此文件由 tools/generate_extension_features.py 生成，请勿直接修改。 */
#ifndef XIMAP_FEATURES_H
#define XIMAP_FEATURES_H

/* imap_compress 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_COMPRESS)
#ifndef XIMAP_FEATURE_IMAP_COMPRESS
#define XIMAP_FEATURE_IMAP_COMPRESS
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_DEFLATE
#define XMAIL_MODULE_MAIL_NET_DEFLATE
#endif
#endif

/* imap_append 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_APPEND)
#ifndef XIMAP_FEATURE_IMAP_APPEND
#define XIMAP_FEATURE_IMAP_APPEND
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#endif

/* imap_message 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_MESSAGE)
#ifndef XIMAP_FEATURE_IMAP_MESSAGE
#define XIMAP_FEATURE_IMAP_MESSAGE
#endif
#ifndef XIMAP_MODULE_IMAP_COMMAND
#define XIMAP_MODULE_IMAP_COMMAND
#endif
#ifndef XIMAP_MODULE_IMAP_DATA
#define XIMAP_MODULE_IMAP_DATA
#endif
#ifndef XMAIL_MODULE_MAIL_TREE
#define XMAIL_MODULE_MAIL_TREE
#endif
#ifndef XRT_MODULE_BUFFER
#define XRT_MODULE_BUFFER
#endif
#endif

/* imap_command 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_COMMAND)
#ifndef XIMAP_FEATURE_IMAP_COMMAND
#define XIMAP_FEATURE_IMAP_COMMAND
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#endif

/* imap_auth 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_AUTH)
#ifndef XIMAP_FEATURE_IMAP_AUTH
#define XIMAP_FEATURE_IMAP_AUTH
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XRT_MODULE_CODEC_BASE64
#define XRT_MODULE_CODEC_BASE64
#endif
#endif

/* imap_client_tls 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_CLIENT_TLS)
#ifndef XIMAP_FEATURE_IMAP_CLIENT_TLS
#define XIMAP_FEATURE_IMAP_CLIENT_TLS
#endif
#ifndef XIMAP_MODULE_IMAP_CLIENT
#define XIMAP_MODULE_IMAP_CLIENT
#endif
#ifndef XMAIL_MODULE_MAIL_NET_TLS
#define XMAIL_MODULE_MAIL_NET_TLS
#endif
#endif

/* imap_client 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_CLIENT)
#ifndef XIMAP_FEATURE_IMAP_CLIENT
#define XIMAP_FEATURE_IMAP_CLIENT
#endif
#ifndef XIMAP_MODULE_IMAP
#define XIMAP_MODULE_IMAP
#endif
#ifndef XMAIL_MODULE_MAIL_NET
#define XMAIL_MODULE_MAIL_NET
#endif
#endif

/* imap_body 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_BODY)
#ifndef XIMAP_FEATURE_IMAP_BODY
#define XIMAP_FEATURE_IMAP_BODY
#endif
#ifndef XIMAP_MODULE_IMAP_DATA
#define XIMAP_MODULE_IMAP_DATA
#endif
#endif

/* imap_data 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP_DATA)
#ifndef XIMAP_FEATURE_IMAP_DATA
#define XIMAP_FEATURE_IMAP_DATA
#endif
#ifndef XIMAP_MODULE_IMAP
#define XIMAP_MODULE_IMAP
#endif
#endif

/* imap 及其直接依赖。 */
#if defined(XIMAP_MODULE_ALL) || defined(XIMAP_MODULE_IMAP)
#ifndef XIMAP_FEATURE_IMAP
#define XIMAP_FEATURE_IMAP
#endif
#ifndef XMAIL_MODULE_MAIL_WIRE
#define XMAIL_MODULE_MAIL_WIRE
#endif
#endif

#endif /* XIMAP_FEATURES_H */


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap.h */
/* ========================================================================== */

#ifndef XRT_IMAP_H
#define XRT_IMAP_H




#if defined(XIMAP_FEATURE_IMAP) && !defined(XMAIL_FEATURE_MAIL_WIRE)
	#error "XIMAP_FEATURE_IMAP requires XMAIL_FEATURE_MAIL_WIRE"
#endif



#if defined(XIMAP_FEATURE_IMAP)

#define XIMAP_COMMAND_LINE_DEFAULT (64u * 1024u)

#define XIMAP_CAP_IMAP4REV1 UINT64_C(0x00000001)
#define XIMAP_CAP_IMAP4REV2 UINT64_C(0x00000002)
#define XIMAP_CAP_STARTTLS UINT64_C(0x00000004)
#define XIMAP_CAP_AUTH_PLAIN UINT64_C(0x00000008)
#define XIMAP_CAP_AUTH_XOAUTH2 UINT64_C(0x00000010)
#define XIMAP_CAP_IDLE UINT64_C(0x00000020)
#define XIMAP_CAP_UIDPLUS UINT64_C(0x00000040)
#define XIMAP_CAP_MOVE UINT64_C(0x00000080)
#define XIMAP_CAP_NAMESPACE UINT64_C(0x00000100)
#define XIMAP_CAP_ENABLE UINT64_C(0x00000200)
#define XIMAP_CAP_UTF8_ACCEPT UINT64_C(0x00000400)
#define XIMAP_CAP_CONDSTORE UINT64_C(0x00000800)
#define XIMAP_CAP_QRESYNC UINT64_C(0x00001000)
#define XIMAP_CAP_LITERAL_PLUS UINT64_C(0x00002000)
#define XIMAP_CAP_SASL_IR UINT64_C(0x00004000)
#define XIMAP_CAP_BINARY UINT64_C(0x00008000)
#define XIMAP_CAP_LOGIN_DISABLED UINT64_C(0x00010000)
#define XIMAP_CAP_AUTH_OAUTHBEARER UINT64_C(0x00020000)
#define XIMAP_CAP_UNSELECT UINT64_C(0x00040000)
#define XIMAP_CAP_ESEARCH UINT64_C(0x00080000)
#define XIMAP_CAP_LIST_EXTENDED UINT64_C(0x00100000)
#define XIMAP_CAP_SPECIAL_USE UINT64_C(0x00200000)
#define XIMAP_CAP_SORT UINT64_C(0x00400000)
#define XIMAP_CAP_THREAD_REFERENCES UINT64_C(0x00800000)
#define XIMAP_CAP_QUOTA UINT64_C(0x01000000)
#define XIMAP_CAP_ACL UINT64_C(0x02000000)
#define XIMAP_CAP_METADATA UINT64_C(0x04000000)
#define XIMAP_CAP_NOTIFY UINT64_C(0x08000000)
#define XIMAP_CAP_COMPRESS_DEFLATE UINT64_C(0x10000000)
#define XIMAP_CAP_APPENDLIMIT UINT64_C(0x20000000)
#define XIMAP_CAP_LITERAL_MINUS UINT64_C(0x40000000)



typedef enum ximapresponsekind {
	XIMAP_RESPONSE_TAGGED = 1,
	XIMAP_RESPONSE_UNTAGGED,
	XIMAP_RESPONSE_CONTINUATION
} ximapresponsekind;



typedef enum ximapstatus {
	XIMAP_STATUS_NONE = 0,
	XIMAP_STATUS_OK,
	XIMAP_STATUS_NO,
	XIMAP_STATUS_BAD,
	XIMAP_STATUS_PREAUTH,
	XIMAP_STATUS_BYE
} ximapstatus;



/* IMAP 响应视图借用输入；Text 是状态后的文本或完整非状态 untagged 内容。 */
typedef struct ximapresponseview {
	xstrview Source;
	xstrview Tag;
	xstrview Text;
	ximapresponsekind Kind;
	ximapstatus Status;
} ximapresponseview;



/* literal 标记位于行尾，支持同步、LITERAL+ 和 binary literal。 */
typedef struct ximapliteralview {
	xstrview Source;
	size_t Size;
	bool NonSynchronizing;
	bool Binary;
} ximapliteralview;



/* 响应码视图借用状态后的文本；Text 是右方括号后的说明文本。 */
typedef struct ximapcodeview {
	xstrview Source;
	xstrview Name;
	xstrview Arguments;
	xstrview Text;
} ximapcodeview;



/* 数字响应视图借用非标记响应 Text，例如 `23 FETCH (...)`。 */
typedef struct ximapnumberview {
	xstrview Source;
	xstrview Name;
	xstrview Text;
	uint64 Number;
} ximapnumberview;



/* 空白分隔 atom 游标用于 CAPABILITY、SEARCH 等简单响应。 */
typedef struct ximapatomcursor {
	xstrview Text;
	size_t Position;
	bool Done;
} ximapatomcursor;



XRT_EXTERN_C_BEGIN



/* 判断文本是否可以作为 IMAP atom 使用。 */
XRT_API bool xrtImapAtomValid(xstrview Atom);



/* 验证序号集合、UID 集合或 SEARCHRES 的 `$` 引用。 */
XRT_API bool xrtImapSequenceSetValid(xstrview Set);



/* 解析 tagged、untagged 或 continuation 响应行。 */
XRT_API bool xrtImapResponseParse(
	xstrview Line,
	ximapresponseview* pResponse
);



/* 查找行尾 literal 标记；无标记返回 END，合法标记返回 ITEM。 */
XRT_API xmailnext xrtImapLiteralParse(
	xstrview Line,
	ximapliteralview* pLiteral
);



/* 解析文本开头的 `[code arguments]`；没有响应码返回 END。 */
XRT_API xmailnext xrtImapCodeParse(
	xstrview Text,
	ximapcodeview* pCode
);



/* 解析 `number name [text]`；不是数字响应返回 END。 */
XRT_API xmailnext xrtImapNumberParse(
	xstrview Text,
	ximapnumberview* pNumber
);



/* 初始化空白分隔 atom 游标。 */
XRT_API bool xrtImapAtomCursorInit(
	ximapatomcursor* pCursor,
	xstrview Text
);



/* 返回下一 atom；控制字符或 IMAP atom-specials 返回错误。 */
XRT_API xmailnext xrtImapAtomNext(
	ximapatomcursor* pCursor,
	xstrview* pAtom
);



/* 返回已知 IMAP capability 的稳定标记，未知扩展返回零。 */
XRT_API uint64 xrtImapCapability(xstrview Capability);



/* 写出转义后的 IMAP quoted string，容量包含末尾零字节。 */
XRT_API bool xrtImapQuoteWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 IMAP quoted string。 */
XRT_API str xrtImapQuote(xstrview Text, size_t* pOutputSize);



/* 安全写出 `Tag Command [Arguments]\r\n`。 */
XRT_API bool xrtImapCommandWrite(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 创建由 xrtFree 释放的 IMAP 命令行。 */
XRT_API str xrtImapCommand(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	size_t* pOutputSize
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_data.h */
/* ========================================================================== */

#ifndef XRT_IMAP_DATA_H
#define XRT_IMAP_DATA_H




#if defined(XIMAP_FEATURE_IMAP_DATA) && !defined(XIMAP_FEATURE_IMAP)
	#error "XIMAP_FEATURE_IMAP_DATA requires IMAP"
#endif



#if defined(XIMAP_FEATURE_IMAP_DATA)

/* 数据值只描述当前响应片段；literal 正文仍由 Client 流式交付。 */
typedef enum ximapdatakind {
	XIMAP_DATA_ATOM = 1,
	XIMAP_DATA_NUMBER,
	XIMAP_DATA_QUOTED,
	XIMAP_DATA_NIL,
	XIMAP_DATA_LIST,
	XIMAP_DATA_LITERAL
} ximapdatakind;



/* Source 保留线路表示，Value 为去除引号或括号但尚未反转义的借用内容。 */
typedef struct ximapdataview {
	xstrview Source;
	xstrview Value;
	ximapdatakind Kind;
	uint64 Number;
	size_t LiteralSize;
	bool LiteralNonSynchronizing;
	bool LiteralBinary;
} ximapdataview;



/* 通用数据游标逐项读取一层 IMAP 数据值，不递归分配对象。 */
typedef struct ximapdatacursor {
	xstrview Text;
	size_t Position;
	bool Done;
} ximapdatacursor;



/* LIST 视图公开属性列表、分隔符、邮箱名和可选扩展。 */
typedef struct ximaplistview {
	xstrview Source;
	xstrview Attributes;
	xstrview Extensions;
	ximapdataview Delimiter;
	ximapdataview Mailbox;
} ximaplistview;



/* FLAGS 游标适用于 LIST 属性和 FETCH FLAGS 数据。 */
typedef struct ximapflagcursor {
	ximapdatacursor Data;
} ximapflagcursor;



/* STATUS 视图保留邮箱名并把属性对交给游标。 */
typedef struct ximapmailboxstatusview {
	xstrview Source;
	ximapdataview Mailbox;
	xstrview Items;
} ximapmailboxstatusview;



typedef struct ximapstatuscursor {
	ximapdatacursor Data;
} ximapstatuscursor;



/* 未知 STATUS 扩展仍以名称和值公开，不占用固定结构。 */
typedef struct ximapstatusitem {
	xstrview Name;
	ximapdataview Value;
} ximapstatusitem;



typedef enum ximapsearchitemkind {
	XIMAP_SEARCH_ID = 1,
	XIMAP_SEARCH_MODSEQ
} ximapsearchitemkind;



/* SEARCH 游标逐项返回 ID，并保留可选 MODSEQ 终项。 */
typedef struct ximapsearchcursor {
	ximapdatacursor Data;
} ximapsearchcursor;



typedef struct ximapsearchitem {
	uint64 Number;
	ximapsearchitemkind Kind;
} ximapsearchitem;



/* ESEARCH 不展开 sequence-set；Correlator 和 Items 均借用输入。 */
typedef struct ximapesearchview {
	xstrview Source;
	xstrview Correlator;
	xstrview Items;
	bool Uid;
} ximapesearchview;



typedef struct ximapesearchcursor {
	ximapdatacursor Data;
} ximapesearchcursor;



typedef struct ximapesearchitem {
	xstrview Name;
	ximapdataview Value;
} ximapesearchitem;



/* FETCH 视图只提取消息序号，属性值由可续段游标逐项交付。 */
typedef struct ximapfetchview {
	xstrview Source;
	xstrview Items;
	uint64 Sequence;
} ximapfetchview;



typedef struct ximapfetchcursor {
	xstrview Text;
	size_t Position;
	bool NeedMore;
	bool Done;
} ximapfetchcursor;



typedef struct ximapfetchitem {
	xstrview Attribute;
	ximapdataview Value;
} ximapfetchitem;



XRT_EXTERN_C_BEGIN



/* 初始化一层 IMAP 数据值游标。 */
XRT_API bool xrtImapDataCursorInit(
	ximapdatacursor* pCursor,
	xstrview Text
);



/* 读取下一 atom、number、quoted、NIL、完整 list 或行尾 literal 标记。 */
XRT_API xmailnext xrtImapDataNext(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
);



/* 解码 atom、quoted 或 NIL；查询模式返回精确字节数，容量包含末尾零字节。 */
XRT_API bool xrtImapStringWrite(
	const ximapdataview* pValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
);



/* 解析完整 `LIST attributes delimiter mailbox [extensions]` 响应。 */
XRT_API bool xrtImapListParse(
	xstrview Text,
	ximaplistview* pList
);



/* 初始化完整括号 flag 列表的借用游标。 */
XRT_API bool xrtImapFlagCursorInit(
	ximapflagcursor* pCursor,
	xstrview Flags
);



/* 返回下一系统 flag、关键字或 LIST 属性。 */
XRT_API xmailnext xrtImapFlagNext(
	ximapflagcursor* pCursor,
	xstrview* pFlag
);



/* 解析完整 `STATUS mailbox (name value ...)` 响应。 */
XRT_API bool xrtImapStatusParse(
	xstrview Text,
	ximapmailboxstatusview* pStatus
);



/* 初始化 STATUS 属性对游标。 */
XRT_API bool xrtImapStatusCursorInit(
	ximapstatuscursor* pCursor,
	const ximapmailboxstatusview* pStatus
);



/* 返回下一 STATUS 名称和值，未知扩展保持可见。 */
XRT_API xmailnext xrtImapStatusNext(
	ximapstatuscursor* pCursor,
	ximapstatusitem* pItem
);



/* 初始化完整 `SEARCH [id ...] [(MODSEQ value)]` 响应游标。 */
XRT_API bool xrtImapSearchCursorInit(
	ximapsearchcursor* pCursor,
	xstrview Text
);



/* 返回下一 SEARCH ID 或 MODSEQ，不展开和分配数组。 */
XRT_API xmailnext xrtImapSearchNext(
	ximapsearchcursor* pCursor,
	ximapsearchitem* pItem
);



/* 解析 ESEARCH correlator、UID 指示器和返回数据区。 */
XRT_API bool xrtImapESearchParse(
	xstrview Text,
	ximapesearchview* pSearch
);



/* 初始化 ESEARCH 返回数据对游标。 */
XRT_API bool xrtImapESearchCursorInit(
	ximapesearchcursor* pCursor,
	const ximapesearchview* pSearch
);



/* 返回下一 ESEARCH 名称和值；ALL 等集合保持原始 atom 视图。 */
XRT_API xmailnext xrtImapESearchNext(
	ximapesearchcursor* pCursor,
	ximapesearchitem* pItem
);



/* 解析完整 `sequence FETCH (...)` 非标记响应。 */
XRT_API bool xrtImapFetchParse(
	xstrview Text,
	ximapfetchview* pFetch
);



/* 初始化 FETCH 属性游标；literal 正文不会进入该游标。 */
XRT_API bool xrtImapFetchCursorInit(
	ximapfetchcursor* pCursor,
	const ximapfetchview* pFetch
);



/* 在读取 literal 正文后继续解析下一行 FETCH 片段。 */
XRT_API bool xrtImapFetchCursorContinue(
	ximapfetchcursor* pCursor,
	xstrview Text
);



/* 返回下一 FETCH 属性和值；literal 值会把 NeedMore 置真。 */
XRT_API xmailnext xrtImapFetchNext(
	ximapfetchcursor* pCursor,
	ximapfetchitem* pItem
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_body.h */
/* ========================================================================== */

#ifndef XRT_IMAP_BODY_H
#define XRT_IMAP_BODY_H




#if defined(XIMAP_FEATURE_IMAP_BODY) && !defined(XIMAP_FEATURE_IMAP_DATA)
	#error "XIMAP_FEATURE_IMAP_BODY requires IMAP_DATA"
#endif



#if defined(XIMAP_FEATURE_IMAP_BODY)

/* BODYSTRUCTURE 递归校验的固定深度上限。 */
#define XIMAP_BODY_DEPTH_MAX 64u



/* 单部分消息按协议字段分为普通、文本和嵌套消息，multipart 单独表示。 */
typedef enum ximapbodykind {
	XIMAP_BODY_BASIC = 1,
	XIMAP_BODY_TEXT,
	XIMAP_BODY_MESSAGE,
	XIMAP_BODY_MULTIPART
} ximapbodykind;



/*
	BODYSTRUCTURE 的零分配视图。
	所有字符串和值都借用 Source；未在线路中出现的可选字段 Kind 为零。
*/
typedef struct ximapbodyview {
	xstrview Source;
	xstrview Children;
	xstrview Extensions;
	ximapdataview Type;
	ximapdataview Subtype;
	ximapdataview Parameters;
	ximapdataview Id;
	ximapdataview Description;
	ximapdataview Encoding;
	ximapdataview Envelope;
	ximapdataview Body;
	ximapdataview Md5;
	ximapdataview Disposition;
	ximapdataview Language;
	ximapdataview Location;
	uint64 Octets;
	uint64 Lines;
	size_t ChildCount;
	ximapbodykind Kind;
} ximapbodyview;



/* multipart 子部分游标只借用父视图的 Children 区。 */
typedef struct ximapbodycursor {
	ximapdatacursor Data;
	size_t Remaining;
} ximapbodycursor;



/* 参数游标用于 body-fld-param 和 disposition 参数列表。 */
typedef struct ximapbodyparamcursor {
	ximapdatacursor Data;
} ximapbodyparamcursor;



/* 参数名和值保留 IMAP string 的线路表示，可由 xrtImapStringWrite 解码。 */
typedef struct ximapbodyparam {
	ximapdataview Name;
	ximapdataview Value;
} ximapbodyparam;



XRT_EXTERN_C_BEGIN



/* 解析并递归校验一个完整的 BODY 或 BODYSTRUCTURE 括号值。 */
XRT_API bool xrtImapBodyParse(
	xstrview Text,
	ximapbodyview* pBody
);



/* 初始化 multipart 直接子部分的零分配游标。 */
XRT_API bool xrtImapBodyChildCursorInit(
	ximapbodycursor* pCursor,
	const ximapbodyview* pBody
);



/* 返回下一个直接子部分；每个结果仍会完整校验自己的递归结构。 */
XRT_API xmailnext xrtImapBodyChildNext(
	ximapbodycursor* pCursor,
	ximapbodyview* pBody
);



/* 初始化 NIL 或括号参数列表的成对游标。 */
XRT_API bool xrtImapBodyParamCursorInit(
	ximapbodyparamcursor* pCursor,
	const ximapdataview* pParameters
);



/* 返回下一参数名和值。 */
XRT_API xmailnext xrtImapBodyParamNext(
	ximapbodyparamcursor* pCursor,
	ximapbodyparam* pParameter
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_client.h */
/* ========================================================================== */

#ifndef XRT_IMAP_CLIENT_H
#define XRT_IMAP_CLIENT_H




#if defined(XIMAP_FEATURE_IMAP_CLIENT) && \
	(!defined(XIMAP_FEATURE_IMAP) || !defined(XMAIL_FEATURE_MAIL_NET))
	#error "XIMAP_FEATURE_IMAP_CLIENT requires IMAP and mail net"
#endif

#if defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) && \
	(!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_TLS))
	#error "XIMAP_FEATURE_IMAP_CLIENT_TLS requires IMAP client and mail net TLS"
#endif



#if defined(XIMAP_FEATURE_IMAP_CLIENT)

#define XIMAP_CLIENT_TAG_MAX 15u
#define XIMAP_APPEND_LIMIT_UNKNOWN UINT64_MAX



typedef struct ximapclient ximapclient;



/* 会话状态只表达 IMAP 协议层级，不隐藏正在交换的原始命令。 */
typedef enum ximapclientstate {
	XIMAP_CLIENT_NOT_AUTHENTICATED = 0,
	XIMAP_CLIENT_AUTHENTICATED,
	XIMAP_CLIENT_SELECTED,
	XIMAP_CLIENT_CLOSED,
	XIMAP_CLIENT_FAILED
} ximapclientstate;



/* RESPONSE 是新响应首行，FRAGMENT 是服务器 literal 后的同一响应续行。 */
typedef enum ximapeventkind {
	XIMAP_EVENT_RESPONSE = 1,
	XIMAP_EVENT_FRAGMENT
} ximapeventkind;



/* 客户端只在 Open 期间借用配置；默认连接明文 143 端口。 */
typedef struct ximapclientconfig {
	xmailnetconfig Net;
	size_t CommandLineLimit;
} ximapclientconfig;



/*
	事件中的所有视图借用客户端线路缓冲；下一次 Receive、Next 或 ReadLiteral
	调用后失效。Literal 只在 HasLiteral 为真时有效。
*/
typedef struct ximapevent {
	xstrview Source;
	ximapresponseview Response;
	ximapliteralview Literal;
	ximapeventkind Kind;
	bool HasLiteral;
} ximapevent;



XRT_EXTERN_C_BEGIN



/* 初始化 IMAP 143 端口、64 KiB 命令/响应行和 XRT 网络默认值。 */
XRT_API void xrtImapClientConfigInit(ximapclientconfig* pConfig);



/* 验证网络所有者、线路上限和命令上限。 */
XRT_API bool xrtImapClientConfigValid(const ximapclientconfig* pConfig);



/* 建立连接，读取 greeting，完成 CAPABILITY 和可选 STARTTLS。 */
XRT_API ximapclient* xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
);



/* 返回当前 IMAP 协议层级或终态。 */
XRT_API ximapclientstate xrtImapClientState(const ximapclient* pClient);



/* 返回最近一次 CAPABILITY 快照中的已知能力位。 */
XRT_API uint64 xrtImapClientCapabilities(const ximapclient* pClient);



/* 返回当前会话实际使用的明文或 TLS 传输安全级别。 */
XRT_API xmailsecurity xrtImapClientSecurity(const ximapclient* pClient);



/* 返回顺序命令当前使用的自动 tag；没有活动命令时返回空视图。 */
XRT_API xstrview xrtImapClientTag(const ximapclient* pClient);



/* 返回当前服务器 literal 尚未读取的字节数。 */
XRT_API size_t xrtImapClientLiteralRemaining(const ximapclient* pClient);



/* 返回当前命令行总长度上限。 */
XRT_API size_t xrtImapClientCommandLimit(const ximapclient* pClient);



/* 返回 CAPABILITY 声明的全局 APPEND 上限，未知或按邮箱决定时返回 UNKNOWN。 */
XRT_API uint64 xrtImapClientAppendLimit(const ximapclient* pClient);



/* 返回最近 greeting 或 tagged completion 的稳定借用视图。 */
XRT_API bool xrtImapClientLastResponse(
	const ximapclient* pClient,
	ximapresponseview* pResponse
);



/*
	直接发送显式 tag 的完整命令行，不创建临时拼接缓冲。
	低层调用方可连续发送多个命令，并用 Receive 按 tag 关联完成响应。
*/
XRT_API bool xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	int64 iTimeout,
	xcancel* pCancel
);



/*
	直接发送由独立参数片组成的显式 tag 命令；各片之间自动插入一个空格，
	不创建参数拼接缓冲。空参数数组表示无参数命令。
*/
XRT_API bool xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	int64 iTimeout,
	xcancel* pCancel
);



/* 发送原始命令续传字节，用于 APPEND literal 等协议扩展。 */
XRT_API bool xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	int64 iTimeout,
	xcancel* pCancel
);



/* 发送一条不带 tag 的 continuation 数据并自动追加 CRLF。 */
XRT_API bool xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	int64 iTimeout,
	xcancel* pCancel
);



/*
	读取下一个响应首行或 literal 后续片段；遇到 literal 后必须先读取全部
	literal，才能继续接收下一事件。低层流式接口不设置 literal 大小上限；
	调用方可检查 pEvent->Literal.Size，并在超出自身预算时立即 Abort。
*/
XRT_API bool xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	int64 iTimeout,
	xcancel* pCancel
);



/* 把当前 literal 的下一段读入调用方缓冲区，Read 返回实际读取量。 */
XRT_API bool xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	int64 iTimeout,
	xcancel* pCancel
);



/* 生成唯一 tag 并开始一个顺序命令。 */
XRT_API bool xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	int64 iTimeout,
	xcancel* pCancel
);



/* 生成唯一 tag 并以零拼接方式开始多参数顺序命令。 */
XRT_API bool xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	int64 iTimeout,
	xcancel* pCancel
);



/*
	返回顺序命令的下一事件；匹配 tag 的 completion 返回 END，其他响应返回
	ITEM，传输、协议或状态错误返回 ERROR。
*/
XRT_API xmailnext xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	int64 iTimeout,
	xcancel* pCancel
);



/* 在没有活动顺序命令时重新获取并替换 CAPABILITY 快照。 */
XRT_API bool xrtImapClientRefresh(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 发送 LOGOUT，消费 BYE 和 tagged completion，并正常关闭传输。 */
XRT_API bool xrtImapClientLogout(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 不发送 LOGOUT，直接正常关闭传输。 */
XRT_API bool xrtImapClientClose(
	ximapclient* pClient,
	int64 iTimeout
);



/* 立即异常中止连接；重复调用成功，FAILED 状态保留到销毁。 */
XRT_API bool xrtImapClientAbort(ximapclient* pClient);



/* 释放客户端；尚未关闭时执行异常中止。 */
XRT_API void xrtImapClientDestroy(ximapclient* pClient);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_auth.h */
/* ========================================================================== */

#ifndef XRT_IMAP_AUTH_H
#define XRT_IMAP_AUTH_H




#if defined(XIMAP_FEATURE_IMAP_AUTH) && \
	(!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	 !defined(XRT_FEATURE_CODEC_BASE64))
	#error "XIMAP_FEATURE_IMAP_AUTH requires IMAP client and Base64"
#endif



#if defined(XIMAP_FEATURE_IMAP_AUTH)

/* LOGIN 保留传统命令，其余机制使用标准 SASL continuation 交换。 */
typedef enum ximapauthmethod {
	XIMAP_AUTH_LOGIN = 0,
	XIMAP_AUTH_PLAIN,
	XIMAP_AUTH_XOAUTH2,
	XIMAP_AUTH_OAUTHBEARER
} ximapauthmethod;



/* 凭据只在 Auth 调用期间借用，结束前所有临时副本都会被清零。 */
typedef struct ximapauthconfig {
	ximapauthmethod Method;
	xstrview Username;
	xstrview Secret;
	xstrview AuthorizationId;
	bool InitialResponse;
	bool AllowPlaintext;
} ximapauthconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 PLAIN、SASL-IR 开启和明文凭据关闭的安全默认值。 */
XRT_API void xrtImapAuthConfigInit(ximapauthconfig* pConfig);



/* 验证认证机制、凭据视图和机制专属分隔符。 */
XRT_API bool xrtImapAuthConfigValid(const ximapauthconfig* pConfig);



/* 在 NOT_AUTHENTICATED 会话上执行配置的认证机制。 */
XRT_API bool xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_command.h */
/* ========================================================================== */

#ifndef XRT_IMAP_COMMAND_H
#define XRT_IMAP_COMMAND_H




#if defined(XIMAP_FEATURE_IMAP_COMMAND) && \
	!defined(XIMAP_FEATURE_IMAP_CLIENT)
	#error "XIMAP_FEATURE_IMAP_COMMAND requires IMAP client"
#endif



#if defined(XIMAP_FEATURE_IMAP_COMMAND)

#define XIMAP_MAILBOX_EXISTS UINT32_C(0x00000001)
#define XIMAP_MAILBOX_RECENT UINT32_C(0x00000002)
#define XIMAP_MAILBOX_UNSEEN UINT32_C(0x00000004)
#define XIMAP_MAILBOX_UID_VALIDITY UINT32_C(0x00000008)
#define XIMAP_MAILBOX_UID_NEXT UINT32_C(0x00000010)
#define XIMAP_MAILBOX_HIGHEST_MODSEQ UINT32_C(0x00000020)
#define XIMAP_MAILBOX_ACCESS UINT32_C(0x00000040)



/* STORE 模式映射到 FLAGS、+FLAGS、-FLAGS 及其静默形式。 */
typedef enum ximapstoremode {
	XIMAP_STORE_SET = 0,
	XIMAP_STORE_SET_SILENT,
	XIMAP_STORE_ADD,
	XIMAP_STORE_ADD_SILENT,
	XIMAP_STORE_REMOVE,
	XIMAP_STORE_REMOVE_SILENT
} ximapstoremode;



/* SELECT/EXAMINE 摘要不拥有字符串，Present 区分缺失字段和零值。 */
typedef struct ximapmailboxinfo {
	uint64 Exists;
	uint64 Recent;
	uint64 Unseen;
	uint64 UidValidity;
	uint64 UidNext;
	uint64 HighestModSeq;
	uint32 Present;
	bool ReadOnly;
} ximapmailboxinfo;



XRT_EXTERN_C_BEGIN



/* 初始化空邮箱摘要。 */
XRT_API void xrtImapMailboxInfoInit(ximapmailboxinfo* pInfo);



/* 把一条 SELECT、EXAMINE 或未请求更新合并到邮箱摘要。 */
XRT_API xmailnext xrtImapMailboxInfoUpdate(
	const ximapresponseview* pResponse,
	ximapmailboxinfo* pInfo
);



/* 执行 NOOP 并消费命令期间的未请求响应。 */
XRT_API bool xrtImapClientNoop(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 选择可写邮箱，并返回零分配状态摘要。 */
XRT_API bool xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	int64 iTimeout,
	xcancel* pCancel
);



/* 以只读方式选择邮箱，并返回零分配状态摘要。 */
XRT_API bool xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	int64 iTimeout,
	xcancel* pCancel
);



/* 对当前选中邮箱执行 CHECK。 */
XRT_API bool xrtImapClientCheck(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 不执行隐式 EXPUNGE 地离开当前邮箱。 */
XRT_API bool xrtImapClientUnselect(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 执行 CLOSE，提交删除标记并离开当前邮箱。 */
XRT_API bool xrtImapClientCloseMailbox(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



/* 创建、删除、重命名、订阅或取消订阅邮箱。 */
XRT_API bool xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
);



/* 开始会返回数据的命令；结果继续通过 Next 和 ReadLiteral 流式读取。 */
XRT_API bool xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
);

/* 空 UidSet 开始 EXPUNGE；非空集合开始 UID EXPUNGE。 */
XRT_API bool xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	int64 iTimeout,
	xcancel* pCancel
);

/* 开始 IDLE；收到 continuation 后可读取事件，结束时发送 DONE。 */
XRT_API bool xrtImapClientBeginIdle(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);

XRT_API bool xrtImapClientEndIdle(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_message.h */
/* ========================================================================== */

#ifndef XRT_IMAP_MESSAGE_H
#define XRT_IMAP_MESSAGE_H




#if defined(XIMAP_FEATURE_IMAP_MESSAGE) && \
	(!defined(XIMAP_FEATURE_IMAP_COMMAND) || \
	 !defined(XIMAP_FEATURE_IMAP_DATA) || \
	 !defined(XMAIL_FEATURE_MAIL_TREE) || \
	 !defined(XRT_FEATURE_BUFFER))
	#error "XIMAP_FEATURE_IMAP_MESSAGE requires IMAP command/data, mail tree and buffer"
#endif



#if defined(XIMAP_FEATURE_IMAP_MESSAGE)

#define XIMAP_MESSAGE_BYTES_DEFAULT XMAIL_TREE_SOURCE_BYTES_DEFAULT



XRT_EXTERN_C_BEGIN



/* 流式读取一封消息的 BODY section；空 Section 表示完整 RFC 消息。 */
XRT_API bool xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	int64 iTimeout,
	xcancel* pCancel
);



/* 读取 BODY section 并返回由 xrtFree 释放、末尾附零的连续字节。 */
XRT_API bytes xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	int64 iTimeout,
	xcancel* pCancel
);



/* 读取完整 BODY[] 并按 MIME 树预算解析；成功结果不依赖网络缓冲。 */
XRT_API bool xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_append.h */
/* ========================================================================== */

#ifndef XRT_IMAP_APPEND_H
#define XRT_IMAP_APPEND_H




#if defined(XIMAP_FEATURE_IMAP_APPEND) && \
	!defined(XIMAP_FEATURE_IMAP_CLIENT)
	#error "XIMAP_FEATURE_IMAP_APPEND requires IMAP client"
#endif



#if defined(XIMAP_FEATURE_IMAP_APPEND)

/* 自动模式优先遵守 APPENDLIMIT；显式模式允许调用方选择往返与吞吐。 */
typedef enum ximapliteralmode {
	XIMAP_LITERAL_AUTO = 0,
	XIMAP_LITERAL_SYNC,
	XIMAP_LITERAL_NONSYNC
} ximapliteralmode;



/* Flags 是可选括号列表，InternalDate 是可选未加引号日期文本。 */
typedef struct ximapappendconfig {
	xstrview Mailbox;
	xstrview Flags;
	xstrview InternalDate;
	size_t Size;
	ximapliteralmode Literal;
} ximapappendconfig;



/* UIDPLUS 结果只有在服务器返回合法 APPENDUID 时才标记 Present。 */
typedef struct ximapappendresult {
	uint64 UidValidity;
	uint64 Uid;
	bool Present;
} ximapappendresult;



XRT_EXTERN_C_BEGIN



/* 初始化同步策略为 AUTO，其余字段为空。 */
XRT_API void xrtImapAppendConfigInit(ximapappendconfig* pConfig);



/* 初始化无 APPENDUID 的空结果。 */
XRT_API void xrtImapAppendResultInit(ximapappendresult* pResult);



/*
	发送 APPEND 命令头并在同步 literal 模式下等待 continuation。
	成功后必须精确写入 Size 字节，再调用 AppendEnd。
*/
XRT_API bool xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
);



/* 返回活动 APPEND literal 尚未写入的字节数。 */
XRT_API size_t xrtImapClientAppendRemaining(const ximapclient* pClient);



/* 零复制发送下一块 literal；超过声明长度时不发送任何字节。 */
XRT_API bool xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	int64 iTimeout,
	xcancel* pCancel
);



/* 结束 literal，消费命令响应并解析可选 APPENDUID。 */
XRT_API bool xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	int64 iTimeout,
	xcancel* pCancel
);



/* 对已经完整驻留内存的消息执行一次 APPEND。 */
XRT_API bool xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/xrt/imap_compress.h */
/* ========================================================================== */

#ifndef XRT_IMAP_COMPRESS_H
#define XRT_IMAP_COMPRESS_H




#if defined(XIMAP_FEATURE_IMAP_COMPRESS) && \
	(!defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	 !defined(XMAIL_FEATURE_MAIL_NET_DEFLATE) || \
	 !defined(XRT_FEATURE_DEFLATE) || !defined(XRT_FEATURE_INFLATE))
	#error "XIMAP_FEATURE_IMAP_COMPRESS requires IMAP client and raw Deflate"
#endif



#if defined(XIMAP_FEATURE_IMAP_COMPRESS)

/* IMAP COMPRESS 使用 raw DEFLATE；级别、策略和窗口可按负载调整。 */
typedef struct ximapcompressconfig {
	int32 Level;
	xdeflatestrategy Strategy;
	uint8 WindowBits;
} ximapcompressconfig;



XRT_EXTERN_C_BEGIN



/* 初始化 XRT 默认压缩级别、策略和 32 KiB 窗口。 */
XRT_API void xrtImapCompressConfigInit(ximapcompressconfig* pConfig);



/* 验证 raw DEFLATE 编码与解码配置可以同时创建。 */
XRT_API bool xrtImapCompressConfigValid(
	const ximapcompressconfig* pConfig
);



/* 返回当前会话是否已经成功启用 IMAP COMPRESS。 */
XRT_API bool xrtImapClientCompressed(const ximapclient* pClient);



/* 协商 COMPRESS DEFLATE；只有 tagged OK 后才切换双向传输。 */
XRT_API bool xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
);



XRT_EXTERN_C_END

#endif

#endif


/* ========================================================================== */
/* public: extlibs/ximap/include/ximap.h */
/* ========================================================================== */

#ifndef XIMAP_H
#define XIMAP_H


#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	defined(XIMAP_FEATURE_IMAP_CLIENT_TLS)
#endif

#if defined(XIMAP_FEATURE_IMAP_AUTH)
#endif

#if defined(XIMAP_FEATURE_IMAP_COMMAND)
#endif

#if defined(XIMAP_FEATURE_IMAP_MESSAGE)
#endif

#if defined(XIMAP_FEATURE_IMAP_APPEND)
#endif

#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
#endif

#endif

#endif

#if defined(XIMAP_IMPLEMENTATION) && !defined(XIMAP_IMPLEMENTATION_ONCE)
#define XIMAP_IMPLEMENTATION_ONCE 1


/* ========================================================================== */
/* internal: extlibs/ximap/src/internal/xrt_mail.h */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP) || \
	defined(XIMAP_FEATURE_IMAP_DATA) || \
	defined(XIMAP_FEATURE_IMAP_BODY) || \
	defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
	defined(XIMAP_FEATURE_IMAP_AUTH) || \
	defined(XIMAP_FEATURE_IMAP_COMMAND) || \
	defined(XIMAP_FEATURE_IMAP_MESSAGE) || \
	defined(XIMAP_FEATURE_IMAP_APPEND) || \
	defined(XIMAP_FEATURE_IMAP_COMPRESS)
#ifndef XIMAP_INTERNAL_XRT_MAIL_H_BRIDGE_H
#define XIMAP_INTERNAL_XRT_MAIL_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/ximap/include/xrt/detail/ximap_wait.h */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP)
#ifndef XRT_DETAIL_XIMAP_WAIT_H
#define XRT_DETAIL_XIMAP_WAIT_H

XRT_EXTERN_C_BEGIN
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool __xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_AUTH))
XRT_API bool __xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API ximapclient* __xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API xmailnext __xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientRefresh(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientLogout(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool __xrtImapClientClose(
	ximapclient* pClient,
	double iDeadline
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientNoop(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCheck(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnselect(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCloseMailbox(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientBeginIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool __xrtImapClientEndIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_COMPRESS))
XRT_API bool __xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bytes __xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
);
#endif
#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool __xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
);
#endif
XRT_EXTERN_C_END
#endif
#endif


/* ========================================================================== */
/* internal: extlibs/ximap/src/internal/xrt_mail_net.h */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	defined(XIMAP_FEATURE_IMAP_CLIENT_TLS) || \
	defined(XIMAP_FEATURE_IMAP_COMPRESS)
#ifndef XIMAP_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H
#define XIMAP_INTERNAL_XRT_MAIL_NET_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* internal: extlibs/ximap/src/internal/xrt_imap_client.h */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_CLIENT) || \
	defined(XIMAP_FEATURE_IMAP_AUTH) || \
	defined(XIMAP_FEATURE_IMAP_COMMAND) || \
	defined(XIMAP_FEATURE_IMAP_APPEND) || \
	defined(XIMAP_FEATURE_IMAP_COMPRESS)
#ifndef XRT_INTERNAL_IMAP_CLIENT_H
#define XRT_INTERNAL_IMAP_CLIENT_H


#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
#endif



#if defined(XIMAP_FEATURE_IMAP_CLIENT)

/* 客户端只保存协议状态、能力快照和当前顺序命令，不缓存完整响应。 */
struct ximapclient {
	__xmailtransport Transport;
	__xmailtext Last;
	ximapclientstate State;
	uint64 Capabilities;
	uint64 AppendLimit;
	size_t CommandLineLimit;
	size_t LiteralRemaining;
	size_t AppendRemaining;
	uint32 TagCounter;
	char ActiveTag[XIMAP_CLIENT_TAG_MAX + 1u];
	size_t ActiveTagSize;
	bool Active;
	bool ExpectFragment;
	bool Idle;
	bool IdleDone;
	bool Append;
	bool Closing;
	bool LogoutSent;
};

bool __xrtImapClientStateCommit(
	ximapclient* pClient,
	ximapclientstate State
);



bool __xrtImapClientProtocolFail(
	ximapclient* pClient,
	cstr sMessage
);



bool __xrtImapClientAppendStart(ximapclient* pClient, size_t iSize);



size_t __xrtImapClientAppendRemaining(const ximapclient* pClient);



bool __xrtImapClientAppendFinish(ximapclient* pClient);



bool __xrtImapClientIdleStart(ximapclient* pClient);



bool __xrtImapClientIdleEnd(ximapclient* pClient);



#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
bool __xrtImapClientCompressStart(
	ximapclient* pClient,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
);



bool __xrtImapClientCompressed(const ximapclient* pClient);
#endif



#endif

#endif
#endif


/* ========================================================================== */
/* internal: extlibs/ximap/src/internal/xrt_mail_auth.h */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_AUTH)
#ifndef XIMAP_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H
#define XIMAP_INTERNAL_XRT_MAIL_AUTH_H_BRIDGE_H


#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP)



#if defined(XIMAP_FEATURE_IMAP)

/* 判断字节是否属于 IMAP atom-specials。 */
static bool __xrtImapAtomSpecial(unsigned char iByte)
{
	return (iByte == (unsigned char)'(') || (iByte == (unsigned char)')') ||
		(iByte == (unsigned char)'{') || (iByte == (unsigned char)' ') ||
		(iByte == (unsigned char)'%') || (iByte == (unsigned char)'*') ||
		(iByte == (unsigned char)'"') || (iByte == (unsigned char)'\\') ||
		(iByte == (unsigned char)']');
}



/* 判断 IMAP atom 的完整语法。 */
XRT_API bool xrtImapAtomValid(xstrview Atom)
{
	if ( !__xrtMailViewValid(Atom) || (Atom.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Atom.Size; i++ ) {
		unsigned char iByte = (unsigned char)Atom.Data[i];

		if ( (iByte <= 31u) || (iByte == 127u) ||
			 __xrtImapAtomSpecial(iByte) ) {
			return false;
		}
	}
	return true;
}



/* 读取一个非零 32 位序号或星号。 */
static bool __xrtImapSequenceNumber(
	xstrview Set,
	size_t* pPosition
)
{
	size_t iPosition = *pPosition;
	uint64 iValue = 0;

	if ( (iPosition < Set.Size) && (Set.Data[iPosition] == '*') ) {
		*pPosition = iPosition + 1u;
		return true;
	}
	if ( (iPosition >= Set.Size) || (Set.Data[iPosition] < '1') ||
		(Set.Data[iPosition] > '9') ) {
		return false;
	}
	while ( (iPosition < Set.Size) && (Set.Data[iPosition] >= '0') &&
		(Set.Data[iPosition] <= '9') ) {
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(Set.Data[iPosition] - '0');
		if ( iValue > UINT32_MAX ) {
			return false;
		}
		iPosition++;
	}
	*pPosition = iPosition;
	return true;
}



/* 验证 RFC sequence-set，并允许 SEARCHRES 的单独 `$`。 */
XRT_API bool xrtImapSequenceSetValid(xstrview Set)
{
	size_t iPosition = 0;

	if ( !__xrtMailViewValid(Set) || (Set.Size == 0) ) {
		return false;
	}
	if ( (Set.Size == 1u) && (Set.Data[0] == '$') ) {
		return true;
	}
	for ( ;; ) {
		if ( !__xrtImapSequenceNumber(Set, &iPosition) ) {
			return false;
		}
		if ( (iPosition < Set.Size) && (Set.Data[iPosition] == ':') ) {
			iPosition++;
			if ( !__xrtImapSequenceNumber(Set, &iPosition) ) {
				return false;
			}
		}
		if ( iPosition == Set.Size ) {
			return true;
		}
		if ( Set.Data[iPosition] != ',' ) {
			return false;
		}
		iPosition++;
		if ( iPosition == Set.Size ) {
			return false;
		}
	}
}



/* 跳过一段 SP。 */
static size_t __xrtImapSpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}



/* 读取一个由 SP 终止的 atom 视图。 */
static bool __xrtImapAtom(
	xstrview Text,
	size_t* pPosition,
	xstrview* pAtom
)
{
	size_t iPosition = *pPosition;
	size_t iStart = iPosition;

	while ( (iPosition < Text.Size) && (Text.Data[iPosition] != ' ') ) {
		iPosition++;
	}
	*pAtom = __xrtMailSlice(Text, iStart, iPosition - iStart);
	if ( !xrtImapAtomValid(*pAtom) ) {
		return false;
	}
	*pPosition = iPosition;
	return true;
}



/* 把状态 atom 映射为稳定枚举。 */
static ximapstatus __xrtImapStatus(xstrview Atom)
{
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("OK")) ) {
		return XIMAP_STATUS_OK;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("NO")) ) {
		return XIMAP_STATUS_NO;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("BAD")) ) {
		return XIMAP_STATUS_BAD;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("PREAUTH")) ) {
		return XIMAP_STATUS_PREAUTH;
	}
	if ( __xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("BYE")) ) {
		return XIMAP_STATUS_BYE;
	}
	return XIMAP_STATUS_NONE;
}



/* 验证响应文本不含线路控制字节。 */
static bool __xrtImapTextValid(xstrview Text)
{
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			 (iByte == (unsigned char)'\n') ||
			 ((iByte < 32u) && (iByte != (unsigned char)'\t')) ) {
			return false;
		}
	}
	return true;
}



/* 解析 IMAP 响应行。 */
XRT_API bool xrtImapResponseParse(
	xstrview Line,
	ximapresponseview* pResponse
)
{
	ximapresponseview Response;
	xstrview Atom;
	size_t iPosition;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pResponse, sizeof(*pResponse)) ||
		 xrtMemRangesOverlap(pResponse, sizeof(*pResponse), Line.Data,
			Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapTextValid(Line) || (Line.Size == 0) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response line"
		);
		return false;
	}
	Response.Source = Line;
	Response.Tag = __xrtMailView(NULL, 0);
	Response.Text = __xrtMailView(NULL, 0);
	Response.Status = XIMAP_STATUS_NONE;
	if ( Line.Data[0] == '+' ) {
		if ( (Line.Size > 1u) && (Line.Data[1] != ' ') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP continuation response"
			);
			return false;
		}
		iPosition = __xrtImapSpace(Line, 1u);
		Response.Kind = XIMAP_RESPONSE_CONTINUATION;
		Response.Text = __xrtMailSlice(Line, iPosition, Line.Size - iPosition);
		*pResponse = Response;
		return true;
	}
	if ( Line.Data[0] == '*' ) {
		if ( (Line.Size < 3u) || (Line.Data[1] != ' ') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP untagged response"
			);
			return false;
		}
		iPosition = 2u;
		if ( !__xrtImapAtom(Line, &iPosition, &Atom) ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP untagged response atom"
			);
			return false;
		}
		Response.Kind = XIMAP_RESPONSE_UNTAGGED;
		Response.Status = __xrtImapStatus(Atom);
		if ( Response.Status == XIMAP_STATUS_NONE ) {
			Response.Text = __xrtMailSlice(Line, 2u, Line.Size - 2u);
		} else {
			iPosition = __xrtImapSpace(Line, iPosition);
			Response.Text = __xrtMailSlice(
				Line,
				iPosition,
				Line.Size - iPosition
			);
		}
		*pResponse = Response;
		return true;
	}
	iPosition = 0;
	if ( !__xrtImapAtom(Line, &iPosition, &Response.Tag) ||
		 (Response.Tag.Data[0] == '+') || (iPosition == Line.Size) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged response"
		);
		return false;
	}
	iPosition = __xrtImapSpace(Line, iPosition);
	if ( !__xrtImapAtom(Line, &iPosition, &Atom) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged status"
		);
		return false;
	}
	Response.Status = __xrtImapStatus(Atom);
	if ( (Response.Status != XIMAP_STATUS_OK) &&
		 (Response.Status != XIMAP_STATUS_NO) &&
		 (Response.Status != XIMAP_STATUS_BAD) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP tagged status"
		);
		return false;
	}
	iPosition = __xrtImapSpace(Line, iPosition);
	Response.Kind = XIMAP_RESPONSE_TAGGED;
	Response.Text = __xrtMailSlice(Line, iPosition, Line.Size - iPosition);
	*pResponse = Response;
	return true;
}



/* 解析行尾 IMAP literal 标记。 */
XRT_API xmailnext xrtImapLiteralParse(
	xstrview Line,
	ximapliteralview* pLiteral
)
{
	ximapliteralview Literal;
	size_t iOpen = XRT_NPOS;
	size_t iDigits;
	size_t iPosition;
	size_t iEnd;
	uint64 iValue = 0;
	bool bPlus = false;

	if ( !__xrtMailViewValid(Line) ||
		 !xrtMemRangeValid(pLiteral, sizeof(*pLiteral)) ||
		 xrtMemRangesOverlap(pLiteral, sizeof(*pLiteral), Line.Data, Line.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Line.Size < 3u) || (Line.Data[Line.Size - 1u] != '}') ) {
		return XMAIL_NEXT_END;
	}
	for ( size_t i = Line.Size - 1u; i > 0; i-- ) {
		if ( Line.Data[i - 1u] == '{' ) {
			iOpen = i - 1u;
			break;
		}
	}
	if ( iOpen == XRT_NPOS ) {
		return XMAIL_NEXT_END;
	}
	Literal.Binary = (iOpen != 0) && (Line.Data[iOpen - 1u] == '~');
	iPosition = iOpen + 1u;
	if ( (Line.Size >= 4u) && (Line.Data[Line.Size - 2u] == '+') ) {
		bPlus = true;
	}
	iDigits = iPosition;
	iEnd = Line.Size - 1u - (bPlus ? 1u : 0u);
	while ( iPosition < iEnd ) {
		unsigned char iByte = (unsigned char)Line.Data[iPosition++];

		if ( (iByte < (unsigned char)'0') || (iByte > (unsigned char)'9') ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP literal marker"
			);
			return XMAIL_NEXT_ERROR;
		}
		if ( iValue > (((uint64)INT64_MAX - (uint64)(iByte - (unsigned char)'0')) / UINT64_C(10)) ) {
			__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT, "IMAP literal size exceeds 63 bits");
			return XMAIL_NEXT_ERROR;
		}
		iValue = (iValue * UINT64_C(10)) +
			(uint64)(iByte - (unsigned char)'0');
	}
	if ( (iPosition == iDigits) || (iValue > (uint64)SIZE_MAX) ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"IMAP literal size is invalid or too large"
		);
		return XMAIL_NEXT_ERROR;
	}
	Literal.Source = __xrtMailSlice(
		Line,
		Literal.Binary ? iOpen - 1u : iOpen,
		Line.Size - (Literal.Binary ? iOpen - 1u : iOpen)
	);
	Literal.Size = (size_t)iValue;
	Literal.NonSynchronizing = bPlus;
	*pLiteral = Literal;
	return XMAIL_NEXT_ITEM;
}



/* 解析状态文本开头的方括号响应码。 */
XRT_API xmailnext xrtImapCodeParse(
	xstrview Text,
	ximapcodeview* pCode
)
{
	ximapcodeview Code;
	size_t iClose = XRT_NPOS;
	size_t iNameEnd;
	size_t iArguments;
	size_t iArgumentsEnd;
	size_t iText;

	if ( !__xrtMailViewValid(Text) ||
		!xrtMemRangeValid(pCode, sizeof(*pCode)) ||
		xrtMemRangesOverlap(pCode, sizeof(*pCode), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Text.Size == 0) || (Text.Data[0] != '[') ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapTextValid(Text) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response code text"
		);
		return XMAIL_NEXT_ERROR;
	}
	for ( size_t i = 1u; i < Text.Size; i++ ) {
		if ( Text.Data[i] == ']' ) {
			iClose = i;
			break;
		}
	}
	if ( iClose == XRT_NPOS ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"unterminated IMAP response code"
		);
		return XMAIL_NEXT_ERROR;
	}
	iNameEnd = 1u;
	while ( (iNameEnd < iClose) && (Text.Data[iNameEnd] != ' ') ) {
		iNameEnd++;
	}
	Code.Name = __xrtMailSlice(Text, 1u, iNameEnd - 1u);
	if ( !xrtImapAtomValid(Code.Name) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP response code name"
		);
		return XMAIL_NEXT_ERROR;
	}
	iArguments = __xrtImapSpace(Text, iNameEnd);
	iArgumentsEnd = iClose;
	while ( (iArgumentsEnd > iArguments) &&
		(Text.Data[iArgumentsEnd - 1u] == ' ') ) {
		iArgumentsEnd--;
	}
	iText = iClose + 1u;
	if ( iText < Text.Size ) {
		if ( Text.Data[iText] != ' ' ) {
			__xrtMailError(
				XERR_PROTOCOL,
				XMAIL_ERROR_PROTOCOL,
				"invalid IMAP response code separator"
			);
			return XMAIL_NEXT_ERROR;
		}
		iText = __xrtImapSpace(Text, iText);
	}
	Code.Source = __xrtMailSlice(Text, 0, iClose + 1u);
	Code.Arguments = __xrtMailSlice(
		Text,
		iArguments,
		iArgumentsEnd - iArguments
	);
	Code.Text = __xrtMailSlice(Text, iText, Text.Size - iText);
	*pCode = Code;
	return XMAIL_NEXT_ITEM;
}



/* 解析数字开头的非标记响应文本。 */
XRT_API xmailnext xrtImapNumberParse(
	xstrview Text,
	ximapnumberview* pNumber
)
{
	ximapnumberview Number;
	size_t iPosition = 0;
	size_t iNameStart;
	size_t iNameEnd;
	uint64 iValue = 0;

	if ( !__xrtMailViewValid(Text) ||
		!xrtMemRangeValid(pNumber, sizeof(*pNumber)) ||
		xrtMemRangesOverlap(pNumber, sizeof(*pNumber), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (Text.Size == 0) || (Text.Data[0] < '0') ||
		(Text.Data[0] > '9') ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapTextValid(Text) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response text"
		);
		return XMAIL_NEXT_ERROR;
	}
	while ( (iPosition < Text.Size) &&
		(Text.Data[iPosition] >= '0') && (Text.Data[iPosition] <= '9') ) {
		uint64 iDigit = (uint64)(Text.Data[iPosition] - '0');

		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			__xrtMailError(
				XERR_RANGE,
				XMAIL_ERROR_LIMIT,
				"IMAP numeric response exceeds uint64"
			);
			return XMAIL_NEXT_ERROR;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
		iPosition++;
	}
	if ( (iPosition == Text.Size) || (Text.Data[iPosition] != ' ') ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response separator"
		);
		return XMAIL_NEXT_ERROR;
	}
	iNameStart = __xrtImapSpace(Text, iPosition);
	iNameEnd = iNameStart;
	while ( (iNameEnd < Text.Size) && (Text.Data[iNameEnd] != ' ') ) {
		iNameEnd++;
	}
	Number.Name = __xrtMailSlice(Text, iNameStart, iNameEnd - iNameStart);
	if ( !xrtImapAtomValid(Number.Name) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP numeric response name"
		);
		return XMAIL_NEXT_ERROR;
	}
	iPosition = __xrtImapSpace(Text, iNameEnd);
	Number.Source = Text;
	Number.Text = __xrtMailSlice(Text, iPosition, Text.Size - iPosition);
	Number.Number = iValue;
	*pNumber = Number;
	return XMAIL_NEXT_ITEM;
}



/* 初始化简单 atom 游标。 */
XRT_API bool xrtImapAtomCursorInit(
	ximapatomcursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !__xrtMailViewValid(Text) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回空白分隔的下一 atom。 */
XRT_API xmailnext xrtImapAtomNext(
	ximapatomcursor* pCursor,
	xstrview* pAtom
)
{
	size_t iPosition;
	xstrview Atom;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		 !xrtMemRangeValid(pAtom, sizeof(*pAtom)) ||
		 (pCursor == NULL) || !__xrtMailViewValid(pCursor->Text) ||
		 (pCursor->Position > pCursor->Text.Size) ||
		 xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pAtom, sizeof(*pAtom)) ||
		 xrtMemRangesOverlap(pAtom, sizeof(*pAtom), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	iPosition = __xrtImapSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapAtom(pCursor->Text, &iPosition, &Atom) ) {
		__xrtMailError(
			XERR_PROTOCOL,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP atom list"
		);
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Position = iPosition;
	*pAtom = Atom;
	return XMAIL_NEXT_ITEM;
}



/* 查找常用 IMAP capability 的稳定标记。 */
XRT_API uint64 xrtImapCapability(xstrview Capability)
{
	#define __XRT_IMAP_CAPABILITY(Name, Value) \
		{ Name, sizeof(Name) - 1u, Value }

	static const struct {
		cstr Name;
		size_t Size;
		uint64 Value;
	} arrCapabilities[] = {
		__XRT_IMAP_CAPABILITY("IMAP4REV1", XIMAP_CAP_IMAP4REV1),
		__XRT_IMAP_CAPABILITY("IMAP4REV2", XIMAP_CAP_IMAP4REV2),
		__XRT_IMAP_CAPABILITY("STARTTLS", XIMAP_CAP_STARTTLS),
		__XRT_IMAP_CAPABILITY("AUTH=PLAIN", XIMAP_CAP_AUTH_PLAIN),
		__XRT_IMAP_CAPABILITY("AUTH=XOAUTH2", XIMAP_CAP_AUTH_XOAUTH2),
		__XRT_IMAP_CAPABILITY("IDLE", XIMAP_CAP_IDLE),
		__XRT_IMAP_CAPABILITY("UIDPLUS", XIMAP_CAP_UIDPLUS),
		__XRT_IMAP_CAPABILITY("MOVE", XIMAP_CAP_MOVE),
		__XRT_IMAP_CAPABILITY("NAMESPACE", XIMAP_CAP_NAMESPACE),
		__XRT_IMAP_CAPABILITY("ENABLE", XIMAP_CAP_ENABLE),
		__XRT_IMAP_CAPABILITY("UTF8=ACCEPT", XIMAP_CAP_UTF8_ACCEPT),
		__XRT_IMAP_CAPABILITY("CONDSTORE", XIMAP_CAP_CONDSTORE),
		__XRT_IMAP_CAPABILITY("QRESYNC", XIMAP_CAP_QRESYNC),
		__XRT_IMAP_CAPABILITY("LITERAL+", XIMAP_CAP_LITERAL_PLUS),
		__XRT_IMAP_CAPABILITY("SASL-IR", XIMAP_CAP_SASL_IR),
		__XRT_IMAP_CAPABILITY("BINARY", XIMAP_CAP_BINARY),
		__XRT_IMAP_CAPABILITY("LOGINDISABLED", XIMAP_CAP_LOGIN_DISABLED),
		__XRT_IMAP_CAPABILITY(
			"AUTH=OAUTHBEARER",
			XIMAP_CAP_AUTH_OAUTHBEARER
		),
		__XRT_IMAP_CAPABILITY("UNSELECT", XIMAP_CAP_UNSELECT),
		__XRT_IMAP_CAPABILITY("ESEARCH", XIMAP_CAP_ESEARCH),
		__XRT_IMAP_CAPABILITY("LIST-EXTENDED", XIMAP_CAP_LIST_EXTENDED),
		__XRT_IMAP_CAPABILITY("SPECIAL-USE", XIMAP_CAP_SPECIAL_USE),
		__XRT_IMAP_CAPABILITY("SORT", XIMAP_CAP_SORT),
		__XRT_IMAP_CAPABILITY(
			"THREAD=REFERENCES",
			XIMAP_CAP_THREAD_REFERENCES
		),
		__XRT_IMAP_CAPABILITY("QUOTA", XIMAP_CAP_QUOTA),
		__XRT_IMAP_CAPABILITY("ACL", XIMAP_CAP_ACL),
		__XRT_IMAP_CAPABILITY("METADATA", XIMAP_CAP_METADATA),
		__XRT_IMAP_CAPABILITY("NOTIFY", XIMAP_CAP_NOTIFY),
		__XRT_IMAP_CAPABILITY(
			"COMPRESS=DEFLATE",
			XIMAP_CAP_COMPRESS_DEFLATE
		),
		__XRT_IMAP_CAPABILITY("APPENDLIMIT", XIMAP_CAP_APPENDLIMIT),
		__XRT_IMAP_CAPABILITY("LITERAL-", XIMAP_CAP_LITERAL_MINUS)
	};

	if ( !__xrtMailViewValid(Capability) ) {
		return 0;
	}
	if ( (Capability.Size > sizeof("APPENDLIMIT=") - 1u) &&
		__xrtMailAsciiEqualI(
			__xrtMailSlice(
				Capability,
				0,
				sizeof("APPENDLIMIT=") - 1u
			),
			XRT_STR_LITERAL("APPENDLIMIT=")
		) ) {
		return XIMAP_CAP_APPENDLIMIT;
	}
	for ( size_t i = 0; i < sizeof(arrCapabilities) /
		sizeof(arrCapabilities[0]); i++ ) {
		if ( __xrtMailAsciiEqualI(
			Capability,
			__xrtMailView(arrCapabilities[i].Name, arrCapabilities[i].Size)
		) ) {
			return arrCapabilities[i].Value;
		}
	}
	return 0;

	#undef __XRT_IMAP_CAPABILITY
}



/* 计算 quoted string 大小并验证输入。 */
static bool __xrtImapQuoteMeasure(xstrview Text, size_t* pRequired)
{
	size_t iRequired = 2u;

	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			__xrtMailError(
				XERR_ARGUMENT,
				XMAIL_ERROR_PROTOCOL,
				"IMAP quoted string requires a literal for control data"
			);
			return false;
		}
		if ( !__xrtMailSizeAdd(iRequired,
			((iByte == (unsigned char)'"') ||
			 (iByte == (unsigned char)'\\')) ? 2u : 1u,
			&iRequired) ) {
			return false;
		}
	}
	*pRequired = iRequired;
	return true;
}



/* 写出 IMAP quoted string。 */
XRT_API bool xrtImapQuoteWrite(
	xstrview Text,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iOutput = 0;

	if ( !__xrtMailViewValid(Text) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapQuoteMeasure(Text, &iRequired) ) {
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Text.Data,
			Text.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	sOutput[iOutput++] = '"';
	for ( size_t i = 0; i < Text.Size; i++ ) {
		char iByte = Text.Data[i];

		if ( (iByte == '"') || (iByte == '\\') ) {
			sOutput[iOutput++] = '\\';
		}
		sOutput[iOutput++] = iByte;
	}
	sOutput[iOutput++] = '"';
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 分配并写出 IMAP quoted string。 */
XRT_API str xrtImapQuote(xstrview Text, size_t* pOutputSize)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtImapQuoteWrite(Text, NULL, 0, &iRequired) ) {
		return NULL;
	}
	sOutput = (str)xrtMalloc(iRequired + 1u);
	if ( sOutput == NULL ) {
		return NULL;
	}
	if ( !xrtImapQuoteWrite(
		Text,
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



/* 验证命令参数只含单行可发送字节。 */
static bool __xrtImapArgumentsValid(xstrview Arguments)
{
	for ( size_t i = 0; i < Arguments.Size; i++ ) {
		unsigned char iByte = (unsigned char)Arguments.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 写出安全 IMAP 命令行。 */
XRT_API bool xrtImapCommandWrite(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired;
	size_t iSeparator = Arguments.Size != 0 ? 1u : 0;

	if ( !__xrtMailViewValid(Tag) || !__xrtMailViewValid(Command) ||
		 !__xrtMailViewValid(Arguments) ||
		 !xrtMemRangeValid(sOutput, iCapacity) ||
		 !xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAtomValid(Tag) || (Tag.Data[0] == '+') ||
		 !xrtImapAtomValid(Command) ||
		 !__xrtImapArgumentsValid(Arguments) ) {
		__xrtMailError(
			XERR_ARGUMENT,
			XMAIL_ERROR_PROTOCOL,
			"invalid IMAP command"
		);
		return false;
	}
	if ( iMaxLine == 0 ) {
		iMaxLine = XIMAP_COMMAND_LINE_DEFAULT;
	}
	if ( !__xrtMailSizeAdd(Tag.Size, 1u, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Command.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, iSeparator, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, Arguments.Size, &iRequired) ||
		 !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ) {
		return false;
	}
	if ( iRequired > iMaxLine ) {
		__xrtMailError(
			XERR_RANGE,
			XMAIL_ERROR_LIMIT,
			"IMAP command exceeds the line limit"
		);
		return false;
	}
	if ( xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Tag.Data,
			Tag.Size) ||
		 xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), Command.Data,
			Command.Size) ||
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
	if ( xrtMemRangesOverlap(sOutput, iRequired + 1u, Tag.Data, Tag.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Command.Data,
			Command.Size) ||
		 xrtMemRangesOverlap(sOutput, iRequired + 1u, Arguments.Data,
			Arguments.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	memcpy(sOutput, Tag.Data, Tag.Size);
	sOutput[Tag.Size] = ' ';
	memcpy(sOutput + Tag.Size + 1u, Command.Data, Command.Size);
	if ( iSeparator != 0 ) {
		sOutput[Tag.Size + 1u + Command.Size] = ' ';
		memcpy(
			sOutput + Tag.Size + Command.Size + 2u,
			Arguments.Data,
			Arguments.Size
		);
	}
	sOutput[iRequired - 2u] = '\r';
	sOutput[iRequired - 1u] = '\n';
	sOutput[iRequired] = 0;
	*pOutputSize = iRequired;
	return true;
}



/* 分配并写出 IMAP 命令行。 */
XRT_API str xrtImapCommand(
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	size_t iMaxLine,
	size_t* pOutputSize
)
{
	size_t iRequired;
	str sOutput;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtImapCommandWrite(
		Tag,
		Command,
		Arguments,
		iMaxLine,
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
	if ( !xrtImapCommandWrite(
		Tag,
		Command,
		Arguments,
		iMaxLine,
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
/* source: extlibs/ximap/src/imap/imap_data.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_DATA)




#if defined(XIMAP_FEATURE_IMAP_DATA)

/* 创建稳定的 IMAP 数据语法错误。 */
static bool __xrtImapDataError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 跳过 IMAP 数据项之间的 SP。 */
static size_t __xrtImapDataSpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}



/* 验证数据片段不含线路控制字节。 */
static bool __xrtImapDataByteValid(unsigned char Byte)
{
	return Byte >= 32u && Byte != 127u;
}

static bool __xrtImapDataTextValid(xstrview Text)
{
	if ( !__xrtMailViewValid(Text) ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		unsigned char iByte = (unsigned char)Text.Data[i];

		if ( !__xrtImapDataByteValid(iByte) ) {
			return false;
		}
	}
	return true;
}



/* 解析并保留 quoted string 的线路表示。 */
static bool __xrtImapDataQuoted(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart + 1u;

	while ( iPosition < Text.Size ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];
		if ( !__xrtImapDataByteValid(iByte) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}

		if ( iByte == (unsigned char)'"' ) {
			pValue->Source = __xrtMailSlice(
				Text,
				iStart,
				(iPosition - iStart) + 1u
			);
			pValue->Value = __xrtMailSlice(
				Text,
				iStart + 1u,
				iPosition - iStart - 1u
			);
			pValue->Kind = XIMAP_DATA_QUOTED;
			*pPosition = iPosition + 1u;
			return true;
		}
		if ( iByte == (unsigned char)'\\' ) {
			iPosition++;
			if ( (iPosition >= Text.Size) ||
				((Text.Data[iPosition] != '"') &&
				 (Text.Data[iPosition] != '\\')) ) {
				return __xrtImapDataError(
					XERR_PROTOCOL,
					"invalid IMAP quoted string escape"
				);
			}
		}
		iPosition++;
	}
	return __xrtImapDataError(
		XERR_PROTOCOL,
		"unterminated IMAP quoted string"
	);
}



/* 扫描一个完整、可嵌套但不跨 literal 的括号列表。 */
static bool __xrtImapDataList(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart + 1u;
	size_t iDepth = 1u;

	while ( iPosition < Text.Size ) {
		char iByte = Text.Data[iPosition];
		if ( !__xrtImapDataByteValid((unsigned char)iByte) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}

		if ( iByte == '"' ) {
			ximapdataview Quoted;

			if ( !__xrtImapDataQuoted(Text, &iPosition, &Quoted) ) {
				return false;
			}
			continue;
		}
		if ( iByte == '(' ) {
			if ( iDepth == SIZE_MAX ) {
				return __xrtImapDataError(
					XERR_RANGE,
					"IMAP data nesting exceeds size_t"
				);
			}
			iDepth++;
		} else if ( iByte == ')' ) {
			iDepth--;
			if ( iDepth == 0 ) {
				pValue->Source = __xrtMailSlice(
					Text,
					iStart,
					(iPosition - iStart) + 1u
				);
				pValue->Value = __xrtMailSlice(
					Text,
					iStart + 1u,
					iPosition - iStart - 1u
				);
				pValue->Kind = XIMAP_DATA_LIST;
				*pPosition = iPosition + 1u;
				return true;
			}
		}
		iPosition++;
	}
	return __xrtImapDataError(
		XERR_PROTOCOL,
		"unterminated IMAP data list"
	);
}



/* 解析 number 或非空原子值。 */
static bool __xrtImapDataAtom(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue,
	bool bAtomOnly
)
{
	size_t iStart = *pPosition;
	size_t iPosition = iStart;
	uint64 iNumber = 0;
	bool bNumber = true;
	bool bOverflow = false;

	while ( (iPosition < Text.Size) && (Text.Data[iPosition] != ' ') &&
		(Text.Data[iPosition] != '(') && (Text.Data[iPosition] != ')') ) {
		unsigned char iByte = (unsigned char)Text.Data[iPosition];
		if ( !__xrtImapDataByteValid(iByte) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}

		if ( bAtomOnly || (iByte < (unsigned char)'0') ||
			(iByte > (unsigned char)'9') ) {
			bNumber = false;
		} else if ( bNumber && !bOverflow ) {
			uint64 iDigit = (uint64)(iByte - (unsigned char)'0');

			if ( iNumber > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
				bOverflow = true;
			} else {
				iNumber = (iNumber * UINT64_C(10)) + iDigit;
			}
		}
		iPosition++;
	}
	if ( iPosition == iStart ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid empty IMAP data value"
		);
	}
	pValue->Source = __xrtMailSlice(Text, iStart, iPosition - iStart);
	pValue->Value = pValue->Source;
	if ( bNumber ) {
		if ( bOverflow ) {
			return __xrtImapDataError(
				XERR_RANGE,
				"IMAP number exceeds uint64"
			);
		}
		pValue->Kind = XIMAP_DATA_NUMBER;
		pValue->Number = iNumber;
	} else if ( !bAtomOnly && __xrtMailAsciiEqualI(
		pValue->Source,
		XRT_STR_LITERAL("NIL")
	) ) {
		pValue->Kind = XIMAP_DATA_NIL;
		pValue->Value = __xrtMailView(NULL, 0);
	} else {
		pValue->Kind = XIMAP_DATA_ATOM;
	}
	*pPosition = iPosition;
	return true;
}



/* 解析当前位置的一项数据值。 */
static bool __xrtImapDataValue(
	xstrview Text,
	size_t* pPosition,
	ximapdataview* pValue
)
{
	size_t iPosition = *pPosition;
	ximapliteralview Literal;
	xmailnext Next;

	memset(pValue, 0, sizeof(*pValue));
	if ( Text.Data[iPosition] == '"' ) {
		return __xrtImapDataQuoted(Text, pPosition, pValue);
	}
	if ( Text.Data[iPosition] == '(' ) {
		return __xrtImapDataList(Text, pPosition, pValue);
	}
	if ( (Text.Data[iPosition] == '{') ||
		((Text.Data[iPosition] == '~') &&
		 ((iPosition + 1u) < Text.Size) &&
		 (Text.Data[iPosition + 1u] == '{')) ) {
		xstrview Marker = __xrtMailSlice(
			Text,
			iPosition,
			Text.Size - iPosition
		);

		Next = xrtImapLiteralParse(Marker, &Literal);
		if ( Next != XMAIL_NEXT_ITEM ) {
			return Next == XMAIL_NEXT_ERROR ? false : __xrtImapDataError(
				XERR_PROTOCOL,
				"invalid IMAP literal data value"
			);
		}
		if ( (Literal.Source.Data != Marker.Data) ||
			(Literal.Source.Size != Marker.Size) ) {
			return __xrtImapDataError(
				XERR_PROTOCOL,
				"IMAP literal marker must occupy the line tail"
			);
		}
		pValue->Source = Literal.Source;
		pValue->Kind = XIMAP_DATA_LITERAL;
		pValue->LiteralSize = Literal.Size;
		pValue->LiteralNonSynchronizing = Literal.NonSynchronizing;
		pValue->LiteralBinary = Literal.Binary;
		*pPosition = Text.Size;
		return true;
	}
	return __xrtImapDataAtom(Text, pPosition, pValue, false);
}



/* 提取完整非标记响应的名称后数据区。 */
static bool __xrtImapDataPayload(
	xstrview Text,
	xstrview Name,
	xstrview* pPayload
)
{
	if ( !__xrtImapDataTextValid(Text) || (Text.Size < Name.Size) ||
		!__xrtMailAsciiEqualI(
			__xrtMailSlice(Text, 0, Name.Size),
			Name
		) || ((Text.Size > Name.Size) &&
		(Text.Data[Name.Size] != ' ')) ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"unexpected IMAP data response name"
		);
	}
	*pPayload = __xrtMailSlice(
		Text,
		__xrtImapDataSpace(Text, Name.Size),
		Text.Size - __xrtImapDataSpace(Text, Name.Size)
	);
	return true;
}



/* 初始化通用 IMAP 数据游标。 */
XRT_API bool xrtImapDataCursorInit(
	ximapdatacursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!__xrtImapDataTextValid(Text) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->Done = false;
	return true;
}



/* 返回一层数据区的下一项。 */
static xmailnext __xrtImapDataNext(
	ximapdatacursor* pCursor,
	ximapdataview* pValue,
	bool bAtomOnly
)
{
	size_t iPosition;
	ximapdataview Value;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pValue, sizeof(*pValue)) ||
		(pCursor == NULL) || !__xrtMailViewValid(pCursor->Text) ||
		(pCursor->Position > pCursor->Text.Size) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pValue, sizeof(*pValue)) ||
		xrtMemRangesOverlap(pValue, sizeof(*pValue), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done ) {
		return XMAIL_NEXT_END;
	}
	iPosition = __xrtImapDataSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	if ( pCursor->Text.Data[iPosition] == ')' ) {
		__xrtImapDataError(XERR_PROTOCOL, "unexpected IMAP list terminator");
		return XMAIL_NEXT_ERROR;
	}
	bool bLiteral = pCursor->Text.Data[iPosition] == '{' ||
		(pCursor->Text.Data[iPosition] == '~' && iPosition + 1u < pCursor->Text.Size &&
		 pCursor->Text.Data[iPosition + 1u] == '{');
	memset(&Value, 0, sizeof(Value));
	if ( !(bAtomOnly && !bLiteral && pCursor->Text.Data[iPosition] != '"' &&
		pCursor->Text.Data[iPosition] != '(' ?
		__xrtImapDataAtom(pCursor->Text, &iPosition, &Value, true) :
		__xrtImapDataValue(pCursor->Text, &iPosition, &Value)) ) {
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Position = iPosition;
	*pValue = Value;
	return XMAIL_NEXT_ITEM;
}

/* General data also serves adjacent BODY child lists. SP is imposed only by
 * the response grammar that requires it, without changing the generic API. */
XRT_API xmailnext xrtImapDataNext(ximapdatacursor* pCursor, ximapdataview* pValue)
{
	return __xrtImapDataNext(pCursor, pValue, false);
}

static xmailnext __xrtImapDataSeparated(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bAtomOnly)
{
	size_t iPosition = pCursor->Position;
	xmailnext Next = __xrtImapDataNext(pCursor, pValue, bAtomOnly);
	if ( Next == XMAIL_NEXT_ITEM && iPosition != 0 &&
		pValue->Source.Data == pCursor->Text.Data + iPosition ) {
		pCursor->Position = iPosition;
		__xrtImapDataError(XERR_PROTOCOL, "missing space between IMAP response fields");
		return XMAIL_NEXT_ERROR;
	}
	return Next;
}

static bool __xrtImapDataRequired(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bAtomOnly)
{
	xmailnext Next = __xrtImapDataSeparated(pCursor, pValue, bAtomOnly);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_ITEM || __xrtImapDataError(XERR_PROTOCOL, "missing IMAP response field");
}

static bool __xrtImapDataEnd(ximapdatacursor* pCursor)
{
	ximapdataview Extra;
	xmailnext Next = __xrtImapDataSeparated(pCursor, &Extra, false);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_END || __xrtImapDataError(XERR_PROTOCOL, "unexpected IMAP response field");
}

static bool __xrtImapDataAStringByte(unsigned char Byte)
{
	return __xrtImapDataByteValid(Byte) && Byte != (unsigned char)' ' &&
		Byte != (unsigned char)'(' && Byte != (unsigned char)')' && Byte != (unsigned char)'{' &&
		Byte != (unsigned char)'%' && Byte != (unsigned char)'*' && Byte != (unsigned char)'"' && Byte != (unsigned char)'\\';
}

/* An astring is a lexical string: numeric and NIL mailbox names must not be
 * converted to numbers/nstrings, including names larger than uint64. */
static bool __xrtImapDataAString(const ximapdataview* pValue)
{
	if ( pValue->Kind == XIMAP_DATA_QUOTED || pValue->Kind == XIMAP_DATA_LITERAL ) return true;
	if ( pValue->Kind != XIMAP_DATA_ATOM ) return false;
	for ( size_t i = 0; i < pValue->Source.Size; i++ )
		if ( !__xrtImapDataAStringByte((unsigned char)pValue->Source.Data[i]) ) return false;
	return pValue->Source.Size != 0;
}

/* LIST's quoted hierarchy delimiter is one character, including a single
 * UTF-8 scalar or an escaped quote/backslash. No Unicode module is needed. */
static bool __xrtImapDataDelimiter(const ximapdataview* pValue)
{
	if ( pValue->Kind == XIMAP_DATA_NIL ) return true;
	if ( pValue->Kind != XIMAP_DATA_QUOTED || pValue->Value.Size == 0 ) return false;
	const unsigned char* Bytes = (const unsigned char*)pValue->Value.Data;
	size_t Size = pValue->Value.Size;
	if ( Bytes[0] == (unsigned char)'\\' )
		return Size == 2u && (Bytes[1] == (unsigned char)'\\' || Bytes[1] == (unsigned char)'"');
	if ( Bytes[0] < 128u ) return Size == 1u;
	size_t Expected = Bytes[0] >= 0xc2u && Bytes[0] <= 0xdfu ? 2u :
		(Bytes[0] >= 0xe0u && Bytes[0] <= 0xefu ? 3u : (Bytes[0] >= 0xf0u && Bytes[0] <= 0xf4u ? 4u : 0u));
	if ( Size != Expected || Expected == 0 ) return false;
	for ( size_t i = 1; i < Size; i++ ) if ( Bytes[i] < 0x80u || Bytes[i] > 0xbfu ) return false;
	return !(Bytes[0] == 0xe0u && Bytes[1] < 0xa0u) && !(Bytes[0] == 0xedu && Bytes[1] >= 0xa0u) &&
		!(Bytes[0] == 0xf0u && Bytes[1] < 0x90u) && !(Bytes[0] == 0xf4u && Bytes[1] >= 0x90u);
}



/* 解码 quoted string，或直接复制 atom 和 NIL。 */
XRT_API bool xrtImapStringWrite(
	const ximapdataview* pValue,
	char* sOutput,
	size_t iCapacity,
	size_t* pOutputSize
)
{
	size_t iRequired = 0;
	size_t iOutput = 0;

	if ( !xrtMemRangeValid(pValue, sizeof(*pValue)) ||
		!xrtMemRangeValid(sOutput, iCapacity) ||
		!xrtMemRangeValid(pOutputSize, sizeof(*pOutputSize)) ||
		(pValue == NULL) || !__xrtMailViewValid(pValue->Source) ||
		!__xrtMailViewValid(pValue->Value) ||
		xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), pValue, sizeof(*pValue)) ||
		xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize), sOutput, iCapacity) ||
		xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			pValue->Source.Data, pValue->Source.Size) ||
		xrtMemRangesOverlap(pOutputSize, sizeof(*pOutputSize),
			pValue->Value.Data, pValue->Value.Size) ||
		xrtMemRangesOverlap(sOutput, iCapacity, pValue, sizeof(*pValue)) ||
		xrtMemRangesOverlap(sOutput, iCapacity, pValue->Source.Data, pValue->Source.Size) ||
		xrtMemRangesOverlap(sOutput, iCapacity, pValue->Value.Data, pValue->Value.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pValue->Kind != XIMAP_DATA_ATOM) &&
		(pValue->Kind != XIMAP_DATA_QUOTED) &&
		(pValue->Kind != XIMAP_DATA_NIL) ) {
		return __xrtImapDataError(
			XERR_STATE,
			"IMAP data value is not a string"
		);
	}
	if ( pValue->Kind == XIMAP_DATA_QUOTED ) {
		for ( size_t i = 0; i < pValue->Value.Size; i++ ) {
			if ( pValue->Value.Data[i] == '\\' ) {
				i++;
				if ( (i >= pValue->Value.Size) ||
					((pValue->Value.Data[i] != '"') &&
					 (pValue->Value.Data[i] != '\\')) ) {
					return __xrtImapDataError(
						XERR_PROTOCOL,
						"invalid IMAP quoted string escape"
					);
				}
			}
			iRequired++;
		}
	} else if ( pValue->Kind == XIMAP_DATA_ATOM ) {
		iRequired = pValue->Value.Size;
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
	if ( pValue->Kind == XIMAP_DATA_QUOTED ) {
		for ( size_t i = 0; i < pValue->Value.Size; i++ ) {
			if ( pValue->Value.Data[i] == '\\' ) {
				i++;
			}
			sOutput[iOutput++] = pValue->Value.Data[i];
		}
	} else if ( iRequired != 0 ) {
		memcpy(sOutput, pValue->Value.Data, iRequired);
		iOutput = iRequired;
	}
	sOutput[iOutput] = 0;
	*pOutputSize = iOutput;
	return true;
}



/* 解析 LIST 的三项固定前缀，并保留后续扩展。 */
XRT_API bool xrtImapListParse(
	xstrview Text,
	ximaplistview* pList
)
{
	ximaplistview List;
	ximapdatacursor Cursor;
	ximapdataview Attributes;
	ximapdataview Delimiter;
	ximapdataview Mailbox;
	xstrview Payload;
	size_t iExtensions;

	if ( !xrtMemRangeValid(pList, sizeof(*pList)) || !__xrtMailViewValid(Text) ||
		xrtMemRangesOverlap(pList, sizeof(*pList), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("LIST"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) ||
		!__xrtImapDataRequired(&Cursor, &Attributes, false) ) return false;
	if ( Attributes.Kind != XIMAP_DATA_LIST )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP LIST attributes");
	if ( !__xrtImapDataRequired(&Cursor, &Delimiter, false) ) return false;
	if ( !__xrtImapDataDelimiter(&Delimiter) )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP LIST delimiter");
	if ( !__xrtImapDataRequired(&Cursor, &Mailbox, true) ) return false;
	if ( !__xrtImapDataAString(&Mailbox) ||
		(Cursor.Position < Payload.Size && Payload.Data[Cursor.Position] != ' ') )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP LIST mailbox or extension separator");
	iExtensions = __xrtImapDataSpace(Payload, Cursor.Position);
	List.Source = Text;
	List.Attributes = Attributes.Value;
	List.Delimiter = Delimiter;
	List.Mailbox = Mailbox;
	List.Extensions = __xrtMailSlice(
		Payload,
		iExtensions,
		Payload.Size - iExtensions
	);
	*pList = List;
	return true;
}



/* 初始化括号 flag 列表游标。 */
XRT_API bool xrtImapFlagCursorInit(
	ximapflagcursor* pCursor,
	xstrview Flags
)
{
	ximapdatacursor Outer;
	ximapdataview List;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) || !__xrtMailViewValid(Flags) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Flags.Data, Flags.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&Outer, Flags) || !__xrtImapDataRequired(&Outer, &List, false) ) return false;
	if ( List.Kind != XIMAP_DATA_LIST )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP flag list");
	if ( !__xrtImapDataEnd(&Outer) ) return false;
	return xrtImapDataCursorInit(&pCursor->Data, List.Value);
}



/* 返回 flag 列表中的下一项。 */
XRT_API xmailnext xrtImapFlagNext(
	ximapflagcursor* pCursor,
	xstrview* pFlag
)
{
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pFlag, sizeof(*pFlag)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pFlag, sizeof(*pFlag)) ||
		xrtMemRangesOverlap(pFlag, sizeof(*pFlag), pCursor->Data.Text.Data, pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	ximapdatacursor Cursor = pCursor->Data;
	Next = __xrtImapDataSeparated(&Cursor, &Value, true);
	if ( Next != XMAIL_NEXT_ITEM ) {
		if ( Next == XMAIL_NEXT_END ) pCursor->Data = Cursor;
		return Next;
	}
	xstrview Atom = Value.Source;
	bool bStar = Atom.Size == 2u && Atom.Data[0] == '\\' && Atom.Data[1] == '*';
	if ( Atom.Size != 0 && Atom.Data[0] == '\\' ) Atom = __xrtMailSlice(Atom, 1u, Atom.Size - 1u);
	if ( Value.Kind != XIMAP_DATA_ATOM || (!bStar && !xrtImapAtomValid(Atom)) ) {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP flag value");
		return XMAIL_NEXT_ERROR;
	}
	pCursor->Data = Cursor;
	*pFlag = Value.Source;
	return XMAIL_NEXT_ITEM;
}



/* 解析 STATUS 的 mailbox 和属性列表。 */
XRT_API bool xrtImapStatusParse(
	xstrview Text,
	ximapmailboxstatusview* pStatus
)
{
	ximapmailboxstatusview Status;
	ximapdatacursor Cursor;
	ximapdataview Mailbox;
	ximapdataview Items;
	xstrview Payload;

	if ( !xrtMemRangeValid(pStatus, sizeof(*pStatus)) || !__xrtMailViewValid(Text) ||
		xrtMemRangesOverlap(pStatus, sizeof(*pStatus), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("STATUS"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) || !__xrtImapDataRequired(&Cursor, &Mailbox, true) ) return false;
	if ( !__xrtImapDataAString(&Mailbox) ) return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP STATUS mailbox");
	if ( !__xrtImapDataRequired(&Cursor, &Items, false) ) return false;
	if ( Items.Kind != XIMAP_DATA_LIST ) return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP STATUS items");
	if ( !__xrtImapDataEnd(&Cursor) ) return false;
	Status.Source = Text;
	Status.Mailbox = Mailbox;
	Status.Items = Items.Value;
	*pStatus = Status;
	return true;
}



/* 初始化 STATUS 名称和值游标。 */
XRT_API bool xrtImapStatusCursorInit(
	ximapstatuscursor* pCursor,
	const ximapmailboxstatusview* pStatus
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pStatus, sizeof(*pStatus)) ||
		(pStatus == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pStatus, sizeof(*pStatus)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtImapDataCursorInit(&pCursor->Data, pStatus->Items);
}



/* 返回下一 STATUS 属性对。 */
static xmailnext __xrtImapDataPair(
	ximapdatacursor* pCursor, xstrview* pName, ximapdataview* pValue)
{
	ximapdataview Name;
	xmailnext Next = __xrtImapDataSeparated(pCursor, &Name, true);
	if ( Next != XMAIL_NEXT_ITEM ) return Next;
	if ( Name.Kind != XIMAP_DATA_ATOM || !xrtImapAtomValid(Name.Source) ) {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP response item name");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapDataRequired(pCursor, pValue, false) ) return XMAIL_NEXT_ERROR;
	*pName = Name.Source;
	return XMAIL_NEXT_ITEM;
}

/* Known scalar fields retain their specified bounds; vendor extensions keep
 * their generic values instead of being coerced to a fixed schema. */
static bool __xrtImapDataScalar(const ximapdataview* pValue, bool bNonzero, uint64 Maximum)
{
	if ( pValue->Kind != XIMAP_DATA_NUMBER || (bNonzero && pValue->Number == 0) )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP numeric response item");
	return pValue->Number <= Maximum || __xrtImapDataError(XERR_RANGE, "IMAP response item exceeds its numeric range");
}

XRT_API xmailnext xrtImapStatusNext(
	ximapstatuscursor* pCursor,
	ximapstatusitem* pItem
)
{
	ximapstatusitem Item;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pItem, sizeof(*pItem), pCursor->Data.Text.Data, pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	ximapstatuscursor Cursor = *pCursor;
	Next = __xrtImapDataPair(&Cursor.Data, &Item.Name, &Item.Value);
	if ( Next == XMAIL_NEXT_ERROR ) return Next;
	if ( Next == XMAIL_NEXT_ITEM ) {
		bool Nonzero = __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("UIDNEXT")) ||
			__xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("UIDVALIDITY"));
		bool Size = __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("SIZE"));
		bool Count = __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("MESSAGES")) ||
			__xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("UNSEEN")) ||
			__xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("DELETED"));
		if ( (Nonzero || Size || Count) && !__xrtImapDataScalar(&Item.Value, Nonzero, Size ? (uint64)INT64_MAX : UINT32_MAX) )
			return XMAIL_NEXT_ERROR;
		*pItem = Item;
	}
	*pCursor = Cursor;
	return Next;
}



/* 初始化 SEARCH 结果游标。 */
XRT_API bool xrtImapSearchCursorInit(
	ximapsearchcursor* pCursor,
	xstrview Text
)
{
	xstrview Payload;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) || !__xrtMailViewValid(Text) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("SEARCH"), &Payload) ) return false;
	return xrtImapDataCursorInit(&pCursor->Data, Payload);
}



/* 返回 SEARCH ID 或 MODSEQ。 */
XRT_API xmailnext xrtImapSearchNext(
	ximapsearchcursor* pCursor,
	ximapsearchitem* pItem
)
{
	ximapdataview Value;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pItem, sizeof(*pItem), pCursor->Data.Text.Data, pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	ximapsearchcursor Working = *pCursor;
	ximapsearchitem Item;
	Next = __xrtImapDataSeparated(&Working.Data, &Value, false);
	if ( Next != XMAIL_NEXT_ITEM ) {
		if ( Next == XMAIL_NEXT_END ) *pCursor = Working;
		return Next;
	}
	if ( Value.Kind == XIMAP_DATA_NUMBER ) {
		if ( !__xrtImapDataScalar(&Value, true, UINT32_MAX) ) return XMAIL_NEXT_ERROR;
		Item.Kind = XIMAP_SEARCH_ID;
		Item.Number = Value.Number;
	} else if ( Value.Kind == XIMAP_DATA_LIST ) {
		ximapdatacursor Cursor;
		ximapdataview Name;
		ximapdataview Number;
		if ( !xrtImapDataCursorInit(&Cursor, Value.Value) ||
			!__xrtImapDataRequired(&Cursor, &Name, true) ) return XMAIL_NEXT_ERROR;
		if ( Name.Kind != XIMAP_DATA_ATOM || !__xrtMailAsciiEqualI(Name.Source, XRT_STR_LITERAL("MODSEQ")) ) {
			__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP SEARCH MODSEQ name");
			return XMAIL_NEXT_ERROR;
		}
		if ( !__xrtImapDataRequired(&Cursor, &Number, false) ||
			!__xrtImapDataScalar(&Number, false, UINT64_MAX) ||
			!__xrtImapDataEnd(&Cursor) || !__xrtImapDataEnd(&Working.Data) ) return XMAIL_NEXT_ERROR;
		Item.Kind = XIMAP_SEARCH_MODSEQ;
		Item.Number = Number.Number;
	} else {
		__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP SEARCH result");
		return XMAIL_NEXT_ERROR;
	}
	*pCursor = Working;
	*pItem = Item;
	return XMAIL_NEXT_ITEM;
}



/* 解析 ESEARCH 的可选 correlator 和 UID 指示器。 */
static bool __xrtImapDataCorrelator(const ximapdataview* pList)
{
	ximapdatacursor Cursor;
	ximapdataview Name, Tag;
	if ( !xrtImapDataCursorInit(&Cursor, pList->Value) ||
		!__xrtImapDataRequired(&Cursor, &Name, true) ) return false;
	if ( Name.Kind != XIMAP_DATA_ATOM || !__xrtMailAsciiEqualI(Name.Source, XRT_STR_LITERAL("TAG")) )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP ESEARCH correlator name");
	if ( !__xrtImapDataRequired(&Cursor, &Tag, true) ) return false;
	if ( (Tag.Kind != XIMAP_DATA_ATOM && Tag.Kind != XIMAP_DATA_QUOTED) || Tag.Value.Size == 0 )
		return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP ESEARCH correlator tag");
	for ( size_t i = 0; i < Tag.Value.Size; i++ ) {
		unsigned char Byte = (unsigned char)Tag.Value.Data[i];
		if ( Tag.Kind == XIMAP_DATA_QUOTED && Byte == (unsigned char)'\\' ) Byte = (unsigned char)Tag.Value.Data[++i];
		if ( Byte == (unsigned char)'+' || !__xrtImapDataAStringByte(Byte) )
			return __xrtImapDataError(XERR_PROTOCOL, "invalid IMAP ESEARCH correlator tag character");
	}
	return __xrtImapDataEnd(&Cursor);
}

XRT_API bool xrtImapESearchParse(
	xstrview Text,
	ximapesearchview* pSearch
)
{
	ximapesearchview Search;
	ximapdatacursor Cursor;
	ximapdataview Value;
	xstrview Payload;
	size_t iPosition;
	xmailnext Next;

	if ( !xrtMemRangeValid(pSearch, sizeof(*pSearch)) || !__xrtMailViewValid(Text) ||
		xrtMemRangesOverlap(pSearch, sizeof(*pSearch), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapDataPayload(Text, XRT_STR_LITERAL("ESEARCH"), &Payload) ||
		!xrtImapDataCursorInit(&Cursor, Payload) ) {
		return false;
	}
	memset(&Search, 0, sizeof(Search));
	Search.Source = Text;
	iPosition = Cursor.Position;
	Next = __xrtImapDataSeparated(&Cursor, &Value, false);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_ITEM) && (Value.Kind == XIMAP_DATA_LIST) ) {
		if ( !__xrtImapDataCorrelator(&Value) ) return false;
		Search.Correlator = Value.Value;
	} else {
		Cursor.Position = iPosition;
		Cursor.Done = false;
	}
	iPosition = Cursor.Position;
	Next = __xrtImapDataSeparated(&Cursor, &Value, true);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_ITEM) && (Value.Kind == XIMAP_DATA_ATOM) &&
		__xrtMailAsciiEqualI(Value.Source, XRT_STR_LITERAL("UID")) ) {
		Search.Uid = true;
		if ( Cursor.Position < Payload.Size && Payload.Data[Cursor.Position] != ' ' )
			return __xrtImapDataError(XERR_PROTOCOL, "missing space after IMAP ESEARCH UID");
	} else {
		Cursor.Position = iPosition;
		Cursor.Done = false;
	}
	iPosition = __xrtImapDataSpace(Payload, Cursor.Position);
	Search.Items = __xrtMailSlice(
		Payload,
		iPosition,
		Payload.Size - iPosition
	);
	*pSearch = Search;
	return true;
}



/* 初始化 ESEARCH 名称和值游标。 */
XRT_API bool xrtImapESearchCursorInit(
	ximapesearchcursor* pCursor,
	const ximapesearchview* pSearch
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pSearch, sizeof(*pSearch)) ||
		(pSearch == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pSearch,
			sizeof(*pSearch)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return xrtImapDataCursorInit(&pCursor->Data, pSearch->Items);
}



/* 返回下一 ESEARCH 返回数据对。 */
XRT_API xmailnext xrtImapESearchNext(
	ximapesearchcursor* pCursor,
	ximapesearchitem* pItem
)
{
	ximapesearchitem Item;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pItem, sizeof(*pItem), pCursor->Data.Text.Data, pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	ximapesearchcursor Cursor = *pCursor;
	Next = __xrtImapDataPair(&Cursor.Data, &Item.Name, &Item.Value);
	if ( Next == XMAIL_NEXT_ERROR ) return Next;
	if ( Next == XMAIL_NEXT_ITEM ) {
		bool Nonzero = __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("MIN")) ||
			__xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("MAX"));
		if ( (Nonzero || __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("COUNT"))) &&
			!__xrtImapDataScalar(&Item.Value, Nonzero, UINT32_MAX) ) return XMAIL_NEXT_ERROR;
		if ( __xrtMailAsciiEqualI(Item.Name, XRT_STR_LITERAL("ALL")) &&
			(!xrtImapSequenceSetValid(Item.Value.Source) ||
			 __xrtMailAsciiEqualI(Item.Value.Source, XRT_STR_LITERAL("$"))) ) {
			__xrtImapDataError(XERR_PROTOCOL, "invalid IMAP ESEARCH ALL set");
			return XMAIL_NEXT_ERROR;
		}
		*pItem = Item;
	}
	*pCursor = Cursor;
	return Next;
}



/* 解析数字开头的 FETCH 响应前缀。 */
XRT_API bool xrtImapFetchParse(
	xstrview Text,
	ximapfetchview* pFetch
)
{
	ximapnumberview Number;
	ximapfetchview Fetch;
	xmailnext Next;

	if ( !xrtMemRangeValid(pFetch, sizeof(*pFetch)) || !__xrtMailViewValid(Text) ||
		xrtMemRangesOverlap(pFetch, sizeof(*pFetch), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Next = xrtImapNumberParse(Text, &Number);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( Next != XMAIL_NEXT_ITEM || !__xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("FETCH")) ||
		(Number.Number == 0) ||
		(Number.Text.Size == 0) || (Number.Text.Data[0] != '(') ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP FETCH response"
		);
	}
	if ( Number.Number > UINT32_MAX ) return __xrtImapDataError(XERR_RANGE, "IMAP FETCH sequence exceeds uint32");
	Fetch.Source = Text;
	Fetch.Items = Number.Text;
	Fetch.Sequence = Number.Number;
	*pFetch = Fetch;
	return true;
}



/* 初始化 FETCH 属性游标。 */
XRT_API bool xrtImapFetchCursorInit(
	ximapfetchcursor* pCursor,
	const ximapfetchview* pFetch
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pFetch, sizeof(*pFetch)) ||
		(pFetch == NULL) || !__xrtImapDataTextValid(pFetch->Items) ||
		(pFetch->Items.Size == 0) || (pFetch->Items.Data[0] != '(') ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pFetch,
			sizeof(*pFetch)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pFetch->Items.Data, pFetch->Items.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = pFetch->Items;
	pCursor->Position = 1u;
	pCursor->NeedMore = false;
	pCursor->Done = false;
	return true;
}



/* 在 literal 正文消费完成后接入后续 FETCH 行片段。 */
XRT_API bool xrtImapFetchCursorContinue(
	ximapfetchcursor* pCursor,
	xstrview Text
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		(pCursor == NULL) || !pCursor->NeedMore || pCursor->Done ||
		!__xrtImapDataTextValid(Text) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	pCursor->Text = Text;
	pCursor->Position = 0;
	pCursor->NeedMore = false;
	return true;
}



/* 读取一个允许 section 内含空格的 FETCH 属性名。 */
static bool __xrtImapFetchAttribute(
	xstrview Text,
	size_t* pPosition,
	xstrview* pAttribute
)
{
	size_t iPosition = *pPosition;
	size_t iStart = iPosition;
	size_t iBrackets = 0;

	while ( iPosition < Text.Size ) {
		char iByte = Text.Data[iPosition];
		if ( !__xrtImapDataByteValid((unsigned char)iByte) ) {
			__xrtMailSetInvalidArgument();
			return false;
		}

		if ( iByte == '[' ) {
			iBrackets++;
		} else if ( iByte == ']' ) {
			if ( iBrackets == 0 ) {
				return __xrtImapDataError(
					XERR_PROTOCOL,
					"invalid IMAP FETCH section"
				);
			}
			iBrackets--;
		} else if ( (iBrackets == 0) &&
			((iByte == ' ') || (iByte == ')')) ) {
			break;
		}
		iPosition++;
	}
	if ( (iPosition == iStart) || (iBrackets != 0) ||
		(iPosition == Text.Size) || (Text.Data[iPosition] != ' ') ) {
		return __xrtImapDataError(
			XERR_PROTOCOL,
			"invalid IMAP FETCH attribute"
		);
	}
	*pAttribute = __xrtMailSlice(Text, iStart, iPosition - iStart);
	*pPosition = __xrtImapDataSpace(Text, iPosition);
	return true;
}



/* 返回下一 FETCH 属性和值。 */
XRT_API xmailnext xrtImapFetchNext(
	ximapfetchcursor* pCursor,
	ximapfetchitem* pItem
)
{
	ximapfetchitem Item;
	ximapdataview Value;
	size_t iPosition;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pItem, sizeof(*pItem)) ||
		(pCursor == NULL) || !__xrtMailViewValid(pCursor->Text) ||
		(pCursor->Position > pCursor->Text.Size) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pItem, sizeof(*pItem)) ||
		xrtMemRangesOverlap(pItem, sizeof(*pItem), pCursor->Text.Data,
			pCursor->Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Done || pCursor->NeedMore ) {
		return XMAIL_NEXT_END;
	}
	/* After a value (or a consumed literal), the next attribute needs SP;
	 * an immediate closing ')' is valid, including in a continuation. */
	if ( (pCursor->Position == 0 || pCursor->Position > 1u) &&
		pCursor->Position < pCursor->Text.Size &&
		pCursor->Text.Data[pCursor->Position] != ' ' &&
		pCursor->Text.Data[pCursor->Position] != ')' ) {
		__xrtImapDataError(XERR_PROTOCOL, "missing space between IMAP FETCH attributes");
		return XMAIL_NEXT_ERROR;
	}
	iPosition = __xrtImapDataSpace(pCursor->Text, pCursor->Position);
	if ( iPosition == pCursor->Text.Size ) {
		__xrtImapDataError(XERR_PROTOCOL, "unterminated IMAP FETCH response");
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Text.Data[iPosition] == ')' ) {
		iPosition = __xrtImapDataSpace(pCursor->Text, iPosition + 1u);
		if ( iPosition != pCursor->Text.Size ) {
			__xrtImapDataError(
				XERR_PROTOCOL,
				"trailing data after IMAP FETCH response"
			);
			return XMAIL_NEXT_ERROR;
		}
		pCursor->Position = iPosition;
		pCursor->Done = true;
		return XMAIL_NEXT_END;
	}
	memset(&Item, 0, sizeof(Item));
	if ( !__xrtImapFetchAttribute(
		pCursor->Text,
		&iPosition,
		&Item.Attribute
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( iPosition == pCursor->Text.Size ) {
		__xrtImapDataError(XERR_PROTOCOL, "missing IMAP FETCH value");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapDataValue(pCursor->Text, &iPosition, &Value) ) return XMAIL_NEXT_ERROR;
	Item.Value = Value;
	pCursor->Position = iPosition;
	if ( Value.Kind == XIMAP_DATA_LITERAL ) {
		pCursor->NeedMore = true;
	}
	*pItem = Item;
	return XMAIL_NEXT_ITEM;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_body.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_BODY)




#if defined(XIMAP_FEATURE_IMAP_BODY)

/* 创建稳定的 BODYSTRUCTURE 协议错误。 */
static bool __xrtImapBodyError(cstr sMessage)
{
	__xrtMailError(XERR_PROTOCOL, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 跳过 BODYSTRUCTURE 数据项之间的 SP。 */
static size_t __xrtImapBodySpace(xstrview Text, size_t iPosition)
{
	while ( (iPosition < Text.Size) && (Text.Data[iPosition] == ' ') ) {
		iPosition++;
	}
	return iPosition;
}

/* BODY fields require SP; only multipart's 1*body permits adjacent lists.
 * Let the data cursor validate its state before inspecting the old offset. */
static xmailnext __xrtImapBodyNext(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bAdjacentChildren)
{
	size_t iPosition = pCursor->Position;
	xmailnext Next = xrtImapDataNext(pCursor, pValue);
	if ( (Next == XMAIL_NEXT_ITEM) && (iPosition != 0) &&
		(pValue->Source.Data == pCursor->Text.Data + iPosition) &&
		!(bAdjacentChildren && pValue->Kind == XIMAP_DATA_LIST) ) {
		pCursor->Position = iPosition;
		__xrtImapBodyError("missing space between IMAP body fields");
		return XMAIL_NEXT_ERROR;
	}
	return Next;
}



/* 判断数据值是否是可在线路内完整表示的 IMAP string。 */
static bool __xrtImapBodyString(const ximapdataview* pValue)
{
	/* Complete lines cannot carry literal contents. A bare atom belongs to
	 * the separate astring grammar, not to IMAP string or nstring. */
	return pValue->Kind == XIMAP_DATA_QUOTED;
}



/* 判断数据值是否是可选 IMAP string。 */
static bool __xrtImapBodyNString(const ximapdataview* pValue)
{
	return (pValue->Kind == XIMAP_DATA_NIL) || __xrtImapBodyString(pValue);
}



/* 从游标读取一个必须存在的数据值。 */
static bool __xrtImapBodyRequired(
	ximapdatacursor* pCursor,
	ximapdataview* pValue
)
{
	xmailnext Next = __xrtImapBodyNext(pCursor, pValue, false);

	if ( Next == XMAIL_NEXT_ITEM ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_END ) {
		return __xrtImapBodyError("incomplete IMAP BODYSTRUCTURE");
	}
	return false;
}

/* A child parser owns its diagnostic. Validate a successful value separately
 * so a failed diagnostic allocation cannot be replaced by a parent error. */
static bool __xrtImapBodyRequiredString(
	ximapdatacursor* pCursor, ximapdataview* pValue, bool bOptional)
{
	if ( !__xrtImapBodyRequired(pCursor, pValue) ) return false;
	return (bOptional ? __xrtImapBodyNString(pValue) : __xrtImapBodyString(pValue)) ||
		__xrtImapBodyError("invalid IMAP body string field");
}

static bool __xrtImapBodyRequiredKind(
	ximapdatacursor* pCursor, ximapdataview* pValue, ximapdatakind Kind)
{
	if ( !__xrtImapBodyRequired(pCursor, pValue) ) return false;
	return pValue->Kind == Kind || __xrtImapBodyError("invalid IMAP body field type");
}

/* Known body sizes and extension numbers use the 63-bit IMAP domain. */
static bool __xrtImapBodyNumber64(const ximapdataview* pValue)
{
	if ( pValue->Number <= (uint64)INT64_MAX ) return true;
	__xrtMailError(XERR_RANGE, XMAIL_ERROR_LIMIT, "IMAP body number exceeds 63 bits");
	return false;
}

static bool __xrtImapBodyEnd(ximapdatacursor* pCursor)
{
	ximapdataview Extra;
	xmailnext Next = __xrtImapBodyNext(pCursor, &Extra, false);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_END || __xrtImapBodyError("unexpected IMAP envelope data");
}

/* Four nstrings; NIL group markers and empty strings remain valid. */
static bool __xrtImapBodyAddress(const ximapdataview* pAddress)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	if ( pAddress->Kind != XIMAP_DATA_LIST ) return __xrtImapBodyError("invalid IMAP envelope address");
	if ( !xrtImapDataCursorInit(&Cursor, pAddress->Value) ) return false;
	for ( size_t i = 0; i < 4u; i++ )
		if ( !__xrtImapBodyRequiredString(&Cursor, &Value, true) ) return false;
	return __xrtImapBodyEnd(&Cursor);
}

static bool __xrtImapBodyAddresses(const ximapdataview* pAddresses)
{
	ximapdatacursor Cursor;
	ximapdataview Address;
	bool Found = false;
	if ( pAddresses->Kind == XIMAP_DATA_NIL ) return true;
	if ( pAddresses->Kind != XIMAP_DATA_LIST ) return __xrtImapBodyError("invalid IMAP envelope address list");
	if ( !xrtImapDataCursorInit(&Cursor, pAddresses->Value) ) return false;
	for ( ;; ) {
		xmailnext Next = __xrtImapBodyNext(&Cursor, &Address, true);
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( Next == XMAIL_NEXT_END ) return Found || __xrtImapBodyError("empty IMAP envelope address list");
		if ( !__xrtImapBodyAddress(&Address) ) return false;
		Found = true;
	}
}

static bool __xrtImapBodyEnvelope(const ximapdataview* pEnvelope)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	if ( !xrtImapDataCursorInit(&Cursor, pEnvelope->Value) ) return false;
	for ( size_t i = 0; i < 10u; i++ ) {
		if ( !__xrtImapBodyRequired(&Cursor, &Value) ) return false;
		if ( i >= 2u && i <= 7u ) {
			if ( !__xrtImapBodyAddresses(&Value) ) return false;
		} else if ( !__xrtImapBodyNString(&Value) ) return __xrtImapBodyError("invalid IMAP envelope nstring");
	}
	return __xrtImapBodyEnd(&Cursor);
}



/* 校验 body-fld-param 的 NIL 或成对字符串列表。 */
static bool __xrtImapBodyParameters(const ximapdataview* pParameters)
{
	ximapdatacursor Cursor;
	ximapdataview Name;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( pParameters->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	if ( pParameters->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body parameters");
	if ( !xrtImapDataCursorInit(&Cursor, pParameters->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Name, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body parameters");
		}
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( !__xrtImapBodyString(&Name) )
			return __xrtImapBodyError("invalid IMAP body parameter name");
		if ( !__xrtImapBodyRequiredString(&Cursor, &Value, false) ) return false;
		iCount++;
	}
}



/* 校验 body-fld-dsp 的 NIL 或 `(type parameters)` 结构。 */
static bool __xrtImapBodyDisposition(const ximapdataview* pDisposition)
{
	ximapdatacursor Cursor;
	ximapdataview Type;
	ximapdataview Parameters;
	ximapdataview Extra;

	if ( pDisposition->Kind == XIMAP_DATA_NIL ) {
		return true;
	}
	if ( pDisposition->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body disposition");
	if ( !xrtImapDataCursorInit(&Cursor, pDisposition->Value) ||
		!__xrtImapBodyRequiredString(&Cursor, &Type, false) ||
		!__xrtImapBodyRequired(&Cursor, &Parameters) ||
		!__xrtImapBodyParameters(&Parameters) ) return false;
	xmailnext Next = __xrtImapBodyNext(&Cursor, &Extra, false);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	return Next == XMAIL_NEXT_END || __xrtImapBodyError("unexpected IMAP body disposition data");
}



/* 校验 body-fld-lang 的 NIL、单字符串或非空字符串列表。 */
static bool __xrtImapBodyLanguage(const ximapdataview* pLanguage)
{
	ximapdatacursor Cursor;
	ximapdataview Value;
	xmailnext Next;
	size_t iCount = 0;

	if ( (pLanguage->Kind == XIMAP_DATA_NIL) ||
		__xrtImapBodyString(pLanguage) ) {
		return true;
	}
	if ( pLanguage->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body language");
	if ( !xrtImapDataCursorInit(&Cursor, pLanguage->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Value, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body language list");
		}
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( !__xrtImapBodyString(&Value) )
			return __xrtImapBodyError("invalid IMAP body language string");
		iCount++;
	}
}



/* 递归校验服务器定义的 body-extension 值。 */
static bool __xrtImapBodyExtension(
	const ximapdataview* pValue,
	size_t iDepth
)
{
	ximapdatacursor Cursor;
	ximapdataview Child;
	xmailnext Next;
	size_t iCount = 0;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP body extension nesting is too deep");
	}
	if ( pValue->Kind == XIMAP_DATA_NUMBER ) return __xrtImapBodyNumber64(pValue);
	if ( __xrtImapBodyNString(pValue) ) return true;
	if ( pValue->Kind != XIMAP_DATA_LIST )
		return __xrtImapBodyError("invalid IMAP body extension");
	if ( !xrtImapDataCursorInit(&Cursor, pValue->Value) ) return false;
	for ( ;; ) {
		Next = __xrtImapBodyNext(&Cursor, &Child, false);
		if ( Next == XMAIL_NEXT_END ) {
			return iCount != 0 || __xrtImapBodyError("empty IMAP body extension list");
		}
		if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyExtension(&Child, iDepth + 1u) ) return false;
		iCount++;
	}
}



/* 前置声明递归 BODYSTRUCTURE 解析器。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
);



/* 校验未知扩展尾并保留其原始数据区。 */
static bool __xrtImapBodyExtensionTail(
	ximapdatacursor* pCursor,
	xstrview Text,
	xstrview* pExtensions
)
{
	ximapdataview Value;
	xmailnext Next;
	size_t iStart = __xrtImapBodySpace(Text, pCursor->Position);
	size_t iEnd = iStart;

	for ( ;; ) {
		Next = __xrtImapBodyNext(pCursor, &Value, false);
		if ( Next == XMAIL_NEXT_END ) {
			pExtensions->Data = Text.Data != NULL ? Text.Data + iStart : NULL;
			pExtensions->Size = iEnd - iStart;
			return true;
		}
		if ( (Next != XMAIL_NEXT_ITEM) ||
			!__xrtImapBodyExtension(&Value, 1u) ) {
			return false;
		}
		iEnd = (size_t)((Value.Source.Data + Value.Source.Size) - Text.Data);
	}
}



/* 按 RFC 顺序读取一个单部分的可选扩展字段。 */
static bool __xrtImapBodyOneExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body MD5 field");
	}
	pBody->Md5 = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyDisposition(&Value) ) return false;
	pBody->Disposition = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyLanguage(&Value) ) return false;
	pBody->Language = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP body location field");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 按 RFC 顺序读取 multipart 的可选扩展字段。 */
static bool __xrtImapBodyMultiExtensions(
	ximapdatacursor* pCursor,
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	xmailnext Next;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyParameters(&Value) ) return false;
	pBody->Parameters = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyDisposition(&Value) ) return false;
	pBody->Disposition = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR || !__xrtImapBodyLanguage(&Value) ) return false;
	pBody->Language = Value;

	Next = __xrtImapBodyNext(pCursor, &Value, false);
	if ( Next == XMAIL_NEXT_END ) {
		return true;
	}
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( !__xrtImapBodyNString(&Value) ) {
		return __xrtImapBodyError("invalid IMAP multipart location");
	}
	pBody->Location = Value;
	return __xrtImapBodyExtensionTail(pCursor, Text, &pBody->Extensions);
}



/* 解析普通、TEXT 或 MESSAGE/RFC822、MESSAGE/GLOBAL 单部分。 */
static bool __xrtImapBodyOnePart(
	ximapdatacursor* pCursor,
	const ximapdataview* pType,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Nested;
	bool bText;
	bool bMessage;

	pBody->Type = *pType;
	if ( !__xrtImapBodyString(&pBody->Type) )
		return __xrtImapBodyError("invalid IMAP body media type");
	if ( !__xrtImapBodyRequiredString(pCursor, &pBody->Subtype, false) ||
		!__xrtImapBodyRequired(pCursor, &pBody->Parameters) ||
		!__xrtImapBodyParameters(&pBody->Parameters) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Id, true) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Description, true) ||
		!__xrtImapBodyRequiredString(pCursor, &pBody->Encoding, false) ||
		!__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
		!__xrtImapBodyNumber64(&Value) ) return false;
	pBody->Octets = Value.Number;
	bText = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("TEXT")
	);
	bMessage = __xrtMailAsciiEqualI(
		pBody->Type.Value,
		XRT_STR_LITERAL("MESSAGE")
	) && (__xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("RFC822")
	) || __xrtMailAsciiEqualI(
		pBody->Subtype.Value,
		XRT_STR_LITERAL("GLOBAL")
	));
	if ( bText ) {
		pBody->Kind = XIMAP_BODY_TEXT;
		if ( !__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
			!__xrtImapBodyNumber64(&Value) ) return false;
		pBody->Lines = Value.Number;
	} else if ( bMessage ) {
		pBody->Kind = XIMAP_BODY_MESSAGE;
		if ( !__xrtImapBodyRequiredKind(pCursor, &pBody->Envelope, XIMAP_DATA_LIST) ||
			!__xrtImapBodyEnvelope(&pBody->Envelope) ||
			!__xrtImapBodyRequiredKind(pCursor, &pBody->Body, XIMAP_DATA_LIST) ||
			!__xrtImapBodyParseList(
				&pBody->Body,
				iDepth + 1u,
				&Nested
			) || !__xrtImapBodyRequiredKind(pCursor, &Value, XIMAP_DATA_NUMBER) ||
			!__xrtImapBodyNumber64(&Value) ) return false;
		pBody->Lines = Value.Number;
	} else {
		pBody->Kind = XIMAP_BODY_BASIC;
	}
	return __xrtImapBodyOneExtensions(pCursor, Text, pBody);
}



/* 解析一个或多个子部分、subtype 与 multipart 扩展字段。 */
static bool __xrtImapBodyMultipart(
	ximapdatacursor* pCursor,
	const ximapdataview* pFirst,
	xstrview Text,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapdataview Value = *pFirst;
	ximapbodyview Child;
	const char* sChildren = pFirst->Source.Data;
	const char* sChildrenEnd = sChildren;

	pBody->Kind = XIMAP_BODY_MULTIPART;
	for ( ;; ) {
		if ( !__xrtImapBodyParseList(&Value, iDepth + 1u, &Child) ) return false;
		if ( pBody->ChildCount == SIZE_MAX ) {
			return __xrtImapBodyError("too many IMAP multipart children");
		}
		pBody->ChildCount++;
		sChildrenEnd = Value.Source.Data + Value.Source.Size;
		xmailnext Next = __xrtImapBodyNext(pCursor, &Value, true);
		if ( Next == XMAIL_NEXT_ERROR ) return false;
		if ( Next == XMAIL_NEXT_END )
			return __xrtImapBodyError("missing IMAP multipart subtype");
		if ( Value.Kind != XIMAP_DATA_LIST ) {
			break;
		}
	}
	pBody->Children.Data = sChildren;
	pBody->Children.Size = (size_t)(sChildrenEnd - sChildren);
	pBody->Subtype = Value;
	if ( !__xrtImapBodyString(&pBody->Subtype) ) {
		return __xrtImapBodyError("invalid IMAP multipart subtype");
	}
	return __xrtImapBodyMultiExtensions(pCursor, Text, pBody);
}



/* 解析一个已由通用数据层定界的括号 BODYSTRUCTURE 值。 */
static bool __xrtImapBodyParseList(
	const ximapdataview* pList,
	size_t iDepth,
	ximapbodyview* pBody
)
{
	ximapbodyview Body;
	ximapdatacursor Cursor;
	ximapdataview First;

	if ( iDepth > XIMAP_BODY_DEPTH_MAX ) {
		return __xrtImapBodyError("IMAP BODYSTRUCTURE nesting is too deep");
	}
	if ( pList->Kind != XIMAP_DATA_LIST ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE list");
	}
	if ( !xrtImapDataCursorInit(&Cursor, pList->Value) ||
		!__xrtImapBodyRequired(&Cursor, &First) ) return false;
	memset(&Body, 0, sizeof(Body));
	Body.Source = pList->Source;
	if ( First.Kind == XIMAP_DATA_LIST ) {
		if ( !__xrtImapBodyMultipart(
			&Cursor,
			&First,
			pList->Value,
			iDepth,
			&Body
		) ) {
			return false;
		}
	} else if ( !__xrtImapBodyOnePart(
		&Cursor,
		&First,
		pList->Value,
		iDepth,
		&Body
	) ) {
		return false;
	}
	*pBody = Body;
	return true;
}



/* 解析并递归校验完整 BODYSTRUCTURE。 */
XRT_API bool xrtImapBodyParse(
	xstrview Text,
	ximapbodyview* pBody
)
{
	ximapdatacursor Cursor;
	ximapdataview List;
	ximapdataview Extra;
	ximapbodyview Body;
	xmailnext Next;

	if ( !xrtMemRangeValid(pBody, sizeof(*pBody)) ||
		!xrtMemRangeValid(Text.Data, Text.Size) ||
		xrtMemRangesOverlap(pBody, sizeof(*pBody), Text.Data, Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&Cursor, Text) ) return false;
	Next = xrtImapDataNext(&Cursor, &List);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( (Next != XMAIL_NEXT_ITEM) || (List.Kind != XIMAP_DATA_LIST) ) {
		return __xrtImapBodyError("invalid IMAP BODYSTRUCTURE");
	}
	Next = xrtImapDataNext(&Cursor, &Extra);
	if ( Next == XMAIL_NEXT_ERROR ) return false;
	if ( Next != XMAIL_NEXT_END )
		return __xrtImapBodyError("unexpected IMAP BODYSTRUCTURE trailing data");
	if ( !__xrtImapBodyParseList(&List, 0, &Body) ) return false;
	*pBody = Body;
	return true;
}



/* 初始化 multipart 直接子部分游标。 */
XRT_API bool xrtImapBodyChildCursorInit(
	ximapbodycursor* pCursor,
	const ximapbodyview* pBody
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pBody == NULL) ||
		(pBody->Kind != XIMAP_BODY_MULTIPART) ||
		(pBody->ChildCount == 0) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody->Children.Data,
			pBody->Children.Size) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapDataCursorInit(&pCursor->Data, pBody->Children) ) return false;
	pCursor->Remaining = pBody->ChildCount;
	return true;
}



/* 返回下一个 multipart 直接子部分。 */
XRT_API xmailnext xrtImapBodyChildNext(
	ximapbodycursor* pCursor,
	ximapbodyview* pBody
)
{
	ximapdataview Value;
	ximapbodyview Body;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pBody, sizeof(*pBody)) || (pCursor == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pBody,
			sizeof(*pBody)) ||
		xrtMemRangesOverlap(pBody, sizeof(*pBody), pCursor->Data.Text.Data,
			pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( pCursor->Remaining == 0 ) {
		Next = __xrtImapBodyNext(&pCursor->Data, &Value, true);
		if ( Next == XMAIL_NEXT_END ) {
			return XMAIL_NEXT_END;
		}
		if ( Next == XMAIL_NEXT_ERROR ) return XMAIL_NEXT_ERROR;
		__xrtImapBodyError("unexpected IMAP multipart child data");
		return XMAIL_NEXT_ERROR;
	}
	Next = __xrtImapBodyNext(&pCursor->Data, &Value, true);
	if ( Next == XMAIL_NEXT_ERROR ) return XMAIL_NEXT_ERROR;
	if ( (Next != XMAIL_NEXT_ITEM) || (Value.Kind != XIMAP_DATA_LIST) ) {
		__xrtImapBodyError("invalid IMAP multipart child cursor");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapBodyParseList(&Value, 0, &Body) ) return XMAIL_NEXT_ERROR;
	pCursor->Remaining--;
	*pBody = Body;
	return XMAIL_NEXT_ITEM;
}



/* 初始化成对参数游标。 */
XRT_API bool xrtImapBodyParamCursorInit(
	ximapbodyparamcursor* pCursor,
	const ximapdataview* pParameters
)
{
	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameters, sizeof(*pParameters)) ||
		(pParameters == NULL) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameters,
			sizeof(*pParameters)) ||
		((pParameters->Kind != XIMAP_DATA_NIL) &&
		 (pParameters->Kind != XIMAP_DATA_LIST)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	return __xrtImapBodyParameters(pParameters) &&
		xrtImapDataCursorInit(&pCursor->Data, pParameters->Value);
}



/* 返回下一参数名和值。 */
XRT_API xmailnext xrtImapBodyParamNext(
	ximapbodyparamcursor* pCursor,
	ximapbodyparam* pParameter
)
{
	ximapbodyparam Parameter;
	xmailnext Next;

	if ( !xrtMemRangeValid(pCursor, sizeof(*pCursor)) ||
		!xrtMemRangeValid(pParameter, sizeof(*pParameter)) ||
		xrtMemRangesOverlap(pCursor, sizeof(*pCursor), pParameter,
			sizeof(*pParameter)) ||
		xrtMemRangesOverlap(pParameter, sizeof(*pParameter), pCursor->Data.Text.Data,
			pCursor->Data.Text.Size) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	Next = __xrtImapBodyNext(&pCursor->Data, &Parameter.Name, false);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( !__xrtImapBodyString(&Parameter.Name) ) {
		__xrtImapBodyError("invalid IMAP body parameter pair");
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapBodyRequiredString(&pCursor->Data, &Parameter.Value, false) )
		return XMAIL_NEXT_ERROR;
	*pParameter = Parameter;
	return XMAIL_NEXT_ITEM;
}

#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_client.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_CLIENT)




#if defined(XIMAP_FEATURE_IMAP_CLIENT)

/* 设置 IMAP 客户端稳定错误。 */
static bool __xrtImapClientError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证客户端仍可交换协议数据。 */
static bool __xrtImapClientUsable(const ximapclient* pClient)
{
	if ( (pClient == NULL) || (pClient->State == XIMAP_CLIENT_CLOSED) ||
		(pClient->State == XIMAP_CLIENT_FAILED) ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client is not usable"
		);
	}
	return true;
}




/* BYE 或 LOGOUT 后仍可读取响应，但不能再发送命令。 */
static bool __xrtImapClientWritable(const ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( pClient->Closing ) {
		return __xrtImapClientError(XERR_CLOSED, "IMAP server is closing");
	}
	return true;
}



/* 把不可恢复的传输或协议失败记录为终态。 */
static bool __xrtImapClientFailed(ximapclient* pClient)
{
	if ( pClient != NULL ) {
		pClient->State = XIMAP_CLIENT_FAILED;
		__xrtMailTransportAbortPreserveError(&pClient->Transport);
	}
	return false;
}



/* 比较两个 IMAP tag；tag 按字节区分大小写。 */
static bool __xrtImapClientTagEqual(xstrview Left, xstrview Right)
{
	return (Left.Size == Right.Size) &&
		((Left.Size == 0) ||
		 (memcmp(Left.Data, Right.Data, Left.Size) == 0));
}



/* 保存 greeting 或 tagged completion，供错误诊断和高层状态转换使用。 */
static bool __xrtImapClientResponseSave(
	ximapclient* pClient,
	const ximapresponseview* pResponse
)
{
	return __xrtMailTextSet(&pClient->Last, pResponse->Source);
}



/* 生成固定宽度十六进制 tag，避免每条命令分配内存。 */
static xstrview __xrtImapClientTagNext(ximapclient* pClient)
{
	static const char sHex[] = "0123456789ABCDEF";
	uint32 iValue;

	pClient->TagCounter++;
	if ( pClient->TagCounter == 0 ) {
		pClient->TagCounter = 1;
	}
	iValue = pClient->TagCounter;
	pClient->ActiveTag[0] = 'A';
	for ( size_t i = 0; i < 8u; i++ ) {
		pClient->ActiveTag[8u - i] = sHex[iValue & 0x0Fu];
		iValue >>= 4u;
	}
	pClient->ActiveTag[9] = 0;
	pClient->ActiveTagSize = 9u;
	return __xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize);
}



/* 验证参数片只含可直接放入命令行的可见字节。 */
static bool __xrtImapClientPartValid(xstrview Part)
{
	if ( !__xrtMailViewValid(Part) || (Part.Size == 0) ) {
		return false;
	}
	for ( size_t i = 0; i < Part.Size; i++ ) {
		unsigned char iByte = (unsigned char)Part.Data[i];

		if ( (iByte < 32u) || (iByte == 127u) ) {
			return false;
		}
	}
	return true;
}



/* 验证线路可发送后，逐片写出完整 tagged 命令。 */
static bool __xrtImapClientSendCommandParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
)
{
	size_t iRequired;

	if ( !__xrtMailViewValid(Tag) || !__xrtMailViewValid(Command) ||
		(iCount > (SIZE_MAX / sizeof(*pArguments))) ||
		!xrtMemRangeValid(pArguments, iCount * sizeof(*pArguments)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAtomValid(Tag) || (Tag.Data[0] == '+') ||
		!xrtImapAtomValid(Command) ) {
		return __xrtImapClientError(XERR_ARGUMENT, "invalid IMAP command");
	}
	if ( !__xrtMailSizeAdd(Tag.Size, 1u, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, Command.Size, &iRequired) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP command size overflow"
		);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtImapClientPartValid(pArguments[i]) ) {
			return __xrtImapClientError(
				XERR_ARGUMENT,
				"invalid IMAP command argument"
			);
		}
		if ( !__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
			!__xrtMailSizeAdd(
				iRequired,
				pArguments[i].Size,
				&iRequired
			) ) {
			return __xrtImapClientError(
				XERR_RANGE,
				"IMAP command size overflow"
			);
		}
	}
	if ( !__xrtMailSizeAdd(iRequired, 2u, &iRequired) ||
		(iRequired > pClient->CommandLineLimit) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP command exceeds the line limit"
		);
	}
	if ( !__xrtMailTransportWrite(
		&pClient->Transport,
		Tag.Data,
		Tag.Size,
		false,
		iDeadline,
		pCancel
	) || !__xrtMailTransportWrite(
		&pClient->Transport,
		" ",
		1u,
		false,
		iDeadline,
		pCancel
	) || !__xrtMailTransportWrite(
		&pClient->Transport,
		Command.Data,
		Command.Size,
		false,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	for ( size_t i = 0; i < iCount; i++ ) {
		if ( !__xrtMailTransportWrite(
			&pClient->Transport,
			" ",
			1u,
			false,
			iDeadline,
			pCancel
		) || !__xrtMailTransportWrite(
			&pClient->Transport,
			pArguments[i].Data,
			pArguments[i].Size,
			false,
			iDeadline,
			pCancel
		) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
	if ( !__xrtMailTransportSend(
		&pClient->Transport,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( __xrtMailAsciiEqualI(Command, XRT_STR_LITERAL("LOGOUT")) ) {
		pClient->LogoutSent = true;
		pClient->Closing = true;
	}
	return true;
}



/* 把旧的单参数入口映射到分片发送核心。 */
static bool __xrtImapClientSendCommand(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		Arguments.Size != 0 ? &Arguments : NULL,
		Arguments.Size != 0 ? 1u : 0,
		iDeadline,
		pCancel
	);
}



/* 把一条 CAPABILITY 响应合并到局部快照。 */
static bool __xrtImapClientCapabilityLine(
	xstrview Text,
	uint64* pCapabilities,
	uint64* pAppendLimit,
	bool* pSeen
)
{
	ximapatomcursor Cursor;
	xstrview Atom;
	xmailnext Next;

	if ( !xrtImapAtomCursorInit(&Cursor, Text) ||
		xrtImapAtomNext(&Cursor, &Atom) != XMAIL_NEXT_ITEM ) {
		return false;
	}
	if ( !__xrtMailAsciiEqualI(Atom, XRT_STR_LITERAL("CAPABILITY")) ) {
		return true;
	}
	*pSeen = true;
	for ( ;; ) {
		Next = xrtImapAtomNext(&Cursor, &Atom);
		if ( Next == XMAIL_NEXT_END ) {
			return true;
		}
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		*pCapabilities |= xrtImapCapability(Atom);
		if ( (Atom.Size > sizeof("APPENDLIMIT=") - 1u) &&
			__xrtMailAsciiEqualI(
				__xrtMailSlice(
					Atom,
					0,
					sizeof("APPENDLIMIT=") - 1u
				),
				XRT_STR_LITERAL("APPENDLIMIT=")
			) ) {
			uint64 iLimit = 0;

			for ( size_t i = sizeof("APPENDLIMIT=") - 1u;
				i < Atom.Size; i++ ) {
				uint64 iDigit;

				if ( (Atom.Data[i] < '0') || (Atom.Data[i] > '9') ) {
					return __xrtImapClientError(
						XERR_PROTOCOL,
						"invalid IMAP APPENDLIMIT capability"
					);
				}
				iDigit = (uint64)(Atom.Data[i] - '0');
				if ( iLimit > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
					return __xrtImapClientError(
						XERR_RANGE,
						"IMAP APPENDLIMIT capability is too large"
					);
				}
				iLimit = (iLimit * UINT64_C(10)) + iDigit;
			}
			*pAppendLimit = iLimit;
		}
	}
}



/* 等待当前顺序命令完成，不接受 literal，并返回最终状态。 */
static bool __xrtImapClientFinishSimple(
	ximapclient* pClient,
	bool bCapabilities,
	uint64* pCapabilities,
	uint64* pAppendLimit,
	bool* pCapabilitySeen,
	bool* pBye,
	ximapstatus* pStatus,
	double iDeadline,
	xcancel* pCancel
)
{
	ximapevent Event;
	xmailnext Next;

	for ( ;; ) {
		Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			*pStatus = Final.Status;
			return true;
		}
		if ( Event.HasLiteral ) {
			(void)__xrtImapClientError(
				XERR_PROTOCOL,
				"unexpected IMAP literal in a simple command"
			);
			return __xrtImapClientFailed(pClient);
		}
		if ( Event.Kind != XIMAP_EVENT_RESPONSE ) {
			continue;
		}
		if ( (pBye != NULL) &&
			(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
			(Event.Response.Status == XIMAP_STATUS_BYE) ) {
			*pBye = true;
		}
		if ( bCapabilities &&
			(Event.Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
			(Event.Response.Status == XIMAP_STATUS_NONE) &&
			!__xrtImapClientCapabilityLine(
				Event.Response.Text,
				pCapabilities,
				pAppendLimit,
				pCapabilitySeen
			) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
}



/* 初始化 IMAP 客户端配置。 */
XRT_API void xrtImapClientConfigInit(ximapclientconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	xrtMailNetConfigInit(&pConfig->Net);
	pConfig->Net.Port = 143u;
	pConfig->Net.LineLimit = XIMAP_COMMAND_LINE_DEFAULT;
	pConfig->CommandLineLimit = XIMAP_COMMAND_LINE_DEFAULT;
}



/* 验证 IMAP 客户端配置。 */
XRT_API bool xrtImapClientConfigValid(const ximapclientconfig* pConfig)
{
	size_t iLimit;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	iLimit = pConfig->CommandLineLimit != 0 ?
		pConfig->CommandLineLimit : XIMAP_COMMAND_LINE_DEFAULT;
	if ( (iLimit < 16u) || (iLimit == SIZE_MAX) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"invalid IMAP command line limit"
		);
	}
	return xrtMailNetConfigValid(&pConfig->Net);
}



/* 建立 IMAP 会话并完成 greeting、CAPABILITY 与可选 STARTTLS。 */
XRT_API ximapclient* __xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return NULL; }

	ximapclient* pClient;
	ximapevent Event;
	ximapstatus Status = XIMAP_STATUS_NONE;

	if ( !xrtImapClientConfigValid(pConfig) ) {
		return NULL;
	}
	pClient = (ximapclient*)xrtCalloc(1u, sizeof(*pClient));
	if ( pClient == NULL ) {
		return NULL;
	}
	pClient->State = XIMAP_CLIENT_NOT_AUTHENTICATED;
	pClient->AppendLimit = XIMAP_APPEND_LIMIT_UNKNOWN;
	pClient->CommandLineLimit = pConfig->CommandLineLimit != 0 ?
		pConfig->CommandLineLimit : XIMAP_COMMAND_LINE_DEFAULT;
	if ( !__xrtMailTransportOpen(
		&pClient->Transport,
		&pConfig->Net,
		iDeadline,
		pCancel
	) || !__xrtImapClientReceive(
		pClient,
		&Event,
		iDeadline,
		pCancel
	) ) {
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	if ( (Event.Kind != XIMAP_EVENT_RESPONSE) || Event.HasLiteral ||
		(Event.Response.Kind != XIMAP_RESPONSE_UNTAGGED) ||
		((Event.Response.Status != XIMAP_STATUS_OK) &&
		 (Event.Response.Status != XIMAP_STATUS_PREAUTH)) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP server greeting"
		);
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	if ( !__xrtImapClientResponseSave(pClient, &Event.Response) ) {
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	pClient->State = Event.Response.Status == XIMAP_STATUS_PREAUTH ?
		XIMAP_CLIENT_AUTHENTICATED : XIMAP_CLIENT_NOT_AUTHENTICATED;
	if ( !__xrtImapClientRefresh(pClient, iDeadline, pCancel) ) {
		xrtImapClientDestroy(pClient);
		return NULL;
	}
	if ( pConfig->Net.Security == XMAIL_SECURITY_STARTTLS ) {
		if ( (pClient->State != XIMAP_CLIENT_NOT_AUTHENTICATED) ||
			((pClient->Capabilities & XIMAP_CAP_STARTTLS) == 0) ) {
			(void)__xrtImapClientError(
				XERR_UNSUPPORTED,
				"IMAP server does not permit STARTTLS in this session"
			);
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		if ( !__xrtImapClientBegin(
			pClient,
			XRT_STR_LITERAL("STARTTLS"),
			XRT_STR_LITERAL(""),
			iDeadline,
			pCancel
		) || !__xrtImapClientFinishSimple(
			pClient,
			false,
			NULL,
			NULL,
			NULL,
			NULL,
			&Status,
			iDeadline,
			pCancel
		) ) {
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		if ( Status != XIMAP_STATUS_OK ) {
			(void)__xrtImapClientError(
				XERR_PERMISSION,
				"IMAP STARTTLS was rejected"
			);
			xrtImapClientDestroy(pClient);
			return NULL;
		}
		#if defined(XMAIL_FEATURE_MAIL_NET_TLS)
			if ( !__xrtMailTransportStartTls(
				&pClient->Transport,
				&pConfig->Net,
				iDeadline,
				pCancel
			) ) {
				xrtImapClientDestroy(pClient);
				return NULL;
			}
		#else
			xrtImapClientDestroy(pClient);
			return NULL;
		#endif
		pClient->Capabilities = 0;
		if ( !__xrtImapClientRefresh(pClient, iDeadline, pCancel) ) {
			xrtImapClientDestroy(pClient);
			return NULL;
		}
	}
	return pClient;
}



/* 返回 IMAP 客户端状态。 */
XRT_API ximapclientstate xrtImapClientState(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->State : XIMAP_CLIENT_FAILED;
}



/* 返回能力快照。 */
XRT_API uint64 xrtImapClientCapabilities(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->Capabilities : 0;
}



/* 返回实际传输安全级别。 */
XRT_API xmailsecurity xrtImapClientSecurity(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->Transport.Security :
		XMAIL_SECURITY_PLAIN;
}



#if defined(XIMAP_FEATURE_IMAP_COMPRESS)
/* 查询 IMAP 客户端内部传输的压缩状态。 */
bool __xrtImapClientCompressed(const ximapclient* pClient)
{
	return (pClient != NULL) &&
		__xrtMailTransportDeflated(&pClient->Transport);
}



/* 在 COMPRESS completion 后安装 raw DEFLATE 传输层。 */
bool __xrtImapClientCompressStart(
	ximapclient* pClient,
	const xdeflateconfig* pDeflate,
	const xinflateconfig* pInflate
)
{
	if ( !__xrtImapClientUsable(pClient) || pClient->Active ||
		pClient->Append || pClient->Idle ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client is not ready to enable compression"
		);
	}
	if ( !__xrtMailTransportDeflateStart(
		&pClient->Transport,
		pDeflate,
		pInflate
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	return true;
}
#endif



/* 返回当前自动 tag。 */
XRT_API xstrview xrtImapClientTag(const ximapclient* pClient)
{
	return (pClient != NULL) && pClient->Active ?
		__xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize) :
		__xrtMailView(NULL, 0);
}



/* 返回当前 literal 剩余字节数。 */
XRT_API size_t xrtImapClientLiteralRemaining(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->LiteralRemaining : 0;
}



/* 返回命令线路预算。 */
XRT_API size_t xrtImapClientCommandLimit(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->CommandLineLimit : 0;
}



/* 返回当前全局 APPEND 上传上限。 */
XRT_API uint64 xrtImapClientAppendLimit(const ximapclient* pClient)
{
	return pClient != NULL ? pClient->AppendLimit :
		XIMAP_APPEND_LIMIT_UNKNOWN;
}



/* 解析最近保存的稳定响应。 */
XRT_API bool xrtImapClientLastResponse(
	const ximapclient* pClient,
	ximapresponseview* pResponse
)
{
	xstrview Line;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pResponse, sizeof(*pResponse)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	Line = __xrtMailView(pClient->Last.Data, pClient->Last.Size);
	return (Line.Data != NULL) && xrtImapResponseParse(Line, pResponse);
}



/* 发送显式 tag 的低层命令。 */
XRT_API bool __xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientWritable(pClient) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	return __xrtImapClientSendCommand(
		pClient,
		Tag,
		Command,
		Arguments,
		iDeadline,
		pCancel
	);
}



/* 发送显式 tag 的零拼接多参数命令。 */
XRT_API bool __xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientWritable(pClient) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	return __xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		pArguments,
		iCount,
		iDeadline,
		pCancel
	);
}



/* 发送原始命令续传字节。 */
XRT_API bool __xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapClientWritable(pClient) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return false;
	}
	if ( !xrtMemRangeValid(pData, iSize) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pClient->LiteralRemaining != 0) || pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP server literal response must be consumed first"
		);
	}
	if ( pClient->Append && (iSize > pClient->AppendRemaining) ) {
		return __xrtImapClientError(
			XERR_RANGE,
			"IMAP APPEND write exceeds the remaining literal size"
		);
	}
	if ( !__xrtMailTransportWrite(
		&pClient->Transport,
		pData,
		iSize,
		!pClient->Append,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( pClient->Append ) {
		pClient->AppendRemaining -= iSize;
	}
	return true;
}



/* 发送不带 tag 的单行 continuation 数据。 */
XRT_API bool __xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtMailViewValid(Data) ||
		(Data.Size > (pClient != NULL ?
		 pClient->CommandLineLimit - 2u : 0)) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		__xrtMailSetInvalidArgument();
		return false;
	}
	for ( size_t i = 0; i < Data.Size; i++ ) {
		unsigned char iByte = (unsigned char)Data.Data[i];

		if ( (iByte == 0) || (iByte == (unsigned char)'\r') ||
			(iByte == (unsigned char)'\n') ) {
			return __xrtImapClientError(
				XERR_ARGUMENT,
				"IMAP continuation contains a line separator"
			);
		}
	}
	return __xrtImapClientWrite(
		pClient,
		Data.Data,
		Data.Size,
		iDeadline,
		pCancel
	) && __xrtImapClientWrite(
		pClient,
		"\r\n",
		2u,
		iDeadline,
		pCancel
	);
}



/* 读取并分类下一条 IMAP 响应线路。 */
XRT_API bool __xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Line;
	xmailnext Literal;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pEvent, sizeof(*pEvent)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->LiteralRemaining != 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP literal bytes remain unread"
		);
	}
	if ( !__xrtMailTransportLine(
		&pClient->Transport,
		&Line,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	memset(pEvent, 0, sizeof(*pEvent));
	pEvent->Source = Line;
	if ( pClient->ExpectFragment ) {
		pEvent->Kind = XIMAP_EVENT_FRAGMENT;
		pClient->ExpectFragment = false;
	} else {
		pEvent->Kind = XIMAP_EVENT_RESPONSE;
		if ( !xrtImapResponseParse(Line, &pEvent->Response) ) {
			return __xrtImapClientFailed(pClient);
		}
	}
	Literal = xrtImapLiteralParse(Line, &pEvent->Literal);
	if ( Literal == XMAIL_NEXT_ERROR ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( Literal == XMAIL_NEXT_ITEM ) {
		pEvent->HasLiteral = true;
		pClient->LiteralRemaining = pEvent->Literal.Size;
		if ( pClient->LiteralRemaining == 0 ) {
			pClient->ExpectFragment = true;
		}
	}
	if ( (pEvent->Kind == XIMAP_EVENT_RESPONSE) &&
		(pEvent->Response.Kind == XIMAP_RESPONSE_TAGGED) &&
		!__xrtImapClientResponseSave(pClient, &pEvent->Response) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( (pEvent->Kind == XIMAP_EVENT_RESPONSE) &&
		(pEvent->Response.Kind == XIMAP_RESPONSE_UNTAGGED) &&
		(pEvent->Response.Status == XIMAP_STATUS_BYE) ) {
		pClient->Closing = true;
		if ( !__xrtImapClientResponseSave(pClient, &pEvent->Response) ) {
			return __xrtImapClientFailed(pClient);
		}
		if ( !pClient->LogoutSent ) {
			pClient->State = XIMAP_CLIENT_FAILED;
			__xrtMailTransportAbortPreserveError(&pClient->Transport);
		}
	}
	return true;
}



/* 按调用方容量流式读取 literal。 */
XRT_API bool __xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	size_t iRequest;
	size_t iRead;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	if ( !xrtMemRangeValid(pBuffer, iCapacity) || (iCapacity == 0) ||
		!xrtMemRangeValid(pRead, sizeof(*pRead)) ||
		xrtMemRangesOverlap(pRead, sizeof(*pRead), pBuffer, iCapacity) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->LiteralRemaining == 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP literal is not active"
		);
	}
	iRequest = pClient->LiteralRemaining < iCapacity ?
		pClient->LiteralRemaining : iCapacity;
	if ( !__xrtMailTransportRead(
		&pClient->Transport,
		pBuffer,
		iRequest,
		&iRead,
		iDeadline,
		pCancel
	) ) {
		return __xrtImapClientFailed(pClient);
	}
	if ( (iRead == 0) || (iRead > iRequest) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP literal transport progress"
		);
		return __xrtImapClientFailed(pClient);
	}
	pClient->LiteralRemaining -= iRead;
	if ( pClient->LiteralRemaining == 0 ) {
		pClient->ExpectFragment = true;
	}
	*pRead = iRead;
	return true;
}



/* 开始一个由客户端管理 tag 的顺序命令。 */
XRT_API bool __xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Tag;

	if ( !__xrtImapClientWritable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	Tag = __xrtImapClientTagNext(pClient);
	if ( !__xrtImapClientSendCommand(
		pClient,
		Tag,
		Command,
		Arguments,
		iDeadline,
		pCancel
	) ) {
		pClient->ActiveTagSize = 0;
		return false;
	}
	pClient->Active = true;
	return true;
}



/* 开始由客户端管理 tag 的零拼接多参数命令。 */
XRT_API bool __xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Tag;

	if ( !__xrtImapClientWritable(pClient) ) {
		return false;
	}
	if ( pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command or literal response is active"
		);
	}
	Tag = __xrtImapClientTagNext(pClient);
	if ( !__xrtImapClientSendCommandParts(
		pClient,
		Tag,
		Command,
		pArguments,
		iCount,
		iDeadline,
		pCancel
	) ) {
		pClient->ActiveTagSize = 0;
		return false;
	}
	pClient->Active = true;
	return true;
}



/* 读取顺序命令事件并识别匹配 tag 的 completion。 */
XRT_API xmailnext __xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return XMAIL_NEXT_ERROR; }

	xstrview Active;

	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Append ) {
		(void)__xrtImapClientError(
			XERR_STATE,
			"IMAP sequential command is not active"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( !xrtMemRangeValid(pEvent, sizeof(*pEvent)) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( !__xrtImapClientReceive(
		pClient,
		pEvent,
		iDeadline,
		pCancel
	) ) {
		return XMAIL_NEXT_ERROR;
	}
	if ( pClient->State == XIMAP_CLIENT_FAILED ) {
		(void)__xrtImapClientError(XERR_CLOSED, "IMAP server sent BYE");
		return XMAIL_NEXT_ERROR;
	}
	if ( (pEvent->Kind != XIMAP_EVENT_RESPONSE) ||
		(pEvent->Response.Kind != XIMAP_RESPONSE_TAGGED) ) {
		return XMAIL_NEXT_ITEM;
	}
	Active = __xrtMailView(pClient->ActiveTag, pClient->ActiveTagSize);
	if ( !__xrtImapClientTagEqual(pEvent->Response.Tag, Active) ) {
		return XMAIL_NEXT_ITEM;
	}
	pClient->Active = false;
	pClient->ActiveTagSize = 0;
	pClient->Idle = false;
	pClient->IdleDone = false;
	return XMAIL_NEXT_END;
}



/* 激活受长度约束的 APPEND literal 写入。 */
bool __xrtImapClientAppendStart(ximapclient* pClient, size_t iSize)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Append || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is not ready to start"
		);
	}
	pClient->Append = true;
	pClient->AppendRemaining = iSize;
	return true;
}



/* 返回当前 APPEND literal 尚未写入的字节数。 */
size_t __xrtImapClientAppendRemaining(const ximapclient* pClient)
{
	return (pClient != NULL) && pClient->Append ?
		pClient->AppendRemaining : 0;
}



/* 在 literal 完整写入后关闭 APPEND 写阶段。 */
bool __xrtImapClientAppendFinish(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		!pClient->Append ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is not active"
		);
	}
	if ( pClient->AppendRemaining != 0 ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP APPEND literal is incomplete"
		);
	}
	pClient->Append = false;
	return true;
}



/* 在 tagged exchange 完成后提交认证或邮箱选择状态。 */
bool __xrtImapClientStateCommit(
	ximapclient* pClient,
	ximapclientstate State
)
{
	if ( !__xrtImapClientUsable(pClient) ||
		((State != XIMAP_CLIENT_AUTHENTICATED) &&
		 (State != XIMAP_CLIENT_SELECTED)) ||
		pClient->Active || (pClient->LiteralRemaining != 0) ||
		pClient->ExpectFragment ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP client state is not ready to commit"
		);
	}
	pClient->State = State;
	return true;
}



/* 让扩展命令层以统一错误和终态结束不可恢复的协议失败。 */
bool __xrtImapClientProtocolFail(
	ximapclient* pClient,
	cstr sMessage
)
{
	(void)__xrtImapClientError(XERR_PROTOCOL, sMessage);
	return __xrtImapClientFailed(pClient);
}



/* 把刚开始的顺序命令标记为 IDLE。 */
bool __xrtImapClientIdleStart(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		pClient->Idle ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP IDLE command is not ready to start"
		);
	}
	pClient->Idle = true;
	pClient->IdleDone = false;
	return true;
}



/* 验证活动 IDLE 并原子记录 DONE 已发送。 */
bool __xrtImapClientIdleEnd(ximapclient* pClient)
{
	if ( !__xrtImapClientUsable(pClient) || !pClient->Active ||
		!pClient->Idle || pClient->IdleDone ) {
		return __xrtImapClientError(
			XERR_STATE,
			"IMAP IDLE command is not waiting for DONE"
		);
	}
	pClient->IdleDone = true;
	return true;
}



/* 重新获取并原子替换能力快照。 */
XRT_API bool __xrtImapClientRefresh(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	uint64 iCapabilities = 0;
	uint64 iAppendLimit = XIMAP_APPEND_LIMIT_UNKNOWN;
	bool bSeen = false;
	ximapstatus Status = XIMAP_STATUS_NONE;

	if ( !__xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("CAPABILITY"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtImapClientFinishSimple(
		pClient,
		true,
		&iCapabilities,
		&iAppendLimit,
		&bSeen,
		NULL,
		&Status,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (Status != XIMAP_STATUS_OK) || !bSeen ||
		((iCapabilities &
		 (XIMAP_CAP_IMAP4REV1 | XIMAP_CAP_IMAP4REV2)) == 0) ) {
		return __xrtImapClientError(
			XERR_PROTOCOL,
			"IMAP CAPABILITY response is missing or rejected"
		);
	}
	pClient->Capabilities = iCapabilities;
	pClient->AppendLimit = iAppendLimit;
	return true;
}



/* 完成 LOGOUT 并关闭传输。 */
XRT_API bool __xrtImapClientLogout(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	bool bBye = false;
	ximapstatus Status = XIMAP_STATUS_NONE;
	bool bSuccess;

	if ( !__xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("LOGOUT"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) || !__xrtImapClientFinishSimple(
		pClient,
		false,
		NULL,
		NULL,
		NULL,
		&bBye,
		&Status,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !bBye || (Status != XIMAP_STATUS_OK) ) {
		(void)__xrtImapClientError(
			XERR_PROTOCOL,
			"invalid IMAP LOGOUT response"
		);
		return __xrtImapClientFailed(pClient);
	}
	bSuccess = __xrtMailTransportClose(&pClient->Transport, iDeadline);
	pClient->State = bSuccess ? XIMAP_CLIENT_CLOSED : XIMAP_CLIENT_FAILED;
	return bSuccess;
}



/* 不发送协议命令，直接正常关闭传输。 */
XRT_API bool __xrtImapClientClose(
	ximapclient* pClient,
	double iDeadline
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	bool bSuccess;

	if ( !__xrtImapClientUsable(pClient) ) {
		return false;
	}
	bSuccess = __xrtMailTransportClose(&pClient->Transport, iDeadline);
	pClient->State = bSuccess ? XIMAP_CLIENT_CLOSED : XIMAP_CLIENT_FAILED;
	return bSuccess;
}



/* 无等待异常中止 IMAP 连接，保留失败终态和响应诊断。 */
XRT_API bool xrtImapClientAbort(ximapclient* pClient)
{
	bool bFailed;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( pClient->State == XIMAP_CLIENT_CLOSED ) {
		return true;
	}
	bFailed = pClient->State == XIMAP_CLIENT_FAILED;
	if ( !__xrtMailTransportAbort(&pClient->Transport) ) {
		pClient->State = XIMAP_CLIENT_FAILED;
		return false;
	}
	if ( !bFailed ) {
		pClient->State = XIMAP_CLIENT_CLOSED;
	}
	return true;
}



/* 释放 IMAP 客户端及所有内部缓冲。 */
XRT_API void xrtImapClientDestroy(ximapclient* pClient)
{
	if ( pClient == NULL ) {
		return;
	}
	__xrtMailTextDestroy(&pClient->Last);
	__xrtMailTransportDestroy(&pClient->Transport);
	memset(pClient, 0, sizeof(*pClient));
	xrtFree(pClient);
}

#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API ximapclient* xrtImapClientOpen(
	const ximapclientconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientOpen(pConfig, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientSend(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	xstrview Arguments,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientSend(pClient, Tag, Command, Arguments, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientSendParts(
	ximapclient* pClient,
	xstrview Tag,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientSendParts(pClient, Tag, Command, pArguments, iCount, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientWrite(pClient, pData, iSize, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientContinue(
	ximapclient* pClient,
	xstrview Data,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientContinue(pClient, Data, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientReceive(
	ximapclient* pClient,
	ximapevent* pEvent,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientReceive(pClient, pEvent, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientReadLiteral(
	ximapclient* pClient,
	void* pBuffer,
	size_t iCapacity,
	size_t* pRead,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientReadLiteral(pClient, pBuffer, iCapacity, pRead, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientBegin(
	ximapclient* pClient,
	xstrview Command,
	xstrview Arguments,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBegin(pClient, Command, Arguments, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientBeginParts(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pArguments,
	size_t iCount,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginParts(pClient, Command, pArguments, iCount, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API xmailnext xrtImapClientNext(
	ximapclient* pClient,
	ximapevent* pEvent,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientNext(pClient, pEvent, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientRefresh(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientRefresh(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientLogout(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientLogout(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_CLIENT))
XRT_API bool xrtImapClientClose(
	ximapclient* pClient,
	int64 iTimeout
)
{
    return __xrtImapClientClose(pClient, __xrtWaitAfter(iTimeout));
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_auth.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_AUTH)




#if defined(XIMAP_FEATURE_IMAP_AUTH)

typedef enum __ximapauthnext {
	__XIMAP_AUTH_ERROR = 0,
	__XIMAP_AUTH_CONTINUE,
	__XIMAP_AUTH_COMPLETE
} __ximapauthnext;



/* 设置稳定的 IMAP 认证错误。 */
static bool __xrtImapAuthError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_AUTH, sMessage);
	return false;
}



/* 读取到下一个 continuation 或 tagged completion。 */
static __ximapauthnext __xrtImapAuthNext(
	ximapclient* pClient,
	ximapstatus* pStatus,
	double iDeadline,
	xcancel* pCancel
)
{
	ximapevent Event;
	xmailnext Next;

	for ( ;; ) {
		Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return __XIMAP_AUTH_ERROR;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return __XIMAP_AUTH_ERROR;
			}
			*pStatus = Final.Status;
			return __XIMAP_AUTH_COMPLETE;
		}
		if ( Event.HasLiteral ) {
			(void)__xrtImapAuthError(
				XERR_PROTOCOL,
				"unexpected IMAP literal during authentication"
			);
			return __XIMAP_AUTH_ERROR;
		}
		if ( (Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION) ) {
			return __XIMAP_AUTH_CONTINUE;
		}
	}
}



/* 把最终认证状态转换为会话状态或稳定错误。 */
static bool __xrtImapAuthResult(
	ximapclient* pClient,
	ximapstatus Status
)
{
	if ( Status == XIMAP_STATUS_OK ) {
		return __xrtImapClientStateCommit(
			pClient,
			XIMAP_CLIENT_AUTHENTICATED
		);
	}
	return __xrtImapAuthError(
		Status == XIMAP_STATUS_NO ? XERR_PERMISSION : XERR_PROTOCOL,
		"IMAP authentication was rejected"
	);
}



/* 创建 `quoted-user SP quoted-secret` LOGIN 参数并保留精确长度。 */
static char* __xrtImapAuthLoginArguments(
	const ximapauthconfig* pConfig,
	size_t* pSize
)
{
	size_t iUsername;
	size_t iSecret;
	size_t iRequired;
	size_t iWritten;
	char* sArguments;

	if ( !xrtImapQuoteWrite(
		pConfig->Username,
		NULL,
		0,
		&iUsername
	) || !xrtImapQuoteWrite(
		pConfig->Secret,
		NULL,
		0,
		&iSecret
	) || !__xrtMailSizeAdd(iUsername, iSecret, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		(iRequired >= SIZE_MAX) ) {
		return NULL;
	}
	sArguments = (char*)xrtMalloc(iRequired + 1u);
	if ( sArguments == NULL ) {
		return NULL;
	}
	if ( !xrtImapQuoteWrite(
		pConfig->Username,
		sArguments,
		iRequired + 1u,
		&iWritten
	) || (iWritten != iUsername) ) {
		__xrtMailAuthFree(sArguments, iRequired + 1u);
		return NULL;
	}
	sArguments[iUsername] = ' ';
	if ( !xrtImapQuoteWrite(
		pConfig->Secret,
		sArguments + iUsername + 1u,
		iSecret + 1u,
		&iWritten
	) || (iWritten != iSecret) ) {
		__xrtMailAuthFree(sArguments, iRequired + 1u);
		return NULL;
	}
	*pSize = iRequired;
	return sArguments;
}



/* 执行传统 LOGIN 命令。 */
static bool __xrtImapAuthLogin(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	char* sArguments;
	size_t iArguments;
	ximapstatus Status = XIMAP_STATUS_NONE;
	__ximapauthnext Next;
	bool bStarted;

	sArguments = __xrtImapAuthLoginArguments(pConfig, &iArguments);
	if ( sArguments == NULL ) {
		return false;
	}
	bStarted = __xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("LOGIN"),
		(xstrview) { sArguments, iArguments },
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sArguments, iArguments + 1u);
	if ( !bStarted ) {
		return false;
	}
	Next = __xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
	if ( Next == __XIMAP_AUTH_CONTINUE ) {
		if ( !__xrtImapClientContinue(
			pClient,
			XRT_STR_LITERAL("*"),
			iDeadline,
			pCancel
		) ) {
			return false;
		}
		(void)__xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
		return __xrtImapAuthError(
			XERR_PROTOCOL,
			"unexpected continuation for IMAP LOGIN"
		);
	}
	return (Next == __XIMAP_AUTH_COMPLETE) &&
		__xrtImapAuthResult(pClient, Status);
}



/* 创建带可选 SASL-IR 的 AUTHENTICATE 参数。 */
static char* __xrtImapAuthArguments(
	xstrview Mechanism,
	const char* sEncoded,
	size_t iEncoded,
	size_t* pSize
)
{
	char* sArguments;
	size_t iRequired;

	if ( !__xrtMailSizeAdd(Mechanism.Size, iEncoded, &iRequired) ||
		!__xrtMailSizeAdd(iRequired, 1u, &iRequired) ||
		(iRequired >= SIZE_MAX) ) {
		return NULL;
	}
	sArguments = (char*)xrtMalloc(iRequired + 1u);
	if ( sArguments == NULL ) {
		return NULL;
	}
	memcpy(sArguments, Mechanism.Data, Mechanism.Size);
	sArguments[Mechanism.Size] = ' ';
	memcpy(sArguments + Mechanism.Size + 1u, sEncoded, iEncoded);
	sArguments[iRequired] = 0;
	*pSize = iRequired;
	return sArguments;
}



/* 执行 PLAIN、XOAUTH2 或 OAUTHBEARER challenge/response 交换。 */
static bool __xrtImapAuthSasl(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
	xstrview Mechanism = pConfig->Method == XIMAP_AUTH_PLAIN ?
		XRT_STR_LITERAL("PLAIN") :
		(pConfig->Method == XIMAP_AUTH_XOAUTH2 ?
		 XRT_STR_LITERAL("XOAUTH2") : XRT_STR_LITERAL("OAUTHBEARER"));
	xstrview AuthorizationId = pConfig->AuthorizationId.Size != 0 ?
		pConfig->AuthorizationId : pConfig->Username;
	char* sEncoded;
	char* sArguments = NULL;
	size_t iEncoded;
	size_t iArguments = 0;
	ximapstatus Status = XIMAP_STATUS_NONE;
	__ximapauthnext Next;
	bool bInitial;
	bool bSent;
	bool bFinalized = false;
	bool bStarted;

	if ( pConfig->Method == XIMAP_AUTH_PLAIN ) {
		sEncoded = __xrtMailAuthPlain(
			pConfig->AuthorizationId,
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		);
	} else if ( pConfig->Method == XIMAP_AUTH_XOAUTH2 ) {
		sEncoded = __xrtMailAuthXoauth2(
			pConfig->Username,
			pConfig->Secret,
			&iEncoded
		);
	} else {
		sEncoded = __xrtMailAuthOauthBearer(
			AuthorizationId,
			pConfig->Secret,
			&iEncoded
		);
	}
	if ( sEncoded == NULL ) {
		return false;
	}
	bInitial = pConfig->InitialResponse &&
		((xrtImapClientCapabilities(pClient) & XIMAP_CAP_SASL_IR) != 0);
	if ( bInitial ) {
		sArguments = __xrtImapAuthArguments(
			Mechanism,
			sEncoded,
			iEncoded,
			&iArguments
		);
		if ( sArguments == NULL ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
	}
	bStarted = __xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("AUTHENTICATE"),
		bInitial ? (xstrview) { sArguments, iArguments } : Mechanism,
		iDeadline,
		pCancel
	);
	__xrtMailAuthFree(sArguments, iArguments + (sArguments != NULL ? 1u : 0u));
	if ( !bStarted ) {
		__xrtMailAuthFree(sEncoded, iEncoded + 1u);
		return false;
	}
	bSent = bInitial;
	for ( ;; ) {
		Next = __xrtImapAuthNext(pClient, &Status, iDeadline, pCancel);
		if ( Next == __XIMAP_AUTH_ERROR ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
		if ( Next == __XIMAP_AUTH_COMPLETE ) {
			break;
		}
		if ( !bSent ) {
			bStarted = __xrtImapClientContinue(
				pClient,
				(xstrview) { sEncoded, iEncoded },
				iDeadline,
				pCancel
			);
			bSent = true;
		} else if ( (pConfig->Method == XIMAP_AUTH_XOAUTH2) &&
			!bFinalized ) {
			bStarted = __xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL(""),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		} else if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
			!bFinalized ) {
			bStarted = __xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL("AQ=="),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		} else {
			bStarted = __xrtImapClientContinue(
				pClient,
				XRT_STR_LITERAL("*"),
				iDeadline,
				pCancel
			);
			bFinalized = true;
		}
		if ( !bStarted ) {
			__xrtMailAuthFree(sEncoded, iEncoded + 1u);
			return false;
		}
	}
	__xrtMailAuthFree(sEncoded, iEncoded + 1u);
	return __xrtImapAuthResult(pClient, Status);
}



/* 初始化 IMAP 认证配置。 */
XRT_API void xrtImapAuthConfigInit(ximapauthconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Method = XIMAP_AUTH_PLAIN;
	pConfig->InitialResponse = true;
}



/* 验证 IMAP 认证配置。 */
XRT_API bool xrtImapAuthConfigValid(const ximapauthconfig* pConfig)
{
	bool bBearer;
	size_t iQuoted;

	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (pConfig->Method < XIMAP_AUTH_LOGIN) ||
		(pConfig->Method > XIMAP_AUTH_OAUTHBEARER) ) {
		return __xrtImapAuthError(
			XERR_ARGUMENT,
			"invalid IMAP authentication method"
		);
	}
	bBearer = (pConfig->Method == XIMAP_AUTH_XOAUTH2) ||
		(pConfig->Method == XIMAP_AUTH_OAUTHBEARER);
	if ( !__xrtMailAuthFieldValid(pConfig->Username, bBearer) ||
		!__xrtMailAuthFieldValid(pConfig->Secret, bBearer) ||
		(pConfig->Username.Size == 0) || (pConfig->Secret.Size == 0) ||
		!__xrtMailAuthFieldValid(pConfig->AuthorizationId, bBearer) ||
		((pConfig->Method != XIMAP_AUTH_PLAIN) &&
		 (pConfig->Method != XIMAP_AUTH_OAUTHBEARER) &&
		 (pConfig->AuthorizationId.Size != 0)) ) {
		return __xrtImapAuthError(
			XERR_ARGUMENT,
			"invalid IMAP authentication credentials"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_LOGIN) &&
		(!xrtImapQuoteWrite(
			pConfig->Username,
			NULL,
			0,
			&iQuoted
		) || !xrtImapQuoteWrite(
			pConfig->Secret,
			NULL,
			0,
			&iQuoted
		)) ) {
		return false;
	}
	return true;
}



/* 执行选定的 IMAP 认证机制。 */
XRT_API bool __xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	uint64 iCapability;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !xrtImapAuthConfigValid(pConfig) ) {
		return false;
	}
	if ( xrtImapClientState(pClient) != XIMAP_CLIENT_NOT_AUTHENTICATED ) {
		return __xrtImapAuthError(
			XERR_STATE,
			"IMAP authentication requires NOT_AUTHENTICATED state"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
		(xrtImapClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) ) {
		return __xrtImapAuthError(
			XERR_PERMISSION,
			"IMAP OAUTHBEARER requires TLS"
		);
	}
	if ( (xrtImapClientSecurity(pClient) == XMAIL_SECURITY_PLAIN) &&
		!pConfig->AllowPlaintext ) {
		return __xrtImapAuthError(
			XERR_PERMISSION,
			"IMAP credentials require TLS or explicit plaintext opt-in"
		);
	}
	iCapability = xrtImapClientCapabilities(pClient);
	if ( pConfig->Method == XIMAP_AUTH_LOGIN ) {
		if ( (iCapability & XIMAP_CAP_LOGIN_DISABLED) != 0 ) {
			return __xrtImapAuthError(
				XERR_UNSUPPORTED,
				"IMAP server disabled the LOGIN command"
			);
		}
		return __xrtImapAuthLogin(pClient, pConfig, iDeadline, pCancel);
	}
	if ( (pConfig->Method == XIMAP_AUTH_PLAIN) &&
		((iCapability & XIMAP_CAP_AUTH_PLAIN) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=PLAIN"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_XOAUTH2) &&
		((iCapability & XIMAP_CAP_AUTH_XOAUTH2) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=XOAUTH2"
		);
	}
	if ( (pConfig->Method == XIMAP_AUTH_OAUTHBEARER) &&
		((iCapability & XIMAP_CAP_AUTH_OAUTHBEARER) == 0) ) {
		return __xrtImapAuthError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise AUTH=OAUTHBEARER"
		);
	}
	return __xrtImapAuthSasl(pClient, pConfig, iDeadline, pCancel);
}

#endif

#if (defined(XIMAP_FEATURE_IMAP_AUTH))
XRT_API bool xrtImapClientAuth(
	ximapclient* pClient,
	const ximapauthconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientAuth(pClient, pConfig, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_command.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_COMMAND)




#if defined(XIMAP_FEATURE_IMAP_COMMAND)

/* 设置命令层稳定错误。 */
static bool __xrtImapCommandError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 验证必须存在的单行命令参数。 */
static bool __xrtImapCommandRequired(xstrview Text, cstr sMessage)
{
	if ( !__xrtMailViewValid(Text) || (Text.Size == 0) ) {
		return __xrtImapCommandError(XERR_ARGUMENT, sMessage);
	}
	return true;
}



/* 验证序号或 UID 集合并设置明确错误。 */
static bool __xrtImapCommandSet(xstrview Set)
{
	if ( !xrtImapSequenceSetValid(Set) ) {
		return __xrtImapCommandError(
			XERR_ARGUMENT,
			"invalid IMAP sequence set"
		);
	}
	return true;
}



/* 验证命令可在认证态或选中态执行。 */
static bool __xrtImapCommandAuthenticated(ximapclient* pClient)
{
	ximapclientstate State;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	State = xrtImapClientState(pClient);
	if ( (State != XIMAP_CLIENT_AUTHENTICATED) &&
		(State != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapCommandError(
			XERR_STATE,
			"IMAP command requires an authenticated session"
		);
	}
	return true;
}



/* 验证命令只能在选中邮箱上执行。 */
static bool __xrtImapCommandSelected(ximapclient* pClient)
{
	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( xrtImapClientState(pClient) != XIMAP_CLIENT_SELECTED ) {
		return __xrtImapCommandError(
			XERR_STATE,
			"IMAP command requires a selected mailbox"
		);
	}
	return true;
}



/* 以客户端零拼接路径开始多参数命令。 */
static bool __xrtImapCommandBegin(
	ximapclient* pClient,
	xstrview Command,
	const xstrview* pParts,
	size_t iCount,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapClientBeginParts(
		pClient,
		Command,
		pParts,
		iCount,
		iDeadline,
		pCancel
	);
}



/* 把邮箱名转义为 quoted string。 */
static str __xrtImapCommandMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	size_t* pSize
)
{
	size_t iRequired;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return NULL;
	}
	if ( !xrtImapQuoteWrite(Mailbox, NULL, 0, &iRequired) ) {
		return NULL;
	}
	if ( iRequired > xrtImapClientCommandLimit(pClient) ) {
		(void)__xrtImapCommandError(
			XERR_RANGE,
			"IMAP mailbox name exceeds the command line limit"
		);
		return NULL;
	}
	return xrtImapQuote(Mailbox, pSize);
}



/* 开始带一个 quoted mailbox 参数的命令。 */
static bool __xrtImapCommandBeginMailbox(
	ximapclient* pClient,
	xstrview Command,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
	xstrview Part;
	str sMailbox;
	bool bSuccess;

	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Part.Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	Part.Data = sMailbox;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		Command,
		&Part,
		1u,
		iDeadline,
		pCancel
	);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 把一个纯数字响应码参数转换为 uint64。 */
static bool __xrtImapCommandNumber(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint64 iDigit;

		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pValue = iValue;
	return true;
}



/* 合并 SELECT/EXAMINE 的方括号响应码。 */
static xmailnext __xrtImapCommandMailboxCode(
	xstrview Text,
	ximapmailboxinfo* pInfo
)
{
	ximapcodeview Code;
	xmailnext Next = xrtImapCodeParse(Text, &Code);
	uint64 iValue;

	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( __xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("READ-ONLY")) ||
		__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("READ-WRITE")) ) {
		if ( Code.Arguments.Size != 0 ) {
			(void)__xrtImapCommandError(
				XERR_PROTOCOL,
				"invalid IMAP mailbox access response code"
			);
			return XMAIL_NEXT_ERROR;
		}
		pInfo->ReadOnly = __xrtMailAsciiEqualI(
			Code.Name,
			XRT_STR_LITERAL("READ-ONLY")
		);
		pInfo->Present |= XIMAP_MAILBOX_ACCESS;
		return XMAIL_NEXT_ITEM;
	}
	if ( !__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UNSEEN")) &&
		!__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UIDVALIDITY")) &&
		!__xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UIDNEXT")) &&
		!__xrtMailAsciiEqualI(
			Code.Name,
			XRT_STR_LITERAL("HIGHESTMODSEQ")
		) ) {
		return XMAIL_NEXT_END;
	}
	if ( !__xrtImapCommandNumber(Code.Arguments, &iValue) ) {
		(void)__xrtImapCommandError(
			XERR_PROTOCOL,
			"invalid numeric IMAP mailbox response code"
		);
		return XMAIL_NEXT_ERROR;
	}
	if ( __xrtMailAsciiEqualI(Code.Name, XRT_STR_LITERAL("UNSEEN")) ) {
		pInfo->Unseen = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UNSEEN;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("UIDVALIDITY")
	) ) {
		pInfo->UidValidity = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UID_VALIDITY;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("UIDNEXT")
	) ) {
		pInfo->UidNext = iValue;
		pInfo->Present |= XIMAP_MAILBOX_UID_NEXT;
	} else if ( __xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("HIGHESTMODSEQ")
	) ) {
		pInfo->HighestModSeq = iValue;
		pInfo->Present |= XIMAP_MAILBOX_HIGHEST_MODSEQ;
	} else {
		return XMAIL_NEXT_END;
	}
	return XMAIL_NEXT_ITEM;
}



/* 消费一个不允许 literal 的命令并返回最终状态。 */
static bool __xrtImapCommandFinish(
	ximapclient* pClient,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
)
{
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			if ( (pInfo != NULL) &&
				(xrtImapMailboxInfoUpdate(&Final, pInfo) ==
				 XMAIL_NEXT_ERROR) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"invalid IMAP mailbox response code"
				);
			}
			if ( Final.Status == XIMAP_STATUS_OK ) {
				return true;
			}
			return __xrtImapCommandError(
				Final.Status == XIMAP_STATUS_NO ?
					XERR_PERMISSION : XERR_PROTOCOL,
				"IMAP command was rejected"
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected literal in an IMAP control command"
			);
		}
		if ( (pInfo != NULL) &&
			(Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(xrtImapMailboxInfoUpdate(&Event.Response, pInfo) ==
			 XMAIL_NEXT_ERROR) ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"invalid IMAP mailbox status response"
			);
		}
	}
}



/* 执行无参数并且没有 literal 结果的命令。 */
static bool __xrtImapCommandSimple(
	ximapclient* pClient,
	xstrview Command,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapClientBegin(
		pClient,
		Command,
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) && __xrtImapCommandFinish(
		pClient,
		NULL,
		iDeadline,
		pCancel
	);
}



/* 执行带一个 quoted mailbox 的简单命令。 */
static bool __xrtImapCommandMailboxSimple(
	ximapclient* pClient,
	xstrview Command,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
	return __xrtImapCommandAuthenticated(pClient) &&
		__xrtImapCommandBeginMailbox(
			pClient,
			Command,
			Mailbox,
			iDeadline,
			pCancel
		) && __xrtImapCommandFinish(
			pClient,
			NULL,
			iDeadline,
			pCancel
		);
}



/* 初始化空邮箱摘要。 */
XRT_API void xrtImapMailboxInfoInit(ximapmailboxinfo* pInfo)
{
	if ( !xrtMemRangeValid(pInfo, sizeof(*pInfo)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pInfo, 0, sizeof(*pInfo));
}



/* 合并 SELECT、EXAMINE 或未请求邮箱状态响应。 */
XRT_API xmailnext xrtImapMailboxInfoUpdate(
	const ximapresponseview* pResponse,
	ximapmailboxinfo* pInfo
)
{
	ximapnumberview Number;
	xmailnext Next;

	if ( !xrtMemRangeValid(pResponse, sizeof(*pResponse)) ||
		!xrtMemRangeValid(pInfo, sizeof(*pInfo)) ||
		xrtMemRangesOverlap(
			pResponse,
			sizeof(*pResponse),
			pInfo,
			sizeof(*pInfo)
		) ) {
		__xrtMailSetInvalidArgument();
		return XMAIL_NEXT_ERROR;
	}
	if ( (pResponse->Kind == XIMAP_RESPONSE_TAGGED) ||
		((pResponse->Kind == XIMAP_RESPONSE_UNTAGGED) &&
		 (pResponse->Status == XIMAP_STATUS_OK)) ) {
		return __xrtImapCommandMailboxCode(pResponse->Text, pInfo);
	}
	if ( (pResponse->Kind != XIMAP_RESPONSE_UNTAGGED) ||
		(pResponse->Status != XIMAP_STATUS_NONE) ) {
		return XMAIL_NEXT_END;
	}
	Next = xrtImapNumberParse(pResponse->Text, &Number);
	if ( Next != XMAIL_NEXT_ITEM ) {
		return Next;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("EXISTS")) ) {
		pInfo->Exists = Number.Number;
		pInfo->Present |= XIMAP_MAILBOX_EXISTS;
		return XMAIL_NEXT_ITEM;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("RECENT")) ) {
		pInfo->Recent = Number.Number;
		pInfo->Present |= XIMAP_MAILBOX_RECENT;
		return XMAIL_NEXT_ITEM;
	}
	if ( __xrtMailAsciiEqualI(Number.Name, XRT_STR_LITERAL("EXPUNGE")) ) {
		if ( ((pInfo->Present & XIMAP_MAILBOX_EXISTS) != 0) &&
			(pInfo->Exists != 0) ) {
			pInfo->Exists--;
		}
		return XMAIL_NEXT_ITEM;
	}
	return XMAIL_NEXT_END;
}



/* 执行 NOOP。 */
XRT_API bool __xrtImapClientNoop(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("NOOP"),
		iDeadline,
		pCancel
	);
}



/* 执行 SELECT 或 EXAMINE 并提交选中态。 */
static bool __xrtImapCommandSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	bool bReadOnly,
	double iDeadline,
	xcancel* pCancel
)
{
	ximapmailboxinfo Local;
	bool bWasSelected;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	bWasSelected = xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED;
	if ( pInfo == NULL ) {
		pInfo = &Local;
	} else if ( !xrtMemRangeValid(pInfo, sizeof(*pInfo)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	xrtImapMailboxInfoInit(pInfo);
	if ( !__xrtImapCommandBeginMailbox(
		pClient,
		bReadOnly ? XRT_STR_LITERAL("EXAMINE") :
			XRT_STR_LITERAL("SELECT"),
		Mailbox,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( !__xrtImapCommandFinish(
		pClient,
		pInfo,
		iDeadline,
		pCancel
	) ) {
		if ( bWasSelected &&
			(xrtImapClientState(pClient) == XIMAP_CLIENT_SELECTED) ) {
			(void)__xrtImapClientStateCommit(
				pClient,
				XIMAP_CLIENT_AUTHENTICATED
			);
		}
		return false;
	}
	if ( bReadOnly ) {
		pInfo->ReadOnly = true;
		pInfo->Present |= XIMAP_MAILBOX_ACCESS;
	}
	return __xrtImapClientStateCommit(pClient, XIMAP_CLIENT_SELECTED);
}



/* 选择可写邮箱。 */
XRT_API bool __xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandSelect(
		pClient,
		Mailbox,
		pInfo,
		false,
		iDeadline,
		pCancel
	);
}



/* 以只读方式选择邮箱。 */
XRT_API bool __xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandSelect(
		pClient,
		Mailbox,
		pInfo,
		true,
		iDeadline,
		pCancel
	);
}



/* 对当前选中邮箱执行 CHECK。 */
XRT_API bool __xrtImapClientCheck(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandSelected(pClient) && __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("CHECK"),
		iDeadline,
		pCancel
	);
}



/* 执行 UNSELECT 并返回认证态。 */
XRT_API bool __xrtImapClientUnselect(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	uint64 iCapabilities;

	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	iCapabilities = xrtImapClientCapabilities(pClient);
	if ( (iCapabilities &
		(XIMAP_CAP_UNSELECT | XIMAP_CAP_IMAP4REV2)) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise UNSELECT"
		);
	}
	return __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("UNSELECT"),
		iDeadline,
		pCancel
	) && __xrtImapClientStateCommit(
		pClient,
		XIMAP_CLIENT_AUTHENTICATED
	);
}



/* 执行 CLOSE 并返回认证态。 */
XRT_API bool __xrtImapClientCloseMailbox(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandSelected(pClient) && __xrtImapCommandSimple(
		pClient,
		XRT_STR_LITERAL("CLOSE"),
		iDeadline,
		pCancel
	) && __xrtImapClientStateCommit(
		pClient,
		XIMAP_CLIENT_AUTHENTICATED
	);
}



/* 创建邮箱。 */
XRT_API bool __xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("CREATE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 删除邮箱。 */
XRT_API bool __xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("DELETE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 重命名邮箱。 */
XRT_API bool __xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	str sSource;
	str sTarget;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	sSource = __xrtImapCommandMailbox(pClient, Source, &Parts[0].Size);
	if ( sSource == NULL ) {
		return false;
	}
	sTarget = __xrtImapCommandMailbox(pClient, Target, &Parts[1].Size);
	if ( sTarget == NULL ) {
		xrtFree(sSource);
		return false;
	}
	Parts[0].Data = sSource;
	Parts[1].Data = sTarget;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("RENAME"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sTarget);
	xrtFree(sSource);
	return bSuccess && __xrtImapCommandFinish(
		pClient,
		NULL,
		iDeadline,
		pCancel
	);
}



/* 订阅邮箱。 */
XRT_API bool __xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("SUBSCRIBE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 取消订阅邮箱。 */
XRT_API bool __xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandMailboxSimple(
		pClient,
		XRT_STR_LITERAL("UNSUBSCRIBE"),
		Mailbox,
		iDeadline,
		pCancel
	);
}



/* 开始 LIST 并保留流式响应。 */
XRT_API bool __xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	str sReference;
	str sPattern;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	sReference = __xrtImapCommandMailbox(
		pClient,
		Reference,
		&Parts[0].Size
	);
	if ( sReference == NULL ) {
		return false;
	}
	sPattern = __xrtImapCommandMailbox(pClient, Pattern, &Parts[1].Size);
	if ( sPattern == NULL ) {
		xrtFree(sReference);
		return false;
	}
	Parts[0].Data = sReference;
	Parts[1].Data = sPattern;
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("LIST"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sPattern);
	xrtFree(sReference);
	return bSuccess;
}



/* 开始 STATUS；空 Items 使用常见状态集合。 */
XRT_API bool __xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	str sMailbox;
	xstrview Parts[2];
	bool bSuccess;

	if ( !__xrtImapCommandAuthenticated(pClient) ) {
		return false;
	}
	if ( !__xrtMailViewValid(Items) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Parts[0].Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	Parts[0].Data = sMailbox;
	Parts[1] = Items.Size != 0 ? Items : XRT_STR_LITERAL(
		"(MESSAGES UNSEEN UIDNEXT UIDVALIDITY)"
	);
	bSuccess = __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("STATUS"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 开始 SEARCH 或 UID SEARCH。 */
XRT_API bool __xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Parts[2];

	if ( !__xrtImapCommandSelected(pClient) ||
		!__xrtImapCommandRequired(
			Criteria,
			"IMAP SEARCH criteria are missing"
		) ) {
		return false;
	}
	if ( !bUid ) {
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("SEARCH"),
			&Criteria,
			1u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("SEARCH");
	Parts[1] = Criteria;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
}



/* 开始 FETCH 或 UID FETCH。 */
XRT_API bool __xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Parts[3];

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ||
		!__xrtImapCommandRequired(Items, "IMAP FETCH items are missing") ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1] = Items;
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("FETCH"),
			Parts,
			2u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("FETCH");
	Parts[1] = Set;
	Parts[2] = Items;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		3u,
		iDeadline,
		pCancel
	);
}



/* 返回 STORE 模式的协议 atom。 */
static xstrview __xrtImapCommandStoreMode(ximapstoremode Mode)
{
	switch ( Mode ) {
		case XIMAP_STORE_SET:
			return XRT_STR_LITERAL("FLAGS");
		case XIMAP_STORE_SET_SILENT:
			return XRT_STR_LITERAL("FLAGS.SILENT");
		case XIMAP_STORE_ADD:
			return XRT_STR_LITERAL("+FLAGS");
		case XIMAP_STORE_ADD_SILENT:
			return XRT_STR_LITERAL("+FLAGS.SILENT");
		case XIMAP_STORE_REMOVE:
			return XRT_STR_LITERAL("-FLAGS");
		case XIMAP_STORE_REMOVE_SILENT:
			return XRT_STR_LITERAL("-FLAGS.SILENT");
		default:
			return __xrtMailView(NULL, 0);
	}
}



/* 开始 STORE 或 UID STORE。 */
XRT_API bool __xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Operation = __xrtImapCommandStoreMode(Mode);
	xstrview Parts[4];

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ||
		!__xrtImapCommandRequired(
			Operation,
			"invalid IMAP STORE mode"
		) || !__xrtImapCommandRequired(
			Flags,
			"IMAP STORE flags are missing"
		) ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1] = Operation;
		Parts[2] = Flags;
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("STORE"),
			Parts,
			3u,
			iDeadline,
			pCancel
		);
	}
	Parts[0] = XRT_STR_LITERAL("STORE");
	Parts[1] = Set;
	Parts[2] = Operation;
	Parts[3] = Flags;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		4u,
		iDeadline,
		pCancel
	);
}



/* 开始 COPY 或 MOVE 的共享实现。 */
static bool __xrtImapCommandBeginCopyMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	bool bMove,
	double iDeadline,
	xcancel* pCancel
)
{
	str sMailbox;
	xstrview Parts[3];
	bool bSuccess;

	if ( !__xrtImapCommandSelected(pClient) || !__xrtImapCommandSet(Set) ) {
		return false;
	}
	if ( bMove && ((xrtImapClientCapabilities(pClient) &
		XIMAP_CAP_MOVE) == 0) ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise MOVE"
		);
	}
	sMailbox = __xrtImapCommandMailbox(pClient, Mailbox, &Parts[2].Size);
	if ( sMailbox == NULL ) {
		return false;
	}
	if ( !bUid ) {
		Parts[0] = Set;
		Parts[1].Data = sMailbox;
		Parts[1].Size = Parts[2].Size;
		bSuccess = __xrtImapCommandBegin(
			pClient,
			bMove ? XRT_STR_LITERAL("MOVE") : XRT_STR_LITERAL("COPY"),
			Parts,
			2u,
			iDeadline,
			pCancel
		);
	} else {
		Parts[0] = bMove ? XRT_STR_LITERAL("MOVE") :
			XRT_STR_LITERAL("COPY");
		Parts[1] = Set;
		Parts[2].Data = sMailbox;
		bSuccess = __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("UID"),
			Parts,
			3u,
			iDeadline,
			pCancel
		);
	}
	xrtFree(sMailbox);
	return bSuccess;
}



/* 开始 COPY。 */
XRT_API bool __xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandBeginCopyMove(
		pClient,
		Set,
		Mailbox,
		bUid,
		false,
		iDeadline,
		pCancel
	);
}



/* 开始 MOVE。 */
XRT_API bool __xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	return __xrtImapCommandBeginCopyMove(
		pClient,
		Set,
		Mailbox,
		bUid,
		true,
		iDeadline,
		pCancel
	);
}



/* 开始 EXPUNGE 或 UID EXPUNGE。 */
XRT_API bool __xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xstrview Parts[2];

	if ( !__xrtImapCommandSelected(pClient) ) {
		return false;
	}
	if ( !__xrtMailViewValid(UidSet) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( UidSet.Size == 0 ) {
		return __xrtImapCommandBegin(
			pClient,
			XRT_STR_LITERAL("EXPUNGE"),
			NULL,
			0,
			iDeadline,
			pCancel
		);
	}
	if ( !xrtImapSequenceSetValid(UidSet) ) {
		return __xrtImapCommandError(
			XERR_ARGUMENT,
			"invalid IMAP UID set"
		);
	}
	if ( (xrtImapClientCapabilities(pClient) & XIMAP_CAP_UIDPLUS) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise UIDPLUS"
		);
	}
	Parts[0] = XRT_STR_LITERAL("EXPUNGE");
	Parts[1] = UidSet;
	return __xrtImapCommandBegin(
		pClient,
		XRT_STR_LITERAL("UID"),
		Parts,
		2u,
		iDeadline,
		pCancel
	);
}



/* 开始 IDLE，continuation 和未请求事件由调用方读取。 */
XRT_API bool __xrtImapClientBeginIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapCommandSelected(pClient) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return false;
	}
	if ( (xrtImapClientCapabilities(pClient) & XIMAP_CAP_IDLE) == 0 ) {
		return __xrtImapCommandError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise IDLE"
		);
	}
	return __xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("IDLE"),
		XRT_STR_LITERAL(""),
		iDeadline,
		pCancel
	) && __xrtImapClientIdleStart(pClient);
}



/* 向活动 IDLE 命令发送 DONE。 */
XRT_API bool __xrtImapClientEndIdle(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !__xrtImapCommandSelected(pClient) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return false;
	}
	return __xrtImapClientIdleEnd(pClient) && __xrtImapClientContinue(
		pClient,
		XRT_STR_LITERAL("DONE"),
		iDeadline,
		pCancel
	);
}

#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientNoop(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientNoop(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientSelect(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientSelect(pClient, Mailbox, pInfo, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientExamine(
	ximapclient* pClient,
	xstrview Mailbox,
	ximapmailboxinfo* pInfo,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientExamine(pClient, Mailbox, pInfo, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientCheck(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientCheck(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientUnselect(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientUnselect(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientCloseMailbox(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientCloseMailbox(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientCreateMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientCreateMailbox(pClient, Mailbox, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientDeleteMailbox(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientDeleteMailbox(pClient, Mailbox, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientRenameMailbox(
	ximapclient* pClient,
	xstrview Source,
	xstrview Target,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientRenameMailbox(pClient, Source, Target, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientSubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientSubscribe(pClient, Mailbox, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientUnsubscribe(
	ximapclient* pClient,
	xstrview Mailbox,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientUnsubscribe(pClient, Mailbox, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginList(
	ximapclient* pClient,
	xstrview Reference,
	xstrview Pattern,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginList(pClient, Reference, Pattern, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginStatus(
	ximapclient* pClient,
	xstrview Mailbox,
	xstrview Items,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginStatus(pClient, Mailbox, Items, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginSearch(
	ximapclient* pClient,
	xstrview Criteria,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginSearch(pClient, Criteria, bUid, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginFetch(
	ximapclient* pClient,
	xstrview Set,
	xstrview Items,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginFetch(pClient, Set, Items, bUid, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginStore(
	ximapclient* pClient,
	xstrview Set,
	ximapstoremode Mode,
	xstrview Flags,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginStore(pClient, Set, Mode, Flags, bUid, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginCopy(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginCopy(pClient, Set, Mailbox, bUid, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginMove(
	ximapclient* pClient,
	xstrview Set,
	xstrview Mailbox,
	bool bUid,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginMove(pClient, Set, Mailbox, bUid, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginExpunge(
	ximapclient* pClient,
	xstrview UidSet,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginExpunge(pClient, UidSet, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientBeginIdle(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBeginIdle(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_COMMAND))
XRT_API bool xrtImapClientEndIdle(
	ximapclient* pClient,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientEndIdle(pClient, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_message.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_MESSAGE)



#if defined(XIMAP_FEATURE_IMAP_MESSAGE)

#define __XRT_IMAP_MESSAGE_CHUNK (16u * 1024u)



/* BODY 请求同时保留发送属性和服务器响应属性。 */
typedef struct __ximapmessageitems {
	xstrview Command;
	xstrview Response;
	str Storage;
} __ximapmessageitems;



/* owned 字节路径借用本次调用栈上的连续缓冲。 */
typedef struct __ximapmessagebuffer {
	xbuffer Buffer;
} __ximapmessagebuffer;



/* 设置 IMAP 消息便利层的稳定错误。 */
static bool __xrtImapMessageError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 保留首个错误并关闭无法安全继续解析的活动命令。 */
static bool __xrtImapMessageRecover(
	ximapclient* pClient
)
{
	xerror* pPrimaryError = xrtTakeError();
	xerror* pCloseError;
	ximapclientstate State = xrtImapClientState(pClient);

	if ( State != XIMAP_CLIENT_CLOSED ) {
		(void)xrtImapClientAbort(pClient);
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



/* 验证 section-spec 可以安全嵌入 BODY 方括号。 */
static bool __xrtImapMessageSectionValid(xstrview Section)
{
	if ( !__xrtMailViewValid(Section) ) {
		return false;
	}
	for ( size_t i = 0; i < Section.Size; i++ ) {
		unsigned char iByte = (unsigned char)Section.Data[i];

		if ( (iByte < (unsigned char)' ') || (iByte > (unsigned char)'~') ||
			(iByte == (unsigned char)'[') || (iByte == (unsigned char)']') ) {
			return false;
		}
	}
	return true;
}



/* 构建 BODY[section] 与可选 BODY.PEEK[section] 属性。 */
static bool __xrtImapMessageItemsCreate(
	ximapclient* pClient,
	xstrview Section,
	bool bPeek,
	__ximapmessageitems* pItems
)
{
	static const char sBody[] = "BODY";
	static const char sPeek[] = ".PEEK";
	static const char sBodyResponse[] = "BODY[]";
	static const char sBodyPeek[] = "BODY.PEEK[]";
	size_t iCommand;
	size_t iResponse;
	size_t iTotal;
	char* sCommand;
	char* sResponse;
	size_t iPosition = 0;

	memset(pItems, 0, sizeof(*pItems));
	if ( (pClient == NULL) || !__xrtImapMessageSectionValid(Section) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( Section.Size == 0 ) {
		pItems->Command = bPeek ?
			__xrtMailView(sBodyPeek, sizeof(sBodyPeek) - 1u) :
			__xrtMailView(sBodyResponse, sizeof(sBodyResponse) - 1u);
		pItems->Response = __xrtMailView(
			sBodyResponse,
			sizeof(sBodyResponse) - 1u
		);
		return true;
	}
	if ( Section.Size > xrtImapClientCommandLimit(pClient) ) {
		return __xrtImapMessageError(
			XERR_RANGE,
			"IMAP BODY section exceeds the command line limit"
		);
	}
	iResponse = sizeof(sBody) - 1u + 2u;
	if ( !__xrtMailSizeAdd(iResponse, Section.Size, &iResponse) ) {
		return false;
	}
	iCommand = iResponse;
	if ( bPeek && !__xrtMailSizeAdd(
		iCommand,
		sizeof(sPeek) - 1u,
		&iCommand
	) ) {
		return false;
	}
	if ( !__xrtMailSizeAdd(iCommand, iResponse, &iTotal) ) {
		return false;
	}
	pItems->Storage = (str)xrtMalloc(iTotal);
	if ( pItems->Storage == NULL ) {
		return false;
	}
	sCommand = pItems->Storage;
	sResponse = sCommand + iCommand;
	memcpy(sCommand + iPosition, sBody, sizeof(sBody) - 1u);
	iPosition += sizeof(sBody) - 1u;
	if ( bPeek ) {
		memcpy(sCommand + iPosition, sPeek, sizeof(sPeek) - 1u);
		iPosition += sizeof(sPeek) - 1u;
	}
	sCommand[iPosition++] = '[';
	if ( Section.Size != 0 ) {
		memcpy(sCommand + iPosition, Section.Data, Section.Size);
		iPosition += Section.Size;
	}
	sCommand[iPosition] = ']';
	memcpy(sResponse, sBody, sizeof(sBody) - 1u);
	sResponse[sizeof(sBody) - 1u] = '[';
	if ( Section.Size != 0 ) {
		memcpy(
			sResponse + sizeof(sBody),
			Section.Data,
			Section.Size
		);
	}
	sResponse[iResponse - 1u] = ']';
	pItems->Command = (xstrview){ sCommand, iCommand };
	pItems->Response = (xstrview){ sResponse, iResponse };
	return true;
}



/* 释放 BODY 属性临时文本。 */
static void __xrtImapMessageItemsFree(__ximapmessageitems* pItems)
{
	xrtFree(pItems->Storage);
	memset(pItems, 0, sizeof(*pItems));
}



/* 验证 literal 属于本次请求的 FETCH BODY 属性。 */
static bool __xrtImapMessageLiteralValid(
	const ximapevent* pEvent,
	xstrview Expected
)
{
	ximapfetchview Fetch;
	ximapfetchcursor Cursor;
	ximapfetchitem Item;
	xmailnext Next;

	if ( (pEvent->Kind != XIMAP_EVENT_RESPONSE) ||
		(pEvent->Response.Kind != XIMAP_RESPONSE_UNTAGGED) ||
		(pEvent->Response.Status != XIMAP_STATUS_NONE) ||
		pEvent->Literal.Binary ||
		!xrtImapFetchParse(pEvent->Response.Text, &Fetch) ||
		!xrtImapFetchCursorInit(&Cursor, &Fetch) ) {
		return __xrtImapMessageError(
			XERR_PROTOCOL,
			"invalid IMAP BODY literal response"
		);
	}
	for ( ;; ) {
		Next = xrtImapFetchNext(&Cursor, &Item);
		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			return __xrtImapMessageError(
				XERR_PROTOCOL,
				"IMAP FETCH response did not identify the BODY literal"
			);
		}
		if ( Item.Value.Kind == XIMAP_DATA_LITERAL ) {
			if ( !__xrtMailAsciiEqualI(Item.Attribute, Expected) ||
				(Item.Value.LiteralSize != pEvent->Literal.Size) ) {
				return __xrtImapMessageError(
					XERR_PROTOCOL,
					"IMAP FETCH returned an unexpected literal"
				);
			}
			return true;
		}
	}
}



/* 调用输出 sink，并在无具体原因时补充 callback 错误。 */
static bool __xrtImapMessageOutput(
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
			"IMAP message output callback failed"
		);
	}
	return false;
}



/* 读取并直接交付当前 literal。 */
static bool __xrtImapMessageLiteralWrite(
	ximapclient* pClient,
	size_t iLiteralSize,
	xmailwriteproc pWrite,
	ptr pUserData,
	double iDeadline,
	xcancel* pCancel
)
{
	unsigned char Data[__XRT_IMAP_MESSAGE_CHUNK];
	size_t iReadTotal = 0;

	while ( iReadTotal < iLiteralSize ) {
		size_t iRead;

		if ( !__xrtImapClientReadLiteral(
			pClient,
			Data,
			sizeof(Data),
			&iRead,
			iDeadline,
			pCancel
		) || !__xrtImapMessageOutput(
			pWrite,
			pUserData,
			(xbytesview){ Data, iRead }
		) ) {
			return false;
		}
		iReadTotal += iRead;
	}
	return true;
}



/* 把完成状态映射为稳定错误，并区分空结果。 */
static bool __xrtImapMessageFinish(
	ximapclient* pClient,
	bool bFound
)
{
	ximapresponseview Final;

	if ( !xrtImapClientLastResponse(pClient, &Final) ) {
		return false;
	}
	if ( Final.Status != XIMAP_STATUS_OK ) {
		return __xrtImapMessageError(
			Final.Status == XIMAP_STATUS_NO ?
				XERR_PERMISSION : XERR_PROTOCOL,
			"IMAP BODY command was rejected"
		);
	}
	if ( !bFound ) {
		return __xrtImapMessageError(
			XERR_NOT_FOUND,
			"IMAP BODY command returned no message"
		);
	}
	return true;
}



/* 向连续缓冲追加一个已受外层预算约束的片段。 */
static bool __xrtImapMessageBufferWrite(xbytesview Data, ptr pUserData)
{
	__ximapmessagebuffer* pBuffer = (__ximapmessagebuffer*)pUserData;

	return xrtBufferAppend(&pBuffer->Buffer, Data);
}



/* 流式读取一个 BODY section。 */
XRT_API bool __xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	__ximapmessageitems Items;
	char sMessage[10];
	xstrview Message;
	bool bFound = false;
	size_t iWritten = 0;

	if ( (iMessage == 0) || (pWrite == NULL) || !xrtMemRangeValid(
		pWritten,
		pWritten != NULL ? sizeof(*pWritten) : 0
	) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( iMaxBytes == 0 ) {
		iMaxBytes = XIMAP_MESSAGE_BYTES_DEFAULT;
	}
	if ( !__xrtImapMessageItemsCreate(pClient, Section, bPeek, &Items) ) {
		return false;
	}
	Message = (xstrview){
		sMessage,
		__xrtMailUint64Write(sMessage, iMessage)
	};
	if ( !__xrtImapClientBeginFetch(
		pClient,
		Message,
		Items.Command,
		bUid,
		iDeadline,
		pCancel
	) ) {
		__xrtImapMessageItemsFree(&Items);
		return false;
	}
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		if ( Next == XMAIL_NEXT_END ) {
			bool bSuccess = __xrtImapMessageFinish(pClient, bFound);

			__xrtImapMessageItemsFree(&Items);
			if ( bSuccess && (pWritten != NULL) ) {
				*pWritten = iWritten;
			}
			return bSuccess;
		}
		if ( !Event.HasLiteral ) {
			continue;
		}
		if ( bFound || !__xrtImapMessageLiteralValid(
			&Event,
			Items.Response
		) ) {
			if ( bFound && (xrtGetError() == NULL) ) {
				(void)__xrtImapMessageError(
					XERR_PROTOCOL,
					"IMAP BODY command returned multiple literals"
				);
			}
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		if ( !__xrtMailSizeAdd(
			iWritten,
			Event.Literal.Size,
			&iWritten
		) || ((iMaxBytes != SIZE_MAX) && (iWritten > iMaxBytes)) ) {
			if ( xrtGetError() == NULL ) {
				__xrtMailError(
					XERR_RANGE,
					XMAIL_ERROR_LIMIT,
					"IMAP BODY literal exceeds the byte limit"
				);
			}
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
		bFound = true;
		if ( (Event.Literal.Size != 0) && !__xrtImapMessageLiteralWrite(
			pClient,
			Event.Literal.Size,
			pWrite,
			pUserData,
			iDeadline,
			pCancel
		) ) {
			__xrtImapMessageItemsFree(&Items);
			return __xrtImapMessageRecover(pClient);
		}
	}
}



/* 收集一个 BODY section 并附加零字节。 */
XRT_API bytes __xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return NULL; }

	__ximapmessagebuffer Buffer;
	bytes pData;
	size_t iSize;

	if ( !xrtMemRangeValid(
		pOutputSize,
		pOutputSize != NULL ? sizeof(*pOutputSize) : 0
	) || !xrtBufferInit(&Buffer.Buffer) ) {
		if ( xrtGetError() == NULL ) {
			__xrtMailSetInvalidArgument();
		}
		return NULL;
	}
	if ( !__xrtImapClientBodyWrite(
		pClient,
		iMessage,
		Section,
		bUid,
		bPeek,
		iMaxBytes,
		__xrtImapMessageBufferWrite,
		&Buffer,
		&iSize,
		iDeadline,
		pCancel
	) || !xrtBufferAppendByte(&Buffer.Buffer, 0) ) {
		xrtBufferUnit(&Buffer.Buffer);
		return NULL;
	}
	pData = xrtBufferTake(&Buffer.Buffer, NULL, NULL);
	if ( pOutputSize != NULL ) {
		*pOutputSize = iSize;
	}
	return pData;
}



/* 收集完整 BODY[] 并解析为拥有型 MIME 树。 */
XRT_API bool __xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

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
	pData = __xrtImapClientBodyBytes(
		pClient,
		iMessage,
		XRT_STR_LITERAL(""),
		bUid,
		bPeek,
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

#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool xrtImapClientBodyWrite(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	xmailwriteproc pWrite,
	ptr pUserData,
	size_t* pWritten,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBodyWrite(pClient, iMessage, Section, bUid, bPeek, iMaxBytes, pWrite, pUserData, pWritten, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bytes xrtImapClientBodyBytes(
	ximapclient* pClient,
	uint32 iMessage,
	xstrview Section,
	bool bUid,
	bool bPeek,
	size_t iMaxBytes,
	size_t* pOutputSize,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientBodyBytes(pClient, iMessage, Section, bUid, bPeek, iMaxBytes, pOutputSize, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_MESSAGE))
XRT_API bool xrtImapClientMessageTree(
	ximapclient* pClient,
	uint32 iMessage,
	bool bUid,
	bool bPeek,
	const xmailtreelimits* pLimits,
	xmailtree* pTree,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientMessageTree(pClient, iMessage, bUid, bPeek, pLimits, pTree, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_append.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_APPEND)




#if defined(XIMAP_FEATURE_IMAP_APPEND)

/* 设置 APPEND 层稳定错误。 */
static bool __xrtImapAppendError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 解析严格的无符号十进制数。 */
static bool __xrtImapAppendNumber(xstrview Text, uint64* pValue)
{
	uint64 iValue = 0;

	if ( Text.Size == 0 ) {
		return false;
	}
	for ( size_t i = 0; i < Text.Size; i++ ) {
		uint64 iDigit;

		if ( (Text.Data[i] < '0') || (Text.Data[i] > '9') ) {
			return false;
		}
		iDigit = (uint64)(Text.Data[i] - '0');
		if ( iValue > ((UINT64_MAX - iDigit) / UINT64_C(10)) ) {
			return false;
		}
		iValue = (iValue * UINT64_C(10)) + iDigit;
	}
	*pValue = iValue;
	return true;
}



/* 把 size_t 写成不依赖格式化库的 literal 标记。 */
static xstrview __xrtImapAppendMarker(
	char* sMarker,
	size_t iSize,
	bool bNonSynchronizing
)
{
	char arrDigits[(sizeof(size_t) * 3u) + 1u];
	size_t iDigits = 0;
	size_t iOffset = 0;

	do {
		arrDigits[iDigits++] = (char)('0' + (iSize % 10u));
		iSize /= 10u;
	} while ( iSize != 0 );
	sMarker[iOffset++] = '{';
	while ( iDigits != 0 ) {
		sMarker[iOffset++] = arrDigits[--iDigits];
	}
	if ( bNonSynchronizing ) {
		sMarker[iOffset++] = '+';
	}
	sMarker[iOffset++] = '}';
	return __xrtMailView(sMarker, iOffset);
}



/* 验证 Flags 的外层结构，内部语法保留给服务器和扩展处理。 */
static bool __xrtImapAppendFlagsValid(xstrview Flags)
{
	return (Flags.Size == 0) ||
		(__xrtMailViewValid(Flags) && (Flags.Size >= 2u) &&
		 (Flags.Data[0] == '(') && (Flags.Data[Flags.Size - 1u] == ')'));
}



/* 根据能力、上限和调用方策略选择 literal 形式。 */
static bool __xrtImapAppendLiteralMode(
	const ximapclient* pClient,
	const ximapappendconfig* pConfig,
	bool* pNonSynchronizing
)
{
	uint64 iCapabilities = xrtImapClientCapabilities(pClient);
	uint64 iLimit = xrtImapClientAppendLimit(pClient);
	bool bUnlimited = (iCapabilities & XIMAP_CAP_LITERAL_PLUS) != 0;
	bool bLimited = ((iCapabilities & XIMAP_CAP_LITERAL_MINUS) != 0) ||
		((iCapabilities & XIMAP_CAP_IMAP4REV2) != 0);
	bool bSupported = bUnlimited || (bLimited && (pConfig->Size <= 4096u));

	if ( (iLimit != XIMAP_APPEND_LIMIT_UNKNOWN) &&
		((iLimit == 0) || ((uint64)pConfig->Size > iLimit)) ) {
		return __xrtImapAppendError(
			XERR_RANGE,
			"IMAP APPEND exceeds the advertised upload limit"
		);
	}
	if ( pConfig->Literal == XIMAP_LITERAL_SYNC ) {
		*pNonSynchronizing = false;
		return true;
	}
	if ( pConfig->Literal == XIMAP_LITERAL_NONSYNC ) {
		if ( !bSupported ) {
			return __xrtImapAppendError(
				XERR_UNSUPPORTED,
				"IMAP server does not support this non-synchronizing literal"
			);
		}
		*pNonSynchronizing = true;
		return true;
	}
	if ( pConfig->Literal != XIMAP_LITERAL_AUTO ) {
		return __xrtImapAppendError(
			XERR_ARGUMENT,
			"invalid IMAP APPEND literal mode"
		);
	}
	*pNonSynchronizing = bSupported &&
		(iLimit != XIMAP_APPEND_LIMIT_UNKNOWN);
	return true;
}



/* 等待同步 literal continuation，允许普通未请求响应穿过。 */
static bool __xrtImapAppendWait(
	ximapclient* pClient,
	double iDeadline,
	xcancel* pCancel
)
{
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"missing IMAP APPEND completion"
				);
			}
			if ( (Final.Status != XIMAP_STATUS_NO) &&
				(Final.Status != XIMAP_STATUS_BAD) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"IMAP APPEND completed before literal upload"
				);
			}
			return __xrtImapAppendError(
				Final.Status == XIMAP_STATUS_NO ?
					XERR_PERMISSION : XERR_PROTOCOL,
				"IMAP APPEND was rejected before literal upload"
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected server literal while waiting for IMAP APPEND"
			);
		}
		if ( (Event.Kind == XIMAP_EVENT_RESPONSE) &&
			(Event.Response.Kind == XIMAP_RESPONSE_CONTINUATION) ) {
			return true;
		}
	}
}



/* 解析 tagged OK 中的单消息 APPENDUID。 */
static bool __xrtImapAppendResult(
	xstrview Text,
	ximapappendresult* pResult
)
{
	ximapcodeview Code;
	ximapatomcursor Cursor;
	xstrview UidValidity;
	xstrview Uid;
	xstrview Extra;
	xmailnext Next;

	Next = xrtImapCodeParse(Text, &Code);
	if ( Next == XMAIL_NEXT_ERROR ) {
		return false;
	}
	if ( (Next == XMAIL_NEXT_END) || !__xrtMailAsciiEqualI(
		Code.Name,
		XRT_STR_LITERAL("APPENDUID")
	) ) {
		return true;
	}
	if ( !xrtImapAtomCursorInit(&Cursor, Code.Arguments) ||
		xrtImapAtomNext(&Cursor, &UidValidity) != XMAIL_NEXT_ITEM ||
		xrtImapAtomNext(&Cursor, &Uid) != XMAIL_NEXT_ITEM ||
		xrtImapAtomNext(&Cursor, &Extra) != XMAIL_NEXT_END ||
		!__xrtImapAppendNumber(UidValidity, &pResult->UidValidity) ||
		!__xrtImapAppendNumber(Uid, &pResult->Uid) ||
		(pResult->UidValidity == 0) || (pResult->Uid == 0) ||
		(pResult->UidValidity > UINT32_MAX) || (pResult->Uid > UINT32_MAX) ) {
		return __xrtImapAppendError(
			XERR_PROTOCOL,
			"invalid IMAP APPENDUID response code"
		);
	}
	pResult->Present = true;
	return true;
}



/* 初始化 APPEND 配置。 */
XRT_API void xrtImapAppendConfigInit(ximapappendconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pConfig, 0, sizeof(*pConfig));
	pConfig->Literal = XIMAP_LITERAL_AUTO;
}



/* 初始化 APPEND 结果。 */
XRT_API void xrtImapAppendResultInit(ximapappendresult* pResult)
{
	if ( !xrtMemRangeValid(pResult, sizeof(*pResult)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	memset(pResult, 0, sizeof(*pResult));
}



/* 发送 APPEND 命令头并进入受约束的 literal 写阶段。 */
XRT_API bool __xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	char sMarker[(sizeof(size_t) * 3u) + 4u];
	xstrview Parts[4];
	str sMailbox = NULL;
	str sDate = NULL;
	size_t iCount = 0;
	bool bNonSynchronizing = false;
	bool bSuccess = false;

	if ( (pClient == NULL) ||
		!xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!__xrtMailViewValid(pConfig->Mailbox) ||
		(pConfig->Mailbox.Size == 0) ||
		!__xrtMailViewValid(pConfig->InternalDate) ||
		!__xrtImapAppendFlagsValid(pConfig->Flags) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( (uint64)pConfig->Size > (uint64)INT64_MAX ) {
		return __xrtImapAppendError(XERR_RANGE, "IMAP APPEND size exceeds 63 bits");
	}
	if ( (xrtImapClientState(pClient) != XIMAP_CLIENT_AUTHENTICATED) &&
		(xrtImapClientState(pClient) != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapAppendError(
			XERR_STATE,
			"IMAP APPEND requires an authenticated session"
		);
	}
	if ( !__xrtImapAppendLiteralMode(
		pClient,
		pConfig,
		&bNonSynchronizing
	) ) {
		return false;
	}
	sMailbox = xrtImapQuote(pConfig->Mailbox, &Parts[iCount].Size);
	if ( sMailbox == NULL ) {
		goto cleanup;
	}
	Parts[iCount++].Data = sMailbox;
	if ( pConfig->Flags.Size != 0 ) {
		Parts[iCount++] = pConfig->Flags;
	}
	if ( pConfig->InternalDate.Size != 0 ) {
		sDate = xrtImapQuote(pConfig->InternalDate, &Parts[iCount].Size);
		if ( sDate == NULL ) {
			goto cleanup;
		}
		Parts[iCount++].Data = sDate;
	}
	Parts[iCount++] = __xrtImapAppendMarker(
		sMarker,
		pConfig->Size,
		bNonSynchronizing
	);
	if ( !__xrtImapClientBeginParts(
		pClient,
		XRT_STR_LITERAL("APPEND"),
		Parts,
		iCount,
		iDeadline,
		pCancel
	) ) {
		goto cleanup;
	}
	if ( !bNonSynchronizing && !__xrtImapAppendWait(
		pClient,
		iDeadline,
		pCancel
	) ) {
		goto cleanup;
	}
	bSuccess = __xrtImapClientAppendStart(pClient, pConfig->Size);

cleanup:
	xrtFree(sDate);
	xrtFree(sMailbox);
	return bSuccess;
}



/* 返回活动 literal 剩余字节。 */
XRT_API size_t xrtImapClientAppendRemaining(const ximapclient* pClient)
{
	return __xrtImapClientAppendRemaining(pClient);
}



/* 写入一块 APPEND literal。 */
XRT_API bool __xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( (__xrtImapClientAppendRemaining(pClient) == 0) ||
		(iSize == 0) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		return __xrtImapAppendError(
			XERR_STATE,
			"IMAP APPEND has no remaining literal bytes"
		);
	}
	return __xrtImapClientWrite(
		pClient,
		pData,
		iSize,
		iDeadline,
		pCancel
	);
}



/* 结束 APPEND 并解析 tagged completion。 */
XRT_API bool __xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	ximapappendresult Result;

	xrtImapAppendResultInit(&Result);
	if ( !xrtMemRangeValid(
		pResult,
		pResult != NULL ? sizeof(*pResult) : 0
	) || !__xrtImapClientAppendFinish(pClient) ||
		!__xrtImapClientWrite(pClient, "\r\n", 2u, iDeadline, pCancel) ) {
		return false;
	}
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			if ( (Final.Status == XIMAP_STATUS_NO) ||
				(Final.Status == XIMAP_STATUS_BAD) ) {
				return __xrtImapAppendError(
					Final.Status == XIMAP_STATUS_NO ?
						XERR_PERMISSION : XERR_PROTOCOL,
					"IMAP APPEND was rejected"
				);
			}
			if ( Final.Status != XIMAP_STATUS_OK ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"invalid IMAP APPEND completion status"
				);
			}
			if ( !__xrtImapAppendResult(Final.Text, &Result) ) {
				return __xrtImapClientProtocolFail(
					pClient,
					"invalid IMAP APPEND completion"
				);
			}
			if ( pResult != NULL ) {
				*pResult = Result;
			}
			return true;
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"unexpected server literal in IMAP APPEND response"
			);
		}
	}
}



/* 执行内存消息的一次 APPEND。 */
XRT_API bool __xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	double iDeadline,
	xcancel* pCancel
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xrtMemRangeValid(pData, pConfig != NULL ? pConfig->Size : 0) ) {
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapClientAppendBegin(
		pClient,
		pConfig,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	if ( (pConfig->Size != 0) && !__xrtImapClientAppendWrite(
		pClient,
		pData,
		pConfig->Size,
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	return __xrtImapClientAppendEnd(
		pClient,
		pResult,
		iDeadline,
		pCancel
	);
}

#endif

#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool xrtImapClientAppendBegin(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientAppendBegin(pClient, pConfig, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool xrtImapClientAppendWrite(
	ximapclient* pClient,
	const void* pData,
	size_t iSize,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientAppendWrite(pClient, pData, iSize, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool xrtImapClientAppendEnd(
	ximapclient* pClient,
	ximapappendresult* pResult,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientAppendEnd(pClient, pResult, __xrtWaitAfter(iTimeout), pCancel);
}
#endif

#if (defined(XIMAP_FEATURE_IMAP_APPEND))
XRT_API bool xrtImapClientAppend(
	ximapclient* pClient,
	const ximapappendconfig* pConfig,
	const void* pData,
	ximapappendresult* pResult,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientAppend(pClient, pConfig, pData, pResult, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif


/* ========================================================================== */
/* source: extlibs/ximap/src/imap/imap_compress.c */
/* ========================================================================== */

#if defined(XIMAP_FEATURE_IMAP_COMPRESS)




#if defined(XIMAP_FEATURE_IMAP_COMPRESS)

/* 创建稳定的 IMAP COMPRESS 错误。 */
static bool __xrtImapCompressError(xerrkind Kind, cstr sMessage)
{
	__xrtMailError(Kind, XMAIL_ERROR_PROTOCOL, sMessage);
	return false;
}



/* 从公开配置生成 raw Deflate 双向配置。 */
static bool __xrtImapCompressConfigs(
	const ximapcompressconfig* pConfig,
	xdeflateconfig* pDeflate,
	xinflateconfig* pInflate
)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ||
		!xrtMemRangeValid(pDeflate, sizeof(*pDeflate)) ||
		!xrtMemRangeValid(pInflate, sizeof(*pInflate)) ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	xrtDeflateConfigInit(pDeflate);
	pDeflate->Format = XDEFLATE_RAW;
	pDeflate->Level = pConfig->Level;
	pDeflate->Strategy = pConfig->Strategy;
	pDeflate->WindowBits = pConfig->WindowBits;
	xrtInflateConfigInit(pInflate);
	pInflate->Format = XINFLATE_RAW;
	pInflate->WindowBits = pConfig->WindowBits;
	return xrtDeflateConfigValid(pDeflate) &&
		xrtInflateConfigValid(pInflate);
}



/* 初始化 IMAP 压缩配置。 */
XRT_API void xrtImapCompressConfigInit(ximapcompressconfig* pConfig)
{
	if ( !xrtMemRangeValid(pConfig, sizeof(*pConfig)) ) {
		__xrtMailSetInvalidArgument();
		return;
	}
	pConfig->Level = XDEFLATE_LEVEL_DEFAULT;
	pConfig->Strategy = XDEFLATE_STRATEGY_DEFAULT;
	pConfig->WindowBits = XDEFLATE_WINDOW_MAX;
}



/* 验证 IMAP raw DEFLATE 双向配置。 */
XRT_API bool xrtImapCompressConfigValid(
	const ximapcompressconfig* pConfig
)
{
	xdeflateconfig Deflate;
	xinflateconfig Inflate;

	return __xrtImapCompressConfigs(pConfig, &Deflate, &Inflate);
}



/* 返回当前 IMAP 会话压缩状态。 */
XRT_API bool xrtImapClientCompressed(const ximapclient* pClient)
{
	return __xrtImapClientCompressed(pClient);
}



/* 协商并切换 IMAP COMPRESS=DEFLATE。 */
XRT_API bool __xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	double iDeadline,
	xcancel* pCancel
)
{
    if ( !__xrtWaitValid(iDeadline) ) { return false; }

	xdeflateconfig Deflate;
	xinflateconfig Inflate;
	ximapclientstate State;

	if ( pClient == NULL ) {
		__xrtMailSetInvalidArgument();
		return false;
	}
	if ( !__xrtImapCompressConfigs(pConfig, &Deflate, &Inflate) ) {
		return false;
	}
	State = xrtImapClientState(pClient);
	if ( (State != XIMAP_CLIENT_AUTHENTICATED) &&
		(State != XIMAP_CLIENT_SELECTED) ) {
		return __xrtImapCompressError(
			XERR_STATE,
			"IMAP COMPRESS requires an authenticated session"
		);
	}
	if ( xrtImapClientCompressed(pClient) ) {
		return __xrtImapCompressError(
			XERR_STATE,
			"IMAP compression is already active"
		);
	}
	if ( (xrtImapClientCapabilities(pClient) &
		XIMAP_CAP_COMPRESS_DEFLATE) == 0 ) {
		return __xrtImapCompressError(
			XERR_UNSUPPORTED,
			"IMAP server did not advertise COMPRESS=DEFLATE"
		);
	}
	if ( !__xrtImapClientBegin(
		pClient,
		XRT_STR_LITERAL("COMPRESS"),
		XRT_STR_LITERAL("DEFLATE"),
		iDeadline,
		pCancel
	) ) {
		return false;
	}
	for ( ;; ) {
		ximapevent Event;
		xmailnext Next = __xrtImapClientNext(
			pClient,
			&Event,
			iDeadline,
			pCancel
		);

		if ( Next == XMAIL_NEXT_ERROR ) {
			return false;
		}
		if ( Next == XMAIL_NEXT_END ) {
			ximapresponseview Final;

			if ( !xrtImapClientLastResponse(pClient, &Final) ) {
				return false;
			}
			if ( Final.Status != XIMAP_STATUS_OK ) {
				return __xrtImapCompressError(
					XERR_UNSUPPORTED,
					"IMAP COMPRESS was rejected"
				);
			}
			return __xrtImapClientCompressStart(
				pClient,
				&Deflate,
				&Inflate
			);
		}
		if ( Event.HasLiteral ) {
			return __xrtImapClientProtocolFail(
				pClient,
				"IMAP COMPRESS returned an unexpected literal"
			);
		}
	}
}

#endif

#if (defined(XIMAP_FEATURE_IMAP_COMPRESS))
XRT_API bool xrtImapClientCompress(
	ximapclient* pClient,
	const ximapcompressconfig* pConfig,
	int64 iTimeout,
	xcancel* pCancel
)
{
    return __xrtImapClientCompress(pClient, pConfig, __xrtWaitAfter(iTimeout), pCancel);
}
#endif
#endif

#endif
