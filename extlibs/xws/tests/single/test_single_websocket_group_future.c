#define XWS_IMPLEMENTATION
#include "../../include/xws/features.h"
#include "../../../xhttp/include/xhttp/features.h"
#ifndef XRT_IMPLEMENTATION
#define XRT_IMPLEMENTATION
#endif
#include "../../../../single/xrt.h"
#define XHTTP_IMPLEMENTATION
#include "../../../../single/extlibs/xhttp.h"
#include "../../../../single/extlibs/xws.h"



/* 验证空连接组立即完成，并公开稳定的零槽位结果。 */
int main(void)
{
	xwsgroup* pGroup = xrtWsGroupCreate(0);
	xwsgroupop* pOperation;
	xfuture* pFuture;
	uint8 TooLarge[XWS_CLOSE_PAYLOAD_MAX + 1u];

	if ( pGroup == NULL ) {
		return 1;
	}
	pOperation = xrtWsGroupTextAsync(
		pGroup,
		XRT_STR_LITERAL("ready")
	);
	if ( (pOperation == NULL) ||
		(xrtWsGroupOpCount(pOperation) != 0) ||
		(xrtWsGroupOpAccepted(pOperation) != 0) ||
		(xrtWsGroupOpRejected(pOperation) != 0) ||
		(xrtWsGroupOpWaitFor(pOperation, 0) != XWAIT_OK) ) {
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	pFuture = xrtWsGroupOpFutureRef(pOperation);
	if ( (pFuture == NULL) ||
		(xrtFutureState(pFuture) != XFUTURE_RESOLVED) ) {
		xrtFutureDestroy(pFuture);
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtFutureDestroy(pFuture);
	xrtWsGroupOpDestroy(pOperation);
	pOperation = xrtWsGroupPingAsync(
		pGroup,
		XRT_BYTES_LITERAL("probe")
	);
	if ( (pOperation == NULL) ||
		(xrtWsGroupOpWaitFor(pOperation, 0) != XWAIT_OK) ) {
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtWsGroupOpDestroy(pOperation);
	pOperation = xrtWsGroupCloseAsync(
		pGroup,
		XWS_CLOSE_NORMAL,
		XRT_STR_LITERAL("done")
	);
	if ( (pOperation == NULL) ||
		(xrtWsGroupOpWaitFor(pOperation, 0) != XWAIT_OK) ) {
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtWsGroupOpDestroy(pOperation);
	pOperation = xrtWsGroupWaitAsync(
		pGroup,
		XWS_CONN_WAIT_DRAIN
	);
	if ( (pOperation == NULL) ||
		(xrtWsGroupOpWaitFor(pOperation, 0) != XWAIT_OK) ) {
		xrtWsGroupOpDestroy(pOperation);
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtWsGroupOpDestroy(pOperation);
	memset(TooLarge, 0, sizeof(TooLarge));
	if ( xrtWsGroupPingAsync(
		pGroup,
		(xbytesview) {
			TooLarge,
			sizeof(TooLarge)
		}
	) != NULL ) {
		xrtWsGroupDestroy(pGroup);
		return 1;
	}
	xrtClearError();
	xrtWsGroupDestroy(pGroup);
	return 0;
}
