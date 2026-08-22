#define XRT_IMPLEMENTATION
#include "../../single/xrt.h"

#include "../fixtures/x509_name_vectors.h"

#include <stdio.h>



/* 验证单头文件中的跨编码 Name 比较和子树判断。 */
int main(void)
{
	if ( (xrtX509NameEqual(
		(xbytesview) { X509_NAME_UTF8_SHARP_S,
			sizeof(X509_NAME_UTF8_SHARP_S) },
		(xbytesview) { X509_NAME_PRINTABLE_STRASSE,
			sizeof(X509_NAME_PRINTABLE_STRASSE) }
	) != X509_VALUE) || (xrtX509NameWithin(
		(xbytesview) { X509_NAME_CHILD, sizeof(X509_NAME_CHILD) },
		(xbytesview) { X509_NAME_BASE, sizeof(X509_NAME_BASE) }
	) != X509_VALUE) ) {
		return 1;
	}
	printf("[PASS] single-x509-name\n");
	return 0;
}
