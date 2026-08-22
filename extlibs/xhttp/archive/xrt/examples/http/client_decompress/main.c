#include <stdio.h>

#include <xrt/http_client.h>
#include <xrt/http_client_runtime.h>



/* 配置客户端自动解码 gzip/deflate，并展示单次调用保留原始正文的覆盖方式。 */
int main(void)
{
	xhttpclientconfig ClientConfig;
	xhttpcalloptions CallOptions;

	xrtHttpClientConfigInit(&ClientConfig);
	ClientConfig.Decompress.Enabled = true;
	ClientConfig.Decompress.MaxBody = UINT64_C(16) *
		UINT64_C(1024) * UINT64_C(1024);
	ClientConfig.Decompress.MaxCodings = 4u;

	xrtHttpCallOptionsInit(&CallOptions);
	CallOptions.Decompress = XHTTP_DECOMPRESS_RAW;
	printf(
		"auto=%s max-body=%llu raw-call=%s\n",
		ClientConfig.Decompress.Enabled ? "yes" : "no",
		(unsigned long long)ClientConfig.Decompress.MaxBody,
		CallOptions.Decompress == XHTTP_DECOMPRESS_RAW ?
			"yes" : "no"
	);
	return 0;
}
