#include <xrt/http_encoding.h>

#include <stdio.h>



/* 展示零分配解析与服务器可用编码选择。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Content-Encoding"),
			XRT_STR_INIT("gzip, identity")
		},
		{
			XRT_STR_INIT("content-encoding"),
			XRT_STR_INIT("deflate")
		}
	};
	xhttpacceptencoding Accept;
	xhttpcontentencodingplan Plan;
	xhttpcoding Coding;
	xstrview Name;

	xrtHttpAcceptEncodingInit(&Accept);
	if ( !xrtHttpAcceptEncodingAdd(
		&Accept,
		XRT_STR_LITERAL(
			"gzip;q=0.8, deflate;q=0.4, identity;q=0.1"
		)
	) ) {
		return 1;
	}
	Coding = xrtHttpAcceptEncodingSelect(
		&Accept,
		XHTTP_CODING_IDENTITY |
			XHTTP_CODING_GZIP |
			XHTTP_CODING_DEFLATE,
		XHTTP_CODING_GZIP
	);
	Name = xrtHttpCodingName(Coding);
	if ( !xrtHttpContentEncodingPlan(
		Fields,
		sizeof(Fields) / sizeof(Fields[0]),
		&Plan
	) ) {
		return 1;
	}
	printf(
		"coding=%.*s decoders=%zu\n",
		(int)Name.Size,
		Name.Data,
		Plan.DecoderCount
	);
	return (Coding == XHTTP_CODING_GZIP) &&
		(Plan.DecoderCount == 2) ? 0 : 1;
}
