#include <stdio.h>

#include <xhttp.h>



/* 查看 Digest 算法元数据。 */
int main(void)
{
	xhttpdigestalgorithm Algorithm = xrtHttpDigestAlgorithmParse(
		XRT_STR_LITERAL("SHA-256")
	);
	xstrview Name = xrtHttpDigestAlgorithmName(Algorithm);

	printf(
		"%.*s: %zu bytes\n",
		(int)Name.Size,
		Name.Data,
		xrtHttpDigestSize(Algorithm)
	);
	return 0;
}
