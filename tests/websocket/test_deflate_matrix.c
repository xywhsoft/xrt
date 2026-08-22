#include "../test.h"



/* 为矩阵测试设置可选窗口参数。 */
static void testDeflateWindow(
	xwsdeflate* pConfig,
	uint32 iFlag,
	uint8 iBits
)
{
	if ( iBits == 0 ) {
		return;
	}
	pConfig->Flags |= iFlag;
	if ( iFlag == XWS_DEFLATE_SERVER_MAX_WINDOW ) {
		pConfig->ServerMaxWindowBits = iBits;
	} else {
		pConfig->ClientMaxWindowBits = iBits;
	}
}



/* 验证 server_no_context_takeover 的请求与主动响应规则。 */
static void testDeflateServerContextMatrix(void)
{
	for ( uint32 iOffer = 0; iOffer < 2u; iOffer++ ) {
		for ( uint32 iResponse = 0; iResponse < 2u; iResponse++ ) {
			xwsdeflate Offer;
			xwsdeflate Response;
			bool bExpected = (iOffer == 0) || (iResponse != 0);
			bool bResult;

			xrtWsDeflateInit(&Offer);
			xrtWsDeflateInit(&Response);
			if ( iOffer != 0 ) {
				Offer.Flags |=
					XWS_DEFLATE_SERVER_NO_CONTEXT;
			}
			if ( iResponse != 0 ) {
				Response.Flags |=
					XWS_DEFLATE_SERVER_NO_CONTEXT;
			}
			bResult = xrtWsDeflateResponseCheck(
				&Offer,
				&Response
			);
			testRequire(
				bResult == bExpected,
				"server_no_context_takeover matrix mismatch"
			);
			if ( !bResult ) {
				xrtClearError();
			}
		}
	}
}



/* 验证 client_no_context_takeover 是服务端可独立提出的响应约束。 */
static void testDeflateClientContextMatrix(void)
{
	for ( uint32 iOffer = 0; iOffer < 2u; iOffer++ ) {
		for ( uint32 iResponse = 0; iResponse < 2u; iResponse++ ) {
			xwsdeflate Offer;
			xwsdeflate Response;

			xrtWsDeflateInit(&Offer);
			xrtWsDeflateInit(&Response);
			if ( iOffer != 0 ) {
				Offer.Flags |=
					XWS_DEFLATE_CLIENT_NO_CONTEXT;
			}
			if ( iResponse != 0 ) {
				Response.Flags |=
					XWS_DEFLATE_CLIENT_NO_CONTEXT;
			}
			testRequire(
				xrtWsDeflateResponseCheck(
					&Offer,
					&Response
				),
				"client_no_context_takeover matrix mismatch"
			);
		}
	}
}



/* 验证 server_max_window_bits 必须确认且不得放宽。 */
static void testDeflateServerWindowMatrix(void)
{
	for ( uint8 iOffer = 0;
		iOffer <= XWS_DEFLATE_WINDOW_MAX;
		iOffer++ ) {
		if ( (iOffer != 0) &&
			(iOffer < XWS_DEFLATE_WINDOW_MIN) ) {
			continue;
		}
		for ( uint8 iResponse = 0;
			iResponse <= XWS_DEFLATE_WINDOW_MAX;
			iResponse++ ) {
			xwsdeflate Offer;
			xwsdeflate Response;
			bool bExpected;
			bool bResult;

			if ( (iResponse != 0) &&
				(iResponse < XWS_DEFLATE_WINDOW_MIN) ) {
				continue;
			}
			xrtWsDeflateInit(&Offer);
			xrtWsDeflateInit(&Response);
			testDeflateWindow(
				&Offer,
				XWS_DEFLATE_SERVER_MAX_WINDOW,
				iOffer
			);
			testDeflateWindow(
				&Response,
				XWS_DEFLATE_SERVER_MAX_WINDOW,
				iResponse
			);
			bExpected = (iOffer == 0) ||
				((iResponse != 0) &&
				 (iResponse <= iOffer));
			bResult = xrtWsDeflateResponseCheck(
				&Offer,
				&Response
			);
			testRequire(
				bResult == bExpected,
				"server_max_window_bits matrix mismatch"
			);
			if ( !bResult ) {
				xrtClearError();
			}
		}
	}
}



/* 验证 client_max_window_bits 只要求 offer 中存在，不把提示值当上限。 */
static void testDeflateClientWindowMatrix(void)
{
	for ( uint8 iOffer = 0;
		iOffer <= XWS_DEFLATE_WINDOW_MAX;
		iOffer++ ) {
		if ( (iOffer > 1u) &&
			(iOffer < XWS_DEFLATE_WINDOW_MIN) ) {
			continue;
		}
		for ( uint8 iResponse = 0;
			iResponse <= XWS_DEFLATE_WINDOW_MAX;
			iResponse++ ) {
			xwsdeflate Offer;
			xwsdeflate Response;
			bool bExpected;
			bool bResult;

			if ( (iResponse != 0) &&
				(iResponse < XWS_DEFLATE_WINDOW_MIN) ) {
				continue;
			}
			xrtWsDeflateInit(&Offer);
			xrtWsDeflateInit(&Response);
			if ( iOffer == 1u ) {
				Offer.Flags |=
					XWS_DEFLATE_CLIENT_MAX_WINDOW |
					XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
			} else if ( iOffer >=
				XWS_DEFLATE_WINDOW_MIN ) {
				testDeflateWindow(
					&Offer,
					XWS_DEFLATE_CLIENT_MAX_WINDOW,
					iOffer
				);
			}
			testDeflateWindow(
				&Response,
				XWS_DEFLATE_CLIENT_MAX_WINDOW,
				iResponse
			);
			bExpected = (iResponse == 0) ||
				((Offer.Flags &
				  XWS_DEFLATE_CLIENT_MAX_WINDOW) != 0);
			bResult = xrtWsDeflateResponseCheck(
				&Offer,
				&Response
			);
			testRequire(
				bResult == bExpected,
				"client_max_window_bits matrix mismatch"
			);
			if ( !bResult ) {
				xrtClearError();
			}
		}
	}
}



/* 验证最小接受结果只确认 offer 的服务端方向强制约束。 */
static void testDeflateAccept(void)
{
	xwsdeflate Offer;
	xwsdeflate Response;

	xrtWsDeflateInit(&Offer);
	Offer.Flags =
		XWS_DEFLATE_SERVER_NO_CONTEXT |
		XWS_DEFLATE_CLIENT_NO_CONTEXT |
		XWS_DEFLATE_SERVER_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW |
		XWS_DEFLATE_CLIENT_MAX_WINDOW_ANY;
	Offer.ServerMaxWindowBits = 10u;
	testRequire(
		xrtWsDeflateAccept(
			&Offer,
			&Response
		) &&
		(Response.Flags == (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_SERVER_MAX_WINDOW
		)) &&
		(Response.ServerMaxWindowBits == 10u) &&
		(Response.ClientMaxWindowBits == 15u) &&
		xrtWsDeflateResponseCheck(
			&Offer,
			&Response
		),
		"minimal permessage-deflate acceptance mismatch"
	);

	testRequire(
		xrtWsDeflateAccept(
			&Offer,
			&Offer
		) &&
		(Offer.Flags == (
			XWS_DEFLATE_SERVER_NO_CONTEXT |
			XWS_DEFLATE_SERVER_MAX_WINDOW
		)),
		"in-place permessage-deflate acceptance failed"
	);
}



/* 执行 permessage-deflate 协商矩阵。 */
int main(void)
{
	testDeflateServerContextMatrix();
	testDeflateClientContextMatrix();
	testDeflateServerWindowMatrix();
	testDeflateClientWindowMatrix();
	testDeflateAccept();
	printf("[PASS] websocket_deflate_matrix\n");
	return 0;
}
