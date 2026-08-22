#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"



/* 单头文件必须独立提供 TLS 1.3 CertificateRequest 严格解析。 */
int main(void)
{
	static const uint8 Body[] = {
		0, 0, 8, 0, 13, 0, 4, 0, 2, 4, 3
	};
	xtls13certificaterequest Request;

	return xrtTls13CertificateRequestParse(
		(xbytesview) { Body, sizeof(Body) }, &Request
	) && (xrtTlsIdsCount(&Request.Signatures) == 1u) ? 0 : 1;
}
