#include <xrt.h>

#include <stdio.h>



/* 使用系统默认关联程序打开命令行给出的文件或 URI。 */
int main(int argc, char** argv)
{
	if ( argc != 2 ) {
		printf("usage: process-open <file-or-uri>\n");
		return 0;
	}
	if ( !xrtProcessOpen(argv[1]) ) {
		const xerror* pError = xrtGetError();

		fprintf(
			stderr,
			"open failed: %s (system=%d)\n",
			pError != NULL ? xrtErrorMessage(pError) : "unknown error",
			pError != NULL ? xrtErrorSystemCode(pError) : 0
		);
		return 1;
	}
	return 0;
}
