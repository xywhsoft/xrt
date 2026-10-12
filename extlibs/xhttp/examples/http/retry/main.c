#include <xrt/http_retry.h>

#include <stdio.h>



/* 读取服务端建议，并转换为等待时间。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Retry-After"),
			XRT_STR_INIT("5")
		}
	};
	xhttpretryafter Retry;
	char Value[30];
	uint64 iDelay;
	size_t iSize;

	if ( !xrtHttpRetryAfterParse(Fields[0].Value, &Retry) ||
		!xrtHttpRetryAfterWrite(
			&Retry, Value, sizeof(Value), &iSize
		) ) {
		return 1;
	}

	if ( xrtHttpRetryAfterFields(
		Fields, 1, 0, &iDelay
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	printf(
		"Retry-After: %.*s, wait %llu milliseconds\n",
		(int)iSize,
		Value,
		(unsigned long long)iDelay
	);
	return 0;
}


