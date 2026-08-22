#include <xrt/http_origin.h>

#include <stdio.h>



/* 解析请求 Origin，并与服务允许的 Origin 比较。 */
int main(void)
{
	xhttporigin Request;
	xhttporigin Allowed;
	char sCanonical[128];
	size_t iSize;

	if ( !xrtHttpOriginParse(
		XRT_STR_LITERAL("HTTPS://App.Example:443"), &Request
	) || !xrtHttpOriginParse(
		XRT_STR_LITERAL("https://app.example"), &Allowed
	) || !xrtHttpOriginSame(&Request, &Allowed) ||
		!xrtHttpOriginWrite(
			&Request, sCanonical, sizeof(sCanonical), &iSize
		) ) {
		return 1;
	}
	printf("allowed origin: %.*s\n", (int)iSize, sCanonical);
	return 0;
}
