#include <stdio.h>

#include <xrt.h>



/* 展示归档和静态资源入口的可移植词法校验。 */
int main(void)
{
	cstr arrEntries[] = {
		"assets/icon.png",
		"../secret.txt",
		"CON.txt"
	};

	for ( size_t i = 0; i < sizeof(arrEntries) / sizeof(arrEntries[0]); i++ ) {
		printf("%s: %s\n", arrEntries[i],
			xrtPathIsSafeEntry(xrtStrView(arrEntries[i]), false) ?
			"safe" : "rejected");
	}
	return 0;
}
