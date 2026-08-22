#include <stdio.h>

#include <xrt.h>



/* 从静态资源路径取得可直接写入 Content-Type 的媒体类型。 */
int main(void)
{
	cstr sType = xrtMime("assets/app.min.js");

	printf("Content-Type: %s\n", sType);
	return 0;
}
