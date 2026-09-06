/*
 * 范例：http1/head_tour —— 起始行/字段/分帧与 trailer 区
 * ----------------------------------------------------------------
 * 演示 API：
 *   【限额与目标】xrtHttp1LimitsInit（公网默认三限额）
 *                 xrtHttp1TargetValid（空白/控制/fragment 拒绝）
 *   【字段】      xrtHttp1Field（按名取第一个字段）
 *                 xrtHttp1TransferCodingNext（TE 值严格迭代）
 *   【分帧】      xrtHttp1RequestBodyPlan（RFC 9112 优先级）
 *                 xrtHttp1ChunkLineWrite（十六进制 size 行）
 *   【消息扫描】  xrtHttp1RequestMessageParse（整条请求）
 *                 xrtHttp1MessageBodyView（免解分帧借用正文）
 *   【Trailer】   xrtHttp1BodyTrailers（FIELDS 后重绑描述符）
 *                 xrtHttp1TrailersParse（独立 trailer 区解析）
 * 模块宏：XRT_MODULE_HTTP1
 * 编译（单头形态，Windows）：
 *   gcc -O1 -DXRT_MODULE_ALL -I single -include xrt.h impl.c ${BS}
 *       examples/http1/head_tour/main.c -lws2_32 -liphlpapi
 * 预期输出：
 *   http1: limits + target valid +/- ok
 *   http1: field lookup + transfer-coding iteration ok
 *   http1: request body plan fixed/chunked ok
 *   http1: chunk line 5 -> "5\r\n" (+ext) ok
 *   http1: message parse -> borrowed body "ok" ok
 *   http1: body trailers rebind + trailer parse ok
 *
 * 消息级 Parse 返回 Plan + Trailers + 借用 Wire；
 *   非 chunked 定长正文可直接 MessageBodyView 零复制取得。
 */

#include <stdio.h>
#include <string.h>
#include <xrt.h>

int main(void)
{
	static const char sFixed[] =
		"POST /a HTTP/1.1\r\n"
		"Host: a.test\r\n"
		"Content-Length: 2\r\n"
		"\r\n"
		"ok";
	static const char sChunked[] =
		"POST /b HTTP/1.1\r\n"
		"Host: a.test\r\n"
		"Transfer-Encoding: gzip, chunked\r\n"
		"\r\n";
	xhttp1limits Limits;
	xhttp1bodylimits BodyLimits;
	xhttp1bodyplan Plan;
	xhttp1body Body;
	xhttp1message Message;
	xhttp1errorinfo Error;
	xhttp1head Head;
	xhttpfield Fields[8];
	xhttpfield Trailers[4];
	xhttp1transfercoding Coding;
	const xhttpfield* pField;
	xbytesview View;
	xhttpnext Next;
	size_t iOffset = 0;
	size_t iSize = 0;
	size_t iBytes = 0;
	size_t iCount = 0;
	uint8 arrLine[16];
	int iResult = 1;

	/* ---- 限额默认值 + request-target 合法性正反。 ---- */
	xrtHttp1LimitsInit(&Limits);
	if ( (Limits.MaxStartLine != 8192u) ||
		(Limits.MaxHead != 65536u) ||
		(Limits.MaxFields != 100u) ||
		xrtHttp1TargetValid(XRT_STR_LITERAL("/a b")) ||
		xrtHttp1TargetValid(XRT_STR_LITERAL("/a#f")) ||
		xrtHttp1TargetValid(XRT_STR_LITERAL("")) ||
		!xrtHttp1TargetValid(XRT_STR_LITERAL("/api?v=1")) ) {
		goto Cleanup;
	}
	printf("http1: limits + target valid +/- ok\n");

	/* ---- 字段查找 + Transfer-Encoding 迭代。 ---- */
	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( (xrtHttp1RequestParse(
			(xbytesview) { (const uint8*)sChunked,
				sizeof(sChunked) - 1u },
			&Head, &Limits, &Error) != XHTTP1_READY) ||
		((pField = xrtHttp1Field(&Head,
			XRT_STR_LITERAL("host"))) == NULL) ||
		(pField->Value.Size != 6u) ||
		(memcmp(pField->Value.Data, "a.test", 6u) != 0) ||
		(xrtHttp1Field(&Head, XRT_STR_LITERAL("x-nope")) !=
			NULL) ) {
		goto Cleanup;
	}
	/* TE 迭代须在 Transfer-Encoding 字段值上进行。 */
	if ( (pField = xrtHttp1Field(&Head,
			XRT_STR_LITERAL("transfer-encoding"))) == NULL ) {
		goto Cleanup;
	}
	iOffset = 0;
	if ( ((Next = xrtHttp1TransferCodingNext(
			pField->Value, &iOffset, &Coding)) != XHTTP_NEXT_ITEM) ||
		(Coding.Name.Size != 4u) ||
		(memcmp(Coding.Name.Data, "gzip", 4u) != 0) ||
		(xrtHttp1TransferCodingNext(pField->Value, &iOffset,
			&Coding) != XHTTP_NEXT_ITEM) ||
		(Coding.Name.Size != 7u) ||
		(memcmp(Coding.Name.Data, "chunked", 7u) != 0) ||
		(xrtHttp1TransferCodingNext(pField->Value, &iOffset,
			&Coding) != XHTTP_NEXT_END) ) {
		goto Cleanup;
	}
	printf("http1: field lookup + transfer-coding iteration ok\n");

	/* ---- 分帧计划：chunked 头优先于 Content-Length。 ---- */
	if ( !xrtHttp1RequestBodyPlan(&Head, &Plan) ||
		(Plan.Mode != XHTTP1_BODY_CHUNKED) ) {
		goto Cleanup;
	}
	xrtHttp1HeadInit(&Head, Fields, 8);
	if ( (xrtHttp1RequestParse(
			(xbytesview) { (const uint8*)sFixed,
				sizeof(sFixed) - 1u },
			&Head, &Limits, &Error) != XHTTP1_READY) ||
		!xrtHttp1RequestBodyPlan(&Head, &Plan) ||
		(Plan.Mode != XHTTP1_BODY_FIXED) ||
		(Plan.Length != 2u) ) {
		goto Cleanup;
	}
	printf("http1: request body plan fixed/chunked ok\n");

	/* ---- chunk-size 行：裸 size 与带扩展两种形态。 ---- */
	if ( !xrtHttp1ChunkLineWrite(5u, XRT_STR_LITERAL(""), arrLine,
			sizeof(arrLine), &iSize) ||
		(iSize != 3u) ||
		(memcmp(arrLine, "5\r\n", 3u) != 0) ||
		!xrtHttp1ChunkLineWrite(5u, XRT_STR_LITERAL(";x"),
			arrLine, sizeof(arrLine), &iSize) ||
		(iSize != 5u) ||
		(memcmp(arrLine, "5;x\r\n", 5u) != 0) ) {
		goto Cleanup;
	}
	printf("http1: chunk line 5 -> \"5\\r\\n\" (+ext) ok\n");

	/* ---- 整条消息扫描 + 借用正文视图。
	 * BodyLimits 必须先初始化——消息解析会按限额约束正文。 ---- */
	xrtHttp1BodyLimitsInit(&BodyLimits);
	/* 统一初始化 Head 与 Trailers 槽；本例不需要 trailer 存储。
	 * Head 容量不足时解析器返回 FIELDS。 */
	xrtHttp1MessageInit(&Message, Fields, 8, NULL, 0);
	if ( (xrtHttp1RequestMessageParse(
			(xbytesview) { (const uint8*)sFixed,
				sizeof(sFixed) - 1u },
			true, &Message, &Limits, &BodyLimits,
			&Error) != XHTTP1_READY) ||
		((View = xrtHttp1MessageBodyView(&Message)).Size != 2u) ||
		(memcmp(View.Data, "ok", 2u) != 0) ) {
		goto Cleanup;
	}
	printf("http1: message parse -> borrowed body \"ok\" ok\n");

	/* ---- trailer：chunked 正文走到终止块 → trailer 态重绑
	 * 描述符 → 读入 trailer 区；另演示独立 TrailersParse。 ---- */
	{
		static const char sChunkWire[] = "5\r\nhello\r\n0\r\n";
		static const char sTrailer[] = "X-Sum: 3\r\n\r\n";
		xhttp1bodystatus BodyStatus;
		xbytesview Data;

		/* 换回 chunked 请求头重建 Plan。 */
		xrtHttp1HeadInit(&Head, Fields, 8);
		if ( (xrtHttp1RequestParse(
				(xbytesview) { (const uint8*)sChunked,
					sizeof(sChunked) - 1u },
				&Head, &Limits, &Error) != XHTTP1_READY) ||
			!xrtHttp1RequestBodyPlan(&Head, &Plan) ||
			(Plan.Mode != XHTTP1_BODY_CHUNKED) ||
			!xrtHttp1BodyInit(&Body, &Plan, NULL, 0u,
				&BodyLimits) ) {
			goto Cleanup;
		}
		/* 数据块与终止块分两步喂入。 */
		BodyStatus = xrtHttp1BodyRead(&Body,
			(xbytesview) { (const uint8*)sChunkWire,
				sizeof(sChunkWire) - 1u },
			false, &iBytes, &Data, &Error);
		if ( (BodyStatus != XHTTP1_BODY_DATA) ||
			(Data.Size != 5u) ||
			(memcmp(Data.Data, "hello", 5u) != 0) ) {
			goto Cleanup;
		}
		BodyStatus = xrtHttp1BodyRead(&Body,
			(xbytesview) { (const uint8*)sChunkWire + 8u, 5u },
			false, &iBytes, &Data, &Error);
		if ( (BodyStatus != XHTTP1_BODY_MORE) ||
			(iBytes != 5u) ||
			/* 终止块后处于 trailer 态——此刻才能重绑。 */
			!xrtHttp1BodyTrailers(&Body, Trailers, 4u) ) {
			goto Cleanup;
		}
		BodyStatus = xrtHttp1BodyRead(&Body,
			(xbytesview) { (const uint8*)sTrailer,
				sizeof(sTrailer) - 1u },
			true, &iBytes, &Data, &Error);
		if ( (BodyStatus != XHTTP1_BODY_DONE) ||
			(Body.TrailerCount != 1u) ||
			!xrtHttp1BodyDone(&Body) ) {
			goto Cleanup;
		}
		/* 独立 trailer 区解析（不经 Body Reader）。 */
		iCount = 0;
		if ( (xrtHttp1TrailersParse(
				(xbytesview) { (const uint8*)sTrailer,
					sizeof(sTrailer) - 1u },
				Trailers, 4u, &BodyLimits, &iBytes, &iCount,
				&Error) != XHTTP1_READY) ||
			(iCount != 1u) ||
			(Trailers[0].Value.Size != 1u) ) {
			goto Cleanup;
		}
	}
	printf("http1: body trailers rebind + trailer parse ok\n");
	iResult = 0;

Cleanup:
	return iResult;
}
