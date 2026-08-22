#include <stdio.h>
#include <xrt.h>



/* 演示解析 absolute-form 并选择 target 中的有效 authority。 */
int main(void)
{
	xhttptarget Target;
	xhttpauthority Authority;

	if ( !xrtHttpTargetParse(
		XRT_STR_LITERAL("GET"),
		XRT_STR_LITERAL(
			"https://example.test:8443/items?q=1"
		),
		&Target
	) || !xrtHttpTargetAuthority(
		&Target,
		XRT_STR_LITERAL("ignored.test"),
		&Authority
	) ) {
		return 1;
	}
	printf(
		"host=%.*s port=%u path=%.*s\n",
		(int)Authority.Host.Size,
		Authority.Host.Data,
		(unsigned)Authority.Port,
		(int)Target.Path.Size,
		Target.Path.Data
	);
	return 0;
}
