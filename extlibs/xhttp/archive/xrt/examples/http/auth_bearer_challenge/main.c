#include <stdio.h>

#include <xrt.h>



/* 构建带标准错误信息的 Bearer challenge。 */
int main(void)
{
	xhttpbearerchallenge Challenge = {
		XHTTP_BEARER_HAS_REALM |
		XHTTP_BEARER_HAS_ERROR |
		XHTTP_BEARER_HAS_ERROR_DESCRIPTION,
		XRT_STR_INIT("api"),
		{ NULL, 0 },
		XRT_STR_INIT("invalid_token"),
		XRT_STR_INIT("The access token expired"),
		{ NULL, 0 }
	};
	char Value[160];
	size_t iSize;

	if ( !xrtHttpBearerChallengeWrite(
		&Challenge, Value, sizeof(Value), &iSize
	) ) {
		return 1;
	}
	printf("%.*s\n", (int)iSize, Value);
	return 0;
}
