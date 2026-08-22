#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_crl_vectors.h"

#include <stdio.h>



/* 验证单头文件中的 CRL 解析、条目游标和查找。 */
int main(void)
{
	static const uint8 Serial[] = { 0x20, 0x02 };
	xx509crl Crl;
	xx509crlcursor Cursor;
	xx509crlentry Entry;

	if ( !xrtX509CrlParse(
		X509_CRL_V2, sizeof(X509_CRL_V2), &Crl
	) || (Crl.Version != X509_CRL_VERSION_2) ||
		!xrtX509CrlEntryInit(&Crl, &Cursor) ||
		(xrtX509CrlEntryRead(&Cursor, &Entry) != X509_VALUE) ||
		(xrtX509CrlFind(
			&Crl, (xbytesview) { Serial, sizeof(Serial) }, NULL
		) != X509_VALUE) ) {
		return 1;
	}
	printf("[PASS] single-x509-crl\n");
	return 0;
}
