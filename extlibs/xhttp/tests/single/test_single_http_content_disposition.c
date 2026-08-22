#ifdef XHTTP_MODULE_XHTTP
	#undef XHTTP_MODULE_XHTTP
#endif
#define XHTTP_MODULE_HTTP_CONTENT_DISPOSITION
#define XHTTP_IMPLEMENTATION
#include "../../single/xhttp.h"



/* 验证 Content-Disposition 与 UTF-8 filename* 主路径。 */
int main(void)
{
	xcontentdisposition Disposition;
	char Name[32];
	size_t iSize = 0;

	return xrtHttpContentDispositionParse(
		XRT_STR_LITERAL(
			"form-data; name=upload; filename*=UTF-8''xrt%2Etxt"
		),
		&Disposition
	) && xrtHttpContentDispositionFileNameWrite(
		&Disposition, Name, sizeof(Name), &iSize
	) && (iSize == 7u) && (memcmp(Name, "xrt.txt", 7u) == 0) ? 0 : 1;
}
