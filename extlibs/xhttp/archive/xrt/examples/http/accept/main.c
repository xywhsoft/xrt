#include <stdio.h>

#include <xrt/http_accept.h>



/* 从服务端偏好顺序中选择客户端可接受的表示。 */
int main(void)
{
	static const xhttpfield Fields[] = {
		{
			XRT_STR_INIT("Accept"),
			XRT_STR_INIT(
				"text/html;q=0.7, "
				"application/json;q=1;profile=full"
			)
		}
	};
	static const xstrview Available[] = {
		XRT_STR_INIT("text/html; charset=UTF-8"),
		XRT_STR_INIT("application/json; profile=full")
	};
	size_t iIndex;

	if ( xrtHttpAcceptSelect(
		Fields,
		1,
		Available,
		sizeof(Available) / sizeof(Available[0]),
		&iIndex
	) != XHTTP_NEXT_ITEM ) {
		return 1;
	}
	printf(
		"selected = %.*s\n",
		(int)Available[iIndex].Size,
		Available[iIndex].Data
	);
	return 0;
}
