#include "../internal/xrt_http_server.h"

#if defined(XHTTP_FEATURE_HTTP_SERVER_RESPONSE)

/*
	建立不附加任何协议字节的线缆响应计划。
	Body 由计划保留，调用方仍负责释放自己的引用。
*/
XHTTP_API xhttp1serverresponse* xrtHttpBridgeServerWireBody(
	xhttpbody* pBody,
	uint64 iLength,
	bool bClose,
	bool bTunnel
)
{
	xhttp1serverresponse* pResponse;

	if ( (pBody == NULL) || (iLength == 0) ||
		(iLength == XHTTP_BODY_UNKNOWN) ) {
		return NULL;
	}
	pResponse = (xhttp1serverresponse*)xrtCalloc(
		1, sizeof(*pResponse)
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	pResponse->Body = xrtHttpBodyRef(pBody);
	if ( pResponse->Body == NULL ) {
		xrtFree(pResponse);
		return NULL;
	}
	pResponse->BodyLength = iLength;
	pResponse->BodyRemaining = iLength;
	pResponse->Mode = XHTTP1_BODY_FIXED;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_BODY;
	pResponse->Close = bClose;
	pResponse->Tunnel = bTunnel;
	return pResponse;
}



/* 复制引用描述符并建立不附加协议字节的线缆响应计划。 */
XHTTP_API xhttp1serverresponse* xrtHttpBridgeServerWireChunks(
	const xhttpbodychunk* pRefs,
	size_t iCount,
	uint64 iLength,
	bool bClose,
	bool bTunnel
)
{
	xhttp1serverresponse* pResponse;
	size_t iAllocation;
	size_t iUsed = 0;

	if ( (pRefs == NULL) || (iCount == 0) ||
		(iLength == 0) || (iLength == XHTTP_BODY_UNKNOWN) ||
		(iCount > ((SIZE_MAX - sizeof(*pResponse)) /
		 sizeof(*pResponse->WireRefs))) ) {
		return NULL;
	}
	iAllocation = sizeof(*pResponse) +
		(iCount * sizeof(*pResponse->WireRefs));
	pResponse = (xhttp1serverresponse*)xrtCalloc(
		1, iAllocation
	);
	if ( pResponse == NULL ) {
		return NULL;
	}
	pResponse->WireRefs = (xhttpbodychunk*)(pResponse + 1);
	for ( size_t i = 0; i < iCount; i++ ) {
		xhttpbodychunk Chunk;

		memcpy(&Chunk, ((const uint8*)pRefs) + i * sizeof(Chunk), sizeof(Chunk));
		if ( Chunk.Size != 0 ) {
			pResponse->WireRefs[iUsed++] = Chunk;
		}
	}
	pResponse->WireRefCount = iUsed;
	pResponse->BodyLength = iLength;
	pResponse->BodyRemaining = iLength;
	pResponse->Mode = XHTTP1_BODY_FIXED;
	pResponse->State = XRT_HTTP_SERVER_RESPONSE_BODY;
	pResponse->Close = bClose;
	pResponse->Tunnel = bTunnel;
	return pResponse;
}



#endif
