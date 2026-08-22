#include "../internal/xrt_http_server.h"



#if defined(XRT_FEATURE_HTTP_SERVER_RESPONSE)

/* 验证输出描述符不会覆盖响应状态或已经冻结的线路数据。 */
static bool __xrtHttp1ServerResponseOutputValid(
	const xhttp1serverresponse* pResponse,
	const xbytesview* pOutput
)
{
	if ( !__xrtRangeValid(pOutput, sizeof(*pOutput)) ) {
		return false;
	}
	if ( pResponse == NULL ) {
		return true;
	}
	if ( __xrtRangesOverlap(
		pOutput, sizeof(*pOutput),
		pResponse, sizeof(*pResponse)
	) || __xrtRangesOverlap(
		pOutput, sizeof(*pOutput),
		pResponse->Head, pResponse->HeadSize
	) || __xrtRangesOverlap(
		pOutput, sizeof(*pOutput),
		pResponse->End, pResponse->EndSize
	) || __xrtRangesOverlap(
		pOutput, sizeof(*pOutput),
		pResponse->ChunkLine,
		sizeof(pResponse->ChunkLine)
	) || __xrtRangesOverlap(
		pOutput, sizeof(*pOutput),
		pResponse->Chunk.Data,
		pResponse->Chunk.Size
	) ) {
		return false;
	}
	for ( size_t i = pResponse->WireRefIndex;
		i < pResponse->WireRefCount; i++ ) {
		if ( __xrtRangesOverlap(
			pOutput,
			sizeof(*pOutput),
			pResponse->WireRefs[i].Data,
			pResponse->WireRefs[i].Size
		) ) {
			return false;
		}
	}
	return true;
}



/* 发布一个借用出站片段并记录本次允许消费的长度。 */
static xhttp1serveroutputstatus __xrtHttp1ServerResponseOffer(
	xhttp1serverresponse* pResponse,
	cbytes pData,
	size_t iSize,
	size_t iMaxBytes,
	xbytesview* pOutput
)
{
	xbytesview Output;
	size_t iOffer = iSize < iMaxBytes ?
		iSize : iMaxBytes;

	if ( iOffer == 0 ) {
		(void)__xrtHttp1ServerResponseFail(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_STATE,
			XERR_INTERNAL,
			"output-http1-server-response",
			"HTTP server response attempted to offer empty data"
		);
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	if ( __xrtRangesOverlap(
		pData, iSize, pOutput, sizeof(Output)
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	pResponse->OfferData = pData;
	pResponse->Offered = iOffer;
	Output = (xbytesview){ pData, iOffer };
	memcpy(pOutput, &Output, sizeof(Output));
	return XHTTP1_SERVER_OUTPUT_DATA;
}



/* 结束正文 Reader 并进入普通完成或隧道终态。 */
static xhttp1serveroutputstatus __xrtHttp1ServerResponseFinish(
	xhttp1serverresponse* pResponse
)
{
	xrtHttpBodyChunkRelease(&pResponse->Chunk);
	xrtHttpBodyReaderDestroy(pResponse->Reader);
	pResponse->Reader = NULL;
	pResponse->State = pResponse->Tunnel ?
		XRT_HTTP_SERVER_RESPONSE_TUNNEL :
		XRT_HTTP_SERVER_RESPONSE_DONE;
	return pResponse->Tunnel ?
		XHTTP1_SERVER_OUTPUT_TUNNEL :
		XHTTP1_SERVER_OUTPUT_DONE;
}



/* 惰性建立正文 Reader。 */
static bool __xrtHttp1ServerResponseOpen(
	xhttp1serverresponse* pResponse
)
{
	if ( pResponse->ReaderOpened ) {
		return true;
	}
	pResponse->ReaderOpened = true;
	if ( pResponse->Body == NULL ) {
		return true;
	}
	pResponse->Reader = xrtHttpBodyOpen(pResponse->Body);
	if ( pResponse->Reader == NULL ) {
		return __xrtHttp1ServerResponseFailCause(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_BODY,
			XERR_IO,
			"open-http1-server-body",
			"HTTP server response body could not be opened",
			xrtGetError()
		);
	}
	return true;
}



/* 为当前正文 Chunk 准备最短 chunk-size 行。 */
static bool __xrtHttp1ServerResponseChunkLine(
	xhttp1serverresponse* pResponse
)
{
	if ( !xrtHttp1ChunkLineWrite(
		(uint64)pResponse->Chunk.Size,
		(xstrview){ NULL, 0 },
		pResponse->ChunkLine,
		sizeof(pResponse->ChunkLine),
		&pResponse->ChunkLineSize
	) ) {
		return __xrtHttp1ServerResponseFailCause(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_FRAMING,
			XERR_PROTOCOL,
			"frame-http1-server-body",
			"HTTP server response body Chunk could not be framed",
			xrtGetError()
		);
	}
	pResponse->PartOffset = 0;
	pResponse->ChunkTerminal = false;
	pResponse->State =
		XRT_HTTP_SERVER_RESPONSE_CHUNK_LINE;
	return true;
}



/* 准备最终 last-chunk 与 Trailer 线路。 */
static void __xrtHttp1ServerResponseChunkEnd(
	xhttp1serverresponse* pResponse
)
{
	pResponse->PartOffset = 0;
	pResponse->ChunkTerminal = true;
	pResponse->State =
		XRT_HTTP_SERVER_RESPONSE_CHUNK_END;
}



/* 从正文源取得下一段拥有型数据并选择发送状态。 */
static xhttp1serveroutputstatus __xrtHttp1ServerResponseBodyNext(
	xhttp1serverresponse* pResponse,
	size_t iMaxBytes
)
{
	xhttpbodystatus Status;
	size_t iReadMax = iMaxBytes;

	if ( pResponse->WireRefs != NULL ) {
		if ( pResponse->WireRefIndex ==
			pResponse->WireRefCount ) {
			return __xrtHttp1ServerResponseFinish(pResponse);
		}
		pResponse->Chunk.Data = pResponse->WireRefs[
			pResponse->WireRefIndex
		].Data;
		pResponse->Chunk.Size = pResponse->WireRefs[
			pResponse->WireRefIndex
		].Size;
		pResponse->PartOffset = 0;
		pResponse->State = XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA;
		return XHTTP1_SERVER_OUTPUT_DATA;
	}

	if ( !__xrtHttp1ServerResponseOpen(pResponse) ) {
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	if ( pResponse->Reader == NULL ) {
		if ( pResponse->Mode == XHTTP1_BODY_CHUNKED ) {
			__xrtHttp1ServerResponseChunkEnd(pResponse);
			return XHTTP1_SERVER_OUTPUT_DATA;
		}
		if ( pResponse->BodyRemaining != 0 ) {
			(void)__xrtHttp1ServerResponseFail(
				pResponse,
				XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
				XERR_PROTOCOL,
				"read-http1-server-body",
				"HTTP server response body is missing before Content-Length"
			);
			return XHTTP1_SERVER_OUTPUT_ERROR;
		}
		return __xrtHttp1ServerResponseFinish(pResponse);
	}
	if ( (pResponse->Mode == XHTTP1_BODY_FIXED) &&
		(pResponse->BodyRemaining < (uint64)iReadMax) ) {
		iReadMax = (size_t)pResponse->BodyRemaining;
	}
	if ( iReadMax == 0 ) {
		iReadMax = 1;
	}
	Status = xrtHttpBodyNext(
		pResponse->Reader,
		iReadMax,
		&pResponse->Chunk
	);
	if ( Status == XHTTP_BODY_AGAIN ) {
		pResponse->OutputAgain = true;
		return XHTTP1_SERVER_OUTPUT_AGAIN;
	}
	pResponse->OutputAgain = false;
	if ( Status == XHTTP_BODY_ERROR ) {
		(void)__xrtHttp1ServerResponseFailCause(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_BODY,
			XERR_IO,
			"read-http1-server-body",
			"HTTP server response body source failed",
			xrtHttpBodyReaderError(pResponse->Reader)
		);
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	if ( Status == XHTTP_BODY_EOF ) {
		if ( (pResponse->Mode == XHTTP1_BODY_FIXED) &&
			(pResponse->BodyRemaining != 0) ) {
			(void)__xrtHttp1ServerResponseFail(
				pResponse,
				XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
				XERR_PROTOCOL,
				"read-http1-server-body",
				"HTTP server response body ended before Content-Length"
			);
			return XHTTP1_SERVER_OUTPUT_ERROR;
		}
		if ( pResponse->Mode == XHTTP1_BODY_CHUNKED ) {
			__xrtHttp1ServerResponseChunkEnd(pResponse);
			return XHTTP1_SERVER_OUTPUT_DATA;
		}
		return __xrtHttp1ServerResponseFinish(pResponse);
	}
	if ( (pResponse->Mode == XHTTP1_BODY_FIXED) &&
		((uint64)pResponse->Chunk.Size >
		 pResponse->BodyRemaining) ) {
		(void)__xrtHttp1ServerResponseFail(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
			XERR_PROTOCOL,
			"read-http1-server-body",
			"HTTP server response body exceeded Content-Length"
		);
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	pResponse->PartOffset = 0;
	if ( pResponse->Mode == XHTTP1_BODY_CHUNKED ) {
		if ( !__xrtHttp1ServerResponseChunkLine(
			pResponse
		) ) {
			return XHTTP1_SERVER_OUTPUT_ERROR;
		}
	} else {
		pResponse->State =
			XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA;
	}
	return XHTTP1_SERVER_OUTPUT_DATA;
}



/* 借出下一段 HTTP/1 响应线路数据。 */
XRT_API xhttp1serveroutputstatus xrtHttp1ServerResponseOutput(
	xhttp1serverresponse* pResponse,
	size_t iMaxBytes,
	xbytesview* pData
)
{
	xbytesview Empty = { NULL, 0 };
	xhttp1serveroutputstatus Status;

	if ( !__xrtHttp1ServerResponseOutputValid(
		pResponse, pData
	) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	memcpy(pData, &Empty, sizeof(Empty));
	if ( (pResponse == NULL) ||
		(iMaxBytes == 0) ) {
		__xrtErrorSetInvalidArgument();
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	if ( pResponse->Error != NULL ) {
		xrtSetError(pResponse->Error);
		return XHTTP1_SERVER_OUTPUT_ERROR;
	}
	if ( pResponse->Offered != 0 ) {
		return __xrtHttp1ServerResponseOffer(
			pResponse,
			pResponse->OfferData,
			pResponse->Offered,
			pResponse->Offered,
			pData
		);
	}
	if ( pResponse->OutputAgain ) {
		return XHTTP1_SERVER_OUTPUT_AGAIN;
	}
	for ( ;; ) {
		switch ( pResponse->State ) {
			case XRT_HTTP_SERVER_RESPONSE_HEAD:
				if ( pResponse->HeadOffset <
					pResponse->HeadSize ) {
					return __xrtHttp1ServerResponseOffer(
						pResponse,
						pResponse->Head +
							pResponse->HeadOffset,
						pResponse->HeadSize -
							pResponse->HeadOffset,
						iMaxBytes,
						pData
					);
				}
				pResponse->State =
					XRT_HTTP_SERVER_RESPONSE_BODY;
				break;

			case XRT_HTTP_SERVER_RESPONSE_BODY:
				if ( (pResponse->Mode == XHTTP1_BODY_NONE) ||
					(pResponse->Mode == XHTTP1_BODY_TUNNEL) ) {
					return __xrtHttp1ServerResponseFinish(
						pResponse
					);
				}
				Status = __xrtHttp1ServerResponseBodyNext(
					pResponse, iMaxBytes
				);
				if ( Status != XHTTP1_SERVER_OUTPUT_DATA ) {
					return Status;
				}
				break;

			case XRT_HTTP_SERVER_RESPONSE_CHUNK_LINE:
				return __xrtHttp1ServerResponseOffer(
					pResponse,
					(cbytes)pResponse->ChunkLine +
						pResponse->PartOffset,
					pResponse->ChunkLineSize -
						pResponse->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA:
				return __xrtHttp1ServerResponseOffer(
					pResponse,
					pResponse->Chunk.Data +
						pResponse->PartOffset,
					pResponse->Chunk.Size -
						pResponse->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_SERVER_RESPONSE_CHUNK_END:
				if ( pResponse->ChunkTerminal ) {
					return __xrtHttp1ServerResponseOffer(
						pResponse,
						pResponse->End +
							pResponse->PartOffset,
						pResponse->EndSize -
							pResponse->PartOffset,
						iMaxBytes,
						pData
					);
				}
				return __xrtHttp1ServerResponseOffer(
					pResponse,
					(cbytes)"\r\n" +
						pResponse->PartOffset,
					2u - pResponse->PartOffset,
					iMaxBytes,
					pData
				);

			case XRT_HTTP_SERVER_RESPONSE_DONE:
				return XHTTP1_SERVER_OUTPUT_DONE;

			case XRT_HTTP_SERVER_RESPONSE_TUNNEL:
				return XHTTP1_SERVER_OUTPUT_TUNNEL;

			default:
				(void)__xrtHttp1ServerResponseFail(
					pResponse,
					XHTTP1_SERVER_RESPONSE_ERROR_STATE,
					XERR_INTERNAL,
					"output-http1-server-response",
					"HTTP server response output state is invalid"
				);
				return XHTTP1_SERVER_OUTPUT_ERROR;
		}
	}
}



/* 推进最近一次借出的 HTTP/1 响应线路片段。 */
XRT_API bool xrtHttp1ServerResponseOutputConsume(
	xhttp1serverresponse* pResponse,
	size_t iSize
)
{
	if ( (pResponse == NULL) ||
		(pResponse->Offered == 0) ||
		(iSize > pResponse->Offered) ) {
		__xrtErrorSetInvalidArgument();
		return false;
	}
	if ( iSize == 0 ) {
		return true;
	}
	if ( pResponse->WireBytes >
		(UINT64_MAX - (uint64)iSize) ) {
		__xrtErrorSetSizeOverflow();
		return __xrtHttp1ServerResponseFailCause(
			pResponse,
			XHTTP1_SERVER_RESPONSE_ERROR_LENGTH,
			XERR_RANGE,
			"consume-http1-server-response",
			"HTTP server response wire byte count overflowed",
			xrtGetError()
		);
	}
	pResponse->WireBytes += (uint64)iSize;
	pResponse->Offered = 0;
	pResponse->OfferData = NULL;
	switch ( pResponse->State ) {
		case XRT_HTTP_SERVER_RESPONSE_HEAD:
			pResponse->HeadOffset += iSize;
			break;

		case XRT_HTTP_SERVER_RESPONSE_CHUNK_LINE:
			pResponse->PartOffset += iSize;
			if ( pResponse->PartOffset ==
				pResponse->ChunkLineSize ) {
				pResponse->PartOffset = 0;
				pResponse->State =
					XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA;
			}
			break;

		case XRT_HTTP_SERVER_RESPONSE_CHUNK_DATA:
			pResponse->PartOffset += iSize;
			if ( pResponse->Mode == XHTTP1_BODY_FIXED ) {
				pResponse->BodyRemaining -=
					(uint64)iSize;
			}
			if ( pResponse->PartOffset ==
				pResponse->Chunk.Size ) {
				if ( pResponse->WireRefs != NULL ) {
					__xrtHttp1ServerResponseReleaseCurrentRef(
						pResponse
					);
					memset(
						&pResponse->Chunk,
						0,
						sizeof(pResponse->Chunk)
					);
				} else {
					xrtHttpBodyChunkRelease(
						&pResponse->Chunk
					);
				}
				pResponse->PartOffset = 0;
				if ( pResponse->Mode ==
					XHTTP1_BODY_CHUNKED ) {
					pResponse->ChunkTerminal = false;
					pResponse->State =
						XRT_HTTP_SERVER_RESPONSE_CHUNK_END;
				} else {
					pResponse->State =
						XRT_HTTP_SERVER_RESPONSE_BODY;
				}
			}
			break;

		case XRT_HTTP_SERVER_RESPONSE_CHUNK_END:
			pResponse->PartOffset += iSize;
			if ( pResponse->PartOffset ==
				(pResponse->ChunkTerminal ?
				 pResponse->EndSize : 2u) ) {
				pResponse->PartOffset = 0;
				if ( pResponse->ChunkTerminal ) {
					(void)__xrtHttp1ServerResponseFinish(
						pResponse
					);
				} else {
					pResponse->State =
						XRT_HTTP_SERVER_RESPONSE_BODY;
				}
			}
			break;

		default:
			return __xrtHttp1ServerResponseFail(
				pResponse,
				XHTTP1_SERVER_RESPONSE_ERROR_STATE,
				XERR_INTERNAL,
				"consume-http1-server-response",
				"HTTP server response cannot be consumed in this state"
			);
	}
	return true;
}

#endif
