#include <stdio.h>

#include <xrt/http_proxy_status.h>



/* 从 DNS 展示形式名称生成 next-hop-aliases String 正文。 */
int main(void)
{
	static const xstrview Aliases[] = {
		XRT_STR_INIT("comma,name.example"),
		XRT_STR_INIT("dot\\.label.example")
	};
	char arrOutput[128];
	size_t iSize;

	if ( !xrtHttpProxyAliasesWrite(
		Aliases, 2u, arrOutput, sizeof(arrOutput), &iSize
	) ) {
		return 1;
	}
	printf(
		"next-hop-aliases = \"%.*s\"\n",
		(int)iSize, arrOutput
	);
	return 0;
}
